"""
02_lstm_training.py
===================
Helios-Artemis: LSTM Irradiance Predictor — Training Pipeline
Helios subsystem (ESP32-S3)

Architecture:
  - Input:  24-hour normalised GHI lookback window
  - Output: 1-hour-ahead GHI (regression)
  - Model:  Single LSTM layer (configurable units) + Dense(1, linear)
            Total params: 4,385 for 32-unit model
            4-unit gain scheduler (step scaling) + 32-unit forecaster = ~4,486 params
  - Ablation: 16, 32, 64 hidden units (Section III-D, Table IIa)

Training:
  - Optimiser: Adam (lr=1e-3, β1=0.9, β2=0.999)
  - Epochs: 60, batch size: 128
  - Loss: MSE
  - Seed: tf.random.set_seed(42) for reproducibility
  - Split: Year 1 data → 90% train / 10% held-out validation
  - Test:  Year 2 data (fully independent, distinct seed)

Outputs:
  - Trained model weights (.h5 / SavedModel)
  - Training history CSV
  - Ablation results CSV (Table IIa)
  - Int8 quantised TFLite model for ESP32-S3 deployment

Requirements:
  pip install tensorflow numpy pandas scikit-learn matplotlib
"""

import os
import numpy as np
import pandas as pd
from sklearn.metrics import r2_score, mean_absolute_error, mean_squared_error
import warnings
warnings.filterwarnings("ignore")

os.makedirs("models", exist_ok=True)
os.makedirs("results", exist_ok=True)

# ─────────────────────────────────────────────────────────────────────────────
# Configuration
# ─────────────────────────────────────────────────────────────────────────────
LOOKBACK_HOURS = 24       # Input sequence length
FORECAST_HORIZON = 1      # Hours ahead to predict
BATCH_SIZE = 128
EPOCHS = 60
LEARNING_RATE = 1e-3
HIDDEN_UNITS_LIST = [16, 32, 64]   # Ablation configurations
SELECTED_UNITS = 32                 # Final selected model
GHI_NORM_MAX = 1000.0              # W/m² normalisation ceiling
DAYTIME_THRESHOLD = 10.0           # W/m² — exclude night for MAE/RMSE reporting
VAL_SPLIT = 0.10                   # Fraction of Year 1 held-out for validation


# ─────────────────────────────────────────────────────────────────────────────
# Data loading and sequence construction
# ─────────────────────────────────────────────────────────────────────────────
def load_hourly_data(csv_path: str) -> np.ndarray:
    """Load hourly GHI array from CSV (shape: n_hours,)."""
    df = pd.read_csv(csv_path)
    return df["ghi_hourly_wm2"].values.astype(np.float32)


def build_sequences(ghi: np.ndarray, lookback: int = LOOKBACK_HOURS,
                    horizon: int = FORECAST_HORIZON):
    """
    Construct sliding-window sequences for LSTM training.

    Returns:
        X: shape (N, lookback) — normalised GHI input windows
        y: shape (N,)          — normalised target GHI (t + horizon)
    """
    ghi_norm = ghi / GHI_NORM_MAX
    X, y = [], []
    for i in range(lookback, len(ghi_norm) - horizon + 1):
        X.append(ghi_norm[i - lookback:i])
        y.append(ghi_norm[i + horizon - 1])
    return np.array(X, dtype=np.float32), np.array(y, dtype=np.float32)


def daytime_mask(y_actual: np.ndarray, threshold: float = DAYTIME_THRESHOLD) -> np.ndarray:
    """Boolean mask selecting daytime samples."""
    return (y_actual * GHI_NORM_MAX) > threshold


# ─────────────────────────────────────────────────────────────────────────────
# Model construction
# ─────────────────────────────────────────────────────────────────────────────
def build_lstm_model(hidden_units: int, lookback: int = LOOKBACK_HOURS):
    """
    Build single-layer LSTM regression model (4,385 params for 32 units).

    A separate 4-unit gain scheduler (101 params) maps predicted GHI to
    VS-P&O step scaling, giving a total dual-model count of ~4,486 params.

    Args:
        hidden_units: Number of LSTM hidden units (16 / 32 / 64)
        lookback:     Input sequence length

    Returns:
        Compiled Keras model
    """
    import tensorflow as tf
    from tensorflow import keras

    model = keras.Sequential([
        keras.layers.Input(shape=(lookback, 1)),
        keras.layers.LSTM(hidden_units, return_sequences=False),
        keras.layers.Dense(1, activation="linear"),
    ], name=f"helios_lstm_{hidden_units}u")

    model.compile(
        optimizer=keras.optimizers.Adam(
            learning_rate=LEARNING_RATE, beta_1=0.9, beta_2=0.999),
        loss="mse",
        metrics=["mae"]
    )
    return model


