/**
 * ╔══════════════════════════════════════════════════════════════════════╗
 * ║  HELIOS DATA LOGGER  —  v2.6  (single-file build)                   ║
 * ║  ESP32-S3 N16R8 WROOM · GY-302 (BH1750) · OV2640 · DS3231         ║
 * ║                                                                      ║
 * ║  NEW IN v2.6  — PERMANENT FIX for FB-OVF / gdma_disconnect          ║
 * ║                                                                      ║
 * ║  Root cause (definitive): every previous fix attempted to make the  ║
 * ║  deinit/reinit cycle between RGB565 and JPEG modes safe. This is    ║
 * ║  impossible without patching the esp32-camera library itself.        ║
 * ║  esp_camera_deinit() calls gdma_disconnect() on the camera's DMA   ║
 * ║  channel, but does NOT call gdma_stop() first. If any DMA           ║
 * ║  transaction is in flight (even partially), the channel is left in  ║
 * ║  an undefined hardware state. esp_camera_init() then calls          ║
 * ║  gdma_connect() on the same channel without a reset, so the new    ║
 * ║  mode inherits the corrupt state → FB-OVF → gdma_disconnect on    ║
 * ║  cleanup. Draining frames and adding delays cannot fix this because ║
 * ║  the DMA hardware (not software) is broken.                         ║
 * ║                                                                      ║
 * ║  Fix strategy: ELIMINATE ALL DEINIT/REINIT CYCLES.                  ║
 * ║  The camera is initialised ONCE in RGB565/GRAB_WHEN_EMPTY mode      ║
 * ║  (fb_count=1) and never reinitialized. JPEG images are produced by  ║
 * ║  calling frame2jpg() on a raw RGB565 frame — the esp32-camera       ║
 * ║  library ships this function and it is exactly what the internal    ║
 * ║  JPEG encoder uses. Quality and size are unchanged (96×96, q=5).    ║
 * ║  The camera stream/snapshot handlers are updated similarly.         ║
 * ║  The didSample/didCapture tick-guard and all drain loops are        ║
 * ║  removed — they were workarounds for a problem that no longer       ║
 * ║  exists.                                                             ║
 * ║                                                                      ║
 * ║  NEW IN v2.5                                                         ║
 * ║  · Fix: FB-OVF / gdma_disconnect still firing after v2.4.           ║
 * ║    Root cause 1: restore path at end of captureAndSaveImage() had   ║
 * ║    the identical single-frame drain problem as the entry path.       ║
 * ║  · Root cause 2: didSample guard was one-directional.               ║
 * ║                                                                      ║
 * ║  NEW IN v2.4                                                         ║
 * ║  · Fix: single-frame drain + 80 ms delay was insufficient.          ║
 * ║                                                                      ║
 * ║  NEW IN v2.3                                                         ║
 * ║  · Fix: FB-OVF / gdma_disconnect on image capture                   ║
 * ║  · Fix: drain settle delay 50 ms → 80 ms                            ║
 * ║                                                                      ║
 * ║  NEW IN v2.2                                                         ║
 * ║  · WDT reset inside thermal cooldown loop (prevents panic reboot)   ║
 * ║  · Boot reason logged on startup (WDT / brownout / thermal / etc.)  ║
 * ║  · Thermal event counter — persisted in daily summary CSV           ║
 * ║  · Per-day thermal log  /data/YYYY-MM-DD.therm  (timestamp+temp)   ║
 * ║  · Thermal hysteresis — resumes only when temp drops 10°C below     ║
 * ║    threshold, preventing rapid re-trigger in hot ambient            ║
 * ║  · WiFi auto-off on thermal trigger, restart after cooldown         ║
 * ║  · Camera powered down during thermal sleep, reinit after           ║
 * ║  · Brownout guard — skips flash write if reset reason was brownout  ║
 * ║  · NTP sync attempt when WiFi AP has a connected client             ║
 * ║  · CSV integrity check on openDayFile — trims truncated last line   ║
 * ║                                                                      ║
 * ║  NEW IN v2.1                                                         ║
 * ║  · Thermal protection — configurable shutdown temp (default 75°C)   ║
 * ║    When die temp exceeds threshold, flushes buffers and enters       ║
 * ║    light sleep for configurable cooldown (default 120 s). Disabled  ║
 * ║    by setting threshold to 0. Adjustable via Settings page.         ║
 * ║  · Status LED on GPIO 48 (built-in LED on ESP32-S3 WROOM)          ║
 * ║    Slow blink (1 s)  — logging active                               ║
 * ║    Fast blink (160ms) — error / thermal cooldown                    ║
 * ║    Solid on           — WiFi AP active                              ║
 * ║    Off                — night / idle                                ║
 * ║  · Bug fix: getChipTempC() → readDieTemp() in handleStatus()        ║
 * ║                                                                      ║
 * ║  NEW IN v2.0                                                         ║
 * ║  · Partition changed to 2MB APP / 12.5MB FATFS (16MB flash)         ║
 * ║    Use custom partition table: helios_16mb_2app_12fat.csv           ║
 * ║                                                                      ║
 * ║  NEW IN v1.9                                                         ║
 * ║  · Embedded OTA — no Guardian required                              ║
 * ║    /ota page: drag-and-drop .bin upload with progress bar           ║
 * ║    POST /api/ota-upload: streams .bin via Update library, reboots   ║
 * ║                                                                      ║
 * ║  FROM v1.5                                                           ║
 * ║  · Sky images — JPEG 96x96 every 3 min, stored in /imgs/            ║
 * ║  · Settings page (/settings) — all params adjustable, saved to      ║
 * ║    /data/config.json, loaded on boot                                 ║
 * ║  · GPIO 3 second AP button — wire external pushbutton               ║
 * ║  · Light mode dashboard — high contrast for outdoor use             ║
 * ║                                                                      ║
 * ║  PARTITION: 16M Flash (2MB APP / 12.5MB FATFS)  ← REQUIRED         ║
 * ║                                                                      ║
 * ║  CSV FORMAT  (/data/YYYY-MM-DD.csv)                                 ║
 * ║  date,time,elapsed_s,lux,irradiance_wm2,blue_channel,temp_c        ║
 * ║                                                                      ║
 * ║  IMAGES  (/imgs/YYYYMMDD_HHMMSS.jpg)  96x96 JPEG q=5               ║
 * ║                                                                      ║
 * ║  Hussain Touhid Siddiquee · Leading University Sylhet · 2026        ║
 * ╚══════════════════════════════════════════════════════════════════════╝
 *
 * WIRING
 * ──────
 * GY-302   SDA → GPIO 45   SCL → GPIO 46   VCC → 3.3V   GND → GND
 * DS3231   SDA → GPIO 1    SCL → GPIO 2    VCC → 3.3V   GND → GND
 * OV2640   XCLK→15 SDA→4 SCL→5 D0→11 D1→9 D2→8 D3→10
 *          D4→12 D5→18 D6→17 D7→16 VSYNC→6 HREF→7 PCLK→13
 * AP BTN 1 → GPIO 0  (BOOT button, built-in)
 * AP BTN 2 → GPIO 3  (external button, other leg to GND)
 *
 * LIBRARIES
 * ─────────
 * · RTClib by Adafruit
 * · BH1750 by Christopher Laws
 *
 * ARDUINO IDE SETTINGS
 * ─────────────────────
 * Board            : ESP32S3 Dev Module
 * Flash Size       : 16MB
 * Partition Scheme : Custom (16M Flash: 2MB APP / 12.5MB FATFS)  ← IMPORTANT
 * PSRAM            : OPI PSRAM
 * Upload Speed     : 921600
 */

// ═══════════════════════════════════════════════════════════════════════════
//  SECTION 0 — INCLUDES
// ═══════════════════════════════════════════════════════════════════════════
#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <FFat.h>
#include <BH1750.h>
#include <RTClib.h>
#include "esp_camera.h"
#include "esp_sleep.h"
#include "esp_wifi.h"
#include "driver/temperature_sensor.h"
#include "esp_task_wdt.h"
#include "esp_system.h"
#include <time.h>
#include <Preferences.h>
#include <Update.h>
#include <math.h>
#include <pgmspace.h>
#include <cstdarg>
#include <Adafruit_NeoPixel.h>   // GPIO 48 on ESP32-S3 WROOM is WS2812B RGB, not plain GPIO

// ═══════════════════════════════════════════════════════════════════════════
//  SECTION 1 — CAMERA PINS (ESP32-S3 N16R8 WROOM)
// ═══════════════════════════════════════════════════════════════════════════
#define PWDN_GPIO_NUM  -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM  15
#define SIOD_GPIO_NUM   4
#define SIOC_GPIO_NUM   5
#define Y9_GPIO_NUM    16
#define Y8_GPIO_NUM    17
#define Y7_GPIO_NUM    18
#define Y6_GPIO_NUM    12
#define Y5_GPIO_NUM    10
#define Y4_GPIO_NUM     8
#define Y3_GPIO_NUM     9
#define Y2_GPIO_NUM    11
#define VSYNC_GPIO_NUM  6
#define HREF_GPIO_NUM   7
#define PCLK_GPIO_NUM  13

// ═══════════════════════════════════════════════════════════════════════════
//  SECTION 2 — SOLAR CALCULATOR  (NOAA / Meeus algorithm)
// ═══════════════════════════════════════════════════════════════════════════

// Convert Gregorian date to Julian Day Number
static double dateToJulian(int yr, int mo, int dy) {
  if (mo <= 2) { yr -= 1; mo += 12; }
  int A = (int)(yr / 100);
  int B = 2 - A + (int)(A / 4);
  return (int)(365.25 * (yr + 4716)) + (int)(30.6001 * (mo + 1)) + dy + B - 1524.5;
}

// calcSunTimes — fills srMin and ssMin (minutes from local midnight)
// Returns false if sun never rises/sets (polar conditions — won't happen in BD).
static bool calcSunTimes(int yr, int mo, int dy,
                         double lat, double lon, float utcOffsetH,
                         int &srMin, int &ssMin)
{
  const double DEG = M_PI / 180.0;
  const double RAD = 180.0 / M_PI;

  double JD    = dateToJulian(yr, mo, dy);
  double T     = (JD - 2451545.0) / 36525.0;       // Julian centuries from J2000

  // Solar mean longitude & mean anomaly (degrees)
  double L0    = fmod(280.46646 + T * (36000.76983 + T * 0.0003032), 360.0);
  double M     = fmod(357.52911 + T * (35999.05029 - T * 0.0001537), 360.0);
  double Mrad  = M * DEG;

  // Equation of centre
  double C = (1.914602 - T * (0.004817 + 0.000014 * T)) * sin(Mrad)
           + (0.019993 - 0.000101 * T) * sin(2.0 * Mrad)
           +  0.000289 * sin(3.0 * Mrad);

  double sunLon = L0 + C;                           // Sun true longitude
  double omega  = 125.04 - 1934.136 * T;            // Ascending node
  double lambda = sunLon - 0.00569 - 0.00478 * sin(omega * DEG); // apparent lon

  // Obliquity of ecliptic (corrected)
  double eps0   = 23.0 + (26.0 + (21.448 - T * (46.8150 + T * (0.00059 - T * 0.001813))) / 60.0) / 60.0;
  double eps    = eps0 + 0.00256 * cos(omega * DEG);

  // Sun declination
  double sinDec = sin(eps * DEG) * sin(lambda * DEG);
  double decl   = asin(sinDec);                     // radians

  // Equation of time (minutes)
  double e2     = (eps / 2.0) * DEG;
  double y      = tan(e2) * tan(e2);
  double L0r    = L0 * DEG;
  double Mr     = M  * DEG;
  double ecc    = 0.016708634 - T * (0.000042037 + 0.0000001267 * T);
  double EqT    = RAD * (y * sin(2.0 * L0r)
                       - 2.0 * ecc * sin(Mr)
                       + 4.0 * ecc * y * sin(Mr) * cos(2.0 * L0r)
                       - 0.5 * y * y * sin(4.0 * L0r)
                       - 1.25 * ecc * ecc * sin(2.0 * Mr)) * 4.0; // → minutes

  // Hour angle for sunrise (zenith = 90.833° accounts for refraction + solar disc)
  double latRad = lat * DEG;
  double cosHA  = (cos(90.833 * DEG) - sin(latRad) * sinDec)
                / (cos(latRad) * cos(decl));

  if (cosHA < -1.0 || cosHA > 1.0) {
    // Polar day or night — use fallback
    srMin = 360; ssMin = 1080;
    return false;
  }

  double HA = acos(cosHA) * RAD;                    // degrees, sunrise side

  // Solar noon in local minutes from midnight
  double solarNoon = 720.0 - 4.0 * lon + EqT + utcOffsetH * 60.0;

  double srMind = solarNoon - 4.0 * HA;             // minutes from midnight
  double ssMind = solarNoon + 4.0 * HA;

  // Clamp to valid 0–1439 range (shouldn't be needed in tropics)
  srMin = (int)round(srMind); if (srMin < 0) srMin += 1440; if (srMin > 1439) srMin -= 1440;
  ssMin = (int)round(ssMind); if (ssMin < 0) ssMin += 1440; if (ssMin > 1439) ssMin -= 1440;
  return true;
}

// ═══════════════════════════════════════════════════════════════════════════
//  WEBLOGGER — Wireless serial monitor over SSE
// ═══════════════════════════════════════════════════════════════════════════
#define WLOG_LINES   120          // ring buffer depth
#define WLOG_WIDTH   224          // max chars per line (including timestamp)
#define WLOG_BUF_MS  40           // SSE flush interval ms

static char     _wlogBuf[WLOG_LINES][WLOG_WIDTH];
static uint16_t _wlogHead   = 0;
static uint16_t _wlogCount  = 0;
static uint32_t _wlogSerial = 0;
static uint32_t _wlogBootMs = 0;

static void _wlogWrite(const char *line) {
  uint32_t ms = millis() - _wlogBootMs;
  snprintf(_wlogBuf[_wlogHead], WLOG_WIDTH,
    "[%3lu:%02lu.%03lu] %s",
    ms/60000, (ms/1000)%60, ms%1000, line);
  _wlogHead = (_wlogHead + 1) % WLOG_LINES;
  if(_wlogCount < WLOG_LINES) _wlogCount++;
  _wlogSerial++;
  Serial.println(line);
}

void wlog(const char *msg)  { _wlogWrite(msg); }
void wlogf(const char *fmt, ...) {
  char tmp[200];
  va_list ap; va_start(ap,fmt); vsnprintf(tmp,sizeof(tmp),fmt,ap); va_end(ap);
  _wlogWrite(tmp);
}
void wlogInit() { _wlogBootMs = millis(); }

// ── Log viewer HTML ──────────────────────────────────────────────────────────
static const char SERIAL_HTML[] PROGMEM = R"SERLOG(
<!DOCTYPE html><html lang="en"><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Helios Serial</title>
<style>
:root{
  --bg:#080E08;--surface:#0D160D;--border:#1A2E1A;
  --green:#4ADE80;--green-dim:#0D2010;--green-mid:#22C55E;
  --amber:#FCD34D;--red:#F87171;
  --text:#D4EDD4;--text-dim:#6A9A6A;--text-muted:#3A5A3A;
  --mono:'Courier New',monospace;
}
*,*::before,*::after{box-sizing:border-box;margin:0;padding:0}
html,body{height:100%;background:var(--bg);color:var(--text);font-family:var(--mono);font-size:13px}
body{display:flex;flex-direction:column}
.topbar{background:var(--surface);border-bottom:2px solid var(--border);
        padding:10px 16px;display:flex;align-items:center;gap:12px;flex-shrink:0}
.topbar .title{font-size:14px;font-weight:700;letter-spacing:0.06em;color:var(--green);flex:1}
.badge{background:var(--green-dim);border:1px solid var(--green);border-radius:4px;
       padding:3px 8px;font-size:11px;font-weight:700;color:var(--green)}
.badge.conn{animation:pulse 1.5s infinite}
.badge.disc{border-color:var(--red);color:var(--red);background:#1A0808}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:0.5}}
.controls{display:flex;gap:8px}
.btn{background:var(--surface);border:1.5px solid var(--border);border-radius:5px;
     color:var(--text-dim);font-family:var(--mono);font-size:11px;font-weight:700;
     padding:5px 10px;cursor:pointer}
.btn:hover{border-color:var(--green);color:var(--green)}
.btn.active{border-color:var(--green);color:var(--green);background:var(--green-dim)}
.filter-row{background:var(--surface);border-bottom:1px solid var(--border);
            padding:6px 16px;display:flex;gap:8px;align-items:center;flex-shrink:0}
.filter-row input{background:var(--bg);border:1.5px solid var(--border);border-radius:4px;
                   color:var(--text);font-family:var(--mono);font-size:12px;
                   padding:4px 8px;flex:1}
.filter-row input:focus{outline:none;border-color:var(--green)}
.filter-row input::placeholder{color:var(--text-muted)}
.log-wrap{flex:1;overflow-y:auto;padding:8px 0}
#log{list-style:none}
#log li{padding:2px 16px;border-bottom:1px solid #0A120A;white-space:pre-wrap;word-break:break-all;
        font-size:12px;line-height:1.6}
#log li:hover{background:#0D160D}
#log li .ts{color:var(--text-muted)}
#log li.warn{color:var(--amber)}
#log li.err{color:var(--red)}
#log li.ok{color:var(--green)}
.stats{background:var(--surface);border-top:1px solid var(--border);
       padding:5px 16px;font-size:10px;color:var(--text-muted);display:flex;gap:16px;flex-shrink:0}
#scrollAnchor{height:1px}
</style>
</head>
<body>
<div class="topbar">
  <span class="title">&#9654; HELIOS SERIAL MONITOR</span>
  <span class="badge conn" id="connBadge">LIVE</span>
  <div class="controls">
    <button class="btn active" id="scrollBtn" onclick="toggleScroll()">&#8595; Scroll</button>
    <button class="btn" onclick="clearLog()">&#10005; Clear</button>
    <button class="btn" onclick="downloadLog()">&#8595; Save</button>
    <a class="btn" href="/">&#8592; Back</a>
  </div>
</div>
<div class="filter-row">
  <input type="text" id="filterInput" placeholder="Filter logs... (e.g. [WiFi] or ERROR)" oninput="applyFilter()">
</div>
<div class="log-wrap" id="logWrap">
  <ul id="log"></ul>
  <div id="scrollAnchor"></div>
</div>
<div class="stats">
  <span id="statLines">0 lines</span>
  <span id="statFiltered"></span>
  <span id="statDropped"></span>
</div>

<script>
let autoScroll = true;
let filterText = '';
let allLines   = [];
let totalLines = 0;
let droppedLines = 0;
let es = null;

function toggleScroll() {
  autoScroll = !autoScroll;
  document.getElementById('scrollBtn').classList.toggle('active', autoScroll);
  if(autoScroll) document.getElementById('scrollAnchor').scrollIntoView();
}

function clearLog() {
  allLines = [];
  document.getElementById('log').innerHTML = '';
  updateStats();
}

function downloadLog() {
  const text = allLines.join('\n');
  const a = document.createElement('a');
  a.href = URL.createObjectURL(new Blob([text], {type:'text/plain'}));
  a.download = 'helios-serial-' + new Date().toISOString().slice(0,19).replace(/:/g,'-') + '.txt';
  a.click();
}

function classForLine(line) {
  const l = line.toLowerCase();
  if(l.includes('error') || l.includes('fatal') || l.includes('fail')) return 'err';
  if(l.includes('warn') || l.includes('lost') || l.includes('timeout')) return 'warn';
  if(l.includes('ready') || l.includes('ok') || l.includes('done') || l.includes('complete')) return 'ok';
  return '';
}

function appendLine(raw) {
  allLines.push(raw);
  totalLines++;
  if(allLines.length > 2000) { allLines.shift(); droppedLines++; }

  // Only render if passes filter
  if(filterText && !raw.toLowerCase().includes(filterText)) return;

  const li = document.createElement('li');
  li.className = classForLine(raw);
  // Split timestamp from rest for colouring
  const m = raw.match(/^(\[\s*[\d:\.]+\]\s*)(.*)/s);
  if(m) {
    li.innerHTML = '<span class="ts">' + escHtml(m[1]) + '</span>' + escHtml(m[2]);
  } else {
    li.textContent = raw;
  }
  document.getElementById('log').appendChild(li);

  // Cap DOM to 500 visible lines
  const ul = document.getElementById('log');
  while(ul.children.length > 500) ul.removeChild(ul.firstChild);

  if(autoScroll) document.getElementById('scrollAnchor').scrollIntoView({behavior:'instant'});
  updateStats();
}

function escHtml(s) {
  return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
}

function applyFilter() {
  filterText = document.getElementById('filterInput').value.toLowerCase();
  const ul = document.getElementById('log');
  ul.innerHTML = '';
  const visible = filterText ? allLines.filter(l => l.toLowerCase().includes(filterText)) : allLines;
  visible.slice(-500).forEach(l => {
    const li = document.createElement('li');
    li.className = classForLine(l);
    const m = l.match(/^(\[\s*[\d:\.]+\]\s*)(.*)/s);
    if(m) li.innerHTML = '<span class="ts">'+escHtml(m[1])+'</span>'+escHtml(m[2]);
    else li.textContent = l;
    ul.appendChild(li);
  });
  updateStats();
  if(autoScroll) document.getElementById('scrollAnchor').scrollIntoView();
}

function updateStats() {
  document.getElementById('statLines').textContent = totalLines + ' lines';
  const vis = document.getElementById('log').children.length;
  if(filterText) document.getElementById('statFiltered').textContent = vis + ' matching';
  else document.getElementById('statFiltered').textContent = '';
  if(droppedLines) document.getElementById('statDropped').textContent = droppedLines + ' dropped';
}

function connect() {
  if(es) es.close();
  es = new EventSource('/serial/stream');

  es.onopen = () => {
    document.getElementById('connBadge').className = 'badge conn';
    document.getElementById('connBadge').textContent = 'LIVE';
  };

  es.onmessage = e => {
    // Each SSE data field is one log line
    appendLine(e.data);
  };

  es.onerror = () => {
    document.getElementById('connBadge').className = 'badge disc';
    document.getElementById('connBadge').textContent = 'DISCONNECTED';
    es.close();
    // Reconnect after 2s
    setTimeout(connect, 2000);
  };
}

// Load historical lines then start SSE
async function init() {
  try {
    const resp = await fetch('/serial/dump');
    const text = await resp.text();
    text.split('\n').filter(Boolean).forEach(appendLine);
  } catch(e) {}
  connect();
}
init();
</script>
</body></html>
)SERLOG";

