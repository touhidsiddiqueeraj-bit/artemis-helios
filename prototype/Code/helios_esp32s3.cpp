/**
 * =============================================================================
 * HELIOS — ESP32-S3 Firmware (Arduino / ESP-IDF Arduino layer)
 * Helios-Artemis Dual-MCU Predictive MPPT Controller
 * =============================================================================
 *
 * Responsibilities:
 *   - LSTM inference: 32-unit irradiance forecaster + 4-unit gain scheduler
 *   - Irradiance acquisition: GY302 (I²C, 0x23) + OV2640 green-channel
 *   - UART TX to Artemis: predicted V_MPP, irradiance, blend weight α
 *   - UART RX from Artemis: V_bat, I_bat, duty, charge state, G_est
 *   - SD card data logging (SPI): timestamp, G, V_bat, I_bat, η_mppt
 *   - Browser-accessible web dashboard (WiFi AP mode)
 *   - Autonomous on-device LSTM retraining trigger (24-hour cycle)
 *   - TF.js-based weight export / re-import to SPIFFS
 *
 * Hardware:
 *   MCU      : ESP32-S3 DevKit-C, 240 MHz, 512 kB SRAM
 *   I²C      : GPIO 21 (SDA) / GPIO 22 (SCL)
 *     - GY302    @ 0x23 (irradiance)
 *     - INA219   @ 0x40 (shared bus, read by Artemis; Helios monitors)
 *   UART2    : GPIO 17 (TX) / GPIO 18 (RX), 115200 — Artemis link
 *   SPI      : GPIO 23 (MOSI) / 19 (MISO) / 18 (SCK) / 5 (CS) — SD card
 *   Camera   : OV2640, parallel DVOP on ESP32-S3-EYE / DevKit-C
 *   WiFi     : AP mode SSID "Helios-MPPT", IP 192.168.4.1
 *   SPIFFS   : LSTM weights JSON, config, training buffer
 *
 * LSTM architecture (paper §III-C):
 *   Model 1 (irradiance forecaster): 32 units, 1 layer, 24-step lookback
 *     Input:  24 normalised GHI samples (hourly), shape [1,24,1]
 *     Output: 1 scalar → predicted G 30-min ahead (W/m²)
 *     Params: 7,329  |  Inference: <12 ms (fp32), <4.7 ms (int8)
 *   Model 2 (gain scheduler): 4 units, 1 layer
 *     Input:  predicted G (normalised)
 *     Output: 1 scalar → P&O step scale [0.5, 2.0]
 *     Params: 101
 *
 * =============================================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SD.h>
#include <SPI.h>
#include <FS.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include "esp_camera.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

/* ─── Pin definitions ──────────────────────────────────────────────────────
 * ⚠ GPIO CONFLICT MATRIX (requires PCB rev to resolve):
 *   GPIO  5 : SD_CS  + Camera D0      — both peripherals cannot coexist
 *   GPIO 18 : UART RX + SD_SCK         — UART and SPI share the same pin
 *   GPIO 19 : SD_MISO + Camera D2      — Camera D2 not used on Helios-only
 *   GPIO 21 : SDA     + Camera D3      — I²C bus clashes with camera data
 *   GPIO 22 : SCL     + Camera PCLK    — I²C bus clashes with camera pixel clk
 *   GPIO 23 : SD_MOSI + Camera HREF    — SPI MOSI clashes with camera HREF
 * ──────────────────────────────────────────────────────────────────────── */
#define PIN_SDA         21
#define PIN_SCL         22
#define PIN_UART_TX     17      /* → Artemis RX (PA10)                        */
#define PIN_UART_RX     18      /* ← Artemis TX (PA9)                         */
#define PIN_SD_CS        5
#define PIN_SD_MOSI     23
#define PIN_SD_MISO     19
#define PIN_SD_SCK      18      /* conflicts with UART RX — fix in PCB rev     */

/* ─── GY302 (BH1750) command map ──────────────────────────────────────────── */
#define GY302_ADDR            0x23
#define GY302_CMD_POWER_ON    0x01
#define GY302_CMD_POWER_OFF   0x00
#define GY302_CMD_RESET       0x07
#define GY302_CMD_CONT_4LX    0x11  /* Continuous, 4 lx resolution, 16 ms */
/* 4lx mode (16 ms) fits comfortably in the 100 ms inference loop; 1lx mode     */
/* (0x10, 120 ms) would cause every other read to return stale data.            */

/* ─── Lux → irradiance conversion (calibrated from paper §III-D)
 *   G (W/m²) ≈ lux × 0.0079  (approximation for CIE AM1.5 spectrum)        */
#define LUX_TO_WM2            0.0079f

/* ─── MPPT / prediction constants ──────────────────────────────────────────*/
#define INFERENCE_INTERVAL_MS 100U      /* Match Artemis MPPT tick             */
#define LOG_INTERVAL_MS       1000U     /* SD log every 1 second              */
#define RETRAIN_INTERVAL_MS   86400000U /* 24 hours in ms                     */
#define LOOKBACK_STEPS        24        /* 24 hourly samples                  */
#define G_NORM_MAX            1000.0f   /* Normalisation ceiling (W/m²)        */
#define V_OC_STC              21.7f     /* 50 Wp mono-Si Voc at STC            */
#define VMPP_STC              17.8f     /* V_MPP at STC (matches PV S-function) */
#define K_VMPP                14.26f    /* from paper (exact MPP solution)     */
#define ALPHA_DEFAULT         0.35f     /* Optimal blend weight (paper §IV-A)  */

/* ─── WiFi AP credentials ──────────────────────────────────────────────────*/
/* WARNING: Default credentials are insecure. Override via build flags
 * -DWIFI_SSID=\"...\" -DWIFI_PASS=\"...\" in platformio.ini, or set custom
 * values in config.json. The defaults below are for development only.        */
#pragma message "WARNING: Using default WiFi credentials (Helios-MPPT / sylhet2026) — override for production"
#define WIFI_SSID             "Helios-MPPT"
#define WIFI_PASS             "sylhet2026"

/* ─── SPIFFS paths ──────────────────────────────────────────────────────── */
#define LSTM_WEIGHTS_PATH     "/lstm_weights.json"
#define TRAIN_BUFFER_PATH     "/train_buf.csv"
#define CONFIG_PATH           "/config.json"

/* ─── UART to Artemis ───────────────────────────────────────────────────── */
#define ARTEMIS_UART          Serial2

/* ============================================================
 * Minimal LSTM implementation
 * Weights are loaded from SPIFFS JSON at boot and after retraining.
 * Architecture: LSTM(32) → Dense(1) for irradiance forecaster
 *               LSTM(4)  → Dense(1) for gain scheduler
 * ============================================================ */

/* Maximum sizes to fit in SRAM (512 kB total) */
#define LSTM_H_SIZE_MAIN    32
#define LSTM_H_SIZE_GAIN     4
#define LSTM_IN_SIZE         1
#define DENSE_OUT            1

typedef struct {
    /* LSTM weights: kernel [input+h, 4h], bias [4h]                         */
    float Wf[LSTM_H_SIZE_MAIN][LSTM_IN_SIZE + LSTM_H_SIZE_MAIN]; /* forget    */
    float Wi[LSTM_H_SIZE_MAIN][LSTM_IN_SIZE + LSTM_H_SIZE_MAIN]; /* input     */
    float Wc[LSTM_H_SIZE_MAIN][LSTM_IN_SIZE + LSTM_H_SIZE_MAIN]; /* cell      */
    float Wo[LSTM_H_SIZE_MAIN][LSTM_IN_SIZE + LSTM_H_SIZE_MAIN]; /* output    */
    float bf[LSTM_H_SIZE_MAIN];
    float bi[LSTM_H_SIZE_MAIN];
    float bc[LSTM_H_SIZE_MAIN];
    float bo[LSTM_H_SIZE_MAIN];
    /* Dense output layer */
    float Wd[LSTM_H_SIZE_MAIN];
    float bd;
    /* Runtime state */
    float h[LSTM_H_SIZE_MAIN];
    float c[LSTM_H_SIZE_MAIN];
    uint8_t loaded;
} LSTMModel_t;

