#include <Arduino.h>
#include "esp_timer.h"
#include "esp_system.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

// Measures the Helios-side timing budget without printing during timed work.
// USB output is emitted only after each batch so it does not perturb samples.
static constexpr int H = 32;
static constexpr int INPUTS = 33;
static constexpr int LOOKBACK = 24;
static constexpr int N = 400;
static constexpr uint32_t LOOP_US = 100000;

struct Lstm {
  float w[4][H][INPUTS];
  float b[4][H];
  float wd[H];
  float bd;
  float h[H];
  float c[H];
};

static Lstm model;
static volatile float sink;

static inline float sigmoid(float x) {
  return 1.0f / (1.0f + expf(-x));
}

static void init_model() {
  for (int g = 0; g < 4; ++g) {
    for (int i = 0; i < H; ++i) {
      model.b[g][i] = 0.01f * (float)(g + 1);
      for (int j = 0; j < INPUTS; ++j) {
        model.w[g][i][j] = 0.001f * (float)((g + 1) * ((i + j) % 17 - 8));
      }
    }
  }
  for (int i = 0; i < H; ++i) model.wd[i] = 0.002f * (float)((i % 9) - 4);
  model.bd = 0.1f;
}

static float lstm_step(float x) {
  float xh[INPUTS];
  float f[H], in[H], cell[H], out[H];
  xh[0] = x;
  for (int i = 0; i < H; ++i) xh[i + 1] = model.h[i];

  for (int i = 0; i < H; ++i) {
    float sf = model.b[0][i], si = model.b[1][i];
    float sc = model.b[2][i], so = model.b[3][i];
    for (int j = 0; j < INPUTS; ++j) {
      sf += model.w[0][i][j] * xh[j];
      si += model.w[1][i][j] * xh[j];
      sc += model.w[2][i][j] * xh[j];
      so += model.w[3][i][j] * xh[j];
    }
    f[i] = sigmoid(sf);
    in[i] = sigmoid(si);
    cell[i] = tanhf(sc);
    out[i] = sigmoid(so);
    model.c[i] = f[i] * model.c[i] + in[i] * cell[i];
    model.h[i] = out[i] * tanhf(model.c[i]);
  }

  float y = model.bd;
  for (int i = 0; i < H; ++i) y += model.wd[i] * model.h[i];
  return y;
}

static float infer_24() {
  memset(model.h, 0, sizeof(model.h));
  memset(model.c, 0, sizeof(model.c));
  float y = 0.0f;
  for (int i = 0; i < LOOKBACK; ++i) y = lstm_step(0.2f + 0.01f * i);
  sink = y;
  return y;
}

static uint32_t measure_preprocess() {
  uint64_t t = esp_timer_get_time();
  float x[LOOKBACK];
  for (int i = 0; i < LOOKBACK; ++i) x[i] = (i * 37.0f) / 1000.0f;
  sink = x[LOOKBACK - 1];
  return (uint32_t)(esp_timer_get_time() - t);
}

static uint32_t measure_packet() {
  uint64_t t = esp_timer_get_time();
  char packet[96];
  int n = snprintf(packet, sizeof(packet),
                   "H:G=%.2f,V=%.2f,A=%.3f,D=%.4f,S=0\n",
                   642.0f, 16.8f, 2.41f, 0.7123f);
  sink = (float)n;
  return (uint32_t)(esp_timer_get_time() - t);
}

static uint32_t measure_uart() {
  char packet[96];
  int n = snprintf(packet, sizeof(packet),
                   "H:G=%.2f,V=%.2f,A=%.3f,D=%.4f,S=0\n",
                   642.0f, 16.8f, 2.41f, 0.7123f);
  uint64_t t = esp_timer_get_time();
  Serial2.write((const uint8_t *)packet, n);
  Serial2.flush();
  return (uint32_t)(esp_timer_get_time() - t);
}

static void stats(const char *name, uint32_t *v, int n) {
  uint64_t sum = 0;
  uint32_t mn = UINT32_MAX, mx = 0;
  for (int i = 0; i < n; ++i) {
    sum += v[i]; mn = min(mn, v[i]); mx = max(mx, v[i]);
  }
  std::sort(v, v + n);
  auto pct = [v, n](float p) { return v[(int)((n - 1) * p)]; };
  Serial.printf("RESULT %-14s n=%d mean=%.2fus min=%uus p50=%uus p95=%uus p99=%uus max=%uus\n",
                name, n, (double)sum / n, mn, pct(0.50), pct(0.95), pct(0.99), mx);
}

static void run_timing_batch() {
  static uint32_t prep[N], inf[N], packet[N], uart[N], full[N];
  for (int i = 0; i < N; ++i) {
    prep[i] = measure_preprocess();
    uint64_t t = esp_timer_get_time();
    infer_24();
    inf[i] = (uint32_t)(esp_timer_get_time() - t);
    packet[i] = measure_packet();
    uart[i] = measure_uart();
    t = esp_timer_get_time();
    measure_preprocess(); infer_24(); measure_packet(); measure_uart();
    full[i] = (uint32_t)(esp_timer_get_time() - t);
  }
  stats("preprocess", prep, N);
  stats("lstm_24step", inf, N);
  stats("packet_format", packet, N);
  stats("uart_115200", uart, N);
  stats("full_helio_tick", full, N);
}

static void run_loop_jitter() {
  static uint32_t periods[N];
  uint64_t next = esp_timer_get_time() + LOOP_US;
  uint64_t previous = 0;
  for (int i = 0; i < N; ++i) {
    while ((int64_t)(esp_timer_get_time() - next) < 0) delayMicroseconds(50);
    uint64_t start = esp_timer_get_time();
    periods[i] = i == 0 ? LOOP_US : (uint32_t)(start - previous);
    previous = start;
    measure_preprocess(); infer_24(); measure_packet(); measure_uart();
    next += LOOP_US;
  }
  stats("loop_period_us", periods, N);
  uint32_t jitter[N];
  for (int i = 0; i < N; ++i) jitter[i] = periods[i] > LOOP_US ? periods[i] - LOOP_US : LOOP_US - periods[i];
  stats("loop_abs_jitter", jitter, N);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial2.begin(115200, SERIAL_8N1, 18, 17);
  init_model();
  Serial.println("=== ESP32-S3 Helios timing probe ===");
  Serial.printf("CPU=%u MHz PSRAM=%u bytes FLASH probe; N=%d\n", getCpuFrequencyMhz(), ESP.getPsramSize(), N);
  Serial.println("No USB output occurs inside timed sections.");
  run_timing_batch();
  run_loop_jitter();
  Serial.printf("SINK=%.6f\n", (double)sink);
  Serial.println("=== PROBE DONE ===");
}

void loop() { delay(1000); }