static WiFiClient _sseCli;
static bool       _sseActive  = false;
static uint16_t   _sseCursor  = 0;
static uint32_t   _sseLastMs  = 0;

static void _sseSendLine(WiFiClient &cli, const char *line);
static WebServer *_wlogServer = nullptr;

void handleSerialPage() {
  _wlogServer->sendHeader("Cache-Control","no-cache");
  _wlogServer->send_P(200,"text/html",SERIAL_HTML);
}

void handleSerialStream() {
  if(_sseActive) { _sseCli.stop(); _sseActive = false; }

  WiFiClient cli = _wlogServer->client();
  cli.print("HTTP/1.1 200 OK\r\n"
            "Content-Type: text/event-stream\r\n"
            "Cache-Control: no-cache\r\n"
            "Connection: keep-alive\r\n"
            "Access-Control-Allow-Origin: *\r\n\r\n");


  uint16_t start = (_wlogCount >= WLOG_LINES) ? _wlogHead : 0;
  for(uint16_t i = 0; i < _wlogCount; i++) {
    uint16_t idx = (start + i) % WLOG_LINES;
    _sseSendLine(cli, _wlogBuf[idx]);
  }
  cli.flush();

  _sseCli    = cli;
  _sseCursor = _wlogSerial;
  _sseActive = true;
}

void handleSerialDump() {
  String out;
  out.reserve(_wlogCount * 80);
  uint16_t start = (_wlogCount >= WLOG_LINES) ? _wlogHead : 0;
  for(uint16_t i = 0; i < _wlogCount; i++) {
    out += _wlogBuf[(start + i) % WLOG_LINES];
    out += '\n';
  }
  _wlogServer->sendHeader("Access-Control-Allow-Origin","*");
  _wlogServer->send(200,"text/plain",out);
}

static void _sseSendLine(WiFiClient &cli, const char *line) {
  cli.print("data: ");
  cli.print(line);
  cli.print("\n\n");
}

void webLoggerLoop() {
  if(!_sseActive) return;
  if(!_sseCli.connected()) { _sseActive = false; return; }
  uint32_t now = millis();
  if(now - _sseLastMs < WLOG_BUF_MS) return;
  _sseLastMs = now;

  if(_wlogSerial == _sseCursor) return;
  uint32_t lag = _wlogSerial - _sseCursor;
  if(lag > WLOG_LINES) lag = WLOG_LINES;

  uint32_t startSeq = _wlogSerial - lag;
  for(uint32_t i = 0; i < lag; i++) {
    uint16_t idx = (uint16_t)((startSeq + i) % WLOG_LINES);
    _sseSendLine(_sseCli, _wlogBuf[idx]);
  }
  _sseCli.flush();
  _sseCursor = _wlogSerial;
}

void registerWebLogger(WebServer &srv) {
  _wlogServer = &srv;
  srv.on("/serial",        HTTP_GET, handleSerialPage);
  srv.on("/serial/stream", HTTP_GET, handleSerialStream);
  srv.on("/serial/dump",   HTTP_GET, handleSerialDump);
}

// ═══════════════════════════════════════════════════════════════════════════
//  SECTION 3 — HTML: DASHBOARD
// ═══════════════════════════════════════════════════════════════════════════