typedef struct {
    float Wf[LSTM_H_SIZE_GAIN][1 + LSTM_H_SIZE_GAIN];
    float Wi[LSTM_H_SIZE_GAIN][1 + LSTM_H_SIZE_GAIN];
    float Wc[LSTM_H_SIZE_GAIN][1 + LSTM_H_SIZE_GAIN];
    float Wo[LSTM_H_SIZE_GAIN][1 + LSTM_H_SIZE_GAIN];
    float bf[LSTM_H_SIZE_GAIN];
    float bi[LSTM_H_SIZE_GAIN];
    float bc[LSTM_H_SIZE_GAIN];
    float bo[LSTM_H_SIZE_GAIN];
    float Wd[LSTM_H_SIZE_GAIN];
    float bd;
    float h[LSTM_H_SIZE_GAIN];
    float c[LSTM_H_SIZE_GAIN];
    uint8_t loaded;
} GainModel_t;

static LSTMModel_t g_lstm;
static GainModel_t g_gain;

/* ─── Activation helpers ─────────────────────────────────────────────────── */

static inline float sigmoid(float x) {
    return 1.0f / (1.0f + expf(-x));
}
static inline float tanhf_f(float x) {
    return tanhf(x);
}

/* ─── Single LSTM step (main model, h_size=32, in_size=1) ─────────────────── */

static float lstm_step_main(LSTMModel_t *m, float x_in)
{
    /* Concatenate [x_in, h] into xh[1+32] */
    float xh[LSTM_IN_SIZE + LSTM_H_SIZE_MAIN];
    xh[0] = x_in;
    for (int i = 0; i < LSTM_H_SIZE_MAIN; i++) xh[1 + i] = m->h[i];

    float f[LSTM_H_SIZE_MAIN], in[LSTM_H_SIZE_MAIN];
    float cn[LSTM_H_SIZE_MAIN], o[LSTM_H_SIZE_MAIN];

    for (int i = 0; i < LSTM_H_SIZE_MAIN; i++) {
        float sf = m->bf[i], si = m->bi[i], sc = m->bc[i], so = m->bo[i];
        for (int j = 0; j < LSTM_IN_SIZE + LSTM_H_SIZE_MAIN; j++) {
            sf += m->Wf[i][j] * xh[j];
            si += m->Wi[i][j] * xh[j];
            sc += m->Wc[i][j] * xh[j];
            so += m->Wo[i][j] * xh[j];
        }
        f[i]  = sigmoid(sf);
        in[i] = sigmoid(si);
        cn[i] = tanhf_f(sc);
        o[i]  = sigmoid(so);
        m->c[i] = f[i] * m->c[i] + in[i] * cn[i];
        m->h[i] = o[i] * tanhf_f(m->c[i]);
    }

    /* Dense output */
    float out = m->bd;
    for (int i = 0; i < LSTM_H_SIZE_MAIN; i++) out += m->Wd[i] * m->h[i];
    return out;
}

/* ─── Single LSTM step (gain scheduler, h_size=4, in_size=1) ─────────────── */

static float lstm_step_gain(GainModel_t *m, float x_in)
{
    float xh[1 + LSTM_H_SIZE_GAIN];
    xh[0] = x_in;
    for (int i = 0; i < LSTM_H_SIZE_GAIN; i++) xh[1 + i] = m->h[i];

    float f[LSTM_H_SIZE_GAIN], in[LSTM_H_SIZE_GAIN];
    float cn[LSTM_H_SIZE_GAIN], o[LSTM_H_SIZE_GAIN];

    for (int i = 0; i < LSTM_H_SIZE_GAIN; i++) {
        float sf = m->bf[i], si = m->bi[i], sc = m->bc[i], so = m->bo[i];
        for (int j = 0; j < 1 + LSTM_H_SIZE_GAIN; j++) {
            sf += m->Wf[i][j] * xh[j];
            si += m->Wi[i][j] * xh[j];
            sc += m->Wc[i][j] * xh[j];
            so += m->Wo[i][j] * xh[j];
        }
        f[i]  = sigmoid(sf);
        in[i] = sigmoid(si);
        cn[i] = tanhf_f(sc);
        o[i]  = sigmoid(so);
        m->c[i] = f[i] * m->c[i] + in[i] * cn[i];
        m->h[i] = o[i] * tanhf_f(m->c[i]);
    }

    float out = m->bd;
    for (int i = 0; i < LSTM_H_SIZE_GAIN; i++) out += m->Wd[i] * m->h[i];
    return out;
}

/**
 * Run inference over a 24-step lookback window.
 * Returns predicted irradiance G_pred (W/m²).
 */
static float lstm_predict(float *lookback_norm, int steps)
{
    /* Reset hidden state before each inference sequence */
    memset(g_lstm.h, 0, sizeof(g_lstm.h));
    memset(g_lstm.c, 0, sizeof(g_lstm.c));

    float output = 0.0f;
    for (int t = 0; t < steps; t++) {
        output = lstm_step_main(&g_lstm, lookback_norm[t]);
    }
    /* Denormalise */
    return output * G_NORM_MAX;
}

/**
 * Run gain scheduler: input normalised G_pred, output step scale.
 */
static float gain_schedule(float g_pred_norm)
{
    memset(g_gain.h, 0, sizeof(g_gain.h));
    memset(g_gain.c, 0, sizeof(g_gain.c));
    float raw = lstm_step_gain(&g_gain, g_pred_norm);
    /* Scale output to [0.5, 2.0] via sigmoid-based mapping */
    float scale = 0.5f + 1.5f * sigmoid(raw);
    return scale;
}

/* ============================================================
 * LSTM weights — SPIFFS JSON loader
 * JSON schema (abbreviated):
 * {
 *   "lstm": {
 *     "Wf": [[...],[...]], "Wi": ..., "Wc": ..., "Wo": ...,
 *     "bf": [...], "bi": ..., "bc": ..., "bo": ...,
 *     "Wd": [...], "bd": 0.0
 *   },
 *   "gain": { ... same structure, h_size=4 ... }
 * }
 * ============================================================ */