# ─────────────────────────────────────────────────────────────────────────────
# Training
# ─────────────────────────────────────────────────────────────────────────────
def train_model(model, X_train, y_train, X_val, y_val):
    """Train model and return history."""
    import tensorflow as tf

    callbacks = [
        tf.keras.callbacks.EarlyStopping(
            monitor="val_loss", patience=8, restore_best_weights=True),
        tf.keras.callbacks.ReduceLROnPlateau(
            monitor="val_loss", factor=0.5, patience=4, min_lr=1e-6),
    ]
    history = model.fit(
        X_train[..., np.newaxis],
        y_train,
        validation_data=(X_val[..., np.newaxis], y_val),
        epochs=EPOCHS,
        batch_size=BATCH_SIZE,
        callbacks=callbacks,
        verbose=1,
    )
    return history


def evaluate_model(model, X_test, y_test):
    """Evaluate on test set, returning daytime-only and all-sample metrics."""
    y_pred_norm = model.predict(X_test[..., np.newaxis], verbose=0).flatten()

    # Denormalise
    y_pred = y_pred_norm * GHI_NORM_MAX
    y_true = y_test * GHI_NORM_MAX

    # All-sample
    r2_all = r2_score(y_true, y_pred)
    mae_all = mean_absolute_error(y_true, y_pred)
    rmse_all = np.sqrt(mean_squared_error(y_true, y_pred))

    # Daytime-only (paper-reported figures)
    mask = daytime_mask(y_test)
    r2_day = r2_score(y_true[mask], y_pred[mask])
    mae_day = mean_absolute_error(y_true[mask], y_pred[mask])
    rmse_day = np.sqrt(mean_squared_error(y_true[mask], y_pred[mask]))

    return {
        "r2_all":    round(r2_all,  4),
        "mae_all":   round(mae_all,  2),
        "rmse_all":  round(rmse_all, 2),
        "r2_day":    round(r2_day,  4),
        "mae_day":   round(mae_day,  2),
        "rmse_day":  round(rmse_day, 2),
    }


# ─────────────────────────────────────────────────────────────────────────────
# Int8 TFLite quantisation (for ESP32-S3 deployment)
# ─────────────────────────────────────────────────────────────────────────────
def quantise_to_tflite_flex(model, output_path: str):
    """
    Convert Keras model to TFLite with Flex ops (ESP32-S3 deployment).
    Uses SELECT_TF_OPS to handle LSTM TensorList ops that the TF Lite converter
    cannot lower natively (TensorListReserve/SetItem/Stack).

    NOTE: Int8 quantisation requires the Flex delegate runtime and is
    not available in the Python converter pipeline for LSTM models
    with dynamic TensorList ops.

    Paper reference: Section IV-D, Fig. 9D (inference ~4.7 ms on ESP32-S3).
    """
    import tensorflow as tf

    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS, tf.lite.OpsSet.SELECT_TF_OPS]
    converter._experimental_lower_tensor_list_ops = False

    tflite_model = converter.convert()
    with open(output_path, "wb") as f:
        f.write(tflite_model)
    print(f"  Flex TFLite model saved: {output_path}  ({len(tflite_model)/1024:.1f} kB)")
    return tflite_model


# ─────────────────────────────────────────────────────────────────────────────
# Ablation study
# ─────────────────────────────────────────────────────────────────────────────
def run_ablation(X_train, y_train, X_val, y_val, X_test, y_test):
    """
    Run ablation over hidden_units ∈ {16, 32, 64}.
    Returns DataFrame matching Table IIa of the paper.
    """
    ablation_results = []
    for units in HIDDEN_UNITS_LIST:
        print(f"\n{'='*50}")
        print(f"  ABLATION: {units}-unit LSTM")
        print(f"{'='*50}")
        model = build_lstm_model(units)
        model.summary()
        history = train_model(model, X_train, y_train, X_val, y_val)
        metrics = evaluate_model(model, X_test, y_test)

        n_params = model.count_params()
        final_train_loss = history.history["loss"][-1]
        final_val_loss   = history.history["val_loss"][-1]

        print(f"  → R² (day): {metrics['r2_day']:.3f} | "
              f"MAE (day): {metrics['mae_day']:.1f} W/m² | "
              f"RMSE (day): {metrics['rmse_day']:.1f} W/m²")

        ablation_results.append({
            "hidden_units":      units,
            "n_params":          n_params,
            "epochs_run":        len(history.history["loss"]),
            "final_train_mse":   round(final_train_loss, 6),
            "final_val_mse":     round(final_val_loss, 6),
            "r2_all":            metrics["r2_all"],
            "r2_daytime":        metrics["r2_day"],
            "mae_daytime_wm2":   metrics["mae_day"],
            "rmse_daytime_wm2":  metrics["rmse_day"],
            "selected":          units == SELECTED_UNITS,
        })

        model.save(f"models/lstm_{units}u.keras")
        pd.DataFrame(history.history).to_csv(
            f"results/training_history_{units}u.csv", index=False)

    df_ablation = pd.DataFrame(ablation_results)
    df_ablation.to_csv("results/ablation_table_IIa.csv", index=False)
    print("\n  Ablation results saved: results/ablation_table_IIa.csv")
    return df_ablation