const char DASHBOARD_HTML[] PROGMEM = R"DASH(
<!DOCTYPE html><html lang="en"><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Helios Logger</title>
<style>
:root{
  --bg:#F0F4F8;--surface:#FFFFFF;--card:#FFFFFF;--border:#C8D6E0;
  --teal:#007A7A;--teal-light:#E0F4F4;--teal-glow:rgba(0,122,122,0.12);
  --amber:#B86800;--amber-bg:#FFF3DC;
  --blue:#1A5FA8;--blue-bg:#E8F0FB;
  --green:#1A7A45;--green-bg:#E4F5EC;
  --red:#B82222;--red-bg:#FDEAEA;
  --orange:#A04000;--orange-bg:#FFF0E4;
  --text:#1A2530;--text-dim:#4A6070;--text-muted:#7A96A8;
  --mono:'Courier New',monospace;--sans:system-ui,sans-serif;
  --radius:8px;--shadow:0 2px 8px rgba(0,0,0,0.10);
}
*,*::before,*::after{box-sizing:border-box;margin:0;padding:0}
html{font-size:16px}
body{background:var(--bg);color:var(--text);font-family:var(--sans);line-height:1.5;min-height:100vh}
a{color:var(--teal)}
.shell{max-width:980px;margin:0 auto;padding:0 16px 48px}
header{
  display:flex;align-items:center;justify-content:space-between;
  padding:18px 0 16px;border-bottom:3px solid var(--teal);margin-bottom:24px;
  background:var(--surface);position:sticky;top:0;z-index:100;
  box-shadow:var(--shadow);margin-left:-16px;margin-right:-16px;
  padding-left:16px;padding-right:16px;
}
.logo{display:flex;align-items:center;gap:12px}
.logo-icon{width:44px;height:44px;background:var(--teal);border-radius:10px;
  display:flex;align-items:center;justify-content:center;font-size:22px;color:#fff}
.logo-text{font-family:var(--mono);font-size:18px;font-weight:700;color:var(--teal);letter-spacing:0.04em}
.logo-sub{font-size:12px;color:var(--text-dim);letter-spacing:0.04em;margin-top:1px}
.header-right{display:flex;align-items:center;gap:8px;flex-wrap:wrap;justify-content:flex-end}
.nav-btn{
  background:var(--teal);color:#fff;border:none;border-radius:6px;
  font-family:var(--mono);font-size:12px;font-weight:700;
  padding:7px 12px;cursor:pointer;text-decoration:none;display:inline-flex;align-items:center;gap:5px;white-space:nowrap
}
.nav-btn:hover{background:#005555}
.nav-btn-ota{background:#7A4800;border-color:#B86800;color:#FCD34D}
.nav-btn-ota:hover{background:#B86800;color:#fff}
.status-pill{
  display:flex;align-items:center;gap:8px;padding:6px 12px;
  border-radius:20px;border:2px solid var(--border);
  font-family:var(--mono);font-size:12px;font-weight:700;
  background:var(--surface);transition:all 0.3s;white-space:nowrap
}
.status-pill.active{border-color:var(--green);background:var(--green-bg);color:var(--green)}
.status-pill.idle{border-color:var(--text-muted);color:var(--text-dim)}
.dot{width:10px;height:10px;border-radius:50%;background:currentColor;flex-shrink:0}
.dot.pulse{animation:pulse 1.4s ease-in-out infinite}
@keyframes pulse{0%,100%{opacity:1;transform:scale(1)}50%{opacity:0.4;transform:scale(0.7)}}

/* Day/Night banner */
.daynight-banner{
  display:flex;align-items:center;justify-content:space-between;gap:12px;flex-wrap:wrap;
  padding:10px 18px;border-radius:8px;margin-bottom:20px;
  font-family:var(--mono);font-size:13px;font-weight:700;
  border:2px solid var(--border);background:var(--surface);
  box-shadow:var(--shadow);transition:all 0.4s;
}
.daynight-banner.day{
  border-color:#B86800;background:#FFF8E8;color:#7A4800;
}
.daynight-banner.night{
  border-color:#3A5A8A;background:#EEF2FA;color:#1A3A6A;
}
.daynight-banner.override{
  border-color:#1A7A45;background:#E4F5EC;color:#0D4A2A;
}
.dn-left{display:flex;align-items:center;gap:10px}
.dn-icon{font-size:24px;line-height:1}
.dn-label{font-size:16px;font-weight:800;letter-spacing:0.04em}
.dn-sub{font-size:11px;font-weight:600;opacity:0.8;margin-top:2px}
.dn-right{display:flex;align-items:center;gap:8px;flex-wrap:wrap}
.dn-time{font-size:12px;font-weight:700;padding:5px 10px;border-radius:5px;background:rgba(0,0,0,0.06)}
.btn-override{
  font-family:var(--mono);font-size:12px;font-weight:700;border:none;border-radius:6px;
  padding:7px 14px;cursor:pointer;display:inline-flex;align-items:center;gap:5px;
  transition:all 0.15s;white-space:nowrap
}
.btn-override.start{background:#1A7A45;color:#fff}
.btn-override.start:hover{background:#0D5530}
.btn-override.stop{background:#B82222;color:#fff}
.btn-override.stop:hover{background:#8A1010}

.refresh-ring{width:36px;height:36px;position:relative;cursor:pointer;flex-shrink:0}
.refresh-ring svg{transform:rotate(-90deg)}
.refresh-ring circle{fill:none;stroke-width:3}
.ring-bg{stroke:var(--border)}
.ring-fill{stroke:var(--teal);stroke-dasharray:88;stroke-dashoffset:88;transition:stroke-dashoffset 1s linear}
.refresh-ring span{position:absolute;top:50%;left:50%;transform:translate(-50%,-50%);
  font-family:var(--mono);font-size:9px;color:var(--text-dim);font-weight:700}

.section-label{
  font-family:var(--mono);font-size:12px;font-weight:700;letter-spacing:0.10em;
  text-transform:uppercase;color:var(--teal);
  margin-bottom:12px;margin-top:28px;display:flex;align-items:center;gap:8px;
}
.section-label::after{content:'';flex:1;height:2px;background:var(--teal-light)}

.stats-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:12px;margin-bottom:4px}
.stat-card{
  background:var(--card);border:2px solid var(--border);border-radius:var(--radius);
  padding:16px;position:relative;overflow:hidden;box-shadow:var(--shadow);
}
.stat-card::before{content:'';position:absolute;top:0;left:0;right:0;height:4px}
.stat-card.teal::before{background:var(--teal)}
.stat-card.amber::before{background:var(--amber)}
.stat-card.blue::before{background:var(--blue)}
.stat-card.green::before{background:var(--green)}
.stat-card.orange::before{background:var(--orange)}
.stat-card.red::before{background:var(--red)}
.stat-label{font-size:11px;font-weight:700;letter-spacing:0.08em;text-transform:uppercase;
  color:var(--text-dim);margin-bottom:8px}
.stat-value{font-family:var(--mono);font-size:26px;font-weight:700;color:var(--text);line-height:1;margin-bottom:4px}
.stat-value.teal{color:var(--teal)}
.stat-value.amber{color:var(--amber)}
.stat-value.blue{color:var(--blue)}
.stat-value.orange{color:var(--orange)}
.stat-value.green{color:var(--green)}
.stat-value.red{color:var(--red)}
.stat-sub{font-size:11px;color:var(--text-muted);font-weight:500}
.temp-bar-wrap{margin-top:8px}
.temp-bar-track{height:5px;background:var(--border);border-radius:3px;overflow:hidden}
.temp-bar-fill{height:100%;border-radius:3px;transition:width 0.5s,background 0.5s}

.chart-card{background:var(--card);border:2px solid var(--border);border-radius:var(--radius);
  padding:20px;margin-bottom:4px;box-shadow:var(--shadow)}
.chart-header{display:flex;align-items:center;justify-content:space-between;margin-bottom:16px;flex-wrap:wrap;gap:10px}
.chart-title{font-family:var(--mono);font-size:14px;font-weight:700;color:var(--text)}
.chart-legend{display:flex;gap:16px;flex-wrap:wrap}
.legend-item{display:flex;align-items:center;gap:6px;font-size:12px;color:var(--text-dim);font-weight:600}
.legend-dot{width:10px;height:10px;border-radius:50%;flex-shrink:0}
.chart-wrap{position:relative;height:220px}
canvas{display:block;width:100% !important}
.chart-range{display:flex;gap:6px;margin-top:10px;justify-content:flex-end}
.range-btn{
  background:var(--surface);border:2px solid var(--border);border-radius:6px;
  color:var(--text-dim);font-family:var(--mono);font-size:11px;font-weight:700;
  padding:4px 10px;cursor:pointer;transition:all 0.15s
}
.range-btn:hover,.range-btn.active{border-color:var(--teal);color:var(--teal);background:var(--teal-light)}

.day-selector{display:flex;gap:8px;flex-wrap:wrap;margin-bottom:4px}
.day-btn{
  background:var(--surface);border:2px solid var(--border);border-radius:6px;
  color:var(--text-dim);font-family:var(--mono);font-size:12px;font-weight:700;
  padding:6px 12px;cursor:pointer;transition:all 0.15s
}
.day-btn:hover{border-color:var(--teal);color:var(--teal)}
.day-btn.active{border-color:var(--teal);color:var(--teal);background:var(--teal-light)}

.summary-card{background:var(--card);border:2px solid var(--border);border-radius:var(--radius);
  margin-bottom:4px;overflow:hidden;box-shadow:var(--shadow)}
.summary-header{display:flex;align-items:center;justify-content:space-between;
  padding:14px 20px;border-bottom:2px solid var(--border);background:var(--teal-light)}
.summary-title{font-family:var(--mono);font-size:13px;font-weight:700;color:var(--teal)}
.summary-table{width:100%;border-collapse:collapse;font-family:var(--mono);font-size:13px}
.summary-table th{padding:10px 16px;text-align:left;color:var(--text-dim);font-size:11px;
  font-weight:700;letter-spacing:0.08em;border-bottom:2px solid var(--border);background:var(--bg)}
.summary-table td{padding:10px 16px;border-bottom:1px solid var(--border);color:var(--text);font-weight:600}
.summary-table tr:last-child td{border-bottom:none}
.summary-table tr:hover td{background:var(--teal-light)}
.cvi-good{color:var(--green);font-weight:700}
.cvi-med{color:var(--amber);font-weight:700}
.cvi-high{color:var(--red);font-weight:700}

.files-card{background:var(--card);border:2px solid var(--border);border-radius:var(--radius);
  margin-bottom:4px;overflow:hidden;box-shadow:var(--shadow)}
.files-header{display:flex;align-items:center;justify-content:space-between;
  padding:14px 20px;border-bottom:2px solid var(--border);background:var(--bg)}
.files-title{font-family:var(--mono);font-size:13px;font-weight:700;color:var(--text)}
.files-empty{padding:36px 20px;text-align:center;color:var(--text-muted);font-size:14px;font-family:var(--mono)}
.file-row{display:flex;align-items:center;justify-content:space-between;
  padding:12px 20px;border-bottom:1px solid var(--border);transition:background 0.12s;gap:12px}
.file-row:last-child{border-bottom:none}
.file-row:hover{background:var(--teal-light)}
.file-name{font-family:var(--mono);font-size:13px;color:var(--teal);font-weight:700;
  flex:1;min-width:0;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.file-meta{font-family:var(--mono);font-size:11px;color:var(--text-muted);white-space:nowrap;font-weight:600}
.file-actions{display:flex;gap:6px;flex-shrink:0}

.btn{
  display:inline-flex;align-items:center;gap:5px;border:none;border-radius:6px;
  font-family:var(--mono);font-size:12px;font-weight:700;letter-spacing:0.04em;
  cursor:pointer;padding:8px 14px;transition:all 0.15s;text-decoration:none;white-space:nowrap
}
.btn-teal{background:var(--teal);color:#fff}
.btn-teal:hover{background:#005555}
.btn-ghost{background:var(--surface);border:2px solid var(--border);color:var(--text-dim)}
.btn-ghost:hover{border-color:var(--teal);color:var(--teal);background:var(--teal-light)}
.btn-danger{background:var(--surface);border:2px solid var(--border);color:var(--text-dim)}
.btn-danger:hover{border-color:var(--red);color:var(--red);background:var(--red-bg)}
.btn-sm{padding:5px 10px;font-size:11px}

.flash-bar-wrap{background:var(--card);border:2px solid var(--border);border-radius:var(--radius);
  padding:16px 20px;margin-bottom:4px;box-shadow:var(--shadow)}
.flash-bar-header{display:flex;justify-content:space-between;margin-bottom:8px;
  font-size:13px;font-weight:700;color:var(--text-dim)}
.flash-bar-track{height:10px;background:var(--border);border-radius:5px;overflow:hidden}
.flash-bar-fill{height:100%;background:var(--teal);border-radius:5px;transition:width 0.5s}
.flash-bar-fill.warn{background:var(--amber)}
.flash-bar-fill.crit{background:var(--red)}

.rtc-set-card{background:var(--card);border:2px solid var(--border);border-radius:var(--radius);
  padding:20px;margin-bottom:4px;box-shadow:var(--shadow)}
.rtc-row{display:flex;gap:12px;flex-wrap:wrap;align-items:flex-end;margin-top:12px}
.rtc-row input{
  background:var(--bg);border:2px solid var(--border);border-radius:6px;
  color:var(--text);font-family:var(--mono);font-size:15px;font-weight:700;
  padding:8px 12px;min-width:160px
}
.rtc-row input:focus{outline:none;border-color:var(--teal)}
.input-label{font-size:11px;font-weight:700;letter-spacing:0.08em;text-transform:uppercase;
  color:var(--text-dim);margin-bottom:4px}

.sun-info{display:flex;gap:16px;flex-wrap:wrap;margin-top:12px}
.sun-badge{
  background:var(--amber-bg);border:2px solid var(--amber);border-radius:6px;
  padding:8px 14px;font-family:var(--mono);font-size:14px;font-weight:700;color:var(--amber)
}

.img-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(120px,1fr));gap:8px;margin-top:8px}
.img-thumb{background:var(--border);border-radius:6px;overflow:hidden;
  border:2px solid var(--border);transition:border-color 0.15s}
.img-thumb:hover{border-color:var(--teal)}
.img-thumb img{width:100%;height:90px;object-fit:cover;display:block}
.img-thumb .img-time{font-family:var(--mono);font-size:9px;font-weight:700;
  color:var(--text-dim);padding:4px 6px;text-align:center;background:var(--bg)}

footer{text-align:center;font-size:12px;color:var(--text-muted);font-family:var(--mono);
  font-weight:600;padding-top:24px;margin-top:24px;border-top:2px solid var(--border)}

.toast{
  position:fixed;bottom:20px;right:20px;
  background:var(--text);border-radius:var(--radius);padding:12px 18px;
  font-family:var(--mono);font-size:13px;font-weight:700;color:#fff;
  opacity:0;transform:translateY(8px);transition:all 0.25s;z-index:9999;pointer-events:none
}
.toast.show{opacity:1;transform:translateY(0)}
.toast.error{background:var(--red)}
.toast.ok{background:var(--green)}

/* ── Camera live view ──────────────────────────────────────────────────────── */
.cam-card{background:var(--card);border:2px solid var(--border);border-radius:var(--radius);
  margin-bottom:4px;overflow:hidden;box-shadow:var(--shadow)}
.cam-header{display:flex;align-items:center;justify-content:space-between;
  padding:14px 20px;border-bottom:2px solid var(--border);background:var(--bg);flex-wrap:wrap;gap:8px}
.cam-title{font-family:var(--mono);font-size:13px;font-weight:700;color:var(--text)}
.cam-controls{display:flex;gap:8px;align-items:center}
.cam-badge{font-family:var(--mono);font-size:11px;font-weight:700;padding:4px 10px;
  border-radius:4px;border:1.5px solid var(--border);color:var(--text-dim)}
.cam-badge.live{border-color:var(--green);color:var(--green);background:var(--green-bg);
  animation:pulse 1.4s ease-in-out infinite}
.cam-badge.off{border-color:var(--red);color:var(--red);background:var(--red-bg)}
.cam-badge.snap{border-color:var(--teal);color:var(--teal);background:var(--teal-light)}
.cam-body{background:#0A0F0A;display:flex;align-items:center;justify-content:center;
  min-height:200px;position:relative;overflow:hidden}
.cam-img{display:block;max-width:100%;max-height:400px;object-fit:contain;
  image-rendering:pixelated}
.cam-overlay{position:absolute;top:0;left:0;right:0;bottom:0;
  display:flex;align-items:center;justify-content:center;flex-direction:column;gap:10px}
.cam-placeholder{color:#3A5A3A;font-family:var(--mono);font-size:13px;text-align:center}
.cam-fps{position:absolute;bottom:8px;right:10px;font-family:var(--mono);font-size:10px;
  font-weight:700;color:rgba(255,255,255,0.5)}
.cam-info{padding:10px 20px;display:flex;gap:16px;flex-wrap:wrap;
  border-top:1px solid var(--border);background:var(--bg)}
.cam-info-item{font-family:var(--mono);font-size:11px;color:var(--text-muted);font-weight:700}
.cam-info-item span{color:var(--teal)}

@media(max-width:560px){
  .stats-grid{grid-template-columns:repeat(2,1fr)}
  .chart-wrap{height:170px}
  .file-row{flex-wrap:wrap}
  .stat-value{font-size:22px}
  .cam-body{min-height:160px}
  .logo-sub{display:none}
  .nav-btn{padding:6px 9px;font-size:11px}
  .status-pill{padding:5px 8px;font-size:11px}
  .dn-label{font-size:14px}
}
</style>
</head>
<body>
<div class="shell">
<header>
  <div class="logo">
    <div class="logo-icon">&#9728;</div>
    <div>
      <div class="logo-text">HELIOS</div>
      <div class="logo-sub">Data Logger v2.0</div>
    </div>
  </div>
  <div class="header-right">
    <div class="refresh-ring" title="Click to refresh" onclick="refreshAll()">
      <svg width="36" height="36" viewBox="0 0 36 36">
        <circle class="ring-bg" cx="18" cy="18" r="14"/>
        <circle class="ring-fill" cx="18" cy="18" r="14" id="ringFill"/>
      </svg>
      <span id="ringCount">5</span>
    </div>
    <a class="nav-btn" href="/serial">&#9654; Serial</a>
    <a class="nav-btn" href="/settings">&#9881; Settings</a>
    <button class="nav-btn nav-btn-ota" onclick="triggerOta()" title="Flash new firmware via OTA">&#9889; OTA</button>
    <div class="status-pill idle" id="statusPill">
      <div class="dot" id="statusDot"></div>
      <span id="statusText">IDLE</span>
    </div>
  </div>
</header>

<div class="section-label">Live Readings</div>
<!-- Day/Night status banner inserted above Live Readings -->
<div class="daynight-banner night" id="daynightBanner">
  <div class="dn-left">
    <div class="dn-icon" id="dnIcon">&#9790;</div>
    <div>
      <div class="dn-label" id="dnLabel">NIGHT — Not Logging</div>
      <div class="dn-sub" id="dnSub">Logging resumes at sunrise</div>
    </div>
  </div>
  <div class="dn-right">
    <div class="dn-time" id="dnSunrise">&#9728; --:--</div>
    <div class="dn-time" id="dnSunset">&#9790; --:--</div>
    <button class="btn-override start" id="overrideBtn" onclick="toggleOverride()">&#9654; Force Start</button>
  </div>
</div>

<div class="section-label" style="margin-top:0">Live Readings</div>
<div class="stats-grid">
  <div class="stat-card teal">
    <div class="stat-label">Irradiance</div>
    <div class="stat-value teal" id="statIrr">&#8212;</div>
    <div class="stat-sub">W / m&#178;</div>
  </div>
  <div class="stat-card amber">
    <div class="stat-label">Illuminance</div>
    <div class="stat-value amber" id="statLux">&#8212;</div>
    <div class="stat-sub">lux &middot; GY-302</div>
  </div>
  <div class="stat-card blue">
    <div class="stat-label">Sky Blue</div>
    <div class="stat-value blue" id="statBlue">&#8212;</div>
    <div class="stat-sub">0&ndash;255</div>
  </div>
  <div class="stat-card orange">
    <div class="stat-label">Die Temp</div>
    <div class="stat-value orange" id="statTemp">&#8212;</div>
    <div class="stat-sub">&deg;C &middot; ESP32-S3</div>
    <div class="temp-bar-wrap"><div class="temp-bar-track"><div class="temp-bar-fill" id="tempFill" style="width:0%"></div></div></div>
  </div>
  <div class="stat-card green">
    <div class="stat-label">Samples Today</div>
    <div class="stat-value green" id="statSamples">&#8212;</div>
    <div class="stat-sub">logged</div>
  </div>
  <div class="stat-card">
    <div class="stat-label">Elapsed</div>
    <div class="stat-value" id="statElapsed" style="font-size:18px">&#8212;</div>
    <div class="stat-sub">hh:mm:ss today</div>
  </div>
  <div class="stat-card red">
    <div class="stat-label">Rejected</div>
    <div class="stat-value red" id="statRejected">&#8212;</div>
    <div class="stat-sub">outliers</div>
  </div>
  <div class="stat-card green">
    <div class="stat-label">RTC Date</div>
    <div class="stat-value green" id="statDate" style="font-size:16px">&#8212;</div>
    <div class="stat-sub">YYYY-MM-DD</div>
  </div>
  <div class="stat-card green">
    <div class="stat-label">RTC Time</div>
    <div class="stat-value green" id="statTime" style="font-size:20px">&#8212;</div>
    <div class="stat-sub">HH:MM:SS</div>
  </div>
  <div class="stat-card">
    <div class="stat-label">WiFi Clients</div>
    <div class="stat-value" id="statClients">&#8212;</div>
    <div class="stat-sub">connected</div>
  </div>
</div>

<div class="section-label">Today&rsquo;s Schedule</div>
<div class="sun-info">
  <div class="sun-badge">&#9728; Sunrise <span id="sunriseTime">&#8212;</span></div>
  <div class="sun-badge">&#9790; Sunset <span id="sunsetTime">&#8212;</span></div>
  <div class="sun-badge">&#128247; Next Image <span id="nextImg">&#8212;</span></div>
  <div class="sun-badge">&#128444; Images Today <span id="imgCount">&#8212;</span></div>
</div>

<div class="section-label">Flash Storage</div>
<div class="flash-bar-wrap">
  <div class="flash-bar-header">
    <span id="flashLabel">&#8212; MB used of &#8212; MB</span>
    <span id="flashPct">&#8212;%</span>
  </div>
  <div class="flash-bar-track"><div class="flash-bar-fill" id="flashFill" style="width:0%"></div></div>
</div>

<div class="section-label">Irradiance &amp; Temperature</div>
<div class="chart-card">
  <div class="chart-header">
    <div class="chart-title" id="chartTitle">Today &mdash; live</div>
    <div class="chart-legend">
      <div class="legend-item"><div class="legend-dot" style="background:var(--teal)"></div>Irradiance (W/m&sup2;)</div>
      <div class="legend-item"><div class="legend-dot" style="background:var(--orange)"></div>Die Temp (&deg;C)</div>
      <div class="legend-item"><div class="legend-dot" style="background:var(--blue)"></div>Blue ch.</div>
    </div>
  </div>
  <div class="chart-wrap"><canvas id="chart"></canvas></div>
  <div class="chart-range">
    <button class="range-btn active" onclick="setRange(60)"  id="r60">1 h</button>
    <button class="range-btn"        onclick="setRange(180)" id="r180">3 h</button>
    <button class="range-btn"        onclick="setRange(360)" id="r360">6 h</button>
    <button class="range-btn"        onclick="setRange(0)"   id="rAll">All</button>
  </div>
</div>

<div class="section-label">Day History</div>
<div class="day-selector" id="daySelector">
  <span style="font-family:var(--mono);font-size:13px;color:var(--text-muted)">No days logged yet.</span>
</div>

<div class="section-label">Daily CVI Summary</div>
<div class="summary-card">
  <div class="summary-header">
    <div class="summary-title">Coefficient of Variation of Irradiance</div>
    <a class="btn btn-teal btn-sm" href="/download?file=summary.csv" download>&#8595; summary.csv</a>
  </div>
  <div id="summaryBody"><div class="files-empty">No completed days yet.</div></div>
</div>

<div class="section-label">Set RTC Clock</div>
<div class="rtc-set-card">
  <div style="display:flex;align-items:center;justify-content:space-between;flex-wrap:wrap;gap:8px">
    <div style="font-family:var(--mono);font-size:14px;font-weight:700;color:var(--text)">DS3231 Real-Time Clock</div>
    <div id="rtcSetStatus" style="font-family:var(--mono);font-size:12px;font-weight:700;color:var(--text-muted)">Not set this session</div>
  </div>
  <div class="rtc-row">
    <div><div class="input-label">Date</div>
      <input type="date" id="rtcDate">
    </div>
    <div><div class="input-label">Time</div>
      <input type="time" id="rtcTime" step="1">
    </div>
    <button class="btn btn-teal" onclick="setRTC()">&#9654; Set Clock</button>
    <button class="btn btn-ghost" onclick="setRTCNow()">&#8635; Use Browser Time</button>
  </div>
  <div style="margin-top:10px;font-size:12px;color:var(--text-muted);font-weight:600">
    After setting, verify the RTC Date/Time cards above update within 5 seconds.
  </div>
</div>

<div class="section-label">Live Camera</div>
<div class="cam-card">
  <div class="cam-header">
    <div class="cam-title">&#128247; OV2640 Real-Time View</div>
    <div class="cam-controls">
      <span class="cam-badge off" id="camBadge">OFFLINE</span>
      <button class="btn btn-teal btn-sm" id="camStartBtn" onclick="camStart()">&#9654; Stream</button>
      <button class="btn btn-ghost btn-sm" onclick="camSnap()">&#128247; Snapshot</button>
      <button class="btn btn-ghost btn-sm" id="camStopBtn" onclick="camStop()" style="display:none">&#9632; Stop</button>
    </div>
  </div>
  <div class="cam-body" id="camBody">
    <div class="cam-overlay" id="camOverlay">
      <div style="font-size:40px">&#128247;</div>
      <div class="cam-placeholder">Camera offline.<br>Press Stream to start live MJPEG feed.</div>
    </div>
    <img class="cam-img" id="camImg" style="display:none" alt="Camera">
    <div class="cam-fps" id="camFps"></div>
  </div>
  <div class="cam-info">
    <div class="cam-info-item">Sensor: <span>OV2640</span></div>
    <div class="cam-info-item">Stream: <span id="camRes">QVGA JPEG</span></div>
    <div class="cam-info-item">Mode: <span id="camMode">idle</span></div>
    <div class="cam-info-item">Frames: <span id="camFrameCount">0</span></div>
  </div>
</div>

<div class="section-label">Sky Images</div>
<div class="files-card">
  <div class="files-header">
    <div class="files-title">Latest Images (96&times;96 JPEG)</div>
    <span id="imgTotalSize" style="font-family:var(--mono);font-size:12px;color:var(--text-muted);font-weight:700"></span>
  </div>
  <div id="imgGrid" style="padding:16px">
    <div class="files-empty">No images yet.</div>
  </div>
</div>

<div class="section-label">CSV Data Files</div>
<div class="files-card">
  <div class="files-header">
    <div class="files-title">CSV Archive</div>
    <button class="btn btn-danger btn-sm" onclick="deleteAll()">&#9888; Wipe All Data</button>
  </div>
  <div id="fileList"><div class="files-empty">No files yet.</div></div>
</div>

<footer>Helios-Artemis &middot; Leading University Sylhet &middot; 2026</footer>
</div>
<div class="toast" id="toast"></div>

<script>
const STATUS_INTERVAL=5, LIVE_INTERVAL=10, FILES_INTERVAL=30;
const RING_C=88;
let ringCountdown=STATUS_INTERVAL;
const ringFill=document.getElementById('ringFill');
const ringCount=document.getElementById('ringCount');

function tickRing(){
  ringCountdown--;
  if(ringCountdown<=0){ringCountdown=STATUS_INTERVAL;fetchStatus();}
  ringFill.style.strokeDashoffset=RING_C*(ringCountdown/STATUS_INTERVAL);
  ringCount.textContent=ringCountdown;
}

function refreshAll(){
  ringCountdown=STATUS_INTERVAL;
  fetchStatus();fetchLive();fetchFiles();fetchSummary();fetchImages();
}

// Chart
const canvas=document.getElementById('chart');
const ctx=canvas.getContext('2d');
let chartData=[],rangeCount=60;

function fmtE(s){
  const h=Math.floor(s/3600),m=Math.floor((s%3600)/60),sec=s%60;
  return String(h).padStart(2,'0')+':'+String(m).padStart(2,'0')+':'+String(sec).padStart(2,'0');
}

function setRange(n){
  rangeCount=n;
  ['r60','r180','r360','rAll'].forEach(id=>document.getElementById(id).classList.remove('active'));
  const m={60:'r60',180:'r180',360:'r360',0:'rAll'};
  if(m[n]) document.getElementById(m[n]).classList.add('active');
  drawChart();
}

function drawChart(){
  const w=canvas.parentElement.clientWidth, h=canvas.parentElement.clientHeight;
  canvas.width=w; canvas.height=h;
  const data=rangeCount===0?chartData:chartData.slice(-rangeCount);
  const PAD={top:12,right:24,bottom:32,left:62};
  const cw=w-PAD.left-PAD.right, ch=h-PAD.top-PAD.bottom;
  ctx.clearRect(0,0,w,h);
  if(data.length===0){
    ctx.fillStyle='#7A96A8';ctx.font='bold 14px system-ui';ctx.textAlign='center';
    ctx.fillText('No data yet',w/2,h/2);return;
  }
  const maxIrr=Math.max(...data.map(d=>d.irr),0.01);
  const temps=data.filter(d=>d.temp>0).map(d=>d.temp);
  const maxT=temps.length?Math.max(...temps,50):50;
  const minT=temps.length?Math.min(...temps,20):20;

  // Grid
  ctx.strokeStyle='#C8D6E0';ctx.lineWidth=1;
  for(let i=0;i<=4;i++){
    const y=PAD.top+(ch/4)*i;
    ctx.beginPath();ctx.moveTo(PAD.left,y);ctx.lineTo(PAD.left+cw,y);ctx.stroke();
    ctx.fillStyle='#4A6070';ctx.font='bold 10px Courier New,monospace';ctx.textAlign='right';
    ctx.fillText((maxIrr*(1-i/4)).toFixed(1),PAD.left-6,y+4);
  }
  // X labels
  ctx.fillStyle='#4A6070';ctx.font='bold 10px Courier New,monospace';ctx.textAlign='center';
  const xStep=Math.max(1,Math.floor(data.length/6));
  for(let i=0;i<data.length;i+=xStep){
    const x=PAD.left+(i/(data.length-1||1))*cw;
    const s=data[i].t,hh=Math.floor(s/3600),mm=Math.floor((s%3600)/60);
    ctx.fillText(hh+'h'+(mm?mm+'m':''),x,h-8);
  }
  const px=i=>PAD.left+(i/(data.length-1||1))*cw;
  const pyI=v=>PAD.top+ch-(v/maxIrr)*ch;
  const pyB=v=>PAD.top+ch-(v/255)*ch;
  const pyT=v=>PAD.top+ch-((v-minT)/(maxT-minT||1))*ch;

  // Irradiance fill
  ctx.beginPath();ctx.moveTo(px(0),pyI(data[0].irr));
  data.forEach((d,i)=>ctx.lineTo(px(i),pyI(d.irr)));
  ctx.lineTo(px(data.length-1),PAD.top+ch);ctx.lineTo(px(0),PAD.top+ch);ctx.closePath();
  const g=ctx.createLinearGradient(0,PAD.top,0,PAD.top+ch);
  g.addColorStop(0,'rgba(0,122,122,0.25)');g.addColorStop(1,'rgba(0,122,122,0.02)');
  ctx.fillStyle=g;ctx.fill();
  // Irradiance line
  ctx.beginPath();ctx.strokeStyle='#007A7A';ctx.lineWidth=2.5;ctx.lineJoin='round';
  data.forEach((d,i)=>i===0?ctx.moveTo(px(i),pyI(d.irr)):ctx.lineTo(px(i),pyI(d.irr)));
  ctx.stroke();
  // Blue
  ctx.beginPath();ctx.strokeStyle='#1A5FA8';ctx.lineWidth=1.5;ctx.setLineDash([4,3]);
  data.forEach((d,i)=>i===0?ctx.moveTo(px(i),pyB(d.blue)):ctx.lineTo(px(i),pyB(d.blue)));
  ctx.stroke();ctx.setLineDash([]);
  // Temperature
  if(temps.length>0){
    ctx.beginPath();ctx.strokeStyle='#A04000';ctx.lineWidth=1.5;ctx.setLineDash([2,4]);
    data.forEach((d,i)=>i===0?ctx.moveTo(px(i),pyT(d.temp)):ctx.lineTo(px(i),pyT(d.temp)));
    ctx.stroke();ctx.setLineDash([]);
    ctx.fillStyle='#A04000';ctx.font='bold 9px Courier New';ctx.textAlign='left';
    ctx.fillText(maxT.toFixed(0)+'°',PAD.left+cw+3,PAD.top+5);
    ctx.fillText(minT.toFixed(0)+'°',PAD.left+cw+3,PAD.top+ch);
  }
  // Live dot
  const last=data[data.length-1];
  const lx=px(data.length-1),ly=pyI(last.irr);
  ctx.beginPath();ctx.arc(lx,ly,5,0,Math.PI*2);
  ctx.fillStyle='#007A7A';ctx.fill();
  ctx.strokeStyle='#fff';ctx.lineWidth=2;ctx.stroke();
}

// Day selector
let currentDay='live';
function buildDaySelector(files){
  const sel=document.getElementById('daySelector');
  const days=files?files.filter(f=>f.name.match(/^\d{4}-\d{2}-\d{2}\.csv$/)):[];
  if(!days.length){sel.innerHTML='<span style="font-family:var(--mono);font-size:13px;color:var(--text-muted)">No days logged yet.</span>';return;}
  let h='<button class="day-btn'+(currentDay==='live'?' active':'')+'" onclick="selectDay(\'live\',this)">&#9679; Live</button>';
  days.forEach(f=>{
    const n=f.name.replace('.csv','');
    h+='<button class="day-btn'+(currentDay===n?' active':'')+'" onclick="selectDay(\''+n+'\',this)">'+n+'</button>';
  });
  sel.innerHTML=h;
}

function selectDay(d,btn){
  currentDay=d;
  document.querySelectorAll('.day-btn').forEach(b=>b.classList.remove('active'));
  if(btn) btn.classList.add('active');
  document.getElementById('chartTitle').textContent=d==='live'?'Today — live':'History: '+d;
  if(d==='live') fetchLive(); else fetchDayHistory(d);
}

async function fetchDayHistory(dateStr){
  try{
    const t=await(await fetch('/download?file='+encodeURIComponent(dateStr+'.csv'))).text();
    chartData=t.trim().split('\n').slice(1).map(l=>{
      const p=l.split(',');
      return{t:parseFloat(p[2]),lux:parseFloat(p[3]),irr:parseFloat(p[4]),blue:parseInt(p[5]),temp:parseFloat(p[6]||0)};
    }).filter(d=>!isNaN(d.t));
    drawChart();
  }catch(e){}
}

async function fetchSummary(){
  try{
    const t=await(await fetch('/download?file=summary.csv')).text();
    const lines=t.trim().split('\n');
    if(lines.length<2){document.getElementById('summaryBody').innerHTML='<div class="files-empty">No completed days yet.</div>';return;}
    let h='<table class="summary-table"><thead><tr><th>Date</th><th>Samples</th><th>Duration</th><th>Mean W/m&sup2;</th><th>Peak W/m&sup2;</th><th>CVI</th><th>Mean &deg;C</th></tr></thead><tbody>';
    lines.slice(1).forEach(l=>{
      const p=l.split(',');if(p.length<8)return;
      const cvi=parseFloat(p[5]);
      const cc=cvi<0.5?'cvi-good':cvi<0.85?'cvi-med':'cvi-high';
      const dur=parseInt(p[2]);const hh=Math.floor(dur/3600),mm=Math.floor((dur%3600)/60);
      h+=`<tr><td>${p[0]}</td><td>${p[1]}</td><td>${hh}h ${mm}m</td><td>${parseFloat(p[3]).toFixed(3)}</td><td>${parseFloat(p[6]).toFixed(3)}</td><td class="${cc}">${cvi.toFixed(3)}</td><td>${parseFloat(p[7]).toFixed(1)}</td></tr>`;
    });
    h+='</tbody></table>';
    document.getElementById('summaryBody').innerHTML=h;
  }catch(e){document.getElementById('summaryBody').innerHTML='<div class="files-empty">No completed days yet.</div>';}
}

async function fetchStatus(){
  try{
    const d=await(await fetch('/api/status')).json();
    const pill=document.getElementById('statusPill');
    const dot=document.getElementById('statusDot'),txt=document.getElementById('statusText');
    if(d.logging){pill.className='status-pill active';dot.classList.add('pulse');txt.textContent='LOGGING';}
    else{pill.className='status-pill idle';dot.classList.remove('pulse');txt.textContent='IDLE';}

    // ── Day/Night banner ──────────────────────────────────────────────────────
    const banner=document.getElementById('daynightBanner');
    const dnIcon=document.getElementById('dnIcon');
    const dnLabel=document.getElementById('dnLabel');
    const dnSub=document.getElementById('dnSub');
    const overrideBtn=document.getElementById('overrideBtn');
    const sr=d.sunrise||'--:--', ss=d.sunset||'--:--';
    document.getElementById('dnSunrise').textContent='\u2600 '+sr;
    document.getElementById('dnSunset').textContent='\u263D '+ss;
    const ovr=d.manual_override===true;
    if(ovr){
      banner.className='daynight-banner override';
      dnIcon.textContent='\u26A1';
      dnLabel.textContent='OVERRIDE — Forced Logging';
      dnSub.textContent='Manual override active — logging outside solar hours';
      overrideBtn.className='btn-override stop';
      overrideBtn.textContent='\u25A0 Stop Override';
    } else if(d.logging){
      banner.className='daynight-banner day';
      dnIcon.textContent='\u2600';
      dnLabel.textContent='DAYTIME — Logging Active';
      dnSub.textContent='Sunrise '+sr+' \u2192 Sunset '+ss;
      overrideBtn.className='btn-override start';
      overrideBtn.textContent='\u25BA Force Start';
      overrideBtn.style.display='none';
    } else {
      overrideBtn.style.display='';
      banner.className='daynight-banner night';
      dnIcon.textContent='\u263D';
      dnLabel.textContent='NIGHT \u2014 Not Logging';
      dnSub.textContent='Logging resumes at sunrise '+sr;
      overrideBtn.className='btn-override start';
      overrideBtn.textContent='\u25BA Force Start';
    }
    if(d.logging) overrideBtn.style.display=ovr?'':'none';
    else overrideBtn.style.display='';

    // Always show counters — 0 when no samples, never leave as —
    document.getElementById('statSamples').textContent=d.total_samples.toLocaleString();
    document.getElementById('statClients').textContent=d.clients!==undefined?d.clients:'0';
    document.getElementById('statRejected').textContent=d.rejected!==undefined?d.rejected:'0';
    // Die temp — always available from ESP32-S3 internal sensor, independent of BH1750
    if(d.die_temp_c!==undefined && d.die_temp_c>-99){
      const t=d.die_temp_c;
      document.getElementById('statTemp').textContent=t.toFixed(1);
      const pct=Math.min(100,Math.max(0,(t-20)/70*100));
      const fill=document.getElementById('tempFill');
      fill.style.width=pct+'%';
      fill.style.background=t<50?'#1A7A45':t<70?'#B86800':'#B82222';
    }
    if(d.rtc_date){document.getElementById('statDate').textContent=d.rtc_date;}
    if(d.rtc_time){document.getElementById('statTime').textContent=d.rtc_time;}
    if(d.sunrise){document.getElementById('sunriseTime').textContent=d.sunrise;}
    if(d.sunset){document.getElementById('sunsetTime').textContent=d.sunset;}
    if(d.next_img){document.getElementById('nextImg').textContent=d.next_img;}
    if(d.img_count!==undefined){document.getElementById('imgCount').textContent=d.img_count;}
    // Live sample fields — only update if a sample exists
    if(d.latest){
      document.getElementById('statIrr').textContent=d.latest.irradiance_wm2.toFixed(3);
      document.getElementById('statLux').textContent=d.latest.lux.toFixed(1);
      document.getElementById('statBlue').textContent=d.latest.blue_channel;
      document.getElementById('statElapsed').textContent=fmtE(d.latest.elapsed_s);
    } else {
      document.getElementById('statElapsed').textContent='0s';
    }
    const pct2=d.fs_total_kb>0?(d.fs_used_kb/d.fs_total_kb*100):0;
    const usedMB=(d.fs_used_kb/1024).toFixed(1),totMB=(d.fs_total_kb/1024).toFixed(1);
    document.getElementById('flashLabel').textContent=usedMB+' MB used of '+totMB+' MB';
    document.getElementById('flashPct').textContent=pct2.toFixed(1)+'%';
    const fbar=document.getElementById('flashFill');
    fbar.style.width=pct2+'%';
    fbar.className='flash-bar-fill'+(pct2>85?' crit':pct2>65?' warn':'');
  }catch(e){console.warn('status fetch:',e);}
}

let _overrideActive=false;
async function toggleOverride(){
  _overrideActive=!_overrideActive;
  try{
    const r=await fetch('/api/override?on='+(_overrideActive?'1':'0'));
    const j=await r.json();
    if(!j.ok){_overrideActive=!_overrideActive;toast('Override failed','error');return;}
    toast(_overrideActive?'Manual override ON — logging started':'Override OFF','ok');
    fetchStatus();
  }catch(e){_overrideActive=!_overrideActive;toast('Network error','error');}
}

async function fetchLive(){
  if(currentDay!=='live')return;
  try{
    const count=rangeCount===0?360:rangeCount;
    chartData=await(await fetch('/api/live?count='+count)).json();
    drawChart();
  }catch(e){}
}

async function fetchFiles(){
  try{
    const files=await(await fetch('/api/files')).json();
    buildDaySelector(files);
    const list=document.getElementById('fileList');
    const dataFiles=files?files.filter(f=>!f.name.includes('summary')&&!f.name.includes('.meta')):[];
    if(!dataFiles.length){list.innerHTML='<div class="files-empty">No CSV files yet.</div>';return;}
    list.innerHTML=dataFiles.map(f=>
      '<div class="file-row">'+
        '<div class="file-name">'+f.name+'</div>'+
        '<div class="file-meta">'+fmtBytes(f.size)+'&nbsp;&nbsp;'+f.samples+' rows</div>'+
        '<div class="file-actions">'+
          '<a class="btn btn-teal btn-sm" href="/download?file='+encodeURIComponent(f.name)+'" download>&#8595; CSV</a>'+
          '<button class="btn btn-danger btn-sm" onclick="deleteFile(\''+f.name+'\')">&#10005;</button>'+
        '</div>'+
      '</div>'
    ).join('');
  }catch(e){}
}

async function fetchImages(){
  try{
    const imgs=await(await fetch('/api/images')).json();
    const grid=document.getElementById('imgGrid');
    if(!imgs||!imgs.length){grid.innerHTML='<div class="files-empty">No images yet.</div>';return;}
    let totalSz=0;imgs.forEach(i=>totalSz+=i.size);
    document.getElementById('imgTotalSize').textContent=fmtBytes(totalSz)+' total, '+imgs.length+' images';
    grid.innerHTML='<div class="img-grid">'+imgs.slice(-48).reverse().map(i=>
      '<div class="img-thumb">'+
        '<a href="/img?file='+encodeURIComponent(i.name)+'" target="_blank">'+
          '<img src="/img?file='+encodeURIComponent(i.name)+'" loading="lazy" alt="'+i.name+'">'+
        '</a>'+
        '<div class="img-time">'+i.name.replace('.jpg','').replace(/_/,' ').replace(/(\d{4})(\d{2})(\d{2}) /,'$2-$3 ')+'</div>'+
      '</div>'
    ).join('')+'</div>';
  }catch(e){}
}

async function deleteFile(name){
  if(!confirm('Delete '+name+'?'))return;
  try{
    const d=await(await fetch('/api/delete?file='+encodeURIComponent(name),{method:'DELETE'})).json();
    if(d.ok){toast('Deleted','ok');fetchFiles();}else toast('Delete failed','error');
  }catch(e){toast('Error','error');}
}

async function deleteAll(){
  if(!confirm('Wipe ALL data and images? Cannot be undone.'))return;
  try{
    const d=await(await fetch('/api/deleteall',{method:'DELETE'})).json();
    if(d.ok){chartData=[];drawChart();currentDay='live';toast('All data wiped','ok');fetchFiles();fetchStatus();fetchSummary();fetchImages();}
    else toast('Wipe failed','error');
  }catch(e){toast('Error','error');}
}

function fmtBytes(b){
  if(b<1024)return b+' B';
  if(b<1048576)return(b/1024).toFixed(1)+' KB';
  return(b/1048576).toFixed(2)+' MB';
}

function setRTCNow(){
  const now=new Date();
  document.getElementById('rtcDate').value=now.toISOString().slice(0,10);
  document.getElementById('rtcTime').value=now.toTimeString().slice(0,8);
}

async function setRTC(){
  const d=document.getElementById('rtcDate').value;
  const t=document.getElementById('rtcTime').value;
  if(!d||!t){toast('Pick date and time first','error');return;}
  const st=document.getElementById('rtcSetStatus');
  st.style.color='#B86800';st.textContent='Setting...';
  try{
    const r=await fetch('/api/settime?date='+encodeURIComponent(d)+'&time='+encodeURIComponent(t));
    const j=await r.json();
    if(j.ok){st.style.color='#1A7A45';st.textContent='Set to '+d+' '+t;toast('RTC set','ok');}
    else{st.style.color='#B82222';st.textContent='Failed: '+(j.error||'unknown');toast('Set failed','error');}
  }catch(e){st.style.color='#B82222';st.textContent='Network error';toast('Error','error');}
}

let toastTimer=null;
function toast(msg,type='ok'){
  const el=document.getElementById('toast');
  el.textContent=msg;el.className='toast show '+(type==='error'?'error':'ok');
  clearTimeout(toastTimer);
  toastTimer=setTimeout(()=>{el.className='toast';},2500);
}

window.addEventListener('resize',drawChart);
drawChart();
setRTCNow();
fetchStatus();fetchLive();fetchFiles();fetchSummary();fetchImages();
setInterval(tickRing,1000);
setInterval(fetchLive,LIVE_INTERVAL*1000);
setInterval(fetchFiles,FILES_INTERVAL*1000);
setInterval(fetchSummary,FILES_INTERVAL*1000);
setInterval(fetchImages,60000);

// ── Camera live view ────────────────────────────────────────────────────────
let camStreaming = false;
let camFrameCount = 0;
let camFpsTs = 0, camFpsFrames = 0;

function camSetBadge(state) {
  const b = document.getElementById('camBadge');
  const startBtn = document.getElementById('camStartBtn');
  const stopBtn  = document.getElementById('camStopBtn');
  b.className = 'cam-badge ' + state;
  if (state === 'live') {
    b.textContent = 'LIVE';
    startBtn.style.display = 'none';
    stopBtn.style.display = '';
    document.getElementById('camMode').textContent = 'MJPEG stream';
  } else if (state === 'snap') {
    b.textContent = 'SNAP';
    startBtn.style.display = '';
    stopBtn.style.display = 'none';
    document.getElementById('camMode').textContent = 'snapshot';
  } else {
    b.textContent = 'OFFLINE';
    startBtn.style.display = '';
    stopBtn.style.display = 'none';
    document.getElementById('camMode').textContent = 'idle';
  }
}

function camShowImage(src) {
  const img = document.getElementById('camImg');
  const ov  = document.getElementById('camOverlay');
  img.src = src;
  img.style.display = 'block';
  ov.style.display  = 'none';
}

function camHideImage() {
  const img = document.getElementById('camImg');
  const ov  = document.getElementById('camOverlay');
  img.style.display = 'none';
  ov.style.display  = 'flex';
  img.src = '';
}

async function camSnap() {
  camStop();
  camSetBadge('snap');
  try {
    const url = '/api/cam-snapshot?' + Date.now();
    const resp = await fetch(url);
    if (!resp.ok) throw new Error(resp.status);
    const blob = await resp.blob();
    camShowImage(URL.createObjectURL(blob));
    camFrameCount++;
    document.getElementById('camFrameCount').textContent = camFrameCount;
    document.getElementById('camFps').textContent = '';
    document.getElementById('camRes').textContent = 'QVGA JPEG';
    toast('Snapshot captured', 'ok');
  } catch(e) {
    camHideImage();
    camSetBadge('off');
    toast('Camera error: ' + e.message, 'error');
  }
}

function camStart() {
  if (camStreaming) return;
  camStreaming = true;
  camFrameCount = 0;
  camFpsTs = Date.now();
  camFpsFrames = 0;
  camSetBadge('live');
  document.getElementById('camRes').textContent = 'QVGA MJPEG';
  const img = document.getElementById('camImg');
  const ov  = document.getElementById('camOverlay');
  img.style.display = 'block';
  ov.style.display  = 'none';

  // MJPEG: set src to stream endpoint — browser decodes multipart/x-mixed-replace
  img.src = '/api/cam-stream?' + Date.now();

  img.onload = () => {};
  img.onerror = () => {
    if (!camStreaming) return;
    camStreaming = false;
    camSetBadge('off');
    camHideImage();
    toast('Stream ended', 'error');
  };

  // FPS counter via polling — MJPEG doesn't fire onload per-frame
  // We poll /api/cam-stats every 2s for frame count from firmware side
  camFpsInterval = setInterval(async () => {
    if (!camStreaming) { clearInterval(camFpsInterval); return; }
    try {
      const d = await (await fetch('/api/cam-stats')).json();
      const fps = d.fps !== undefined ? d.fps.toFixed(1) : '?';
      document.getElementById('camFps').textContent = fps + ' fps';
      document.getElementById('camFrameCount').textContent = d.frames || '?';
      camFrameCount = d.frames || camFrameCount;
    } catch(e) {}
  }, 2000);
}

function camStop() {
  camStreaming = false;
  clearInterval(camFpsInterval);
  const img = document.getElementById('camImg');
  img.src = '';
  img.style.display = 'none';
  document.getElementById('camOverlay').style.display = 'flex';
  document.getElementById('camFps').textContent = '';
  camSetBadge('off');
}

async function triggerOta(){
  window.location.href='/ota';
}
</script>
</body></html>
)DASH";

// ═══════════════════════════════════════════════════════════════════════════
//  SECTION 3b — SETUP WIZARD HTML  (served on first boot until setup_done)
// ═══════════════════════════════════════════════════════════════════════════
const char SETUP_HTML[] PROGMEM = R"SETUP(
<!DOCTYPE html><html lang="en"><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Helios Setup</title>
<style>
:root{
  --bg:#F0F4F8;--surface:#FFFFFF;--border:#C8D6E0;
  --teal:#007A7A;--teal-light:#E0F4F4;
  --green:#1A7A45;--green-bg:#E4F5EC;
  --red:#B82222;--red-bg:#FDEAEA;
  --amber:#B86800;--amber-bg:#FFF3DC;
  --text:#1A2530;--text-dim:#4A6070;--text-muted:#7A96A8;
  --mono:'Courier New',monospace;--sans:system-ui,sans-serif;
  --radius:8px;--shadow:0 2px 8px rgba(0,0,0,0.10);
}
*,*::before,*::after{box-sizing:border-box;margin:0;padding:0}
html{font-size:16px}
body{background:var(--bg);color:var(--text);font-family:var(--sans);min-height:100vh;display:flex;align-items:flex-start;justify-content:center;padding:24px 16px 48px}
.card{background:var(--surface);border:2px solid var(--border);border-radius:12px;box-shadow:var(--shadow);width:100%;max-width:520px;overflow:hidden}
.card-header{background:var(--teal);padding:24px 28px;color:#fff}
.card-header .logo{display:flex;align-items:center;gap:14px;margin-bottom:6px}
.card-header .logo-icon{width:48px;height:48px;background:rgba(255,255,255,0.2);border-radius:10px;display:flex;align-items:center;justify-content:center;font-size:26px}
.card-header h1{font-family:var(--mono);font-size:20px;font-weight:700;letter-spacing:0.04em}
.card-header p{font-size:13px;opacity:0.85;margin-top:4px}
.card-body{padding:24px 28px}
.step{display:none}.step.active{display:block}
.step-title{font-family:var(--mono);font-size:13px;font-weight:700;color:var(--teal);text-transform:uppercase;letter-spacing:0.08em;margin-bottom:16px;display:flex;align-items:center;gap:8px}
.step-title::after{content:'';flex:1;height:2px;background:var(--teal-light)}
.field{margin-bottom:18px}
.field label{display:block;font-size:12px;font-weight:700;letter-spacing:0.07em;text-transform:uppercase;color:var(--text-dim);margin-bottom:6px}
.field input{width:100%;background:var(--bg);border:2px solid var(--border);border-radius:6px;color:var(--text);font-family:var(--mono);font-size:15px;font-weight:700;padding:10px 12px}
.field input:focus{outline:none;border-color:var(--teal)}
.field-row{display:flex;gap:12px}
.field-row .field{flex:1;min-width:0}
.field-hint{font-size:11px;color:var(--text-muted);margin-top:5px;font-weight:500}
.gps-row{display:flex;align-items:flex-end;gap:10px}
.gps-row .field{flex:1;min-width:0;margin-bottom:0}
.gps-btn{background:var(--teal);color:#fff;border:none;border-radius:6px;font-family:var(--mono);font-size:12px;font-weight:700;padding:10px 14px;cursor:pointer;white-space:nowrap;flex-shrink:0;height:42px}
.gps-btn:hover{background:#005555}
.gps-status{font-family:var(--mono);font-size:11px;font-weight:700;margin-top:8px;min-height:16px}
.gps-status.ok{color:var(--green)}
.gps-status.err{color:var(--red)}
.gps-status.wait{color:var(--amber)}
.btn-row{display:flex;gap:10px;margin-top:24px;justify-content:flex-end}
.btn{display:inline-flex;align-items:center;gap:5px;border:none;border-radius:6px;font-family:var(--mono);font-size:14px;font-weight:700;cursor:pointer;padding:11px 22px;transition:all 0.15s}
.btn-teal{background:var(--teal);color:#fff}
.btn-teal:hover{background:#005555}
.btn-ghost{background:var(--surface);border:2px solid var(--border);color:var(--text-dim)}
.btn-ghost:hover{border-color:var(--teal);color:var(--teal);background:var(--teal-light)}
.btn:disabled{opacity:0.4;cursor:not-allowed}
.progress{display:flex;gap:6px;justify-content:center;padding:14px 0 2px}
.progress-dot{width:8px;height:8px;border-radius:50%;background:var(--border)}
.progress-dot.done{background:var(--teal)}
.progress-dot.active{background:var(--teal);box-shadow:0 0 0 3px var(--teal-light)}
.summary-box{background:var(--bg);border:2px solid var(--border);border-radius:6px;padding:16px;margin-bottom:8px}
.summary-row{display:flex;justify-content:space-between;align-items:center;padding:5px 0;border-bottom:1px solid var(--border);font-size:13px}
.summary-row:last-child{border-bottom:none}
.summary-key{color:var(--text-dim);font-weight:600}
.summary-val{font-family:var(--mono);font-weight:700;color:var(--text)}
.toast{position:fixed;bottom:20px;right:20px;background:var(--text);border-radius:var(--radius);padding:12px 18px;font-family:var(--mono);font-size:13px;font-weight:700;color:#fff;opacity:0;transform:translateY(8px);transition:all 0.25s;z-index:9999;pointer-events:none}
.toast.show{opacity:1;transform:translateY(0)}
.toast.error{background:var(--red)}
.toast.ok{background:var(--green)}
</style>
</head>
<body>
<div class="card">
  <div class="card-header">
    <div class="logo">
      <div class="logo-icon">&#9728;</div>
      <div>
        <h1>HELIOS SETUP</h1>
        <p>Data Logger v1.6 &mdash; First Boot Configuration</p>
      </div>
    </div>
  </div>
  <div class="card-body">
    <div class="progress">
      <div class="progress-dot active" id="pd0"></div>
      <div class="progress-dot" id="pd1"></div>
      <div class="progress-dot" id="pd2"></div>
    </div>

    <!-- Step 0: Date & Time -->
    <div class="step active" id="step0">
      <div class="step-title">&#9312; Date &amp; Time</div>
      <div class="field-row">
        <div class="field">
          <label>Date</label>
          <input type="date" id="setupDate">
        </div>
        <div class="field">
          <label>Time</label>
          <input type="time" id="setupTime" step="1">
        </div>
      </div>
      <div class="field-hint">This will set the RTC clock on the device. Use your local time.</div>
      <div class="btn-row">
        <button class="btn btn-ghost" onclick="fillNow()">&#8635; Use Phone Time</button>
        <button class="btn btn-teal" onclick="goStep(1)">Next &#8594;</button>
      </div>
    </div>

    <!-- Step 1: Location -->
    <div class="step" id="step1">
      <div class="step-title">&#9313; Location</div>
      <div class="gps-row">
        <div class="field">
          <label>Latitude</label>
          <input type="number" id="setupLat" step="0.000001" placeholder="e.g. 24.9045">
        </div>
        <div class="field">
          <label>Longitude</label>
          <input type="number" id="setupLon" step="0.000001" placeholder="e.g. 91.8611">
        </div>
        <button class="gps-btn" onclick="grabGPS()">&#127757; GPS</button>
      </div>
      <div class="gps-status" id="gpsStatus">Tap GPS to auto-fill from phone location.</div>
      <div class="field-hint" style="margin-top:12px">These coordinates are used to calculate sunrise and sunset times for your exact location.</div>
      <div class="btn-row">
        <button class="btn btn-ghost" onclick="goStep(0)">&#8592; Back</button>
        <button class="btn btn-teal" onclick="goStep(2)">Next &#8594;</button>
      </div>
    </div>

    <!-- Step 2: Deployment Duration + Confirm -->
    <div class="step" id="step2">
      <div class="step-title">&#9314; Deployment &amp; Confirm</div>
      <div class="field">
        <label>Deployment Duration</label>
        <input type="number" id="setupDays" min="1" max="365" step="1" value="15">
        <div class="field-hint">How many days will this device be deployed? The solar calculator will cover this range.</div>
      </div>
      <div id="summaryBox" class="summary-box" style="display:none"></div>
      <div class="btn-row">
        <button class="btn btn-ghost" onclick="goStep(1)">&#8592; Back</button>
        <button class="btn btn-teal" id="submitBtn" onclick="submitSetup()">&#10003; Finish Setup</button>
      </div>
    </div>
  </div>
</div>
<div class="toast" id="toast"></div>

<script>
// Auto-fill date/time on load
function fillNow(){
  const n=new Date();
  document.getElementById('setupDate').value=n.toISOString().slice(0,10);
  document.getElementById('setupTime').value=n.toTimeString().slice(0,8);
}
fillNow();

function goStep(n){
  // Validate before advancing
  if(n===1){
    if(!document.getElementById('setupDate').value||!document.getElementById('setupTime').value){
      toast('Please set date and time first','error');return;
    }
  }
  if(n===2){
    const lat=parseFloat(document.getElementById('setupLat').value);
    const lon=parseFloat(document.getElementById('setupLon').value);
    if(isNaN(lat)||isNaN(lon)||lat<-90||lat>90||lon<-180||lon>180){
      toast('Please enter valid coordinates (or tap GPS)','error');return;
    }
    buildSummary();
  }
  document.querySelectorAll('.step').forEach((s,i)=>{
    s.classList.toggle('active',i===n);
  });
  document.querySelectorAll('.progress-dot').forEach((d,i)=>{
    d.classList.toggle('done',i<n);
    d.classList.toggle('active',i===n);
  });
}

function grabGPS(){
  const st=document.getElementById('gpsStatus');
  st.className='gps-status wait';
  st.textContent='Requesting location from phone\u2026';
  if(!navigator.geolocation){
    st.className='gps-status err';
    st.textContent='Geolocation not available on this browser.';
    return;
  }
  navigator.geolocation.getCurrentPosition(
    pos=>{
      document.getElementById('setupLat').value=pos.coords.latitude.toFixed(6);
      document.getElementById('setupLon').value=pos.coords.longitude.toFixed(6);
      st.className='gps-status ok';
      st.textContent='\u2713 Got location: '+pos.coords.latitude.toFixed(4)+', '+pos.coords.longitude.toFixed(4)
        +' (\u00b1'+Math.round(pos.coords.accuracy)+'m)';
    },
    err=>{
      st.className='gps-status err';
      const msgs={1:'Permission denied \u2014 allow location in browser.',2:'Position unavailable.',3:'Timed out.'};
      st.textContent=msgs[err.code]||'Error: '+err.message;
    },
    {enableHighAccuracy:true,timeout:15000,maximumAge:0}
  );
}

function utcOffsetHours(){
  // getTimezoneOffset returns minutes BEHIND UTC (negative for east)
  return -(new Date().getTimezoneOffset()/60);
}

function buildSummary(){
  const lat=parseFloat(document.getElementById('setupLat').value).toFixed(4);
  const lon=parseFloat(document.getElementById('setupLon').value).toFixed(4);
  const utc=utcOffsetHours();
  const utcStr=(utc>=0?'+':'')+utc;
  const dt=document.getElementById('setupDate').value;
  const ti=document.getElementById('setupTime').value;
  const days=document.getElementById('setupDays').value;
  const box=document.getElementById('summaryBox');
  box.style.display='';
  box.innerHTML=
    '<div class="summary-row"><span class="summary-key">Date &amp; Time</span><span class="summary-val">'+dt+' '+ti+'</span></div>'+
    '<div class="summary-row"><span class="summary-key">Coordinates</span><span class="summary-val">'+lat+', '+lon+'</span></div>'+
    '<div class="summary-row"><span class="summary-key">UTC Offset</span><span class="summary-val">UTC'+utcStr+'</span></div>'+
    '<div class="summary-row"><span class="summary-key">Deployment</span><span class="summary-val">'+days+' days from '+dt+'</span></div>';
}

async function submitSetup(){
  const btn=document.getElementById('submitBtn');
  btn.disabled=true;btn.textContent='Saving\u2026';
  const da=document.getElementById('setupDate').value;
  const ti=document.getElementById('setupTime').value;
  const lat=parseFloat(document.getElementById('setupLat').value);
  const lon=parseFloat(document.getElementById('setupLon').value);
  const days=parseInt(document.getElementById('setupDays').value)||15;
  const utcOff=utcOffsetHours();
  const parts=da.split('-');
  const payload={
    lat:lat, lon:lon, utc_offset:utcOff, deploy_days:days,
    deploy_yr:parseInt(parts[0]), deploy_mo:parseInt(parts[1]), deploy_dy:parseInt(parts[2])
  };
  try{
    const r=await fetch('/api/setup?date='+encodeURIComponent(da)+'&time='+encodeURIComponent(ti),{
      method:'POST',
      headers:{'Content-Type':'application/json'},
      body:JSON.stringify(payload)
    });
    const j=await r.json();
    if(j.ok){
      toast('Setup complete! Loading dashboard\u2026','ok');
      setTimeout(()=>window.location.href='/',2000);
    } else {
      toast('Save failed: '+(j.error||'unknown'),'error');
      btn.disabled=false;btn.textContent='\u2713 Finish Setup';
    }
  }catch(e){
    toast('Network error','error');
    btn.disabled=false;btn.textContent='\u2713 Finish Setup';
  }
}

let toastTimer=null;
function toast(msg,type='ok'){
  const el=document.getElementById('toast');
  el.textContent=msg;el.className='toast show '+(type==='error'?'error':'ok');
  clearTimeout(toastTimer);
  toastTimer=setTimeout(()=>{el.className='toast';},3000);
}
</script>
</body></html>
)SETUP";

// ═══════════════════════════════════════════════════════════════════════════
//  SECTION 4 — SETTINGS PAGE HTML
// ═══════════════════════════════════════════════════════════════════════════
const char SETTINGS_HTML[] PROGMEM = R"SETT(
<!DOCTYPE html><html lang="en"><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Helios Settings</title>
<style>
:root{
  --bg:#F0F4F8;--surface:#FFFFFF;--card:#FFFFFF;--border:#C8D6E0;
  --teal:#007A7A;--teal-light:#E0F4F4;
  --green:#1A7A45;--green-bg:#E4F5EC;
  --red:#B82222;--red-bg:#FDEAEA;
  --text:#1A2530;--text-dim:#4A6070;--text-muted:#7A96A8;
  --mono:'Courier New',monospace;--sans:system-ui,sans-serif;
  --radius:8px;--shadow:0 2px 8px rgba(0,0,0,0.10);
}
*,*::before,*::after{box-sizing:border-box;margin:0;padding:0}
html{font-size:16px}
body{background:var(--bg);color:var(--text);font-family:var(--sans);min-height:100vh}
.shell{max-width:700px;margin:0 auto;padding:0 16px 48px}
header{display:flex;align-items:center;justify-content:space-between;
  padding:18px 0 16px;border-bottom:3px solid var(--teal);margin-bottom:28px;
  background:var(--bg);position:sticky;top:0;z-index:100}
.logo-text{font-family:var(--mono);font-size:18px;font-weight:700;color:var(--teal)}
.back-btn{background:var(--teal);color:#fff;border:none;border-radius:6px;
  font-family:var(--mono);font-size:13px;font-weight:700;padding:8px 16px;
  cursor:pointer;text-decoration:none;display:inline-flex;align-items:center;gap:6px}
.back-btn:hover{background:#005555}
.section-label{font-family:var(--mono);font-size:12px;font-weight:700;letter-spacing:0.10em;
  text-transform:uppercase;color:var(--teal);margin-bottom:12px;margin-top:28px;
  display:flex;align-items:center;gap:8px}
.section-label::after{content:'';flex:1;height:2px;background:var(--teal-light)}
.settings-card{background:var(--card);border:2px solid var(--border);border-radius:var(--radius);
  padding:0;overflow:hidden;box-shadow:var(--shadow);margin-bottom:4px}
.setting-row{display:flex;align-items:center;justify-content:space-between;
  padding:16px 20px;border-bottom:1px solid var(--border);gap:16px;flex-wrap:wrap}
.setting-row:last-child{border-bottom:none}
.setting-row:hover{background:var(--teal-light)}
.setting-info{flex:1;min-width:180px}
.setting-name{font-size:15px;font-weight:700;color:var(--text);margin-bottom:3px}
.setting-desc{font-size:12px;color:var(--text-dim);font-weight:500}
.setting-input{display:flex;align-items:center;gap:8px;flex-shrink:0}
.setting-input input{
  background:var(--bg);border:2px solid var(--border);border-radius:6px;
  color:var(--text);font-family:var(--mono);font-size:16px;font-weight:700;
  padding:8px 12px;width:110px;text-align:right
}
.setting-input input:focus{outline:none;border-color:var(--teal)}
.setting-unit{font-family:var(--mono);font-size:13px;font-weight:700;color:var(--text-dim);min-width:30px}
.btn-row{display:flex;gap:12px;margin-top:24px;flex-wrap:wrap}
.btn{display:inline-flex;align-items:center;gap:5px;border:none;border-radius:6px;
  font-family:var(--mono);font-size:14px;font-weight:700;cursor:pointer;
  padding:10px 20px;transition:all 0.15s;text-decoration:none;white-space:nowrap}
.btn-teal{background:var(--teal);color:#fff}
.btn-teal:hover{background:#005555}
.btn-ghost{background:var(--surface);border:2px solid var(--border);color:var(--text-dim)}
.btn-ghost:hover{border-color:var(--teal);color:var(--teal);background:var(--teal-light)}
.toast{position:fixed;bottom:20px;right:20px;background:var(--text);border-radius:var(--radius);
  padding:12px 18px;font-family:var(--mono);font-size:13px;font-weight:700;color:#fff;
  opacity:0;transform:translateY(8px);transition:all 0.25s;z-index:9999;pointer-events:none}
.toast.show{opacity:1;transform:translateY(0)}
.toast.error{background:var(--red)}
.toast.ok{background:var(--green)}
.defaults-note{font-size:12px;color:var(--text-muted);margin-top:12px;font-weight:600}
</style>
</head>
<body>
<div class="shell">
<header>
  <div class="logo-text">&#9881; HELIOS SETTINGS</div>
  <a class="back-btn" href="/">&#8592; Dashboard</a>
</header>

<div class="section-label">Logging Parameters</div>
<div class="settings-card">
  <div class="setting-row">
    <div class="setting-info">
      <div class="setting-name">Sample Interval</div>
      <div class="setting-desc">How often sensor readings are taken and logged</div>
    </div>
    <div class="setting-input">
      <input type="number" id="sampleInterval" min="5" max="300" step="5" value="10">
      <span class="setting-unit">sec</span>
    </div>
  </div>
  <div class="setting-row">
    <div class="setting-info">
      <div class="setting-name">Image Interval</div>
      <div class="setting-desc">How often sky images are captured during the day</div>
    </div>
    <div class="setting-input">
      <input type="number" id="imgInterval" min="1" max="60" step="1" value="3">
      <span class="setting-unit">min</span>
    </div>
  </div>
  <div class="setting-row">
    <div class="setting-info">
      <div class="setting-name">JPEG Quality</div>
      <div class="setting-desc">Image compression (1=smallest, 63=best quality)</div>
    </div>
    <div class="setting-input">
      <input type="number" id="jpegQuality" min="1" max="63" step="1" value="5">
      <span class="setting-unit">1-63</span>
    </div>
  </div>
  <div class="setting-row">
    <div class="setting-info">
      <div class="setting-name">Write Buffer Size</div>
      <div class="setting-desc">Samples to hold in RAM before writing to flash</div>
    </div>
    <div class="setting-input">
      <input type="number" id="flushCount" min="1" max="50" step="1" value="10">
      <span class="setting-unit">samples</span>
    </div>
  </div>
</div>

<div class="section-label">WiFi Parameters</div>
<div class="settings-card">
  <div class="setting-row">
    <div class="setting-info">
      <div class="setting-name">WiFi Auto-Off</div>
      <div class="setting-desc">Minutes of no clients before AP shuts down automatically</div>
    </div>
    <div class="setting-input">
      <input type="number" id="wifiAutoOff" min="1" max="120" step="1" value="5">
      <span class="setting-unit">min</span>
    </div>
  </div>
</div>

<div class="section-label">Data Quality</div>
<div class="settings-card">
  <div class="setting-row">
    <div class="setting-info">
      <div class="setting-name">Outlier Factor</div>
      <div class="setting-desc">Reject reading if lux jumps more than N&times; previous value</div>
    </div>
    <div class="setting-input">
      <input type="number" id="outlierFactor" min="2" max="100" step="1" value="10">
      <span class="setting-unit">&times;</span>
    </div>
  </div>
  <div class="setting-row">
    <div class="setting-info">
      <div class="setting-name">Night Confirm Count</div>
      <div class="setting-desc">Consecutive below-threshold lux readings to confirm sunset</div>
    </div>
    <div class="setting-input">
      <input type="number" id="nightConfirm" min="1" max="20" step="1" value="5">
      <span class="setting-unit">readings</span>
    </div>
  </div>
</div>

<div class="section-label">&#9719; Thermal Protection</div>
<div class="settings-card">
  <div class="setting-row">
    <div class="setting-info">
      <div class="setting-name">Shutdown Temperature</div>
      <div class="setting-desc">If die temp exceeds this, Helios pauses logging to cool down. Set to 0 to disable. (ESP32-S3 max rated: 85&deg;C)</div>
    </div>
    <div class="setting-input">
      <input type="number" id="tempShutdown" min="0" max="85" step="1" value="75">
      <span class="setting-unit">&deg;C</span>
    </div>
  </div>
  <div class="setting-row">
    <div class="setting-info">
      <div class="setting-name">Cooldown Duration</div>
      <div class="setting-desc">How long Helios sleeps after a thermal shutdown before resuming</div>
    </div>
    <div class="setting-input">
      <input type="number" id="tempSleep" min="30" max="600" step="30" value="120">
      <span class="setting-unit">sec</span>
    </div>
  </div>
</div>

<div class="section-label">&#128161; Status LED (GPIO 48 — WS2812B NeoPixel)</div>
<div class="settings-card">
  <div class="setting-row" style="flex-direction:column;align-items:flex-start;gap:12px;padding-bottom:18px">
    <div style="font-size:12px;color:var(--text-dim);font-weight:600;margin-bottom:2px">GPIO 48 on the ESP32-S3 WROOM is a WS2812B RGB LED, not a plain GPIO. Colours below match the firmware exactly.</div>
    <div style="display:flex;gap:14px;flex-wrap:wrap">

      <div style="display:flex;align-items:center;gap:8px;min-width:220px">
        <span style="display:inline-block;width:16px;height:16px;border-radius:50%;background:#0000B4;animation:solidpulse 1s infinite;flex-shrink:0"></span>
        <div>
          <div style="font-size:13px;font-weight:700;color:var(--text)">Solid Blue</div>
          <div style="font-size:11px;color:var(--text-dim)">WiFi AP is active</div>
        </div>
      </div>

      <div style="display:flex;align-items:center;gap:8px;min-width:220px">
        <span style="display:inline-block;width:16px;height:16px;border-radius:50%;background:#007800;animation:slowblink 1s infinite;flex-shrink:0"></span>
        <div>
          <div style="font-size:13px;font-weight:700;color:var(--text)">Slow blink Green (900 ms on / 100 ms off)</div>
          <div style="font-size:11px;color:var(--text-dim)">Logging active (daytime)</div>
        </div>
      </div>

      <div style="display:flex;align-items:center;gap:8px;min-width:220px">
        <span style="display:inline-block;width:16px;height:16px;border-radius:50%;background:#B40000;animation:fastblink 160ms infinite;flex-shrink:0"></span>
        <div>
          <div style="font-size:13px;font-weight:700;color:var(--text)">Fast blink Red (80 ms on / 80 ms off)</div>
          <div style="font-size:11px;color:var(--text-dim)">Error — RTC not ready, or thermal cooldown active</div>
        </div>
      </div>

      <div style="display:flex;align-items:center;gap:8px;min-width:220px">
        <span style="display:inline-block;width:16px;height:16px;border-radius:50%;background:#C8D6E0;flex-shrink:0"></span>
        <div>
          <div style="font-size:13px;font-weight:700;color:var(--text)">Off</div>
          <div style="font-size:11px;color:var(--text-dim)">Night / idle — RTC ready, WiFi off, not logging</div>
        </div>
      </div>

    </div>
    <div style="font-size:11px;color:var(--text-muted);font-weight:600;margin-top:2px">
      Priority: WiFi (blue) &gt; RTC error (fast red) &gt; logging (slow green) &gt; off.<br>
      Thermal cooldown overrides the LED to fast red regardless of WiFi/logging state.
    </div>
  </div>
</div>
<style>
@keyframes slowblink{0%,89%{opacity:1}90%,100%{opacity:0.08}}
@keyframes fastblink{0%,49%{opacity:1}50%,100%{opacity:0.08}}
@keyframes solidpulse{0%,100%{opacity:1}50%{opacity:0.85}}
</style>

<div class="btn-row">
  <button class="btn btn-teal" onclick="saveSettings()">&#10003; Save Settings</button>
  <button class="btn btn-ghost" onclick="loadSettings()">&#8635; Reload from Device</button>
  <button class="btn btn-ghost" onclick="resetDefaults()">&#9888; Reset Defaults</button>
</div>
<div class="defaults-note">Settings are saved to /data/config.json on the device and loaded on every boot.</div>
</div>

<div class="settings-card" style="margin-top:12px;border-color:#C0392B">
  <div class="settings-header" style="background:#FDEAEA;border-color:#C0392B">
    <div class="settings-title" style="color:#B82222">&#9888; Danger Zone</div>
  </div>
  <div class="settings-body">
    <div id="locationInfo" class="defaults-note" style="margin-bottom:12px;font-family:var(--mono);font-size:12px">Loading location info...</div>
    <div class="btn-row" style="margin-top:0">
      <button class="btn btn-danger" onclick="resetSetup()">&#8635; Re-run Setup Wizard</button>
    </div>
    <div class="defaults-note" style="margin-top:8px">This clears the location config. On next WiFi connect the setup wizard will appear instead of the dashboard. The device will keep logging with the current settings in the meantime.</div>
  </div>
</div>
<div class="toast" id="toast"></div>
<script>
const DEFAULTS={sampleInterval:10,imgInterval:3,jpegQuality:5,flushCount:10,wifiAutoOff:5,outlierFactor:10,nightConfirm:5,tempShutdown:75,tempSleep:120};

async function loadSettings(){
  try{
    const d=await(await fetch('/api/config')).json();
    document.getElementById('sampleInterval').value=d.sample_interval_s||DEFAULTS.sampleInterval;
    document.getElementById('imgInterval').value=d.img_interval_min||DEFAULTS.imgInterval;
    document.getElementById('jpegQuality').value=d.jpeg_quality||DEFAULTS.jpegQuality;
    document.getElementById('flushCount').value=d.flush_count||DEFAULTS.flushCount;
    document.getElementById('wifiAutoOff').value=d.wifi_autooff_min||DEFAULTS.wifiAutoOff;
    document.getElementById('outlierFactor').value=d.outlier_factor||DEFAULTS.outlierFactor;
    document.getElementById('nightConfirm').value=d.night_confirm||DEFAULTS.nightConfirm;
    document.getElementById('tempShutdown').value=(d.temp_shutdown_c!==undefined)?d.temp_shutdown_c:DEFAULTS.tempShutdown;
    document.getElementById('tempSleep').value=d.temp_sleep_s||DEFAULTS.tempSleep;
    toast('Settings loaded','ok');
  }catch(e){toast('Load failed — showing defaults','error');}
}

async function saveSettings(){
  const cfg={
    sample_interval_s:parseInt(document.getElementById('sampleInterval').value),
    img_interval_min:parseInt(document.getElementById('imgInterval').value),
    jpeg_quality:parseInt(document.getElementById('jpegQuality').value),
    flush_count:parseInt(document.getElementById('flushCount').value),
    wifi_autooff_min:parseInt(document.getElementById('wifiAutoOff').value),
    outlier_factor:parseFloat(document.getElementById('outlierFactor').value),
    night_confirm:parseInt(document.getElementById('nightConfirm').value),
    temp_shutdown_c:parseFloat(document.getElementById('tempShutdown').value),
    temp_sleep_s:parseInt(document.getElementById('tempSleep').value)
  };
  try{
    const r=await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(cfg)});
    const j=await r.json();
    if(j.ok) toast('Saved — device updated immediately','ok');
    else toast('Save failed: '+(j.error||'unknown'),'error');
  }catch(e){toast('Network error','error');}
}

function resetDefaults(){
  if(!confirm('Reset all settings to defaults?'))return;
  Object.entries(DEFAULTS).forEach(([k,v])=>{
    const el=document.getElementById(k);if(el)el.value=v;
  });
  saveSettings();
}

async function loadLocationInfo(){
  try{
    const d=await(await fetch('/api/config')).json();
    const el=document.getElementById('locationInfo');
    if(!el)return;
    const utcStr=(d.utc_offset>=0?'+':'')+d.utc_offset;
    const startStr=d.deploy_yr+'-'+String(d.deploy_mo).padStart(2,'0')+'-'+String(d.deploy_dy).padStart(2,'0');
    el.textContent='Current: '+d.lat.toFixed(4)+', '+d.lon.toFixed(4)
      +' • UTC'+utcStr
      +' • '+d.deploy_days+' days from '+startStr;
  }catch(e){/* silent */}
}

async function resetSetup(){
  if(!confirm('Re-run the setup wizard?\nThe device will show the setup page on next WiFi connect.'))return;
  try{
    const r=await fetch('/api/reset-setup',{method:'POST'});
    const j=await r.json();
    if(j.ok) toast('Setup reset — reconnect to see wizard','ok');
    else toast('Reset failed','error');
  }catch(e){toast('Network error','error');}
}

let toastTimer=null;
function toast(msg,type='ok'){
  const el=document.getElementById('toast');
  el.textContent=msg;el.className='toast show '+(type==='error'?'error':'ok');
  clearTimeout(toastTimer);
  toastTimer=setTimeout(()=>{el.className='toast';},3000);
}

loadSettings();
loadLocationInfo();
</script>
</body></html>
)SETT";

// ═══════════════════════════════════════════════════════════════════════════
//  SECTION 5 — FIRMWARE
// ═══════════════════════════════════════════════════════════════════════════

// ── Fixed config ────────────────────────────────────────────────────────────
#define AP_SSID          "Helios-Logger"
#define AP_PASSWORD      "helios2026"
#define MDNS_HOSTNAME    "helios"
#define DATA_DIR         "/data"
#define IMGS_DIR         "/imgs"
#define CONFIG_FILE      "/data/config.json"
#define DNS_PORT         53
#define WDT_TIMEOUT_S    30
#define BH1750_SDA       45
#define BH1750_SCL       46
#define WIFI_BTN1        0
#define WIFI_BTN2        3
#define WIFI_BTN_HOLD_MS 2000UL
#define I2C_RECOVERY_CLK 9
#define BH1750_ADDR      0x23
#define LUX_TO_WM2       (1.0f/116.0f)
#define LIVE_BUFFER_SIZE 360
#define STATUS_LED_PIN   48
#define STATUS_LED_COUNT 1
#define TEMP_SLEEP_S     120

struct Config {
  uint32_t sample_interval_s  = 10;
  uint32_t img_interval_min   = 3;
  uint8_t  jpeg_quality       = 5;
  uint8_t  flush_count        = 10;
  uint32_t wifi_autooff_min   = 5;
  float    outlier_factor     = 10.0f;
  uint8_t  night_confirm      = 5;
  float    temp_shutdown_c    = 75.0f;
  uint16_t temp_sleep_s       = 120;
  bool     setup_done         = false;
  double   lat                = 24.9;
  double   lon                = 91.9;
  float    utc_offset         = 6.0f;
  uint16_t deploy_days        = 30;
  uint16_t deploy_yr          = 2026;
  uint8_t  deploy_mo          = 6;
  uint8_t  deploy_dy          = 1;
};
Config cfg;

BH1750     lightMeter;
RTC_DS3231 rtc;
WebServer  server(80);
DNSServer  dnsServer;

static temperature_sensor_handle_t tsens_handle = NULL;

bool     cameraReady   = false;
bool     rtcReady      = false;
bool     isLogging     = false;
uint32_t lastSampleMs  = 0;
uint32_t lastImgMs     = 0;
uint32_t dayStartMs    = 0;
char     currentFile[48];
char     currentDateStr[12];
uint8_t  nightCount    = 0;
uint8_t  btn1Prev      = HIGH, btn2Prev = HIGH;
uint32_t btn1PressMs   = 0,    btn2PressMs = 0;
bool     btn1Fired     = false, btn2Fired = false;
bool     wifiActive    = false;
bool     wifiHadClient = false;
uint32_t lastClientMs  = 0;
uint32_t imgCountToday = 0;
char     sunriseStr[6] = "--:--";
char     sunsetStr[6]  = "--:--";
char     nextImgStr[6] = "--:--";

enum LedMode { LED_OFF, LED_SLOW_BLINK, LED_FAST_BLINK, LED_SOLID };
static LedMode  ledMode       = LED_OFF;
static uint32_t ledLastMs     = 0;
static bool     ledState      = false;
#define LED_SLOW_ON_MS   900
#define LED_SLOW_OFF_MS  100
#define LED_FAST_ON_MS   80
#define LED_FAST_OFF_MS  80
static uint8_t _ledR = 0, _ledG = 0, _ledB = 0;

static void _ledShow(bool on) {
  if (on) _ledStrip.setPixelColor(0, _ledStrip.Color(_ledR, _ledG, _ledB));
  else    _ledStrip.setPixelColor(0, 0);
  _ledStrip.show();
}

void initStatusLed() {
  _ledStrip.begin();
  _ledStrip.setBrightness(80);
  _ledStrip.clear();
  _ledStrip.show();
}

void setLedMode(LedMode m) {
  if (ledMode == m) return;
  ledMode = m;
  if      (m == LED_SOLID)      { _ledR=0;   _ledG=0;   _ledB=180; }
  else if (m == LED_FAST_BLINK) { _ledR=180; _ledG=0;   _ledB=0;   }
  else if (m == LED_SLOW_BLINK) { _ledR=0;   _ledG=120; _ledB=0;   }
  if (m == LED_SOLID) { ledState = true;  _ledShow(true);  }
  else if (m == LED_OFF) { ledState = false; _ledShow(false); }
  ledLastMs = millis();
}

void tickLed() {
  if (ledMode == LED_SOLID || ledMode == LED_OFF) return;
  uint32_t now   = millis();
  uint32_t onMs  = (ledMode == LED_FAST_BLINK) ? LED_FAST_ON_MS  : LED_SLOW_ON_MS;
  uint32_t offMs = (ledMode == LED_FAST_BLINK) ? LED_FAST_OFF_MS : LED_SLOW_OFF_MS;
  uint32_t period = ledState ? onMs : offMs;
  if (now - ledLastMs >= period) {
    ledState = !ledState;
    _ledShow(ledState);
    ledLastMs = now;
  }
}

void updateLedState() {
  if (wifiActive)    { setLedMode(LED_SOLID);      return; }
  if (!rtcReady)     { setLedMode(LED_FAST_BLINK); return; }
  if (isLogging)     { setLedMode(LED_SLOW_BLINK); return; }
  setLedMode(LED_OFF);
}

static bool     inThermalSleep      = false;
static uint32_t thermalSleepEndMs   = 0;
static uint32_t thermalEventCount   = 0;
static bool     thermalWifiWasOn    = false;
static bool     thermalCamWasReady  = false;
static bool     manualOverride      = false;
static bool     ntpSynced           = false;
static uint32_t lastNtpAttemptMs    = 0;
#define NTP_RETRY_MS  (10UL * 60UL * 1000UL)

struct Sample {
  uint32_t elapsed_s;
  char     date[12];
  char     time[10];
  float    lux;
  float    irradiance_wm2;
  uint8_t  blue_channel;
  float    temp_c;
};
Sample   liveBuffer[LIVE_BUFFER_SIZE];
uint16_t liveHead     = 0;
uint16_t liveCount    = 0;
uint32_t totalSamples = 0;
uint32_t rejectedCount = 0;
Sample   writeBuf[50];
uint8_t  writeBufCount = 0;
float    prevLux = -1.0f;
double   dayIrrSum=0,dayIrrSumSq=0,dayTempSum=0;
float    dayIrrPeak=0;
uint32_t daySamples=0;

// ── Forward declarations ─────────────────────────────────────────────────────
void loadConfig();
void saveConfig();
void getSunTimesForDate(int yr, int mo, int dy, int &srMin, int &ssMin);
bool isDaytime();
void updateSunStrings();
void initWatchdog();
void initTempSensor();
float readDieTemp();
void initRTC();
void getRTCStrings(char *dateOut, char *timeOut);
void configureBH1750();
void recoverI2CBus();
bool readBH1750(float &lux);
bool isValidReading(float lux);
bool initCamera(uint8_t quality, framesize_t fsize, pixformat_t fmt);
void captureAndSaveImage(const char *dateStr, const char *timeStr);
uint8_t captureBlueChannel();
void ensureDirs();
void openDayFile(const char *dateStr);
void flushWriteBuffer(bool force=false);
void bufferSample(const Sample &s);
void pushToLiveBuffer(const Sample &s);
void writeMeta(const char *dateStr, uint32_t rows);
void writeDaySummary(const char *dateStr, uint32_t durationS);
void resetDayAccumulators();
uint32_t countFileRows(const char *path);
void startWiFi();
void stopWiFi();
void checkBtn(uint8_t pin, uint8_t &prevState, uint32_t &pressMs, bool &fired);
void handleWiFiButtons();
void handleWebServer();
void initStatusLed();
void setLedMode(LedMode m);
void tickLed();
void updateLedState();
void logBootReason();
void tryNtpSync();
void writeThermalEvent(const char *dateStr, const char *timeStr, float tempC);
void repairDayFileTail(const char *path);

// Web handler forward declarations
void handleRoot();
void handleSettings();
void handleCaptive();
void handleSetupPage();
void handleStatus();
void handleLive();
void handleFiles();
void handleImages();
void handleImg();
void handleDownload();
void handleDelete();
void handleDeleteAll();
void handleSetTime();
void handleGetConfig();
void handlePostConfig();
void handleSetupPost();
void handleResetSetup();
void handleOverride();
void handleOtaPage();
void handleOtaUpload();
void handleOtaUploadFinish();
void handleCamSnapshot();
void handleCamStream();
void handleCamStats();

// ── Config load/save ─────────────────────────────────────────────────────────
void loadConfig() {
  if (!FFat.exists(CONFIG_FILE)) {
    wlog("[CFG] No config file — using defaults");
    return;
  }
  File f = FFat.open(CONFIG_FILE, "r");
  if (!f) return; String s = f.readString(); f.close();
  auto getInt = [&](const char *key, uint32_t def) -> uint32_t {
    String k = "\""; k += key; k += "\":";
    int idx = s.indexOf(k);
    if (idx < 0) return def;
    return (uint32_t)s.substring(idx + k.length()).toInt();
  };
  auto getFloat = [&](const char *key, float def) -> float {
    String k = "\""; k += key; k += "\":";
    int idx = s.indexOf(k);
    if (idx < 0) return def;
    return s.substring(idx + k.length()).toFloat();
  };
  auto getDouble = [&](const char *key, double def) -> double {
    String k = "\""; k += key; k += "\":";
    int idx = s.indexOf(k);
    if (idx < 0) return def;
    return (double)s.substring(idx + k.length()).toDouble();
  };
  auto getBool = [&](const char *key, bool def) -> bool {
    String k = "\""; k += key; k += "\":";
    int idx = s.indexOf(k);
    if (idx < 0) return def;
    return s.substring(idx + k.length(), idx + k.length() + 5).startsWith("true");
  };
  cfg.sample_interval_s = getInt("sample_interval_s", 10);
  cfg.img_interval_min  = getInt("img_interval_min",  3);
  cfg.jpeg_quality      = (uint8_t)getInt("jpeg_quality",  5);
  cfg.flush_count       = (uint8_t)getInt("flush_count",   10);
  cfg.wifi_autooff_min  = getInt("wifi_autooff_min",  5);
  cfg.outlier_factor    = getFloat("outlier_factor",   10.0f);
  cfg.night_confirm     = (uint8_t)getInt("night_confirm",  5);
  cfg.temp_shutdown_c   = getFloat("temp_shutdown_c",  75.0f);
  cfg.temp_sleep_s      = (uint16_t)getInt("temp_sleep_s",  120);
  cfg.setup_done        = getBool("setup_done",         false);
  cfg.lat               = getDouble("lat",              24.9);
  cfg.lon               = getDouble("lon",              91.9);
  cfg.utc_offset        = getFloat("utc_offset",        6.0f);
  cfg.deploy_days       = (uint16_t)getInt("deploy_days",  30);
  cfg.deploy_yr         = (uint16_t)getInt("deploy_yr",    2026);
  cfg.deploy_mo         = (uint8_t)getInt("deploy_mo",     6);
  cfg.deploy_dy         = (uint8_t)getInt("deploy_dy",     1);
  wlogf("[CFG] setup=%s lat=%.4f lon=%.4f utc=%.1f days=%u\n",
    cfg.setup_done?"done":"pending", cfg.lat, cfg.lon, cfg.utc_offset, cfg.deploy_days);
  wlogf("[CFG] si=%lus ii=%lumin jq=%u fc=%u wao=%lumin of=%.0f nc=%u tsd=%.0f tss=%u\n",
    cfg.sample_interval_s, cfg.img_interval_min, cfg.jpeg_quality,
    cfg.flush_count, cfg.wifi_autooff_min, cfg.outlier_factor, cfg.night_confirm,
    cfg.temp_shutdown_c, cfg.temp_sleep_s);
}

void saveConfig() {
  File f = FFat.open(CONFIG_FILE, "w");
  if (!f) { wlog("[CFG] Save failed"); return; }
  f.printf(
    "{\"sample_interval_s\":%lu,\"img_interval_min\":%lu,"
    "\"jpeg_quality\":%u,\"flush_count\":%u,"
    "\"wifi_autooff_min\":%lu,\"outlier_factor\":%.1f,"
    "\"night_confirm\":%u,"
    "\"temp_shutdown_c\":%.1f,\"temp_sleep_s\":%u,"
    "\"setup_done\":%s,"
    "\"lat\":%.6f,\"lon\":%.6f,\"utc_offset\":%.2f,"
    "\"deploy_days\":%u,\"deploy_yr\":%u,\"deploy_mo\":%u,\"deploy_dy\":%u}",
    cfg.sample_interval_s, cfg.img_interval_min,
    cfg.jpeg_quality, cfg.flush_count,
    cfg.wifi_autooff_min, cfg.outlier_factor, cfg.night_confirm,
    cfg.temp_shutdown_c, cfg.temp_sleep_s,
    cfg.setup_done ? "true" : "false",
    cfg.lat, cfg.lon, cfg.utc_offset,
    cfg.deploy_days, cfg.deploy_yr, cfg.deploy_mo, cfg.deploy_dy);
  f.close();
  wlog("[CFG] Saved");
}

// ── Sunrise/sunset ───────────────────────────────────────────────────────────
void getSunTimesForDate(int yr, int mo, int dy, int &srMin, int &ssMin) {
  calcSunTimes(yr, mo, dy, cfg.lat, cfg.lon, cfg.utc_offset, srMin, ssMin);
}

bool isDaytime() {
  if (manualOverride) return true;
  if (!rtcReady) return false;
  DateTime now = rtc.now();
  int srMin, ssMin;
  getSunTimesForDate(now.year(), now.month(), now.day(), srMin, ssMin);
  int nowMin = now.hour() * 60 + now.minute();
  return (nowMin >= srMin && nowMin < ssMin);
}

void updateSunStrings() {
  if (!rtcReady) return;
  DateTime now = rtc.now();
  int srMin, ssMin;
  getSunTimesForDate(now.year(), now.month(), now.day(), srMin, ssMin);
  snprintf(sunriseStr, sizeof(sunriseStr), "%02d:%02d", srMin / 60, srMin % 60);
  snprintf(sunsetStr,  sizeof(sunsetStr),  "%02d:%02d", ssMin / 60, ssMin % 60);
}

void initWatchdog() {
  esp_task_wdt_config_t wdt_cfg = {
    .timeout_ms     = WDT_TIMEOUT_S * 1000,
    .idle_core_mask = 0,
    .trigger_panic  = true
  };
  esp_task_wdt_reconfigure(&wdt_cfg);
  esp_task_wdt_add(NULL);
  wlogf("[WDT] Armed — %ds\n", WDT_TIMEOUT_S);
}

void initTempSensor() {
  temperature_sensor_config_t tc = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 80);
  if (temperature_sensor_install(&tc, &tsens_handle) != ESP_OK) { tsens_handle=NULL; return; }
  temperature_sensor_enable(tsens_handle);
  wlog("[TEMP] Ready");
}

float readDieTemp() {
  if (!tsens_handle) return 0.0f;
  float t=0; temperature_sensor_get_celsius(tsens_handle, &t); return t;
}

void logBootReason() {
  esp_reset_reason_t r = esp_reset_reason();
  const char *s = "UNKNOWN";
  switch(r){
    case ESP_RST_POWERON:  s="POWER_ON";  break;
    case ESP_RST_SW:       s="SOFTWARE";  break;
    case ESP_RST_PANIC:    s="PANIC";     break;
    case ESP_RST_INT_WDT:  s="INT_WDT";  break;
    case ESP_RST_TASK_WDT: s="TASK_WDT"; break;
    case ESP_RST_WDT:      s="WDT";      break;
    case ESP_RST_DEEPSLEEP:s="DEEPSLEEP";break;
    case ESP_RST_BROWNOUT: s="BROWNOUT"; break;
    case ESP_RST_SDIO:     s="SDIO";     break;
    default: break;
  }
  wlogf("[BOOT] Reset reason: %s (%d)\n", s, (int)r);
  if (r == ESP_RST_TASK_WDT || r == ESP_RST_INT_WDT || r == ESP_RST_WDT)
    wlog("[BOOT] *** WDT reboot detected — check for blocking loops ***");
  if (r == ESP_RST_BROWNOUT)
    wlog("[BOOT] *** Brownout detected — check power supply ***");
}

void tryNtpSync() {
  if (!wifiActive || !rtcReady) return;
  if (ntpSynced && (millis() - lastNtpAttemptMs < NTP_RETRY_MS)) return;
  if (WiFi.softAPgetStationNum() == 0) return;
  lastNtpAttemptMs = millis();
  configTime(0, 0, "pool.ntp.org", "time.google.com");
  struct tm ti;
  uint32_t deadline = millis() + 4000;
  while (millis() < deadline) {
    if (getLocalTime(&ti, 100)) {
      time_t epoch = mktime(&ti) + (time_t)(cfg.utc_offset * 3600.0f);
      struct tm *lt = gmtime(&epoch);
      DateTime dt(lt->tm_year+1900, lt->tm_mon+1, lt->tm_mday,
                  lt->tm_hour, lt->tm_min, lt->tm_sec);
      rtc.adjust(dt);
      ntpSynced = true;
      wlogf("[NTP] Synced — %04d-%02d-%02d %02d:%02d:%02d (UTC%+.1f)\n",
        dt.year(),dt.month(),dt.day(),dt.hour(),dt.minute(),dt.second(),
        cfg.utc_offset);
      return;
    }
    delay(100);
  }
  wlog("[NTP] Sync timed out — will retry");
}

// ── Thermal event log ────────────────────────────────────────────────────────
void writeThermalEvent(const char *dateStr, const char *timeStr, float tempC) {
  char path[52]; snprintf(path, sizeof(path), "%s/%s.therm", DATA_DIR, dateStr);
  bool exists = FFat.exists(path);
  File f = FFat.open(path, "a");
  if (!f) return;
  if (!exists) f.println("date,time,die_temp_c,threshold_c,sleep_s");
  f.printf("%s,%s,%.2f,%.1f,%u\n", dateStr, timeStr, tempC,
           cfg.temp_shutdown_c, cfg.temp_sleep_s);
  f.close();
}

// ── CSV tail repair ──────────────────────────────────────────────────────────
void repairDayFileTail(const char *path) {
  File f = FFat.open(path, "r");
  if (!f) return;
  size_t sz = f.size();
  if (sz < 2) { f.close(); return; }
  size_t checkLen = (sz < 128) ? sz : 128;
  f.seek(sz - checkLen);
  char buf[130]; uint16_t n = 0;
  while (f.available() && n < checkLen) buf[n++] = (char)f.read();
  f.close();
  buf[n] = '\0';
  int lastNl = -1;
  for (int i = n - 1; i >= 0; i--) {
    if (buf[i] == '\n') { lastNl = i; break; } 
  }
  if (lastNl < 0) return;
  if ((size_t)(sz - checkLen + lastNl + 1) == sz) return;
  size_t trimTo = sz - checkLen + lastNl + 1;
  char tmp[52]; snprintf(tmp, sizeof(tmp), "%s/_repair.tmp", DATA_DIR);
  File src = FFat.open(path, "r");
  File dst = FFat.open(tmp, "w");
  if (!src || !dst) { if(src) src.close(); if(dst) dst.close(); return; }
  size_t remaining = trimTo;
  uint8_t rbuf[256];
  while (remaining > 0) {
    size_t chunk = (remaining < sizeof(rbuf)) ? remaining : sizeof(rbuf);
    size_t got = src.read(rbuf, chunk);
    if (!got) break;
    dst.write(rbuf, got);
    remaining -= got;
  }
  src.close(); dst.close();
  FFat.remove(path);
  FFat.rename(tmp, path);
  wlogf("[FS] Repaired truncated CSV: %s (trimmed %u bytes)\n",
        path, (unsigned)(sz - trimTo));
}

// ── DS3231 ───────────────────────────────────────────────────────────────────
void initRTC() {
  if (!rtc.begin(&Wire)) {
    wlog("[RTC] DS3231 not found — check SDA->GPIO45 SCL->GPIO46");
    rtcReady = false; return;
  }
  rtcReady = true;
  if (rtc.lostPower()) wlog("[RTC] Lost power — set time via dashboard");
  DateTime now = rtc.now();
  wlogf("[RTC] Ready — %04d-%02d-%02d %02d:%02d:%02d\n",
    now.year(),now.month(),now.day(),now.hour(),now.minute(),now.second());
}

void getRTCStrings(char *dateOut, char *timeOut) {
  if (!rtcReady) { strcpy(dateOut,"0000-00-00"); strcpy(timeOut,"00:00:00"); return; }
  DateTime now = rtc.now();
  snprintf(dateOut,12,"%04d-%02d-%02d",now.year(),now.month(),now.day());
  snprintf(timeOut,10,"%02d:%02d:%02d",now.hour(),now.minute(),now.second());
}

// ── BH1750 ───────────────────────────────────────────────────────────────────
void configureBH1750() {
  lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, BH1750_ADDR, &Wire);
}

uint8_t i2cFails = 0;

void recoverI2CBus() {
  Wire.end();
  pinMode(BH1750_SDA,INPUT_PULLUP); pinMode(BH1750_SCL,OUTPUT);
  for(int i=0;i<I2C_RECOVERY_CLK;i++){
    digitalWrite(BH1750_SCL,HIGH);delayMicroseconds(5);
    digitalWrite(BH1750_SCL,LOW); delayMicroseconds(5);
    if(digitalRead(BH1750_SDA)==HIGH) break;
  }
  pinMode(BH1750_SDA,OUTPUT);
  digitalWrite(BH1750_SDA,LOW); delayMicroseconds(5);
  digitalWrite(BH1750_SCL,HIGH);delayMicroseconds(5);
  digitalWrite(BH1750_SDA,HIGH);delayMicroseconds(5);
  Wire.begin(BH1750_SDA,BH1750_SCL);
  lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE,BH1750_ADDR,&Wire);
  delay(200);
  wlog("[I2C] Bus recovered");
}

bool readBH1750(float &lux) {
  float r = lightMeter.readLightLevel();
  if (r < 0) {
    lux=0; i2cFails++;
    if(i2cFails>=3){recoverI2CBus();i2cFails=0;}
    return false;
  }
  i2cFails=0; lux=r; return true;
}

bool isValidReading(float lux) {
  if(prevLux<0||prevLux<1) return true;
  if(lux>prevLux*cfg.outlier_factor) return false;
  if(lux<prevLux/cfg.outlier_factor) return false;
  return true;
}

// ── Camera ───────────────────────────────────────────────────────────────────
// ── v2.6 design: ONE init, ZERO deinit/reinit cycles ─────────────────────────
//
// The GDMA corruption (FB-OVF / gdma_disconnect) is caused by the esp32-camera
// library's esp_camera_deinit() not calling gdma_stop() before gdma_disconnect().
// If any DMA transaction is in flight the channel is left in an undefined
// hardware state; the subsequent esp_camera_init() inherits that state and the
// new pixel-format mode is born broken.
//
// Permanent fix: never call esp_camera_deinit() during normal operation.
// The camera is initialised ONCE in RGB565 QQVGA / GRAB_WHEN_EMPTY (fb_count=1)
// and stays in that mode forever.
//
// captureAndSaveImage() captures one RGB565 frame and converts it to JPEG
// using frame2jpg() — the same encoder the driver uses internally. This
// produces identical quality/size output (96×96, configurable quality) without
// any mode switch, deinit, or DMA disruption.
//
// captureBlueChannel() is unchanged in logic; it runs on the same single
// always-on DMA channel without interruption.
//
// The camEnterJpeg / camRestoreRgb helpers for the web stream are replaced:
// the stream simply re-uses the existing RGB565 DMA and compresses each frame
// in software with frame2jpg() before sending it as MJPEG. Frame rate is lower
// (~3-5 fps at QQVGA) but there is zero risk of DMA corruption.

extern "C" bool frame2jpg(camera_fb_t *fb, uint8_t quality, uint8_t **out, size_t *out_len);

static void _camPins(camera_config_t &c) {
  c.ledc_channel  = LEDC_CHANNEL_0; c.ledc_timer = LEDC_TIMER_0;
  c.pin_d0=Y2_GPIO_NUM; c.pin_d1=Y3_GPIO_NUM; c.pin_d2=Y4_GPIO_NUM; c.pin_d3=Y5_GPIO_NUM;
  c.pin_d4=Y6_GPIO_NUM; c.pin_d5=Y7_GPIO_NUM; c.pin_d6=Y8_GPIO_NUM; c.pin_d7=Y9_GPIO_NUM;
  c.pin_xclk=XCLK_GPIO_NUM; c.pin_pclk=PCLK_GPIO_NUM;
  c.pin_vsync=VSYNC_GPIO_NUM; c.pin_href=HREF_GPIO_NUM;
  c.pin_sscb_sda=SIOD_GPIO_NUM; c.pin_sscb_scl=SIOC_GPIO_NUM;
  c.pin_pwdn=PWDN_GPIO_NUM; c.pin_reset=RESET_GPIO_NUM;
  c.fb_location = CAMERA_FB_IN_PSRAM;
}

bool initCamera(uint8_t quality, framesize_t fsize, pixformat_t fmt) {
  camera_config_t c; _camPins(c);
  c.xclk_freq_hz = 20000000;
  c.pixel_format = fmt;
  c.frame_size   = fsize;
  c.jpeg_quality = quality;
  c.fb_count     = 1;
  c.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;
  if (esp_camera_init(&c) != ESP_OK) return false;
  sensor_t *s = esp_camera_sensor_get();
  if (s) {
    s->set_whitebal(s,1); s->set_awb_gain(s,1);
    s->set_exposure_ctrl(s,1); s->set_aec2(s,1);
  }
  return true;
}

void captureAndSaveImage(const char *dateStr, const char *timeStr) {
  if (!cameraReady) { wlog("[IMG] Camera not ready — skip"); return; }

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    wlog("[IMG] fb_get failed");
    return;
  }

  uint8_t *jpgBuf = nullptr;
  size_t   jpgLen = 0;
  bool ok = frame2jpg(fb, cfg.jpeg_quality, &jpgBuf, &jpgLen);
  esp_camera_fb_return(fb);

  if (!ok || !jpgBuf || jpgLen == 0) {
    wlog("[IMG] frame2jpg failed");
    if (jpgBuf) free(jpgBuf);
    return;
  }

  char tNoColon[8];
  snprintf(tNoColon, sizeof(tNoColon), "%c%c%c%c%c%c",
    timeStr[0],timeStr[1],timeStr[3],timeStr[4],timeStr[6],timeStr[7]);
  char dateNoDash[9];
  snprintf(dateNoDash, sizeof(dateNoDash), "%c%c%c%c%c%c%c%c",
    dateStr[0],dateStr[1],dateStr[2],dateStr[3],
    dateStr[5],dateStr[6],dateStr[8],dateStr[9]);
  char imgPath[48];
  snprintf(imgPath, sizeof(imgPath), "%s/%s_%s.jpg", IMGS_DIR, dateNoDash, tNoColon);

  File f = FFat.open(imgPath, "w");
  if (f) {
    f.write(jpgBuf, jpgLen);
    f.close();
    imgCountToday++;
    wlogf("[IMG] Saved %s (%u bytes)\n", imgPath, (unsigned)jpgLen);
  } else {
    wlogf("[IMG] Cannot write %s\n", imgPath);
  }
  free(jpgBuf);
}

#define EXPECTED_FB_SIZE 38400UL   // QQVGA RGB565: 160×120×2
uint8_t captureBlueChannel() {
  if (!cameraReady) return 0;
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) return 0;
  if (fb->len != EXPECTED_FB_SIZE) {
    esp_camera_fb_return(fb);
    wlogf("[CAM] Unexpected fb len %u (expected %lu)\n", (unsigned)fb->len, EXPECTED_FB_SIZE);
    return 0;
  }
  uint32_t blueSum = 0;
  uint32_t samples = 0;
  for (uint32_t i = 0; i < fb->len; i += 32) {
    uint8_t b5 = fb->buf[i] & 0x1F;
    blueSum += (b5 << 3) | (b5 >> 2);
    samples++;
  }
  uint8_t mb = (samples > 0) ? (uint8_t)(blueSum / samples) : 0;
  esp_camera_fb_return(fb);
  return mb;
}

// ── FFat ─────────────────────────────────────────────────────────────────────
void ensureDirs() {
  if(!FFat.exists(DATA_DIR)) FFat.mkdir(DATA_DIR);
  if(!FFat.exists(IMGS_DIR)) FFat.mkdir(IMGS_DIR);
}

void openDayFile(const char *dateStr) {
  snprintf(currentFile,sizeof(currentFile),"%s/%s.csv",DATA_DIR,dateStr);
  strncpy(currentDateStr,dateStr,sizeof(currentDateStr));
  if(!FFat.exists(currentFile)){
    File f=FFat.open(currentFile,"w");
    if(f){f.println("date,time,elapsed_s,lux,irradiance_wm2,blue_channel,temp_c");f.close();}
  } else {
    repairDayFileTail(currentFile);
  }
  wlogf("[FS] Day file: %s\n",currentFile);
}

void flushWriteBuffer(bool force) {
  if(!writeBufCount||(writeBufCount<cfg.flush_count&&!force)) return;
  File f=FFat.open(currentFile,"a");
  if(!f){wlog("[FS] Flush failed");return;}
  for(uint8_t i=0;i<writeBufCount;i++){
    f.printf("%s,%s,%lu,%.2f,%.4f,%u,%.2f\n",
      writeBuf[i].date,writeBuf[i].time,writeBuf[i].elapsed_s,
      writeBuf[i].lux,writeBuf[i].irradiance_wm2,writeBuf[i].blue_channel,writeBuf[i].temp_c);
  }
  f.close();
  wlogf("[FS] Flushed %u samples\n",writeBufCount);
  writeBufCount=0;
}

void bufferSample(const Sample &s) {
  if(writeBufCount>=50) flushWriteBuffer(true);
  writeBuf[writeBufCount++]=s;
  if(writeBufCount>=cfg.flush_count) flushWriteBuffer(true);
}

void pushToLiveBuffer(const Sample &s) {
  liveBuffer[liveHead]=s;
  liveHead=(liveHead+1)%LIVE_BUFFER_SIZE;
  if(liveCount<LIVE_BUFFER_SIZE) liveCount++;
  totalSamples++;
}

void writeMeta(const char *dateStr, uint32_t rows) {
  char mp[48]; snprintf(mp,sizeof(mp),"%s/%s.meta",DATA_DIR,dateStr);
  File f=FFat.open(mp,"w");
  if(f){f.printf("expected_rows=%lu\n",rows);f.close();}
}

void writeDaySummary(const char *dateStr, uint32_t durationS) {
  if(!daySamples) return;
  double mean=dayIrrSum/daySamples;
  double var=(dayIrrSumSq/daySamples)-(mean*mean);
  double std=var>0?sqrt(var):0;
  double cvi=mean>0?std/mean:0;
  float mTemp=(float)(dayTempSum/daySamples);
  const char *sp="/data/summary.csv";
  bool exists=FFat.exists(sp);
  File f=FFat.open(sp,"a");
  if(!f) return;
  if(!exists) f.println("date,samples,duration_s,mean_irr,std_irr,cvi,peak_irr,mean_temp_c,thermal_events");
  f.printf("%s,%lu,%lu,%.4f,%.4f,%.4f,%.4f,%.2f,%lu\n",
    dateStr,daySamples,durationS,(float)mean,(float)std,(float)cvi,dayIrrPeak,mTemp,thermalEventCount);
  f.close();
  wlogf("[CVI] %s: mean=%.3f CVI=%.3f peak=%.3f thermalEvents=%lu\n",
        dateStr,(float)mean,(float)cvi,dayIrrPeak,thermalEventCount);
}

void resetDayAccumulators() {
  dayIrrSum=dayIrrSumSq=dayTempSum=0;
  dayIrrPeak=0; daySamples=0; prevLux=-1; nightCount=0; imgCountToday=0;
  thermalEventCount=0;
}

uint32_t countFileRows(const char *path) {
  File f=FFat.open(path,"r"); if(!f) return 0;
  uint32_t n=0; while(f.available()){f.readStringUntil('\n');n++;}
  f.close(); return n>1?n-1:0;
}

// ── WiFi + captive portal ─────────────────────────────────────────────────────
void startWiFi() {
  if(wifiActive) return;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID,AP_PASSWORD);
  IPAddress apIP(192,168,4,1);
  WiFi.softAPConfig(apIP,apIP,IPAddress(255,255,255,0));
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT,"*",apIP);
  if(MDNS.begin(MDNS_HOSTNAME)) MDNS.addService("http","tcp",80);
  server.begin();
  wifiActive=true;
  lastClientMs=millis();
  wifiHadClient=false;
  wlogf("[WiFi] AP: %s  192.168.4.1\n",AP_SSID);
}

void stopWiFi() {
  if(!wifiActive) return;
  dnsServer.stop(); server.stop(); MDNS.end();
  WiFi.softAPdisconnect(true); WiFi.mode(WIFI_OFF); esp_wifi_stop();
  wifiActive=false;
  wlog("[WiFi] Stopped");
}

void checkBtn(uint8_t pin, uint8_t &prevState, uint32_t &pressMs, bool &fired) {
  uint8_t s=digitalRead(pin);
  if(s==HIGH){ prevState=HIGH; fired=false; return; }
  if(prevState==HIGH) pressMs=millis();
  prevState=LOW;
  if(!fired && (millis()-pressMs>=WIFI_BTN_HOLD_MS)){
    fired=true;
    wifiActive?stopWiFi():startWiFi();
  }
}

void handleWiFiButtons() {
  checkBtn(WIFI_BTN1,btn1Prev,btn1PressMs,btn1Fired);
  checkBtn(WIFI_BTN2,btn2Prev,btn2PressMs,btn2Fired);
  if(wifiActive){
    if(WiFi.softAPgetStationNum()>0){
      lastClientMs=millis();
      if(!wifiHadClient) {
        wifiHadClient=true;
        tryNtpSync();
      }
    } else if(wifiHadClient && millis()-lastClientMs>(uint32_t)cfg.wifi_autooff_min*60000UL){
      wlog("[WiFi] Auto-off"); stopWiFi();
    }
  }
}

void handleWebServer() {
  if(!wifiActive) return;
  dnsServer.processNextRequest();
  server.handleClient();
  webLoggerLoop();
}

// ── Camera stream / snapshot ─────────────────────────────────────────────────

static bool     camStreamActive = false;
static uint32_t camStreamFrames = 0;
static uint32_t camStreamStartMs = 0;
static float    camStreamFps     = 0.0f;

static bool camEnterJpeg(uint8_t /*quality*/ = 10, framesize_t /*size*/ = FRAMESIZE_QVGA) {
  return cameraReady;
}
static void camRestoreRgb() {}

void handleCamSnapshot() {
  if (camStreamActive) {
    server.send(503,"text/plain","Stream active — stop stream first");
    return;
  }
  if (!cameraReady) {
    server.send(503,"text/plain","Camera not ready");
    return;
  }
  esp_task_wdt_reset();
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    server.send(503,"text/plain","Frame capture failed");
    return;
  }
  uint8_t *jpgBuf = nullptr;
  size_t   jpgLen = 0;
  bool ok = frame2jpg(fb, 10, &jpgBuf, &jpgLen);
  esp_camera_fb_return(fb);
  if (!ok || !jpgBuf) {
    server.send(503,"text/plain","JPEG encode failed");
    if (jpgBuf) free(jpgBuf);
    return;
  }
  server.sendHeader("Access-Control-Allow-Origin","*");
  server.sendHeader("Cache-Control","no-store");
  server.send_P(200, "image/jpeg", (const char*)jpgBuf, jpgLen);
  free(jpgBuf);
  esp_task_wdt_reset();
  wlog("[CAM] Snapshot served");
}

void handleCamStream() {
  if (camStreamActive) {
    server.send(503,"text/plain","Already streaming");
    return;
  }
  if (!cameraReady) {
    server.send(503,"text/plain","Camera not ready");
    return;
  }
  camStreamActive = true;
  camStreamFrames = 0;
  camStreamStartMs = millis();
  wlog("[CAM] MJPEG stream start (RGB565→SW-JPEG)");

  WiFiClient client = server.client();
  client.print("HTTP/1.1 200 OK\r\n"
               "Content-Type: multipart/x-mixed-replace; boundary=--jpgbound\r\n"
               "Access-Control-Allow-Origin: *\r\n"
               "Cache-Control: no-store\r\n"
               "Connection: close\r\n\r\n");

  uint32_t lastFpsMs = millis();
  uint32_t fpsCnt = 0;

  while (client.connected()) {
    esp_task_wdt_reset();
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) { delay(20); continue; }

    uint8_t *jpgBuf = nullptr;
    size_t   jpgLen = 0;
    bool ok = frame2jpg(fb, 12, &jpgBuf, &jpgLen);
    esp_camera_fb_return(fb);

    if (!ok || !jpgBuf) {
      if (jpgBuf) free(jpgBuf);
      delay(20);
      continue;
    }

    char hdr[100];
    snprintf(hdr, sizeof(hdr),
      "--jpgbound\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
      (unsigned)jpgLen);
    client.print(hdr);
    client.write(jpgBuf, jpgLen);
    client.print("\r\n");
    free(jpgBuf);

    camStreamFrames++;
    fpsCnt++;
    uint32_t now = millis();
    if (now - lastFpsMs >= 1000) {
      camStreamFps = fpsCnt * 1000.0f / (now - lastFpsMs);
      fpsCnt = 0; lastFpsMs = now;
    }
    delay(1);
  }

  camStreamActive = false;
  wlogf("[CAM] MJPEG stream end — %lu frames, %.1f fps avg\n",
    camStreamFrames,
    camStreamFrames * 1000.0f / max(1UL, millis() - camStreamStartMs));
}

void handleCamStats() {
  char buf[80];
  snprintf(buf, sizeof(buf),
    "{\"active\":%s,\"frames\":%lu,\"fps\":%.1f,\"ready\":%s}",
    camStreamActive ? "true" : "false",
    camStreamFrames,
    camStreamFps,
    cameraReady ? "true" : "false");
  server.sendHeader("Access-Control-Allow-Origin","*");
  server.send(200,"application/json",buf);
}

static bool   otaInProgress = false;
static size_t otaWritten    = 0;
static size_t otaTotal      = 0;

void handleOtaPage() {
  server.sendHeader("Cache-Control","no-cache");
  server.send(200,"text/html",
    "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Helios OTA Update</title>"
    "<style>"
    ":root{--teal:#007A7A;--bg:#F0F4F8;--card:#fff;--border:#C8D6E0;"
    "--text:#1A2530;--dim:#4A6070;--green:#1A7A45;--red:#B82222;--amber:#B86800}"
    "*{box-sizing:border-box;margin:0;padding:0}"
    "body{font-family:system-ui,sans-serif;background:var(--bg);min-height:100vh;"
    "display:flex;align-items:center;justify-content:center;padding:16px}"
    ".card{background:var(--card);border:2px solid var(--border);border-radius:12px;"
    "padding:32px;width:100%;max-width:460px;box-shadow:0 2px 12px rgba(0,0,0,.08)}"
    "h1{font-family:'Courier New';color:var(--teal);font-size:20px;margin-bottom:6px}"
    ".sub{color:var(--dim);font-size:13px;margin-bottom:24px}"
    ".drop{border:2px dashed var(--border);border-radius:10px;padding:36px 20px;"
    "text-align:center;cursor:pointer;transition:all .2s;background:var(--bg);"
    "margin-bottom:18px}"
    ".drop.over{border-color:var(--teal);background:#E0F4F4}"
    ".drop.has-file{border-color:var(--teal);border-style:solid}"
    ".drop-icon{font-size:36px;margin-bottom:10px}"
    ".drop-label{font-family:'Courier New';font-size:13px;color:var(--dim)}"
    ".file-name{font-family:'Courier New';font-size:14px;font-weight:700;"
    "color:var(--teal);margin-top:8px;word-break:break-all}"
    ".btn{width:100%;padding:14px;background:var(--teal);color:#fff;border:none;"
    "border-radius:8px;font-family:'Courier New';font-size:15px;font-weight:700;"
    "cursor:pointer;transition:background .15s}"
    ".btn:hover{background:#005555}"
    ".btn:disabled{background:#90B8B8;cursor:not-allowed}"
    ".progress-wrap{margin-top:18px;display:none}"
    ".progress-label{font-family:'Courier New';font-size:12px;color:var(--dim);"
    "display:flex;justify-content:space-between;margin-bottom:6px}"
    ".progress-track{height:10px;background:var(--border);border-radius:5px;overflow:hidden}"
    ".progress-fill{height:100%;background:var(--teal);border-radius:5px;"
    "transition:width .3s;width:0%}"
    ".status{margin-top:16px;font-family:'Courier New';font-size:13px;"
    "text-align:center;min-height:20px}"
    ".status.ok{color:var(--green);font-weight:700}"
    ".status.err{color:var(--red);font-weight:700}"
    ".status.info{color:var(--dim)}"
    ".back{display:block;text-align:center;margin-top:18px;font-family:'Courier New';"
    "font-size:13px;color:var(--teal);text-decoration:none}"
    "</style></head>"
    "<body><div class='card'>"
    "<h1>&#9889; HELIOS OTA</h1>"
    "<p class='sub'>Upload a compiled .bin firmware file to update Helios in-place.<br>"
    "The device will reboot automatically after flashing.</p>"
    "<div class='drop' id='drop'>"
    "<div class='drop-icon'>&#128190;</div>"
    "<div class='drop-label'>Drag &amp; drop .bin here<br>or click to browse</div>"
    "<div class='file-name' id='fname'></div>"
    "</div>"
    "<input type='file' id='file' accept='.bin' style='display:none'>"
    "<button class='btn' id='btn' disabled onclick='upload()'>Flash Firmware</button>"
    "<div class='progress-wrap' id='prog'>"
    "<div class='progress-label'><span>Uploading &amp; flashing\u2026</span><span id='pct'>0%</span></div>"
    "<div class='progress-track'><div class='progress-fill' id='pfill'></div></div>"
    "</div>"
    "<div class='status info' id='status'>Select a .bin file to begin.</div>"
    "<a class='back' href='/'>&#8592; Back to Dashboard</a>"
    "</div>"
    "<script>"
    "const drop=document.getElementById('drop'),"
    "      finput=document.getElementById('file'),"
    "      btn=document.getElementById('btn'),"
    "      fname=document.getElementById('fname'),"
    "      prog=document.getElementById('prog'),"
    "      pfill=document.getElementById('pfill'),"
    "      pct=document.getElementById('pct'),"
    "      status=document.getElementById('status');"
    "let selFile=null;"
    "drop.addEventListener('click',()=>finput.click());"
    "drop.addEventListener('dragover',e=>{e.preventDefault();drop.classList.add('over');});"
    "drop.addEventListener('dragleave',()=>drop.classList.remove('over'));"
    "drop.addEventListener('drop',e=>{"
    "  e.preventDefault();drop.classList.remove('over');"
    "  const f=e.dataTransfer.files[0];"
    "  if(f)setFile(f);"
    "});"
    "finput.addEventListener('change',()=>{if(finput.files[0])setFile(finput.files[0]);});"
    "function setFile(f){"
    "  if(!f.name.endsWith('.bin')){"
    "    status.className='status err';status.textContent='Error: must be a .bin file.';"
    "    return;"
    "  }"
    "  selFile=f;"
    "  fname.textContent=f.name+' ('+Math.round(f.size/1024)+' KB)';"
    "  drop.classList.add('has-file');"
    "  btn.disabled=false;"
    "  status.className='status info';"
    "  status.textContent='Ready. Press Flash Firmware to begin.';"
    "}"
    "function upload(){"
    "  if(!selFile)return;"
    "  btn.disabled=true;"
    "  prog.style.display='block';"
    "  status.className='status info';status.textContent='Uploading\u2026';"
    "  const fd=new FormData();"
    "  fd.append('firmware',selFile,selFile.name);"
    "  const xhr=new XMLHttpRequest();"
    "  xhr.open('POST','/api/ota-upload');"
    "  xhr.upload.onprogress=e=>{"
    "    if(e.lengthComputable){"
    "      const p=Math.round(e.loaded/e.total*100);"
    "      pfill.style.width=p+'%';pct.textContent=p+'%';"
    "    }"
    "  };"
    "  xhr.onload=()=>{"
    "    if(xhr.status===200){"
    "      pfill.style.width='100%';pct.textContent='100%';"
    "      status.className='status ok';"
    "      status.textContent='\\u2713 Flash complete! Rebooting\u2026 reconnect in 10 s.';"
    "    } else {"
    "      status.className='status err';"
    "      status.textContent='Error: '+xhr.responseText;"
    "      btn.disabled=false;"
    "    }"
    "  };"
    "  xhr.onerror=()=>{"
    "    status.className='status err';"
    "    status.textContent='Network error during upload.';"
    "    btn.disabled=false;"
    "  };"
    "  xhr.send(fd);"
    "}"
    "</script></body></html>");
}

void handleOtaUpload() {
  HTTPUpload &upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    otaWritten  = 0;
    otaTotal    = 0;
    otaInProgress = true;
    wlogf("[OTA] Start: %s  size=%u\n", upload.filename.c_str(), upload.totalSize);
    esp_task_wdt_delete(NULL);
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      wlogf("[OTA] begin() failed: %s\n", Update.errorString());
      otaInProgress = false;
    }

  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (otaInProgress) {
      size_t written = Update.write(upload.buf, upload.currentSize);
      otaWritten += written;
      otaTotal   += upload.currentSize;
      if (written != upload.currentSize) {
        wlogf("[OTA] write mismatch: wrote %u of %u\n", written, upload.currentSize);
        otaInProgress = false;
      }
    }

  } else if (upload.status == UPLOAD_FILE_END) {
    if (otaInProgress && Update.end(true)) {
      wlogf("[OTA] Done — %u bytes written. Rebooting...\n", otaWritten);
    } else {
      wlogf("[OTA] end() failed: %s\n", Update.errorString());
      otaInProgress = false;
    }
  }
}

void handleOtaUploadFinish() {
  if (Update.hasError()) {
    server.sendHeader("Access-Control-Allow-Origin","*");
    server.send(500,"text/plain", String("Flash failed: ") + Update.errorString());
    otaInProgress = false;
  } else {
    server.sendHeader("Connection","close");
    server.sendHeader("Access-Control-Allow-Origin","*");
    server.send(200,"text/plain","OK");
    delay(200);
    ESP.restart();
  }
}
void handleRoot() {
  if(!cfg.setup_done){ 
    server.sendHeader("Location","http://192.168.4.1/setup",true); 
    server.send(302,"text/plain",""); 
    return; 
  }
  server.sendHeader("Cache-Control","no-cache"); 
  server.send_P(200,"text/html",DASHBOARD_HTML);
}

void handleSettings() { 
  server.sendHeader("Cache-Control","no-cache"); 
  server.send_P(200,"text/html",SETTINGS_HTML); 
}

void handleCaptive()  { 
  server.sendHeader("Location","http://192.168.4.1/",true); 
  server.send(302,"text/plain",""); 
}

void handleSetupPage() {
  server.sendHeader("Cache-Control","no-cache");
  server.send_P(200,"text/html",SETUP_HTML);
}

void handleStatus() {
  char ds[12],ts[10]; getRTCStrings(ds,ts);
  updateSunStrings();
  if(isLogging&&cameraReady){
    uint32_t msUntil=((uint32_t)cfg.img_interval_min*60000UL)-(millis()-lastImgMs);
    uint32_t secUntil=msUntil/1000;
    snprintf(nextImgStr,sizeof(nextImgStr),"%lum%lus",secUntil/60,secUntil%60);
  } else { strcpy(nextImgStr,"--:--"); }

  size_t tot=FFat.totalBytes(),used=FFat.usedBytes();
  float dieT = tsens_handle ? readDieTemp() : -99.0f;
  String latJ="null";
  if(liveCount>0){
    uint16_t idx=(liveHead==0)?LIVE_BUFFER_SIZE-1:liveHead-1;
    Sample &ls=liveBuffer[idx];
    char buf[300];
    snprintf(buf,sizeof(buf),
      "{\"elapsed_s\":%lu,\"date\":\"%s\",\"time\":\"%s\","
      "\"lux\":%.2f,\"irradiance_wm2\":%.4f,\"blue_channel\":%u,\"temp_c\":%.2f}",
      ls.elapsed_s,ls.date,ls.time,ls.lux,ls.irradiance_wm2,ls.blue_channel,ls.temp_c);
    latJ=String(buf);
  }
  char json[1100];
  snprintf(json,sizeof(json),
    "{\"logging\":%s,\"current_file\":\"%s\","
    "\"total_samples\":%lu,\"rejected\":%lu,\"buf_pending\":%u,"
    "\"rtc_ready\":%s,\"rtc_date\":\"%s\",\"rtc_time\":\"%s\","
    "\"sunrise\":\"%s\",\"sunset\":\"%s\","
    "\"next_img\":\"%s\",\"img_count\":%lu,"
    "\"fs_total_kb\":%u,\"fs_used_kb\":%u,\"fs_free_kb\":%u,"
    "\"wifi_active\":%s,\"clients\":%u,"
    "\"die_temp_c\":%.2f,"
    "\"temp_shutdown_c\":%.1f,\"in_thermal_sleep\":%s,"
    "\"thermal_events\":%lu,\"ntp_synced\":%s,\"manual_override\":%s,"
    "\"latest\":%s}",
    isLogging?"true":"false",currentFile,
    totalSamples,rejectedCount,(unsigned)writeBufCount,
    rtcReady?"true":"false",ds,ts,
    sunriseStr,sunsetStr,nextImgStr,imgCountToday,
    (unsigned)(tot/1024),(unsigned)(used/1024),(unsigned)((tot-used)/1024),
    wifiActive?"true":"false",(unsigned)WiFi.softAPgetStationNum(),
    dieT,
    cfg.temp_shutdown_c, inThermalSleep?"true":"false",
    thermalEventCount, ntpSynced?"true":"false",
    manualOverride?"true":"false",
    latJ.c_str());
  server.sendHeader("Access-Control-Allow-Origin","*");
  server.send(200,"application/json",json);
}

void handleLive() {
  uint16_t count=server.arg("count").toInt();
  if(!count||count>LIVE_BUFFER_SIZE) count=60;
  if(count>liveCount) count=liveCount;
  server.sendHeader("Access-Control-Allow-Origin","*");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200,"application/json","");
  server.sendContent("[");
  for(uint16_t i=0;i<count;i++){
    uint16_t idx=(liveHead-count+i+LIVE_BUFFER_SIZE)%LIVE_BUFFER_SIZE;
    Sample &s=liveBuffer[idx];
    char buf[256];
    snprintf(buf,sizeof(buf),
      "{\"t\":%lu,\"date\":\"%s\",\"time\":\"%s\","
      "\"lux\":%.2f,\"irr\":%.4f,\"blue\":%u,\"temp\":%.2f}%s",
      s.elapsed_s,s.date,s.time,s.lux,s.irradiance_wm2,s.blue_channel,s.temp_c,
      i<count-1?",":"");
    server.sendContent(buf);
  }
  server.sendContent("]");
}

void handleFiles() {
  server.sendHeader("Access-Control-Allow-Origin","*");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200,"application/json","");
  server.sendContent("[");
  File root=FFat.open(DATA_DIR); bool first=true;
  if(root&&root.isDirectory()){
    File f=root.openNextFile();
    while(f){
      if(!f.isDirectory()){
        const char *nm = (char*)f.name();
        char fp[64]; snprintf(fp,sizeof(fp),"%s/%s",DATA_DIR,nm);
        const char *ext = strrchr(nm, '.');
        bool isCsv   = ext && strcmp(ext,".csv")==0;
        bool isMeta  = ext && strcmp(ext,".meta")==0;
        bool isTherm = ext && strcmp(ext,".therm")==0;
        uint32_t rows = (isCsv||isTherm) ? countFileRows(fp) : 0;
        char buf[200];
        snprintf(buf,sizeof(buf),
          "%s{\"name\":\"%s\",\"size\":%u,\"samples\":%lu,\"type\":\"%s\"}",
          first?"":",",(char*)nm,(unsigned)f.size(),rows,
          isCsv?"csv":isTherm?"therm":isMeta?"meta":"other");
        server.sendContent(buf); first=false;
      }
      f=root.openNextFile();
    }
  }
  server.sendContent("]");
}