static bool load_weights_from_spiffs(void)
{
    if (!SPIFFS.exists(LSTM_WEIGHTS_PATH)) {
        Serial.println("[HELIOS] No weights file found — using zero-init LSTM");
        /* Zero-init is non-functional but safe; system waits for retraining  */
        memset(&g_lstm, 0, sizeof(g_lstm));
        memset(&g_gain, 0, sizeof(g_gain));
        return false;
    }

    File f = SPIFFS.open(LSTM_WEIGHTS_PATH, "r");
    if (!f) return false;

    /* ArduinoJson with a large filter to avoid OOM on 7,429 floats            */
    DynamicJsonDocument doc(131072);  /* 128 KB budget                       */
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
        Serial.printf("[HELIOS] JSON parse error: %s\n", err.c_str());
        return false;
    }

    /* ── Load main LSTM weights ──────────────────────────────────── */
    JsonObject lstm = doc["lstm"];
    int H = LSTM_H_SIZE_MAIN, N = LSTM_IN_SIZE + H;
    for (int i = 0; i < H; i++) {
        for (int j = 0; j < N; j++) {
            g_lstm.Wf[i][j] = lstm["Wf"][i][j].as<float>();
            g_lstm.Wi[i][j] = lstm["Wi"][i][j].as<float>();
            g_lstm.Wc[i][j] = lstm["Wc"][i][j].as<float>();
            g_lstm.Wo[i][j] = lstm["Wo"][i][j].as<float>();
        }
        g_lstm.bf[i] = lstm["bf"][i].as<float>();
        g_lstm.bi[i] = lstm["bi"][i].as<float>();
        g_lstm.bc[i] = lstm["bc"][i].as<float>();
        g_lstm.bo[i] = lstm["bo"][i].as<float>();
        g_lstm.Wd[i] = lstm["Wd"][i].as<float>();
    }
    g_lstm.bd = lstm["bd"].as<float>();
    g_lstm.loaded = 1;

    /* ── Load gain scheduler weights ────────────────────────────── */
    JsonObject gain = doc["gain"];
    int HG = LSTM_H_SIZE_GAIN, NG = 1 + HG;
    for (int i = 0; i < HG; i++) {
        for (int j = 0; j < NG; j++) {
            g_gain.Wf[i][j] = gain["Wf"][i][j].as<float>();
            g_gain.Wi[i][j] = gain["Wi"][i][j].as<float>();
            g_gain.Wc[i][j] = gain["Wc"][i][j].as<float>();
            g_gain.Wo[i][j] = gain["Wo"][i][j].as<float>();
        }
        g_gain.bf[i] = gain["bf"][i].as<float>();
        g_gain.bi[i] = gain["bi"][i].as<float>();
        g_gain.bc[i] = gain["bc"][i].as<float>();
        g_gain.bo[i] = gain["bo"][i].as<float>();
        g_gain.Wd[i] = gain["Wd"][i].as<float>();
    }
    g_gain.bd = gain["bd"].as<float>();
    g_gain.loaded = 1;

    Serial.println("[HELIOS] LSTM weights loaded from SPIFFS");
    return true;
}

/* ============================================================
 * GY302 (BH1750) irradiance sensor driver
 * ============================================================ */

static bool gy302_init(void)
{
    /* Power on */
    Wire.beginTransmission(GY302_ADDR);
    Wire.write(GY302_CMD_POWER_ON);
    if (Wire.endTransmission() != 0) return false;
    delay(10);

    /* Reset */
    Wire.beginTransmission(GY302_ADDR);
    Wire.write(GY302_CMD_RESET);
    if (Wire.endTransmission() != 0) return false;
    delay(10);

    /* Start continuous measurement, 4 lx resolution (16 ms cycle) */
    Wire.beginTransmission(GY302_ADDR);
    Wire.write(GY302_CMD_CONT_4LX);
    if (Wire.endTransmission() != 0) return false;
    delay(20);  /* wait for first measurement to complete (16 ms typ) */
    return true;
}

/**
 * Read raw 16-bit lux value from GY302.
 */
static uint16_t gy302_read_raw(void)
{
    Wire.requestFrom((uint8_t)GY302_ADDR, (uint8_t)2);
    if (Wire.available() < 2) return 0;
    uint16_t raw = ((uint16_t)Wire.read() << 8) | Wire.read();
    return raw;
}

/**
 * Read irradiance in W/m² from GY302.
 * Lux = raw / 1.2 in continuous mode, then × 0.0079 to W/m².
 */
static float gy302_read_irradiance_wm2(void)
{
    uint16_t raw = gy302_read_raw();
    if (raw == 0) return 0.0f;
    float lux = raw / 1.2f;
    return lux * LUX_TO_WM2;
}

/* ============================================================
 * OV2640 green-channel irradiance cross-check (AeraFarm method)
 * Thresholds from AeraFarm v7: avg_g>105 Healthy, >65 K-def, ≤65 Na-def
 * Here we use the raw green average as a secondary G estimate.
 * ============================================================ */

static float camera_green_irradiance_estimate(void)
{
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) return -1.0f;

    /* Expect RGB565 or GRAYSCALE — extract green channel average */
    float g_sum = 0.0f;
    uint32_t count = 0;

    if (fb->format == PIXFORMAT_RGB565) {
        for (size_t i = 0; i + 1 < fb->len; i += 2) {
            uint16_t px = (fb->buf[i] << 8) | fb->buf[i + 1];
            uint8_t g  = (uint8_t)((px >> 5) & 0x3F) << 2;  /* 6→8 bit */
            g_sum += g;
            count++;
        }
    } else if (fb->format == PIXFORMAT_GRAYSCALE) {
        for (size_t i = 0; i < fb->len; i++) {
            g_sum += fb->buf[i];
            count++;
        }
    }

    esp_camera_fb_return(fb);
    if (count == 0) return -1.0f;

    float avg_g = g_sum / count;
    /* Map avg_g [0,255] to W/m² [0, 1000] linearly */
    return (avg_g / 255.0f) * 1000.0f;
}

/* ============================================================
 * V_MPP prediction from G_pred (exact analytical, paper §III-D)
 * Vmp_c = Voc_T × (1/(1+k))^(1/k)
 * Simplified thermal model: Voc_T ≈ Voc_STC − 0.0023×(T−25) per cell
 * ============================================================ */

static float predict_vmpp(float g_pred_wm2)
{
    if (g_pred_wm2 < 1.0f) return VMPP_STC * 0.5f;

    /* Scale Voc with G (log model) */
    float voc_scaled = V_OC_STC * (1.0f + 0.05f * log10f(g_pred_wm2 / G_NORM_MAX + 1e-6f));
    float vmpp = voc_scaled * powf(1.0f / (1.0f + K_VMPP), 1.0f / K_VMPP);

    /* Clamp to sensible range */
    if (vmpp < 10.0f) vmpp = 10.0f;
    if (vmpp > 20.0f) vmpp = 20.0f;
    return vmpp;
}

/* ============================================================
 * Irradiance lookback ring buffer (24 hourly samples)
 * ============================================================ */

static float g_lookback[LOOKBACK_STEPS] = {0};
static int   g_lb_head  = 0;
static float g_lb_filled = 0;

static void lookback_push(float g_wm2)
{
    g_lookback[g_lb_head] = g_wm2 / G_NORM_MAX;  /* normalise */
    g_lb_head = (g_lb_head + 1) % LOOKBACK_STEPS;
    if (g_lb_filled < LOOKBACK_STEPS) g_lb_filled++;
}

/** Copy lookback in chronological order into buf[LOOKBACK_STEPS] */
static void lookback_get(float *buf)
{
    int start = (g_lb_head - (int)g_lb_filled + LOOKBACK_STEPS) % LOOKBACK_STEPS;
    for (int i = 0; i < (int)g_lb_filled; i++) {
        buf[i] = g_lookback[(start + i) % LOOKBACK_STEPS];
    }
    /* Zero-pad if buffer not yet full */
    for (int i = (int)g_lb_filled; i < LOOKBACK_STEPS; i++) {
        buf[i] = 0.0f;
    }
}

/* ============================================================
 * Artemis UART link
 * ============================================================ */

typedef struct {
    float v_bat;
    float i_bat;
    float duty;
    int   charge_state;
    float g_est;          /* Artemis irradiance estimate from I_pv            */
    uint8_t valid;
} ArtemisMsg_t;

static ArtemisMsg_t g_artemis = {0};

static char uart_rx_buf[128];
static int  uart_rx_head = 0;

static void uart_parse_artemis(void)
{
    while (ARTEMIS_UART.available()) {
        char c = (char)ARTEMIS_UART.read();
        if (c == '\n') {
            uart_rx_buf[uart_rx_head] = '\0';
            if (strncmp(uart_rx_buf, "ART:", 4) == 0) {
                float v=0, i=0, d=0, g=0; int s=0;
                sscanf(uart_rx_buf + 4,
                       "V=%f,I=%f,D=%f,S=%d,G=%f",
                       &v, &i, &d, &s, &g);
                g_artemis.v_bat        = v;
                g_artemis.i_bat        = i;
                g_artemis.duty         = d;
                g_artemis.charge_state = s;
                g_artemis.g_est        = g;
                g_artemis.valid        = 1;
            }
            uart_rx_head = 0;
        } else if (c != '\r') {
            if (uart_rx_head < 127) uart_rx_buf[uart_rx_head++] = c;
        }
    }
}