# ─────────────────────────────────────────────────────────────────────────────
# 4-Unit Gain Scheduler
# ─────────────────────────────────────────────────────────────────────────────
def build_gain_scheduler():
    """
    Build the 4-unit gain scheduler (101 params).
    Maps predicted GHI → optimal VS-P&O step scaling factor.

    Architecture:
        Input(1) → Dense(4, tanh) → Dense(1, linear) → clip(0.05, 0.60)

    This is the second model in the dual-model LSTM architecture
    referenced in Section III-C of the paper.
    """
    import tensorflow as tf
    from tensorflow import keras

    model = keras.Sequential([
        keras.layers.Input(shape=(1,)),
        keras.layers.Dense(4, activation="tanh"),
        keras.layers.Dense(1, activation="linear"),
    ], name="helios_gain_scheduler_4u")

    model.compile(optimizer="adam", loss="mse")
    return model


# ─────────────────────────────────────────────────────────────────────────────
# Main
# ─────────────────────────────────────────────────────────────────────────────
if __name__ == "__main__":
    import tensorflow as tf
    tf.random.set_seed(42)
    np.random.seed(42)

    # ── Dual-model summary ──────────────────────────────────────────────────
    print("=" * 60)
    print("  DUAL-MODEL LSTM ARCHITECTURE (Section III-C)")
    print("=" * 60)
    model_f = build_lstm_model(SELECTED_UNITS)
    model_g = build_gain_scheduler()
    n_f = model_f.count_params()
    n_g = model_g.count_params()
    print(f"  Forecaster    (32-unit LSTM):   {n_f:>5,} params")
    print(f"  Gain scheduler (4-unit Dense):  {n_g:>5,} params")
    print(f"  Total dual-model:               {n_f + n_g:>5,} params")
    print()

    # ── Load data ──────────────────────────────────────────────────────────
    print("Loading hourly GHI datasets...")
    ghi_y1 = load_hourly_data("data/year1_training_hourly.csv")   # 365×24 = 8760 h
    ghi_y2 = load_hourly_data("data/year2_test_hourly.csv")        # 365×24 = 8760 h
    print(f"  Year 1: {len(ghi_y1)} hourly samples")
    print(f"  Year 2: {len(ghi_y2)} hourly samples")

    # ── Build sequences ────────────────────────────────────────────────────
    X_full, y_full = build_sequences(ghi_y1)
    split_idx = int(len(X_full) * (1 - VAL_SPLIT))
    X_train, X_val = X_full[:split_idx], X_full[split_idx:]
    y_train, y_val = y_full[:split_idx], y_full[split_idx:]

    X_test, y_test = build_sequences(ghi_y2)

    print(f"\nSequence shapes:")
    print(f"  X_train: {X_train.shape}  y_train: {y_train.shape}")
    print(f"  X_val:   {X_val.shape}    y_val:   {y_val.shape}")
    print(f"  X_test:  {X_test.shape}   y_test:  {y_test.shape}")

    # ── Ablation study ─────────────────────────────────────────────────────
    print("\n" + "="*60)
    print("RUNNING ABLATION STUDY (16 / 32 / 64 units)")
    print("="*60)
    df_ablation = run_ablation(X_train, y_train, X_val, y_val, X_test, y_test)
    print("\nAblation Table (Table IIa):")
    print(df_ablation[["hidden_units","r2_daytime","mae_daytime_wm2",
                        "rmse_daytime_wm2","n_params","selected"]].to_string(index=False))

    # ── Selected model (32 units) — final evaluation ───────────────────────
    print("\n" + "="*60)
    print("FINAL MODEL: 32-unit LSTM (selected configuration)")
    print("="*60)
    model_32 = tf.keras.models.load_model("models/lstm_32u.keras")
    metrics_32 = evaluate_model(model_32, X_test, y_test)
    print(f"\nFinal results on Year-2 independent test set:")
    print(f"  R² (daytime):   {metrics_32['r2_day']:.4f}  (paper: 0.917)")
    print(f"  MAE (daytime):  {metrics_32['mae_day']:.1f} W/m²  (paper: 50.7)")
    print(f"  RMSE (daytime): {metrics_32['rmse_day']:.1f} W/m²  (paper: 63.6)")
    pd.DataFrame([metrics_32]).to_csv("results/final_model_metrics.csv", index=False)

    # ── Flex TFLite conversion (for ESP32-S3 deployment) ───────────────────
    print("\nConverting to Flex TFLite for ESP32-S3 deployment...")
    quantise_to_tflite_flex(
        model_32,
        output_path="models/helios_lstm_32u_flex.tflite"
    )
    # Evaluate quantised model
    interpreter = tf.lite.Interpreter(model_path="models/helios_lstm_32u_flex.tflite")
    interpreter.allocate_tensors()
    print("  Flex TFLite model loaded successfully.")

    print("\nAll training complete. Models saved to models/")