void handleImages() {
  server.sendHeader("Access-Control-Allow-Origin","*");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200,"application/json","");
  server.sendContent("[");
  File root=FFat.open(IMGS_DIR); bool first=true;
  if(root&&root.isDirectory()){
    File f=root.openNextFile();
    while(f){
      if(!f.isDirectory()){
        char buf[120];
        snprintf(buf,sizeof(buf),"%s{\"name\":\"%s\",\"size\":%u}",
          first?"":",",(char*)f.name(),(unsigned)f.size());
        server.sendContent(buf); first=false;
      }
      f=root.openNextFile();
    }
  }
  server.sendContent("]");
}

void handleImg() {
  String fn=server.arg("file");
  if(!fn.length()){server.send(400,"text/plain","Missing file");return;}
  String path=String(IMGS_DIR)+"/"+fn;
  if(!FFat.exists(path)){server.send(404,"text/plain","Not found");return;}
  File f=FFat.open(path,"r");
  if(!f){server.send(500,"text/plain","Open failed");return;}
  server.sendHeader("Cache-Control","max-age=3600");
  server.streamFile(f,"image/jpeg"); f.close();
}

void handleDownload() {
  String fn=server.arg("file");
  if(!fn.length()){server.send(400,"text/plain","Missing file");return;}
  String path=(fn=="summary.csv")?"/data/summary.csv":String(DATA_DIR)+"/"+fn;
  if(!FFat.exists(path)){server.send(404,"text/plain","Not found");return;}
  File f=FFat.open(path,"r");
  if(!f){server.send(500,"text/plain","Open failed");return;}
  server.sendHeader("Content-Disposition","attachment; filename=\""+fn+"\"");
  server.streamFile(f,"text/csv"); f.close();
}