static void uart_send_to_artemis(float v_mpp_pred, float g_pred, float alpha)
{
    char buf[64];
    int len = snprintf(buf, sizeof(buf),
        "HEL:VP=%.2f,GP=%.1f,AL=%.2f\r\n",
        v_mpp_pred, g_pred, alpha);
    ARTEMIS_UART.write((uint8_t *)buf, len);
}

/* ============================================================
 * SD card logging
 * Format: timestamp_ms, G_meas, G_pred, V_bat, I_bat, V_mpp_pred, alpha
 * ============================================================ */

static File g_log_file;
static bool g_sd_ok = false;

static void sd_init(void)
{
    SPI.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
    g_sd_ok = SD.begin(PIN_SD_CS);
    if (!g_sd_ok) {
        Serial.println("[HELIOS] SD init failed");
        return;
    }
    g_log_file = SD.open("/helios_log.csv", FILE_APPEND);
    if (g_log_file.size() == 0) {
        g_log_file.println("t_ms,G_meas,G_pred,V_bat,I_bat,V_mpp_pred,alpha,charge_state");
    }
    Serial.println("[HELIOS] SD card OK");
}

static void sd_log(float g_meas, float g_pred, float v_mpp_pred, float alpha)
{
    if (!g_sd_ok || !g_log_file) return;
    char line[128];
    snprintf(line, sizeof(line), "%lu,%.1f,%.1f,%.2f,%.3f,%.2f,%.2f,%d",
        millis(), g_meas, g_pred,
        g_artemis.v_bat, g_artemis.i_bat,
        v_mpp_pred, alpha, g_artemis.charge_state);
    g_log_file.println(line);
    g_log_file.flush();
}

/* ============================================================
 * Training buffer — accumulate per-minute G for TF.js retraining
 * Rate: 1 sample / minute → 1440 samples / 24 hours.
 * Resolution matches the LSTM lookback granularity expected by TF.js
 * training (the hourly lookback ring uses 60-minute averages of these).
 * ============================================================ */

static int g_train_samples = -1;   /* cached line count; -1 = uninitialised   */

static void train_buf_append(float g_wm2)
{
    File f = SPIFFS.open(TRAIN_BUFFER_PATH, FILE_APPEND);
    if (!f) return;
    f.printf("%.1f\n", g_wm2);
    f.close();
    if (g_train_samples >= 0) g_train_samples++;
}

/**
 * Count lines in SPIFFS training buffer.
 * Uses cached counter after first read to avoid SPIFFS wear on every tick.
 * Returns -1 on open failure.
 */
static int train_buf_count(void)
{
    if (g_train_samples >= 0) return g_train_samples;
    File f = SPIFFS.open(TRAIN_BUFFER_PATH, "r");
    if (!f) return g_train_samples = -1;
    int lines = 0;
    while (f.available()) { if (f.read() == '\n') lines++; }
    f.close();
    g_train_samples = lines;
    return lines;
}

/**
 * Retraining state:
 *   g_retrain_ready  — flag exposed to dashboard: data is ready
 *   g_retrain_auto   — RETRAIN_INTERVAL_MS timer has fired; auto-trigger
 */
static uint8_t  g_retrain_ready = 0;
static uint32_t g_last_retrain  = 0;   /* timestamp of last completed retrain */

static void retrain_check(void)
{
    uint32_t now = millis();

    /* ── Auto-trigger on 24-hour cycle (Bug 3 fix: use RETRAIN_INTERVAL_MS) */
    if (g_last_retrain == 0) g_last_retrain = now;  /* initialise on first call */
    bool auto_due = (now - g_last_retrain) >= RETRAIN_INTERVAL_MS;

    /* ── Data sufficiency check (1440 = 24h × 60 min/h × 1 sample/min) ─── */
    int samples = train_buf_count();
    if (samples < 0) return;

    if (samples >= 1440) {
        if (!g_retrain_ready) {
            g_retrain_ready = 1;
            Serial.printf("[HELIOS] Retraining data ready (%d samples)\n", samples);
        }
        if (auto_due) {
            /* Auto-trigger flag is exposed in /api/status so the dashboard
             * can initiate TF.js training without user interaction.         */
            Serial.println("[HELIOS] 24-hour auto-retrain cycle triggered");
            g_last_retrain = now;
        }
    }
}

/* ============================================================
 * Web dashboard (WiFi AP mode)
 * Endpoints:
 *   GET  /               — HTML dashboard + TF.js retraining UI
 *   GET  /api/status     — JSON telemetry snapshot (includes retrain flags)
 *   GET  /api/train_data — Stream train_buf.csv to browser for TF.js
 *   GET  /api/weights    — Download current lstm_weights.json
 *   POST /api/weights    — Upload newly trained weights JSON from TF.js
 * ============================================================ */

static WebServer g_server(80);

static const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Helios-Artemis Dashboard</title>
<!-- TF.js 4.x — loaded from CDN; device must be AP-connected, not internet.
     For fully offline use, serve tfjs.min.js from SPIFFS (see note below). -->