void handleDelete() {
  String fn=server.arg("file");
  if(!fn.length()){server.send(400,"text/plain","Missing file");return;}
  server.send(200,"application/json",FFat.remove(String(DATA_DIR)+"/"+fn)?"{\"ok\":true}":"{\"ok\":false}");
}

void handleDeleteAll() {
  File root=FFat.open(DATA_DIR);
  if(root&&root.isDirectory()){
    File f=root.openNextFile();
    while(f){if(!f.isDirectory())FFat.remove(String(DATA_DIR)+"/"+f.name());f=root.openNextFile();}
  }
  root=FFat.open(IMGS_DIR);
  if(root&&root.isDirectory()){
    File f=root.openNextFile();
    while(f){if(!f.isDirectory())FFat.remove(String(IMGS_DIR)+"/"+f.name());f=root.openNextFile();}
  }
  totalSamples=0;liveCount=0;liveHead=0;rejectedCount=0;writeBufCount=0;
  resetDayAccumulators();
  server.send(200,"application/json","{\"ok\":true}");
}

void handleSetTime() {
  String da=server.arg("date"),ti=server.arg("time");
  if(da.length()<10||ti.length()<8){server.send(400,"application/json","{\"ok\":false,\"error\":\"bad params\"}");return;}
  if(!rtcReady){server.send(500,"application/json","{\"ok\":false,\"error\":\"RTC not ready\"}");return;}
  rtc.adjust(DateTime(da.substring(0,4).toInt(),da.substring(5,7).toInt(),da.substring(8,10).toInt(),
                      ti.substring(0,2).toInt(),ti.substring(3,5).toInt(),ti.substring(6,8).toInt()));
  wlogf("[RTC] Set to %s %s\n",da.c_str(),ti.c_str());
  server.sendHeader("Access-Control-Allow-Origin","*");
  server.send(200,"application/json","{\"ok\":true}");
}