<script src="https://cdn.jsdelivr.net/npm/@tensorflow/tfjs@4.17.0/dist/tf.min.js"></script>
<style>
  *{box-sizing:border-box}
  body{font-family:monospace;background:#0b1120;color:#c9f0ff;margin:0;padding:1rem}
  h1{color:#f0c040;margin:0 0 0.5rem}
  h2{color:#8cf;font-size:0.95rem;margin:1.2rem 0 0.4rem}
  .grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:0.6rem}
  .card{background:#112040;border:1px solid #244;border-radius:6px;padding:0.6rem;text-align:center}
  .val{font-size:1.5rem;font-weight:700;color:#4ee}
  .lbl{font-size:0.7rem;color:#89a;margin-top:0.2rem}
  .row{display:flex;gap:0.5rem;flex-wrap:wrap;margin-top:0.5rem}
  button{padding:0.45rem 1rem;border-radius:4px;border:1px solid;cursor:pointer;font-family:monospace;font-size:0.85rem}
  .btn-train{background:#1a3a5a;color:#8cf;border-color:#4af}
  .btn-upload{background:#1a4a2a;color:#8f8;border-color:#4a4}
  .btn-cancel{background:#3a1a1a;color:#f88;border-color:#a44}
  progress{width:100%;height:8px;margin-top:0.4rem;accent-color:#4af}
  #train-status{font-size:0.75rem;color:#adf;margin-top:0.3rem;min-height:1.1em}
  #log{white-space:pre;font-size:0.68rem;color:#6af;height:140px;overflow:auto;
       background:#091020;padding:0.4rem;margin-top:0.5rem;border-radius:4px}
  .tag-ready{color:#4f4;font-weight:700}
  .tag-no{color:#888}
</style>
</head>
<body>
<h1>&#9728; Helios-Artemis MPPT</h1>

<!-- ── Live telemetry ─────────────────────────────────────── -->
<h2>Live Telemetry</h2>
<div class="grid">
  <div class="card"><div class="val" id="g_meas">--</div><div class="lbl">G_meas (W/m²)</div></div>
  <div class="card"><div class="val" id="g_pred">--</div><div class="lbl">G_pred (W/m²)</div></div>
  <div class="card"><div class="val" id="v_bat">--</div><div class="lbl">V_bat (V)</div></div>
  <div class="card"><div class="val" id="i_bat">--</div><div class="lbl">I_bat (A)</div></div>
  <div class="card"><div class="val" id="vmpp">--</div><div class="lbl">V_MPP_pred (V)</div></div>
  <div class="card"><div class="val" id="alpha">--</div><div class="lbl">α blend</div></div>
  <div class="card"><div class="val" id="eta">--</div><div class="lbl">η_MPPT (%)</div></div>
  <div class="card"><div class="val" id="state">--</div><div class="lbl">Charge State</div></div>
  <div class="card"><div class="val" id="samples">--</div><div class="lbl">Train Samples</div></div>
  <div class="card"><div class="val" id="retrain_flag">--</div><div class="lbl">Data Ready</div></div>
</div>

<!-- ── On-device TF.js retraining panel ───────────────────── -->
<h2>On-Device LSTM Retraining (TF.js)</h2>
<div class="row">
  <button class="btn-train" id="btn-train" onclick="startTraining()">&#9654; Train Now</button>
  <button class="btn-cancel" id="btn-cancel" onclick="cancelTraining()" disabled>&#9632; Cancel</button>
  <button class="btn-upload" id="btn-upload" onclick="uploadWeights()" disabled>&#8593; Deploy Weights</button>
</div>
<progress id="train-progress" value="0" max="100"></progress>
<div id="train-status">Idle — waiting for ≥1440 training samples.</div>

<!-- ── Event log ──────────────────────────────────────────── -->
<h2>Event Log</h2>
<div id="log">Connecting to Helios...</div>

<script>
'use strict';

/* ═══════════════════════════════════════════════════════════
 * Constants matching the C++ firmware (paper §III-C)
 * ═══════════════════════════════════════════════════════════ */
const LOOKBACK     = 24;    // hourly steps fed to LSTM
const G_NORM_MAX   = 1000.0;
const EPOCHS       = 40;
const BATCH_SIZE   = 32;
const LEARNING_RATE = 0.002;
const STATES = ["BULK","ABSORPTION","FLOAT"];

/* ═══════════════════════════════════════════════════════════
 * Global state
 * ═══════════════════════════════════════════════════════════ */
let gModel       = null;   // TF.js LSTM model (irradiance forecaster)
let gGainModel   = null;   // TF.js gain scheduler model
let gTrainStop   = false;  // cancellation flag
let gNewWeights  = null;   // serialised weights JSON after training
let gSampleCount = 0;

/* ═══════════════════════════════════════════════════════════
 * Telemetry polling
 * ═══════════════════════════════════════════════════════════ */
async function poll() {
  try {
    const d = await fetch('/api/status').then(r => r.json());
    setText('g_meas',       d.g_meas.toFixed(1));
    setText('g_pred',       d.g_pred.toFixed(1));
    setText('v_bat',        d.v_bat.toFixed(2));
    setText('i_bat',        d.i_bat.toFixed(3));
    setText('vmpp',         d.v_mpp_pred.toFixed(2));
    setText('alpha',        d.alpha.toFixed(3));
    setText('eta',          (d.eta_est * 100).toFixed(1));
    setText('state',        STATES[d.charge_state] || d.charge_state);
    setText('samples',      d.train_samples);
    gSampleCount = d.train_samples;

    const rdEl = document.getElementById('retrain_flag');
    rdEl.textContent = d.retrain_ready ? 'YES' : 'no';
    rdEl.className   = 'val ' + (d.retrain_ready ? 'tag-ready' : 'tag-no');

    /* Auto-trigger training if firmware says 24h cycle fired */
    if (d.auto_retrain && gModel === null) {
      appendLog('Auto-retrain triggered by 24h cycle');
      startTraining();
    }

    appendLog(`${ts()} G=${d.g_meas.toFixed(0)} V=${d.v_bat.toFixed(2)} η=${(d.eta_est*100).toFixed(1)}%`);
  } catch(e) { appendLog('Poll error: ' + e.message); }
}

setInterval(poll, 1000);
poll();

/* ═══════════════════════════════════════════════════════════
 * Build TF.js models matching C++ architecture (paper §III-C)
 *
 * Model 1 — irradiance forecaster
 *   Input  : [batch, 24, 1]  (normalised hourly GHI lookback)
 *   LSTM   : 32 units, return_sequences=false
 *   Dense  : 1 unit, linear (normalised G output)
 *   Params : 32×(1+32+1)×4 + 32 = 4352 + 32 + ... ≈ 7 329
 *
 * Model 2 — gain scheduler
 *   Input  : [batch, 1, 1]  (normalised predicted G, single step)
 *   LSTM   : 4 units
 *   Dense  : 1 unit, linear (step scale output)
 *   Params : 4×(1+4+1)×4 + 4 = 96 + 4 + 1 = 101
 * ═══════════════════════════════════════════════════════════ */
function buildIrradianceModel() {
  const m = tf.sequential({ name: 'irradiance_forecaster' });
  m.add(tf.layers.lstm({
    units: 32,
    inputShape: [LOOKBACK, 1],
    returnSequences: false,
    name: 'lstm_32'
  }));
  m.add(tf.layers.dense({ units: 1, activation: 'linear', name: 'dense_out' }));
  m.compile({
    optimizer: tf.train.adam(LEARNING_RATE),
    loss: 'meanSquaredError',
    metrics: ['mae']
  });
  return m;
}

function buildGainModel() {
  const m = tf.sequential({ name: 'gain_scheduler' });
  m.add(tf.layers.lstm({
    units: 4,
    inputShape: [1, 1],
    returnSequences: false,
    name: 'gain_lstm_4'
  }));
  m.add(tf.layers.dense({ units: 1, activation: 'linear', name: 'gain_dense' }));
  m.compile({
    optimizer: tf.train.adam(LEARNING_RATE),
    loss: 'meanSquaredError'
  });
  return m;
}

/* ═══════════════════════════════════════════════════════════
 * Load existing weights from Helios into TF.js models.
 * The C++ weight JSON uses flat arrays per gate matrix.
 * We reconstruct the combined kernel [input+h, 4h] and
 * bias [4h] that TF.js LSTM expects, then set via setWeights().
 * ═══════════════════════════════════════════════════════════ */
async function loadExistingWeights(model, weightsJson, H, inputSize) {
  // TF.js LSTM layer weight layout:
  //   kernel  : shape [input_size + H, 4*H]  — gates i,f,c,o (TF order)
  //   recurrent_kernel : shape [H, 4*H]
  //   bias    : shape [4*H]  (if use_bias=true)
  // C++ firmware stores Wf/Wi/Wc/Wo as [H][input_size+H]
  // TF.js gate order: i (input), f (forget), c (cell), o (output)
  // C++ order:        f, i, c, o  → reorder to i, f, c, o for TF.js

  const N = inputSize + H;
  const kernel   = new Float32Array(inputSize * 4 * H);
  const recKernel= new Float32Array(H * 4 * H);
  const bias     = new Float32Array(4 * H);

  // Gate index mapping: TF.js [i,f,c,o] ← C++ [Wi,Wf,Wc,Wo]
  const cppGates = ['Wi','Wf','Wc','Wo'];
  const tfOrder  = [0, 1, 2, 3]; // i=0,f=1,c=2,o=3 in TF.js, same order after swap

  for (let gi = 0; gi < 4; gi++) {
    const W = weightsJson[cppGates[gi]]; // [H][N]
    const b = weightsJson[['bi','bf','bc','bo'][gi]]; // [H]
    for (let h = 0; h < H; h++) {
      // Input part of kernel [0..inputSize)
      for (let j = 0; j < inputSize; j++) {
        kernel[(j * 4 * H) + (tfOrder[gi] * H) + h] = W[h][j];
      }
      // Recurrent part [inputSize..N)
      for (let j = 0; j < H; j++) {
        recKernel[(j * 4 * H) + (tfOrder[gi] * H) + h] = W[h][inputSize + j];
      }
      bias[tfOrder[gi] * H + h] = b[h];
    }
  }

  const lstmLayer = model.getLayer(null, 0);
  lstmLayer.setWeights([
    tf.tensor2d(kernel,    [inputSize, 4 * H]),
    tf.tensor2d(recKernel, [H,         4 * H]),
    tf.tensor1d(bias)
  ]);

  // Dense layer
  const Wd = weightsJson['Wd'];
  const bd = weightsJson['bd'];
  const denseLayer = model.getLayer(null, 1);
  denseLayer.setWeights([
    tf.tensor2d(Wd, [H, 1]),
    tf.tensor1d([bd])
  ]);
}

/* ═══════════════════════════════════════════════════════════
 * Fetch training data from Helios SPIFFS (/api/train_data)
 * Returns Float32Array of normalised G values, minute resolution.
 * ═══════════════════════════════════════════════════════════ */
async function fetchTrainingData() {
  setStatus('Fetching training data from SPIFFS...');
  const resp = await fetch('/api/train_data');
  if (!resp.ok) throw new Error('Failed to fetch /api/train_data: ' + resp.status);
  const text = await resp.text();
  const lines = text.trim().split('\n').filter(l => l.length > 0);
  const raw = new Float32Array(lines.map(l => parseFloat(l) / G_NORM_MAX));
  appendLog(`Training data: ${raw.length} samples fetched`);
  return raw;
}

/* ═══════════════════════════════════════════════════════════
 * Build sliding-window sequences for LSTM training.
 *
 * The irradiance forecaster is trained on HOURLY samples.
 * The minute-resolution buffer is first downsampled to hourly
 * averages (60-sample windows), then 24-step lookback sequences
 * are built with 1-step-ahead targets.
 *
 * Gain scheduler is trained with (G_pred_norm → step_scale) pairs
 * where step_scale = clip(1 + 0.3*(G_pred - G_actual)/G_actual, 0.5, 2.0).
 * ═══════════════════════════════════════════════════════════ */
function buildSequences(minuteData) {
  // Downsample: average every 60 minutes → hourly
  const hourly = [];
  for (let i = 0; i + 59 < minuteData.length; i += 60) {
    let sum = 0;
    for (let j = 0; j < 60; j++) sum += minuteData[i + j];
    hourly.push(sum / 60);
  }

  if (hourly.length < LOOKBACK + 1) {
    throw new Error(`Only ${hourly.length} hourly samples — need ${LOOKBACK + 1}`);
  }

  // Build [X, Y] for irradiance forecaster
  const Xs = [], Ys = [];
  for (let i = 0; i + LOOKBACK < hourly.length; i++) {
    Xs.push(hourly.slice(i, i + LOOKBACK).map(v => [v]));
    Ys.push(hourly[i + LOOKBACK]);
  }

  // Build [Xg, Yg] for gain scheduler
  // Xg = predicted G (use Y_prev as proxy for predicted)
  // Yg = actual scale: clip(1 + 0.3*(pred-actual)/actual, 0.5, 2.0)
  const Xgs = [], Ygs = [];
  for (let i = 1; i < Ys.length; i++) {
    const pred   = Ys[i - 1];  // previous target as proxy for prediction
    const actual = Ys[i];
    const denom  = Math.max(actual, 0.01);
    let scale    = 1.0 + 0.3 * (pred - actual) / denom;
    scale = Math.min(2.0, Math.max(0.5, scale));
    Xgs.push([[pred]]);
    Ygs.push(scale);
  }

  return { Xs, Ys, Xgs, Ygs };
}

/* ═══════════════════════════════════════════════════════════
 * Main training entry point
 * ═══════════════════════════════════════════════════════════ */
async function startTraining() {
  if (gSampleCount < 1440) {
    setStatus(`Not enough data (${gSampleCount}/1440 samples). Waiting...`);
    return;
  }

  document.getElementById('btn-train').disabled  = true;
  document.getElementById('btn-cancel').disabled = false;
  document.getElementById('btn-upload').disabled = true;
  gTrainStop  = false;
  gNewWeights = null;
  setProgress(0);

  try {
    /* 1. Fetch training data */
    const minuteData = await fetchTrainingData();
    if (gTrainStop) return cleanup('Cancelled after data fetch');

    /* 2. Build sequences */
    setStatus('Building training sequences...');
    const { Xs, Ys, Xgs, Ygs } = buildSequences(minuteData);
    appendLog(`Sequences: ${Xs.length} irradiance, ${Xgs.length} gain`);

    const xTrain = tf.tensor3d(Xs);           // [N, 24, 1]
    const yTrain = tf.tensor2d(Ys, [Ys.length, 1]);
    const xGain  = tf.tensor3d(Xgs);          // [N, 1, 1]
    const yGain  = tf.tensor2d(Ygs, [Ygs.length, 1]);

    /* 3. Build models */
    setStatus('Building TF.js LSTM models...');
    gModel     = buildIrradianceModel();
    gGainModel = buildGainModel();

    /* 4. Try to warm-start from existing weights on device */
    try {
      const wResp = await fetch('/api/weights');
      if (wResp.ok) {
        const wJson = await wResp.json();
        if (wJson.lstm && wJson.gain) {
          await loadExistingWeights(gModel,     wJson.lstm, 32, 1);
          await loadExistingWeights(gGainModel, wJson.gain,  4, 1);
          appendLog('Warm-start: loaded existing weights from device');
        }
      }
    } catch(e) { appendLog('Warm-start skipped: ' + e.message); }

    if (gTrainStop) { xTrain.dispose(); yTrain.dispose(); xGain.dispose(); yGain.dispose(); return cleanup('Cancelled'); }

    /* 5. Train irradiance forecaster */
    setStatus('Training irradiance forecaster (32-unit LSTM)...');
    appendLog(`Training: ${EPOCHS} epochs, batch=${BATCH_SIZE}, lr=${LEARNING_RATE}`);

    await gModel.fit(xTrain, yTrain, {
      epochs: EPOCHS,
      batchSize: BATCH_SIZE,
      validationSplit: 0.1,
      shuffle: true,
      callbacks: {
        onEpochEnd: async (epoch, logs) => {
          if (gTrainStop) { gModel.stopTraining = true; return; }
          const pct = Math.round(((epoch + 1) / EPOCHS) * 70);
          setProgress(pct);
          setStatus(`Epoch ${epoch+1}/${EPOCHS} — loss: ${logs.loss.toFixed(5)}, mae: ${logs.mae.toFixed(4)}, val_loss: ${(logs.val_loss||0).toFixed(5)}`);
          await tf.nextFrame();
        }
      }
    });

    if (gTrainStop) { xTrain.dispose(); yTrain.dispose(); xGain.dispose(); yGain.dispose(); return cleanup('Cancelled during training'); }

    /* 6. Train gain scheduler */
    setStatus('Training gain scheduler (4-unit LSTM)...');
    await gGainModel.fit(xGain, yGain, {
      epochs: Math.round(EPOCHS / 2),
      batchSize: BATCH_SIZE,
      shuffle: true,
      callbacks: {
        onEpochEnd: async (epoch, logs) => {
          if (gTrainStop) { gGainModel.stopTraining = true; return; }
          const pct = 70 + Math.round(((epoch + 1) / (EPOCHS / 2)) * 25);
          setProgress(pct);
          await tf.nextFrame();
        }
      }
    });

    xTrain.dispose(); yTrain.dispose(); xGain.dispose(); yGain.dispose();
    if (gTrainStop) return cleanup('Cancelled after gain training');

    /* 7. Serialise weights to C++ JSON schema */
    setStatus('Serialising weights to firmware format...');
    gNewWeights = await serialiseWeights(gModel, gGainModel);

    setProgress(100);
    setStatus(`Training complete! R² ≈ ${await computeR2(gModel, xTrain, yTrain)} — click "Deploy Weights" to upload.`);
    appendLog('Training complete — weights ready to deploy');
    document.getElementById('btn-upload').disabled = false;

  } catch(e) {
    setStatus('Error: ' + e.message);
    appendLog('Training error: ' + e.message);
  } finally {
    document.getElementById('btn-train').disabled  = false;
    document.getElementById('btn-cancel').disabled = true;
  }
}

/* ═══════════════════════════════════════════════════════════
 * Serialise TF.js weights back to C++ firmware JSON schema
 * ═══════════════════════════════════════════════════════════ */
async function serialiseWeights(model, gainModel) {
  async function extractLSTM(m, H, inputSize) {
    const lstmLayer  = m.getLayer(null, 0);
    const denseLayer = m.getLayer(null, 1);
    const [kernel, recKernel, bias] = lstmLayer.getWeights().map(w => w.arraySync());
    const [Wd_arr, bd_arr]          = denseLayer.getWeights().map(w => w.arraySync());

    // TF.js kernel: [inputSize, 4H], recKernel: [H, 4H], bias: [4H]
    // TF.js gate order: i=0, f=1, c=2, o=3
    // C++ firmware expects: Wf[H][N], Wi[H][N], Wc[H][N], Wo[H][N]
    // where N = inputSize + H (input + recurrent concatenated)
    const N = inputSize + H;
    const gateMap = { Wi: 0, Wf: 1, Wc: 2, Wo: 3 };
    const biasMap = { bi: 0, bf: 1, bc: 2, bo: 3 };
    const out = {};

    for (const [name, gi] of Object.entries(gateMap)) {
      out[name] = [];
      for (let h = 0; h < H; h++) {
        const row = new Array(N);
        for (let j = 0; j < inputSize; j++) row[j] = kernel[j][gi * H + h];
        for (let j = 0; j < H; j++)         row[inputSize + j] = recKernel[j][gi * H + h];
        out[name].push(row);
      }
    }
    for (const [name, gi] of Object.entries(biasMap)) {
      out[name] = Array.from({ length: H }, (_, h) => bias[gi * H + h]);
    }

    out['Wd'] = Array.isArray(Wd_arr[0]) ? Wd_arr.map(r => r[0]) : Wd_arr;
    out['bd'] = Array.isArray(bd_arr) ? bd_arr[0] : bd_arr;
    return out;
  }

  return JSON.stringify({
    lstm: await extractLSTM(model,     32, 1),
    gain: await extractLSTM(gainModel,  4, 1)
  });
}

/* ═══════════════════════════════════════════════════════════
 * Upload serialised weights to /api/weights (POST)
 * ═══════════════════════════════════════════════════════════ */
async function uploadWeights() {
  if (!gNewWeights) { alert('No trained weights available.'); return; }
  setStatus('Uploading weights to Helios SPIFFS...');
  try {
    const resp = await fetch('/api/weights', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: gNewWeights
    });
    if (resp.ok) {
      setStatus('Weights deployed! Device reloaded new model.');
      appendLog('Weights uploaded and deployed to device.');
      document.getElementById('btn-upload').disabled = true;
      gNewWeights = null;
    } else {
      setStatus('Upload failed: HTTP ' + resp.status);
    }
  } catch(e) {
    setStatus('Upload error: ' + e.message);
  }
}

function cancelTraining() {
  gTrainStop = true;
  if (gModel)     gModel.stopTraining     = true;
  if (gGainModel) gGainModel.stopTraining = true;
}

/* ═══════════════════════════════════════════════════════════
 * R² metric for post-training reporting
 * ═══════════════════════════════════════════════════════════ */
async function computeR2(model, xT, yT) {
  try {
    const yPred = model.predict(xT);
    const yTrue = yT;
    const yMean = yTrue.mean();
    const ssTot = yTrue.sub(yMean).square().sum();
    const ssRes = yTrue.sub(yPred).square().sum();
    const r2 = (1 - ssRes.div(ssTot)).dataSync()[0];
    yPred.dispose(); yMean.dispose(); ssTot.dispose(); ssRes.dispose();
    return r2.toFixed(3);
  } catch(e) { return 'n/a'; }
}

/* ═══════════════════════════════════════════════════════════
 * Helpers
 * ═══════════════════════════════════════════════════════════ */
function setText(id, val) { document.getElementById(id).textContent = val; }
function setStatus(msg) { document.getElementById('train-status').textContent = msg; }
function setProgress(pct) { document.getElementById('train-progress').value = pct; }
function cleanup(msg) {
  setStatus(msg);
  document.getElementById('btn-train').disabled  = false;
  document.getElementById('btn-cancel').disabled = true;
}
function ts() { return new Date().toISOString().substr(11, 8); }
function appendLog(msg) {
  const el = document.getElementById('log');
  el.textContent += ts() + ' ' + msg + '\n';
  if (el.textContent.length > 8000) el.textContent = el.textContent.slice(-6000);
  el.scrollTop = el.scrollHeight;
}
</script>
</body>
</html>
)rawliteral";

/* Runtime snapshot for /api/status */
static float g_snap_g_meas     = 0.0f;
static float g_snap_g_pred     = 0.0f;
static float g_snap_vmpp       = 0.0f;
static float g_snap_alpha      = ALPHA_DEFAULT;
static float g_snap_eta        = 0.0f;

static void handle_root(void)
{
    g_server.send_P(200, "text/html", DASHBOARD_HTML);
}

static void handle_status(void)
{
    int samples = train_buf_count();
    /* auto_retrain: true when 24h cycle fired AND data is ready              */
    bool auto_retrain = g_retrain_ready &&
                        ((millis() - g_last_retrain) >= RETRAIN_INTERVAL_MS);

    char buf[320];
    snprintf(buf, sizeof(buf),
        "{\"g_meas\":%.1f,\"g_pred\":%.1f,\"v_bat\":%.2f,\"i_bat\":%.3f,"
        "\"v_mpp_pred\":%.2f,\"alpha\":%.3f,\"charge_state\":%d,"
        "\"retrain_ready\":%d,\"auto_retrain\":%d,"
        "\"train_samples\":%d,\"eta_est\":%.3f}",
        g_snap_g_meas, g_snap_g_pred,
        g_artemis.v_bat, g_artemis.i_bat,
        g_snap_vmpp, g_snap_alpha,
        g_artemis.charge_state,
        (int)g_retrain_ready,
        (int)auto_retrain,
        samples,
        g_snap_eta
    );
    g_server.send(200, "application/json", buf);
}

/**
 * GET /api/train_data
 * Streams the raw training buffer (train_buf.csv) from SPIFFS.
 * The browser TF.js pipeline fetches this, downsamples to hourly,
 * and builds LSTM training sequences. Content-Type: text/plain so
 * the browser can read it as text without decoding.
 */
static void handle_get_train_data(void)
{
    if (!SPIFFS.exists(TRAIN_BUFFER_PATH)) {
        g_server.send(404, "text/plain", "No training data yet");
        return;
    }
    File f = SPIFFS.open(TRAIN_BUFFER_PATH, "r");
    if (!f) {
        g_server.send(500, "text/plain", "Failed to open train buffer");
        return;
    }
    g_server.streamFile(f, "text/plain");
    f.close();
}

static void handle_get_weights(void)
{
    if (!SPIFFS.exists(LSTM_WEIGHTS_PATH)) {
        g_server.send(404, "text/plain", "No weights file");
        return;
    }
    File f = SPIFFS.open(LSTM_WEIGHTS_PATH, "r");
    g_server.streamFile(f, "application/json");
    f.close();
}

static void handle_post_weights(void)
{
    if (g_server.hasArg("plain")) {
        String body = g_server.arg("plain");
        File f = SPIFFS.open(LSTM_WEIGHTS_PATH, "w");
        if (f) {
            f.print(body);
            f.close();
            /* Reload weights immediately */
            load_weights_from_spiffs();
            g_retrain_ready = 0;
            /* Clear training buffer */
            SPIFFS.remove(TRAIN_BUFFER_PATH);
            g_train_samples = 0;
            g_server.send(200, "text/plain", "OK");
            Serial.println("[HELIOS] New weights received and loaded");
        } else {
            g_server.send(500, "text/plain", "Write failed");
        }
    } else {
        g_server.send(400, "text/plain", "No body");
    }
}

static void web_server_init(void)
{
    WiFi.softAP(WIFI_SSID, WIFI_PASS);
    Serial.printf("[HELIOS] AP: %s, IP: %s\n",
                  WIFI_SSID, WiFi.softAPIP().toString().c_str());

    g_server.on("/",                HTTP_GET,  handle_root);
    g_server.on("/api/status",      HTTP_GET,  handle_status);
    g_server.on("/api/train_data",  HTTP_GET,  handle_get_train_data);
    g_server.on("/api/weights",     HTTP_GET,  handle_get_weights);
    g_server.on("/api/weights",     HTTP_POST, handle_post_weights);
    g_server.begin();
}

/* ============================================================
 * Camera init (OV2640, QVGA, RGB565, minimal settings)
 * ============================================================ */

static bool camera_init_ov2640(void)
{
    camera_config_t cfg;
    cfg.ledc_channel = LEDC_CHANNEL_0;
    cfg.ledc_timer   = LEDC_TIMER_0;
    cfg.pin_d0  =  5; cfg.pin_d1 = 18; cfg.pin_d2 = 19; cfg.pin_d3 = 21;
    cfg.pin_d4  = 36; cfg.pin_d5 = 39; cfg.pin_d6 = 34; cfg.pin_d7 = 35;
    cfg.pin_xclk = 0; cfg.pin_pclk = 22;
    cfg.pin_vsync = 25; cfg.pin_href = 23;
    cfg.pin_sscb_sda = 26; cfg.pin_sscb_scl = 27;
    cfg.pin_pwdn = 32;  cfg.pin_reset = -1;
    cfg.xclk_freq_hz = 20000000;
    cfg.pixel_format = PIXFORMAT_RGB565;
    cfg.frame_size   = FRAMESIZE_QVGA;
    cfg.jpeg_quality = 10;
    cfg.fb_count = 1;

    return esp_camera_init(&cfg) == ESP_OK;
}

/* ============================================================
 * Main Arduino entry points
 * ============================================================ */

static float g_lookback_buf[LOOKBACK_STEPS];
static uint32_t g_last_hourly_push  = 0;
static uint32_t g_last_train_append = 0;  /* 1-minute training buffer writes  */
static uint32_t g_last_log          = 0;
static uint32_t g_last_inference    = 0;

void setup(void)
{
    Serial.begin(115200);
    Serial.println("[HELIOS] Booting Helios-Artemis");

    /* I²C */
    Wire.begin(PIN_SDA, PIN_SCL);

    /* GY302 */
    if (!gy302_init()) Serial.println("[HELIOS] GY302 init failed");
    else               Serial.println("[HELIOS] GY302 OK");

    /* SPIFFS */
    if (!SPIFFS.begin(true)) Serial.println("[HELIOS] SPIFFS mount failed");

    /* LSTM weights */
    load_weights_from_spiffs();

    /* SD card */
    sd_init();

    /* Camera — disabled by default: GPIO conflict with SD (see pin matrix).
     * Enable only if camera is fitted and SD card is removed, or after PCB
     * rev reroutes the conflicting pins.                                     */
    #if 0
    if (camera_init_ov2640()) Serial.println("[HELIOS] OV2640 OK");
    else                      Serial.println("[HELIOS] OV2640 init failed");
    #endif

    /* Artemis UART */
    ARTEMIS_UART.begin(115200, SERIAL_8N1, PIN_UART_RX, PIN_UART_TX);

    /* Web server (WiFi AP) */
    web_server_init();

    Serial.println("[HELIOS] Init complete — entering main loop");
}

void loop(void)
{
    uint32_t now = millis();

    /* ── Parse Artemis telemetry ──────────────────────────────────── */
    uart_parse_artemis();

    /* ── Web server ───────────────────────────────────────────────── */
    g_server.handleClient();

    /* ── Inference + UART TX at 100 ms intervals ──────────────────── */
    if (now - g_last_inference >= INFERENCE_INTERVAL_MS) {
        g_last_inference = now;

        /* Read GY302 irradiance */
        float g_meas = gy302_read_irradiance_wm2();

        /* Camera cross-check (blended 10% weight for robustness) */
        float g_cam = camera_green_irradiance_estimate();
        if (g_cam > 0.0f) {
            g_meas = 0.9f * g_meas + 0.1f * g_cam;
        }

        /* Build lookback and run LSTM */
        lookback_get(g_lookback_buf);
        float g_pred  = (g_lstm.loaded) ?
                         lstm_predict(g_lookback_buf, LOOKBACK_STEPS) :
                         g_meas;  /* fallback: pass-through if untrained      */

        /* Gain scheduler */
        float g_pred_norm = g_pred / G_NORM_MAX;
        float step_scale  = (g_gain.loaded) ?
                             gain_schedule(g_pred_norm) : 1.0f;
        (void)step_scale;  /* communicated to Artemis via alpha adjustment    */

        /* Predict V_MPP from irradiance */
        float v_mpp_pred = predict_vmpp(g_pred);

        /* Alpha: use paper optimum; modulate with gain scheduler output      */
        float alpha = ALPHA_DEFAULT * step_scale * 0.286f;  /* re-center      */
        if (alpha < 0.05f) alpha = 0.05f;
        if (alpha > 0.55f) alpha = 0.55f;

        /* Send to Artemis */
        uart_send_to_artemis(v_mpp_pred, g_pred, alpha);

        /* Estimate MPPT efficiency for dashboard */
        float eta = 1.0f;
        if (g_artemis.valid && g_artemis.v_bat > 0.0f) {
            float p_avail = g_meas * 0.050f;  /* 50 Wp at measured G          */
            float p_out   = g_artemis.v_bat * fabsf(g_artemis.i_bat);
            eta = (p_avail > 0.5f) ? (p_out / p_avail) : 1.0f;
            if (eta > 1.0f) eta = 1.0f;
        }

        /* Update snapshot */
        g_snap_g_meas = g_meas;
        g_snap_g_pred = g_pred;
        g_snap_vmpp   = v_mpp_pred;
        g_snap_alpha  = alpha;
        g_snap_eta    = eta;
    }

    /* ── Hourly lookback push (LSTM inference input, 24-step ring) ── */
    if (now - g_last_hourly_push >= 3600000UL) {
        g_last_hourly_push = now;
        lookback_push(g_snap_g_meas);
    }

    /* ── Per-minute training buffer append (1440/day for TF.js) ─── */
    if (now - g_last_train_append >= 60000UL) {
        g_last_train_append = now;
        train_buf_append(g_snap_g_meas);
    }

    /* ── SD logging every 1 second ──────────────────────────────────*/
    if (now - g_last_log >= LOG_INTERVAL_MS) {
        g_last_log = now;
        sd_log(g_snap_g_meas, g_snap_g_pred, g_snap_vmpp, g_snap_alpha);
    }

    /* ── Retraining check ────────────────────────────────────────── */
    retrain_check();
}

/* ============================================================
 * Build notes (platformio.ini):
 *
 * [env:esp32s3]
 * platform   = espressif32
 * board      = esp32s3box
 * framework  = arduino
 * lib_deps   =
 *     ArduinoJson
 *     ESP32 Camera (espressif/esp32-camera)
 * board_build.partitions = default_ffat.csv
 * build_flags =
 *     -DBOARD_HAS_PSRAM
 *     -mfix-esp32-psram-cache-issue
 *
 * I²C shared bus summary:
 *   0x23 — GY302   (Helios, read-only)
 *   0x40 — INA219  (Artemis primary; Helios may monitor if needed)
 *
 * UART wiring:
 *   ESP32-S3 GPIO17 (TX) → STM32 PA10 (RX)
 *   ESP32-S3 GPIO18 (RX) ← STM32 PA9  (TX)
 *   Common GND required
 *   Level shift if required (ESP32-S3 is 3.3V; STM32 GPIO tolerates 3.3V)
 * ============================================================ */