void handleGetConfig() {
  char json[480];
  snprintf(json,sizeof(json),
    "{\"sample_interval_s\":%lu,\"img_interval_min\":%lu,"
    "\"jpeg_quality\":%u,\"flush_count\":%u,"
    "\"wifi_autooff_min\":%lu,\"outlier_factor\":%.1f,"
    "\"night_confirm\":%u,"
    "\"temp_shutdown_c\":%.1f,\"temp_sleep_s\":%u,"
    "\"setup_done\":%s,"
    "\"lat\":%.6f,\"lon\":%.6f,\"utc_offset\":%.2f,"
    "\"deploy_days\":%u,\"deploy_yr\":%u,\"deploy_mo\":%u,\"deploy_dy\":%u}",
    cfg.sample_interval_s,cfg.img_interval_min,cfg.jpeg_quality,
    cfg.flush_count,cfg.wifi_autooff_min,cfg.outlier_factor,cfg.night_confirm,
    cfg.temp_shutdown_c,cfg.temp_sleep_s,
    cfg.setup_done?"true":"false",
    cfg.lat,cfg.lon,cfg.utc_offset,
    cfg.deploy_days,cfg.deploy_yr,cfg.deploy_mo,cfg.deploy_dy);
  server.sendHeader("Access-Control-Allow-Origin","*");
  server.send(200,"application/json",json);
}

void handlePostConfig() {
  if(!server.hasArg("plain")){server.send(400,"application/json","{\"ok\":false,\"error\":\"no body\"}");return;}
  String body=server.arg("plain");
  auto getInt=[&](const char *k,uint32_t def)->uint32_t{
    String key="\"";key+=k;key+="\":";
    int idx=body.indexOf(key);
    return idx<0?def:(uint32_t)body.substring(idx+key.length()).toInt();
  };
  auto getFloat=[&](const char *k,float def)->float{
    String key="\"";key+=k;key+="\":";
    int idx=body.indexOf(key);
    return idx<0?def:body.substring(idx+key.length()).toFloat();
  };
  cfg.sample_interval_s = getInt("sample_interval_s",10);
  cfg.img_interval_min  = getInt("img_interval_min",3);
  cfg.jpeg_quality      = (uint8_t)getInt("jpeg_quality",5);
  cfg.flush_count       = (uint8_t)getInt("flush_count",10);
  cfg.wifi_autooff_min  = getInt("wifi_autooff_min",5);
  cfg.outlier_factor    = getFloat("outlier_factor",10.0f);
  cfg.night_confirm     = (uint8_t)getInt("night_confirm",5);
  cfg.temp_shutdown_c   = getFloat("temp_shutdown_c",75.0f);
  cfg.temp_sleep_s      = (uint16_t)getInt("temp_sleep_s",120);
  saveConfig();
  wlog("[CFG] Updated via settings page");
  server.sendHeader("Access-Control-Allow-Origin","*");
  server.send(200,"application/json","{\"ok\":true}");
}

void handleSetupPost() {
  if(!server.hasArg("plain")){server.send(400,"application/json","{\"ok\":false,\"error\":\"no body\"}");return;}
  String body=server.arg("plain");
  auto getDouble=[&](const char *k,double def)->double{
    String key="\"";key+=k;key+="\":";
    int idx=body.indexOf(key);
    return idx<0?def:(double)body.substring(idx+key.length()).toDouble();
  };
  auto getFloat=[&](const char *k,float def)->float{
    String key="\"";key+=k;key+="\":";
    int idx=body.indexOf(key);
    return idx<0?def:body.substring(idx+key.length()).toFloat();
  };
  auto getInt=[&](const char *k,uint32_t def)->uint32_t{
    String key="\"";key+=k;key+="\":";
    int idx=body.indexOf(key);
    return idx<0?def:(uint32_t)body.substring(idx+key.length()).toInt();
  };
  cfg.lat         = getDouble("lat",   24.9);
  cfg.lon         = getDouble("lon",   91.9);
  cfg.utc_offset  = getFloat("utc_offset", 6.0f);
  cfg.deploy_days = (uint16_t)getInt("deploy_days", 30);
  cfg.deploy_yr   = (uint16_t)getInt("deploy_yr",   2026);
  cfg.deploy_mo   = (uint8_t)getInt("deploy_mo",    6);
  cfg.deploy_dy   = (uint8_t)getInt("deploy_dy",    1);
  cfg.setup_done  = true;
  saveConfig();
  wlogf("[SETUP] Done: lat=%.4f lon=%.4f utc=%.1f days=%u start=%u-%02u-%02u\n",
    cfg.lat, cfg.lon, cfg.utc_offset, cfg.deploy_days,
    cfg.deploy_yr, cfg.deploy_mo, cfg.deploy_dy);
  String da=server.arg("date"),ti=server.arg("time");
  if(rtcReady && da.length()>=10 && ti.length()>=8){
    rtc.adjust(DateTime(
      da.substring(0,4).toInt(), da.substring(5,7).toInt(), da.substring(8,10).toInt(),
      ti.substring(0,2).toInt(), ti.substring(3,5).toInt(), ti.substring(6,8).toInt()));
    wlogf("[RTC] Set via setup: %s %s\n", da.c_str(), ti.c_str());
  }
  updateSunStrings();
  server.sendHeader("Access-Control-Allow-Origin","*");
  server.send(200,"application/json","{\"ok\":true}");
}

void handleOverride() {
  if (server.hasArg("on")) {
    manualOverride = (server.arg("on") == "1");
    wlogf("[OVERRIDE] Manual logging override: %s\n", manualOverride ? "ON" : "OFF");
    if (manualOverride && !isLogging) {
      isLogging = true; dayStartMs = millis();
      nightCount = 0; liveCount = 0; liveHead = 0; writeBufCount = 0;
      lastImgMs = millis();
      resetDayAccumulators();
      char ds[12], ts[10]; getRTCStrings(ds, ts);
      openDayFile(ds);
      updateSunStrings();
      wlogf("[LOG] Day START (manual override) %s %s\n", ds, ts);
    }
    if (!manualOverride && isLogging && !isDaytime()) {
      flushWriteBuffer(true);
      uint32_t dur = (millis() - dayStartMs) / 1000UL;
      writeMeta(currentDateStr, daySamples);
      writeDaySummary(currentDateStr, dur);
      wlogf("[LOG] Day END (override off) — %lus accepted:%lu\n", dur, daySamples);
      isLogging = false; nightCount = 0;
    }
  }
  char buf[48];
  snprintf(buf, sizeof(buf), "{\"ok\":true,\"manual_override\":%s}",
           manualOverride ? "true" : "false");
  server.sendHeader("Access-Control-Allow-Origin","*");
  server.send(200, "application/json", buf);
}

void handleResetSetup() {
  cfg.setup_done = false;
  saveConfig();
  wlog("[SETUP] Reset — will show wizard on next WiFi connect");
  server.sendHeader("Access-Control-Allow-Origin","*");
  server.send(200,"application/json","{\"ok\":true}");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  wlogInit();
  wlog("\n[HELIOS] v2.6 booting (no-deinit camera fix)...");

  initStatusLed();
  logBootReason();
  initWatchdog();

  if(!FFat.begin(true, "/ffat", 20)){
    wlog("[FS] FATAL: FFat mount failed"); while(1)delay(1000);
  }
  wlogf("[FS] Mounted (FFat) — Total:%uMB Used:%uKB\n",
    (unsigned)(FFat.totalBytes()/1048576),
    (unsigned)(FFat.usedBytes()/1024));
  ensureDirs();
  loadConfig();
  esp_task_wdt_reset();

  initTempSensor();

  Wire.begin(BH1750_SDA,BH1750_SCL);
  configureBH1750(); delay(200);
  float tl=lightMeter.readLightLevel();
  if(tl<0) wlog("[BH1750] Warning: check SDA->GPIO45 SCL->GPIO46");
  else      wlogf("[BH1750] Ready — %.1f lux\n",tl);

  if(initCamera(12,FRAMESIZE_QQVGA,PIXFORMAT_RGB565)){
    sensor_t *s=esp_camera_sensor_get();
    if(s){s->set_whitebal(s,1);s->set_awb_gain(s,1);s->set_exposure_ctrl(s,1);s->set_aec2(s,1);}
    cameraReady=true;
    wlog("[CAM] Ready (RGB565 QQVGA)");
  } else {
    wlog("[CAM] Init failed — lux-only");
  }
  esp_task_wdt_reset();

  initRTC();
  updateSunStrings();
  esp_task_wdt_reset();

  pinMode(WIFI_BTN1,INPUT_PULLUP);
  pinMode(WIFI_BTN2,INPUT_PULLUP);

  server.on("/", HTTP_GET, [](){
    if(!cfg.setup_done){
      server.sendHeader("Location","http://192.168.4.1/setup",true);
      server.send(302,"text/plain","");
    } else {
      server.sendHeader("Cache-Control","no-cache");
      server.send_P(200,"text/html",DASHBOARD_HTML);
    }
  });
  server.on("/setup",                     HTTP_GET,  handleSetupPage);
  server.on("/api/setup",                 HTTP_POST, handleSetupPost);
  server.on("/api/reset-setup",           HTTP_POST, handleResetSetup);
  server.on("/api/override",              HTTP_GET,  handleOverride);
  server.on("/ota",                       HTTP_GET,  handleOtaPage);
  server.on("/api/ota-upload",            HTTP_POST, handleOtaUploadFinish, handleOtaUpload);
  server.on("/api/cam-snapshot",          HTTP_GET,  handleCamSnapshot);
  server.on("/api/cam-stream",            HTTP_GET,  handleCamStream);
  server.on("/api/cam-stats",             HTTP_GET,  handleCamStats);
  registerWebLogger(server);
  server.on("/settings",                  HTTP_GET,  handleSettings);
  server.on("/api/status",                HTTP_GET,  handleStatus);
  server.on("/api/live",                  HTTP_GET,  handleLive);
  server.on("/api/files",                 HTTP_GET,  handleFiles);
  server.on("/api/images",                HTTP_GET,  handleImages);
  server.on("/api/settime",               HTTP_GET,  handleSetTime);
  server.on("/api/config",                HTTP_GET,  handleGetConfig);
  server.on("/api/config",                HTTP_POST, handlePostConfig);
  server.on("/download",                  HTTP_GET,  handleDownload);
  server.on("/img",                       HTTP_GET,  handleImg);
  server.on("/api/delete",                HTTP_DELETE,handleDelete);
  server.on("/api/deleteall",             HTTP_DELETE,handleDeleteAll);
  server.on("/generate_204",              HTTP_GET,  handleCaptive);
  server.on("/connecttest.txt",           HTTP_GET,  handleCaptive);
  server.on("/hotspot-detect.html",       HTTP_GET,  handleCaptive);
  server.on("/library/test/success.html", HTTP_GET,  handleCaptive);
  server.on("/ncsi.txt",                  HTTP_GET,  handleCaptive);
  server.on("/redirect",                  HTTP_GET,  handleCaptive);
  server.on("/canonical.html",            HTTP_GET,  handleCaptive);
  server.onNotFound([](){
    if(!cfg.setup_done){
      server.sendHeader("Location","http://192.168.4.1/setup",true);
      server.send(302,"text/plain","");
    } else {
      handleCaptive();
    }
  });

  startWiFi();

  char ds[12],ts[10]; getRTCStrings(ds,ts);
  wlogf("[HELIOS] Boot complete — %s %s\n",ds,ts);
  wlogf("[HELIOS] Sunrise:%s Sunset:%s\n",sunriseStr,sunsetStr);
  wlog("[HELIOS] Hold BOOT or GPIO3 button 2s to toggle WiFi AP");
}

void loop() {
  esp_task_wdt_reset();
  handleWiFiButtons();
  handleWebServer();
  tickLed();
  updateLedState();

  uint32_t now=millis();

  if (cfg.temp_shutdown_c > 0.0f) {
    float dieT = readDieTemp();
    if (inThermalSleep) {
      float resumeThresh = cfg.temp_shutdown_c - 10.0f;
      bool timerDone = (now >= thermalSleepEndMs);
      bool coolEnough = (dieT <= resumeThresh);
      if (timerDone && coolEnough) {
        inThermalSleep = false;
        wlogf("[THERM] Cooldown complete — %.1f°C (resume at <=%.1f°C)\n",
              dieT, resumeThresh);
        if (thermalCamWasReady) {
          if (initCamera(12, FRAMESIZE_QQVGA, PIXFORMAT_RGB565)) {
            sensor_t *s = esp_camera_sensor_get();
            if (s){s->set_whitebal(s,1);s->set_awb_gain(s,1);
                   s->set_exposure_ctrl(s,1);s->set_aec2(s,1);}
            cameraReady = true;
            wlog("[THERM] Camera restored");
          } else {
            wlog("[THERM] Camera restore failed — continuing without");
          }
        }
        if (thermalWifiWasOn) { startWiFi(); thermalWifiWasOn = false; }
      } else {
        esp_task_wdt_reset();
        tickLed(); setLedMode(LED_FAST_BLINK);
        uint32_t remaining = timerDone ? 500 : (thermalSleepEndMs - now);
        uint32_t sleepUs = min(remaining, (uint32_t)500) * 1000ULL;
        esp_sleep_enable_timer_wakeup(sleepUs);
        esp_light_sleep_start();
        return;
      }
    } else if (dieT >= cfg.temp_shutdown_c) {
      char ds[12], ts[10]; getRTCStrings(ds, ts);
      wlogf("[THERM] Die temp %.1f°C >= %.1f°C — cooling down for %us\n",
            dieT, cfg.temp_shutdown_c, cfg.temp_sleep_s);
      flushWriteBuffer(true);
      writeThermalEvent(ds, ts, dieT);
      thermalEventCount++;
      thermalCamWasReady = cameraReady;
      if (cameraReady) { esp_camera_deinit(); cameraReady = false; wlog("[THERM] Camera powered down"); }
      thermalWifiWasOn = wifiActive;
      if (wifiActive) { stopWiFi(); wlog("[THERM] WiFi stopped for cooldown"); }
      inThermalSleep    = true;
      thermalSleepEndMs = now + (uint32_t)cfg.temp_sleep_s * 1000UL;
      setLedMode(LED_FAST_BLINK);
      return;
    }
  }

  bool daytime = isDaytime();

  if(!isLogging && daytime) {
    isLogging=true; dayStartMs=now;
    nightCount=0; liveCount=0; liveHead=0; writeBufCount=0;
    lastImgMs=now;
    resetDayAccumulators();
    char ds[12],ts[10]; getRTCStrings(ds,ts);
    openDayFile(ds);
    updateSunStrings();
    wlogf("[LOG] Day START %s %s (sunrise:%s sunset:%s)\n",ds,ts,sunriseStr,sunsetStr);
  }

  if(isLogging && !daytime) {
    nightCount++;
    if(nightCount>=(uint8_t)cfg.night_confirm){
      isLogging=false;
      flushWriteBuffer(true);
      uint32_t dur=(now-dayStartMs)/1000UL;
      writeMeta(currentDateStr,daySamples);
      writeDaySummary(currentDateStr,dur);
      wlogf("[LOG] Day END %s — %lus accepted:%lu rejected:%lu images:%lu\n",
        currentDateStr,dur,daySamples,rejectedCount,imgCountToday);
      nightCount=0;
    }
  } else if(isLogging && daytime) {
    nightCount=0;
  }

  if(now-lastSampleMs >= cfg.sample_interval_s*1000UL) {
    lastSampleMs=now;

    float lux=0; bool luxOk=readBH1750(lux);
    char ds[12],ts[10]; getRTCStrings(ds,ts);

    if(luxOk&&isLogging){
      if(!isValidReading(lux)){
        rejectedCount++;
        wlogf("[FILTER] Rejected lux=%.1f prev=%.1f #%lu\n",lux,prevLux,rejectedCount);
        lux=prevLux;
      } else { prevLux=lux; }
    } else if(luxOk) { prevLux=lux; }

    if(isLogging){
      Sample s;
      s.elapsed_s=(now-dayStartMs)/1000UL;
      strncpy(s.date,ds,sizeof(s.date));
      strncpy(s.time,ts,sizeof(s.time));
      s.lux=lux;
      s.irradiance_wm2=lux*LUX_TO_WM2;
      s.blue_channel=captureBlueChannel();
      s.temp_c=readDieTemp();
      dayIrrSum+=s.irradiance_wm2; dayIrrSumSq+=(double)s.irradiance_wm2*s.irradiance_wm2;
      if(s.irradiance_wm2>dayIrrPeak) dayIrrPeak=s.irradiance_wm2;
      dayTempSum+=s.temp_c; daySamples++;
      bufferSample(s); pushToLiveBuffer(s);
      wlogf("[SAMPLE] %s %s +%lus lux=%.1f irr=%.3f blue=%u temp=%.1f°C\n",
        s.date,s.time,s.elapsed_s,s.lux,s.irradiance_wm2,s.blue_channel,s.temp_c);
    }
  }

  if(isLogging && (now-lastImgMs >= (uint32_t)cfg.img_interval_min*60000UL)){
    lastImgMs=now;
    char ds[12],ts[10]; getRTCStrings(ds,ts);
    wlogf("[IMG] Capturing sky image at %s %s\n",ds,ts);
    captureAndSaveImage(ds,ts);
    esp_task_wdt_reset();
  }

  if(!wifiActive){
    uint32_t el=millis()-now;
    uint32_t budget=min((uint32_t)2000UL, cfg.sample_interval_s*1000UL/2);
    if(el<budget) delay(budget-el);
    esp_task_wdt_reset();
    uint32_t sleepUs=(cfg.sample_interval_s*1000UL-budget)*1000ULL;
    esp_sleep_enable_timer_wakeup(sleepUs);
    esp_sleep_enable_gpio_wakeup();
    gpio_wakeup_enable((gpio_num_t)WIFI_BTN1,GPIO_INTR_LOW_LEVEL);
    gpio_wakeup_enable((gpio_num_t)WIFI_BTN2,GPIO_INTR_LOW_LEVEL);
    esp_light_sleep_start();
  }
}