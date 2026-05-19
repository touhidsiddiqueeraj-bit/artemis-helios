/**
 * ╔══════════════════════════════════════════════════════════════════════╗
 * ║  HELIOS DATA LOGGER  —  v1.0  (single-file build)                   ║
 * ║  ESP32-S3 N16R8 · GY-302 (BH1750) · OV2640 · LittleFS             ║
 * ║                                                                      ║
 * ║  Logs irradiance (GY-302 BH1750) + sky blue channel (OV2640) to    ║
 * ║  internal flash every 10 s during daylight (lux-triggered).         ║
 * ║  Web dashboard + captive portal + mDNS (helios.local).              ║
 * ║                                                                      ║
 * ║  Hussain Touhid Siddiquee · Leading University Sylhet · 2026        ║
 * ╚══════════════════════════════════════════════════════════════════════╝
 *
 * WIRING
 * ──────
 * GY-302   SDA → GPIO 8   SCL → GPIO 9   VCC → 3.3V   GND → GND
 *          ADDR → GND (I2C 0x23)  |  ADDR → VCC (I2C 0x5C if conflict)
 * OV2640   — on-board (XIAO ESP32S3 Sense embedded module)
 *
 * LIBRARIES  (Arduino Library Manager)
 * ─────────────────────────────────────
 * · BH1750  by Christopher Laws  (search "BH1750")
 * · ESP32 Arduino Core >= 2.0  (includes LittleFS, WebServer, ESPmDNS, camera)
 *
 * ARDUINO IDE SETTINGS
 * ─────────────────────
 * Board            : ESP32S3 Dev Module  (or Seeed XIAO ESP32S3)
 * Flash Size       : 16MB
 * Partition Scheme : Default 8MB with spiffs  ← gives ~3.5 MB LittleFS
 *                    (enough for 14 days; use custom partitions.csv for more)
 * PSRAM            : OPI PSRAM  (required for N16R8 camera framebuffer)
 * Upload Speed     : 921600
 *
 * ACCESS
 * ───────
 * 1. Power on — red LED blinks while waiting for daylight
 * 2. Connect phone/laptop to WiFi: "Helios-Logger"  pw: helios2026
 * 3. Captive portal opens automatically  — or go to http://192.168.4.1
 * 4. mDNS: http://helios.local  (works on iOS/macOS/Windows; use IP on Android)
 *
 * CSV FORMAT  (/data/day_01.csv ... day_14.csv)
 * ────────────────────────────────────────────
 * uptime_ms, lux, irradiance_wm2, blue_channel
 * (BH1750 handles gain/timing internally — no raw sensor metadata needed)
 *
 * SIMULATION COMPARISON
 * ──────────────────────
 * Compute CVI = std(irradiance) / mean(irradiance) per day.
 * Target for Sylhet July: CVI ≈ 0.85  (Table I, Helios-Artemis paper).
 * Peak irradiance should approach ~744 W/m² on clear-sky July days.
 */

// ═══════════════════════════════════════════════════════════════════════════
//  SECTION 1 — CAMERA PIN DEFINITIONS  (XIAO ESP32S3 Sense)
//
//  If using AI-Thinker ESP32-CAM instead, comment out this block and
//  uncomment the AI-Thinker block directly below it.
// ═══════════════════════════════════════════════════════════════════════════

// ── XIAO ESP32S3 Sense ────────────────────────────────────────────────────
#define PWDN_GPIO_NUM    -1
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM    10
#define SIOD_GPIO_NUM    40
#define SIOC_GPIO_NUM    39
#define Y9_GPIO_NUM      48
#define Y8_GPIO_NUM      11
#define Y7_GPIO_NUM      12
#define Y6_GPIO_NUM      14
#define Y5_GPIO_NUM      16
#define Y4_GPIO_NUM      18
#define Y3_GPIO_NUM      17
#define Y2_GPIO_NUM      15
#define VSYNC_GPIO_NUM   38
#define HREF_GPIO_NUM    47
#define PCLK_GPIO_NUM    13

/* ── AI-Thinker ESP32-CAM (uncomment if using this board) ─────────────────
#define PWDN_GPIO_NUM    32
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM     0
#define SIOD_GPIO_NUM    26
#define SIOC_GPIO_NUM    27
#define Y9_GPIO_NUM      35
#define Y8_GPIO_NUM      34
#define Y7_GPIO_NUM      39
#define Y6_GPIO_NUM      36
#define Y5_GPIO_NUM      21
#define Y4_GPIO_NUM      19
#define Y3_GPIO_NUM      18
#define Y2_GPIO_NUM       5
#define VSYNC_GPIO_NUM   25
#define HREF_GPIO_NUM    23
#define PCLK_GPIO_NUM    22
─────────────────────────────────────────────────────────────────────────── */

// ═══════════════════════════════════════════════════════════════════════════
//  SECTION 2 — DASHBOARD HTML  (stored in PROGMEM)
// ═══════════════════════════════════════════════════════════════════════════

#include <pgmspace.h>

const char DASHBOARD_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Helios Logger</title>
<style>
  @import url('https://fonts.googleapis.com/css2?family=Space+Mono:wght@400;700&family=DM+Sans:wght@300;400;500&display=swap');

  :root {
    --bg:        #080E14;
    --surface:   #0D1620;
    --card:      #111C28;
    --border:    #1A2E42;
    --teal:      #0D9B9B;
    --teal-dim:  #0A6E6E;
    --teal-glow: rgba(13,155,155,0.15);
    --amber:     #E8A020;
    --blue:      #4B9EFF;
    --green:     #1DB97A;
    --red:       #E84040;
    --text:      #C8DCE8;
    --text-dim:  #5A7A8E;
    --text-muted:#324858;
    --mono:      'Space Mono', monospace;
    --sans:      'DM Sans', sans-serif;
    --radius:    6px;
  }

  *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }
  html { font-size: 14px; }
  body {
    background: var(--bg);
    color: var(--text);
    font-family: var(--sans);
    line-height: 1.5;
    min-height: 100vh;
  }

  ::-webkit-scrollbar { width: 6px; }
  ::-webkit-scrollbar-track { background: var(--bg); }
  ::-webkit-scrollbar-thumb { background: var(--border); border-radius: 3px; }

  .shell { max-width: 900px; margin: 0 auto; padding: 0 16px 48px; }

  header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 20px 0 18px;
    border-bottom: 1px solid var(--border);
    margin-bottom: 24px;
  }
  .logo { display: flex; align-items: center; gap: 12px; }
  .logo-icon {
    width: 36px; height: 36px;
    background: var(--teal);
    border-radius: 8px;
    display: flex; align-items: center; justify-content: center;
    font-size: 18px;
  }
  .logo-text { font-family: var(--mono); font-size: 15px; font-weight: 700; color: #fff; letter-spacing: 0.04em; }
  .logo-sub  { font-size: 11px; color: var(--text-dim); letter-spacing: 0.06em; text-transform: uppercase; margin-top: 1px; }

  .status-pill {
    display: flex; align-items: center; gap: 7px;
    padding: 5px 12px; border-radius: 20px;
    border: 1px solid var(--border);
    font-family: var(--mono); font-size: 11px; letter-spacing: 0.05em;
    background: var(--surface); transition: border-color 0.3s, background 0.3s;
  }
  .status-pill.active  { border-color: var(--green); background: rgba(29,185,122,0.08); color: var(--green); }
  .status-pill.idle    { border-color: var(--text-muted); color: var(--text-dim); }
  .dot { width: 7px; height: 7px; border-radius: 50%; background: currentColor; flex-shrink: 0; }
  .dot.pulse { animation: pulse 1.4s ease-in-out infinite; }
  @keyframes pulse { 0%,100% { opacity:1; transform:scale(1); } 50% { opacity:0.4; transform:scale(0.7); } }

  .section-label {
    font-family: var(--mono); font-size: 10px; letter-spacing: 0.12em;
    text-transform: uppercase; color: var(--text-dim);
    margin-bottom: 10px; display: flex; align-items: center; gap: 8px;
  }
  .section-label::after { content: ''; flex: 1; height: 1px; background: var(--border); }

  .stats-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(140px, 1fr));
    gap: 10px; margin-bottom: 24px;
  }
  .stat-card {
    background: var(--card); border: 1px solid var(--border);
    border-radius: var(--radius); padding: 14px 16px;
    position: relative; overflow: hidden;
  }
  .stat-card::before { content: ''; position: absolute; top: 0; left: 0; right: 0; height: 2px; }
  .stat-card.teal::before  { background: var(--teal); }
  .stat-card.amber::before { background: var(--amber); }
  .stat-card.blue::before  { background: var(--blue); }
  .stat-card.green::before { background: var(--green); }
  .stat-label { font-size: 10px; letter-spacing: 0.08em; text-transform: uppercase; color: var(--text-dim); margin-bottom: 6px; }
  .stat-value { font-family: var(--mono); font-size: 22px; font-weight: 700; color: #fff; line-height: 1; margin-bottom: 4px; }
  .stat-value.teal  { color: var(--teal); }
  .stat-value.amber { color: var(--amber); }
  .stat-value.blue  { color: var(--blue); }
  .stat-sub { font-size: 10px; color: var(--text-muted); }

  .chart-card {
    background: var(--card); border: 1px solid var(--border);
    border-radius: var(--radius); padding: 20px; margin-bottom: 24px;
  }
  .chart-header { display: flex; align-items: center; justify-content: space-between; margin-bottom: 16px; flex-wrap: wrap; gap: 10px; }
  .chart-title  { font-family: var(--mono); font-size: 12px; font-weight: 700; color: #fff; letter-spacing: 0.05em; }
  .chart-legend { display: flex; gap: 14px; }
  .legend-item  { display: flex; align-items: center; gap: 6px; font-size: 11px; color: var(--text-dim); }
  .legend-dot   { width: 8px; height: 8px; border-radius: 50%; }
  .chart-wrap   { position: relative; height: 220px; }
  canvas        { display: block; width: 100% !important; }

  .chart-range  { display: flex; gap: 6px; margin-top: 10px; justify-content: flex-end; }
  .range-btn {
    background: none; border: 1px solid var(--border); border-radius: 4px;
    color: var(--text-dim); font-family: var(--mono); font-size: 10px;
    padding: 3px 8px; cursor: pointer; transition: all 0.15s;
  }
  .range-btn:hover, .range-btn.active { border-color: var(--teal); color: var(--teal); background: var(--teal-glow); }

  .files-card { background: var(--card); border: 1px solid var(--border); border-radius: var(--radius); margin-bottom: 24px; overflow: hidden; }
  .files-header { display: flex; align-items: center; justify-content: space-between; padding: 14px 20px; border-bottom: 1px solid var(--border); }
  .files-title  { font-family: var(--mono); font-size: 12px; font-weight: 700; color: #fff; letter-spacing: 0.05em; }
  .files-empty  { padding: 36px 20px; text-align: center; color: var(--text-muted); font-size: 13px; font-family: var(--mono); }
  .file-row {
    display: flex; align-items: center; justify-content: space-between;
    padding: 11px 20px; border-bottom: 1px solid var(--border);
    transition: background 0.12s; gap: 12px;
  }
  .file-row:last-child { border-bottom: none; }
  .file-row:hover { background: rgba(255,255,255,0.02); }
  .file-name { font-family: var(--mono); font-size: 12px; color: var(--teal); flex: 1; min-width: 0; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
  .file-size { font-family: var(--mono); font-size: 11px; color: var(--text-muted); white-space: nowrap; }
  .file-actions { display: flex; gap: 6px; flex-shrink: 0; }

  .btn {
    display: inline-flex; align-items: center; gap: 5px;
    border: none; border-radius: 4px;
    font-family: var(--mono); font-size: 11px; font-weight: 700; letter-spacing: 0.04em;
    cursor: pointer; padding: 6px 12px; transition: all 0.15s;
    text-decoration: none; white-space: nowrap;
  }
  .btn-teal   { background: var(--teal); color: #fff; }
  .btn-teal:hover { background: #0bb5b5; }
  .btn-ghost  { background: none; border: 1px solid var(--border); color: var(--text-dim); }
  .btn-ghost:hover { border-color: var(--teal); color: var(--teal); background: var(--teal-glow); }
  .btn-danger { background: none; border: 1px solid var(--border); color: var(--text-dim); }
  .btn-danger:hover { border-color: var(--red); color: var(--red); background: rgba(232,64,64,0.08); }
  .btn-sm { padding: 4px 8px; font-size: 10px; }

  .flash-bar-wrap { background: var(--card); border: 1px solid var(--border); border-radius: var(--radius); padding: 16px 20px; margin-bottom: 24px; }
  .flash-bar-header { display: flex; justify-content: space-between; margin-bottom: 8px; font-size: 11px; color: var(--text-dim); }
  .flash-bar-track  { height: 6px; background: var(--border); border-radius: 3px; overflow: hidden; }
  .flash-bar-fill   { height: 100%; background: var(--teal); border-radius: 3px; transition: width 0.5s ease; }
  .flash-bar-fill.warn { background: var(--amber); }
  .flash-bar-fill.crit { background: var(--red); }

  footer { text-align: center; font-size: 11px; color: var(--text-muted); font-family: var(--mono); letter-spacing: 0.05em; padding-top: 24px; border-top: 1px solid var(--border); }

  .toast {
    position: fixed; bottom: 20px; right: 20px;
    background: var(--surface); border: 1px solid var(--teal);
    border-radius: var(--radius); padding: 10px 16px;
    font-family: var(--mono); font-size: 12px; color: var(--teal);
    opacity: 0; transform: translateY(8px); transition: all 0.25s;
    z-index: 999; pointer-events: none;
  }
  .toast.show  { opacity: 1; transform: translateY(0); }
  .toast.error { border-color: var(--red); color: var(--red); }

  @media (max-width: 480px) {
    .stats-grid { grid-template-columns: repeat(2, 1fr); }
    .chart-wrap { height: 180px; }
    .file-row   { flex-wrap: wrap; }
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
        <div class="logo-sub">Data Logger v1.0</div>
      </div>
    </div>
    <div class="status-pill idle" id="statusPill">
      <div class="dot" id="statusDot"></div>
      <span id="statusText">IDLE</span>
    </div>
  </header>

  <div class="section-label">Live Readings</div>
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
      <div class="stat-sub">0 &#8211; 255</div>
    </div>
    <div class="stat-card green">
      <div class="stat-label">Samples</div>
      <div class="stat-value" id="statSamples">&#8212;</div>
      <div class="stat-sub">total logged</div>
    </div>
    <div class="stat-card">
      <div class="stat-label">Day</div>
      <div class="stat-value" id="statDay">&#8212;</div>
      <div class="stat-sub">of 14</div>
    </div>
    <div class="stat-card">
      <div class="stat-label">Uptime Offset</div>
      <div class="stat-value" id="statUptime" style="font-size:16px">&#8212;</div>
      <div class="stat-sub">ms since day start</div>
    </div>
  </div>

  <div class="section-label">Flash Storage</div>
  <div class="flash-bar-wrap">
    <div class="flash-bar-header">
      <span id="flashLabel">&#8212; KB used of &#8212; KB</span>
      <span id="flashPct">&#8212;%</span>
    </div>
    <div class="flash-bar-track">
      <div class="flash-bar-fill" id="flashFill" style="width:0%"></div>
    </div>
  </div>

  <div class="section-label">Time Series</div>
  <div class="chart-card">
    <div class="chart-header">
      <div class="chart-title">Irradiance &amp; Sky Blue Channel</div>
      <div class="chart-legend">
        <div class="legend-item"><div class="legend-dot" style="background:var(--teal)"></div>Irradiance (W/m&#178;)</div>
        <div class="legend-item"><div class="legend-dot" style="background:var(--blue)"></div>Blue channel</div>
      </div>
    </div>
    <div class="chart-wrap"><canvas id="chart"></canvas></div>
    <div class="chart-range">
      <button class="range-btn active" onclick="setRange(60)"  id="r60" >1 h</button>
      <button class="range-btn"        onclick="setRange(180)" id="r180">3 h</button>
      <button class="range-btn"        onclick="setRange(360)" id="r360">6 h</button>
      <button class="range-btn"        onclick="setRange(0)"   id="rAll">All</button>
    </div>
  </div>

  <div class="section-label">Logged Files</div>
  <div class="files-card">
    <div class="files-header">
      <div class="files-title">CSV Archive</div>
      <button class="btn btn-danger btn-sm" onclick="deleteAll()">&#9888; Wipe All</button>
    </div>
    <div id="fileList"><div class="files-empty">No files yet.</div></div>
  </div>

  <footer>Helios-Artemis &middot; Leading University Sylhet &middot; 2026</footer>
</div>

<div class="toast" id="toast"></div>

<script>
const canvas = document.getElementById('chart');
const ctx    = canvas.getContext('2d');
let chartData  = [];
let rangeCount = 60;

function setRange(n) {
  rangeCount = n;
  ['r60','r180','r360','rAll'].forEach(id => document.getElementById(id).classList.remove('active'));
  const map = {60:'r60',180:'r180',360:'r360',0:'rAll'};
  document.getElementById(map[n]).classList.add('active');
  drawChart();
}

function drawChart() {
  const w = canvas.parentElement.clientWidth;
  const h = canvas.parentElement.clientHeight;
  canvas.width = w; canvas.height = h;
  const data = rangeCount === 0 ? chartData : chartData.slice(-rangeCount);

  if (data.length === 0) {
    ctx.fillStyle = '#324858';
    ctx.font = '12px Space Mono, monospace';
    ctx.textAlign = 'center';
    ctx.fillText('No data yet \u2014 waiting for daylight', w/2, h/2);
    return;
  }

  const PAD = {top:10, right:12, bottom:28, left:52};
  const cw = w - PAD.left - PAD.right;
  const ch = h - PAD.top  - PAD.bottom;
  const maxIrr  = Math.max(...data.map(d => d.irr), 0.01);
  const maxBlue = 255;

  ctx.clearRect(0, 0, w, h);
  ctx.strokeStyle = '#1A2E42'; ctx.lineWidth = 1;
  for (let i = 0; i <= 4; i++) {
    const y = PAD.top + (ch/4)*i;
    ctx.beginPath(); ctx.moveTo(PAD.left, y); ctx.lineTo(PAD.left+cw, y); ctx.stroke();
    ctx.fillStyle = '#5A7A8E'; ctx.font = '9px Space Mono,monospace'; ctx.textAlign = 'right';
    ctx.fillText((maxIrr*(1-i/4)).toFixed(2), PAD.left-4, y+3);
  }

  ctx.fillStyle = '#5A7A8E'; ctx.font = '9px Space Mono,monospace'; ctx.textAlign = 'center';
  const xStep = Math.max(1, Math.floor(data.length/6));
  for (let i = 0; i < data.length; i += xStep) {
    const x = PAD.left + (i/(data.length-1||1))*cw;
    const min = Math.floor(data[i].t/60000);
    ctx.fillText(min < 60 ? min+'m' : Math.floor(min/60)+'h'+(min%60?min%60+'m':''), x, h-6);
  }

  const px   = i => PAD.left + (i/(data.length-1||1))*cw;
  const pyI  = v => PAD.top + ch - (v/maxIrr)*ch;
  const pyB  = v => PAD.top + ch - (v/maxBlue)*ch;

  ctx.beginPath();
  ctx.moveTo(px(0), pyI(data[0].irr));
  data.forEach((d,i) => ctx.lineTo(px(i), pyI(d.irr)));
  ctx.lineTo(px(data.length-1), PAD.top+ch);
  ctx.lineTo(px(0), PAD.top+ch);
  ctx.closePath();
  const grad = ctx.createLinearGradient(0, PAD.top, 0, PAD.top+ch);
  grad.addColorStop(0, 'rgba(13,155,155,0.30)');
  grad.addColorStop(1, 'rgba(13,155,155,0.02)');
  ctx.fillStyle = grad; ctx.fill();

  ctx.beginPath(); ctx.strokeStyle='#0D9B9B'; ctx.lineWidth=2; ctx.lineJoin='round';
  data.forEach((d,i) => i===0 ? ctx.moveTo(px(i),pyI(d.irr)) : ctx.lineTo(px(i),pyI(d.irr)));
  ctx.stroke();

  ctx.beginPath(); ctx.strokeStyle='#4B9EFF'; ctx.lineWidth=1.5; ctx.setLineDash([4,3]);
  data.forEach((d,i) => i===0 ? ctx.moveTo(px(i),pyB(d.blue)) : ctx.lineTo(px(i),pyB(d.blue)));
  ctx.stroke(); ctx.setLineDash([]);

  if (data.length > 0) {
    const last = data[data.length-1];
    const lx = px(data.length-1), ly = pyI(last.irr);
    ctx.beginPath(); ctx.arc(lx, ly, 4, 0, Math.PI*2);
    ctx.fillStyle='#0D9B9B'; ctx.fill();
    ctx.strokeStyle='#080E14'; ctx.lineWidth=2; ctx.stroke();
  }
}

async function fetchStatus() {
  try {
    const d = await (await fetch('/api/status')).json();
    const pill = document.getElementById('statusPill');
    const dot  = document.getElementById('statusDot');
    const txt  = document.getElementById('statusText');
    if (d.logging) {
      pill.className = 'status-pill active'; dot.classList.add('pulse'); txt.textContent = 'LOGGING';
    } else {
      pill.className = 'status-pill idle'; dot.classList.remove('pulse'); txt.textContent = 'IDLE';
    }
    document.getElementById('statSamples').textContent = d.total_samples.toLocaleString();
    document.getElementById('statDay').textContent = d.day;
    if (d.latest) {
      document.getElementById('statIrr').textContent   = d.latest.irradiance_wm2.toFixed(3);
      document.getElementById('statLux').textContent   = d.latest.lux.toFixed(1);
      document.getElementById('statBlue').textContent  = d.latest.blue_channel;
      document.getElementById('statUptime').textContent = d.latest.uptime_ms.toLocaleString();
    }
    const pct  = d.fs_total_kb > 0 ? (d.fs_used_kb/d.fs_total_kb*100) : 0;
    document.getElementById('flashLabel').textContent = d.fs_used_kb+' KB used of '+d.fs_total_kb+' KB';
    document.getElementById('flashPct').textContent   = pct.toFixed(1)+'%';
    const fill = document.getElementById('flashFill');
    fill.style.width = pct+'%';
    fill.className = 'flash-bar-fill'+(pct>85?' crit':pct>65?' warn':'');
  } catch(e) {}
}

async function fetchLive() {
  try {
    const count = rangeCount === 0 ? 360 : rangeCount;
    chartData = await (await fetch('/api/live?count='+count)).json();
    drawChart();
  } catch(e) {}
}

async function fetchFiles() {
  try {
    const files = await (await fetch('/api/files')).json();
    const list  = document.getElementById('fileList');
    if (!files || files.length === 0) {
      list.innerHTML = '<div class="files-empty">No files yet \u2014 waiting for first daylight trigger.</div>';
      return;
    }
    list.innerHTML = files.map(f =>
      '<div class="file-row">' +
        '<div class="file-name">'+f.name+'</div>' +
        '<div class="file-size">'+formatBytes(f.size)+'</div>' +
        '<div class="file-actions">' +
          '<a class="btn btn-teal btn-sm" href="/download?file='+encodeURIComponent(f.name)+'" download>\u2193 CSV</a>' +
          '<button class="btn btn-danger btn-sm" onclick="deleteFile(\''+f.name+'\')">&#10005;</button>' +
        '</div>' +
      '</div>'
    ).join('');
  } catch(e) {}
}

async function deleteFile(name) {
  if (!confirm('Delete '+name+'? This cannot be undone.')) return;
  try {
    const d = await (await fetch('/api/delete?file='+encodeURIComponent(name), {method:'DELETE'})).json();
    if (d.ok) { toast('File deleted'); fetchFiles(); } else toast('Delete failed', true);
  } catch(e) { toast('Error', true); }
}

async function deleteAll() {
  if (!confirm('Wipe ALL logged data? This cannot be undone.')) return;
  try {
    const d = await (await fetch('/api/deleteall', {method:'DELETE'})).json();
    if (d.ok) { chartData=[]; drawChart(); toast('All data wiped'); fetchFiles(); fetchStatus(); }
    else toast('Wipe failed', true);
  } catch(e) { toast('Error', true); }
}

function formatBytes(b) {
  if (b < 1024) return b+' B';
  if (b < 1048576) return (b/1024).toFixed(1)+' KB';
  return (b/1048576).toFixed(2)+' MB';
}

let toastTimer = null;
function toast(msg, isError=false) {
  const el = document.getElementById('toast');
  el.textContent = msg;
  el.className = 'toast show'+(isError?' error':'');
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => { el.className='toast'; }, 2500);
}

window.addEventListener('resize', drawChart);
drawChart();
fetchStatus(); fetchLive(); fetchFiles();
setInterval(fetchStatus, 5000);
setInterval(fetchLive,   10000);
setInterval(fetchFiles,  30000);
</script>
</body>
</html>
)rawhtml";

// ═══════════════════════════════════════════════════════════════════════════
//  SECTION 3 — FIRMWARE
// ═══════════════════════════════════════════════════════════════════════════

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <BH1750.h>
#include "esp_camera.h"

// ── Configuration ──────────────────────────────────────────────────────────
#define AP_SSID             "Helios-Logger"
#define AP_PASSWORD         "helios2026"
#define MDNS_HOSTNAME       "helios"
#define SAMPLE_INTERVAL_MS  10000UL
#define LUX_START_THRESHOLD 20.0f
#define LUX_STOP_THRESHOLD  8.0f
#define DATA_DIR            "/data"
#define MAX_DAYS            14
#define I2C_SDA             8
#define I2C_SCL             9
#define LUX_TO_WM2          (1.0f / 116.0f)
// BH1750 I2C address: 0x23 (ADDR pin LOW) or 0x5C (ADDR pin HIGH)
#define BH1750_ADDR         0x23

// ── Globals ────────────────────────────────────────────────────────────────
BH1750    lightMeter;
WebServer server(80);

bool     isLogging     = false;
uint32_t lastSampleMs  = 0;
uint32_t uptimeStartMs = 0;
uint32_t dayCounter    = 0;
char     currentFile[32];

#define LIVE_BUFFER_SIZE 360
struct Sample {
  uint32_t uptime_ms;
  float    lux;
  float    irradiance_wm2;
  uint8_t  blue_channel;
};
Sample   liveBuffer[LIVE_BUFFER_SIZE];
uint16_t liveHead     = 0;
uint16_t liveCount    = 0;
uint32_t totalSamples = 0;

// ── GY-302 (BH1750) ───────────────────────────────────────────────────────
void configureBH1750() {
  // CONTINUOUS_HIGH_RES_MODE: 1 lux resolution, ~120 ms measurement time
  // Range: 1 – 65535 lux — sufficient for full solar spectrum logging
  lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, BH1750_ADDR, &Wire);
}

/**
 * Read lux from BH1750.
 * Returns true on valid reading, false if sensor returns error value (65535).
 * BH1750 saturates cleanly — no gain management needed.
 */
bool readBH1750(float &lux) {
  float reading = lightMeter.readLightLevel();
  if (reading < 0) {
    // Negative = sensor not ready yet (measurement in progress)
    lux = 0.0f;
    return false;
  }
  lux = reading;
  return true;
}

// ── OV2640 — Blue channel extraction ──────────────────────────────────────
uint8_t captureBlueChannel() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) { Serial.println("[CAM] Frame capture failed"); return 0; }

  // RGB565: byte 1 = GGGBBBBB — extract 5-bit blue, scale to 8-bit
  uint32_t blueSum = 0;
  for (uint32_t i = 1; i < fb->len; i += 8) {
    uint8_t b5 = fb->buf[i] & 0x1F;
    blueSum   += (b5 << 3) | (b5 >> 2);
  }
  uint32_t sampledPixels = (fb->len / 8) + 1;
  uint8_t  meanBlue      = (uint8_t)(blueSum / sampledPixels);

  esp_camera_fb_return(fb);
  return meanBlue;
}

// ── LittleFS ───────────────────────────────────────────────────────────────
void ensureDataDir() {
  if (!LittleFS.exists(DATA_DIR)) {
    LittleFS.mkdir(DATA_DIR);
    Serial.println("[FS] Created /data directory");
  }
}

void openDayFile(uint32_t day) {
  snprintf(currentFile, sizeof(currentFile), "%s/day_%02lu.csv", DATA_DIR, day + 1);
  if (!LittleFS.exists(currentFile)) {
    File f = LittleFS.open(currentFile, "w");
    if (f) {
      f.println("uptime_ms,lux,irradiance_wm2,blue_channel");
      f.close();
      Serial.printf("[FS] Created %s\n", currentFile);
    } else {
      Serial.printf("[FS] ERROR: could not create %s\n", currentFile);
    }
  } else {
    Serial.printf("[FS] Appending to %s\n", currentFile);
  }
}

void writeSample(const Sample &s) {
  File f = LittleFS.open(currentFile, "a");
  if (!f) { Serial.println("[FS] ERROR: append failed"); return; }
  f.printf("%lu,%.2f,%.4f,%u\n",
    s.uptime_ms, s.lux, s.irradiance_wm2, s.blue_channel);
  f.close();
}

void pushToLiveBuffer(const Sample &s) {
  liveBuffer[liveHead] = s;
  liveHead = (liveHead + 1) % LIVE_BUFFER_SIZE;
  if (liveCount < LIVE_BUFFER_SIZE) liveCount++;
  totalSamples++;
}

// ── Web server handlers ────────────────────────────────────────────────────
void handleRoot() {
  server.sendHeader("Cache-Control", "no-cache");
  server.send_P(200, "text/html", DASHBOARD_HTML);
}

void handleCaptivePortal() {
  server.sendHeader("Location", "http://192.168.4.1/", true);
  server.send(302, "text/plain", "");
}

void handleStatus() {
  size_t totalBytes = LittleFS.totalBytes();
  size_t usedBytes  = LittleFS.usedBytes();

  uint8_t fileCount = 0;
  File root = LittleFS.open(DATA_DIR);
  if (root && root.isDirectory()) {
    File f = root.openNextFile();
    while (f) { fileCount++; f = root.openNextFile(); }
  }

  String latestJson = "null";
  if (liveCount > 0) {
    uint16_t idx = (liveHead == 0) ? LIVE_BUFFER_SIZE - 1 : liveHead - 1;
    Sample &ls = liveBuffer[idx];
    char buf[200];
    snprintf(buf, sizeof(buf),
      "{\"uptime_ms\":%lu,\"lux\":%.2f,\"irradiance_wm2\":%.4f,"
      "\"blue_channel\":%u}",
      ls.uptime_ms, ls.lux, ls.irradiance_wm2, ls.blue_channel);
    latestJson = String(buf);
  }

  char json[512];
  snprintf(json, sizeof(json),
    "{\"logging\":%s,\"day\":%lu,\"current_file\":\"%s\","
    "\"total_samples\":%lu,\"lux_start_threshold\":%.1f,"
    "\"lux_stop_threshold\":%.1f,\"fs_total_kb\":%u,"
    "\"fs_used_kb\":%u,\"fs_free_kb\":%u,\"file_count\":%u,\"latest\":%s}",
    isLogging ? "true" : "false",
    dayCounter + 1, currentFile, totalSamples,
    LUX_START_THRESHOLD, LUX_STOP_THRESHOLD,
    (unsigned)(totalBytes / 1024), (unsigned)(usedBytes / 1024),
    (unsigned)((totalBytes - usedBytes) / 1024),
    fileCount, latestJson.c_str());

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

void handleLive() {
  uint16_t count = server.arg("count").toInt();
  if (count == 0 || count > LIVE_BUFFER_SIZE) count = 60;
  if (count > liveCount) count = liveCount;

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");
  server.sendContent("[");
  for (uint16_t i = 0; i < count; i++) {
    uint16_t idx = (liveHead - count + i + LIVE_BUFFER_SIZE) % LIVE_BUFFER_SIZE;
    Sample &s = liveBuffer[idx];
    char buf[160];
    snprintf(buf, sizeof(buf),
      "{\"t\":%lu,\"lux\":%.2f,\"irr\":%.4f,\"blue\":%u}%s",
      s.uptime_ms, s.lux, s.irradiance_wm2, s.blue_channel,
      (i < count - 1) ? "," : "");
    server.sendContent(buf);
  }
  server.sendContent("]");
}

void handleFiles() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");
  server.sendContent("[");
  File root = LittleFS.open(DATA_DIR);
  bool first = true;
  if (root && root.isDirectory()) {
    File f = root.openNextFile();
    while (f) {
      if (!f.isDirectory()) {
        char buf[128];
        snprintf(buf, sizeof(buf),
          "%s{\"name\":\"%s\",\"size\":%u}",
          first ? "" : ",", f.name(), (unsigned)f.size());
        server.sendContent(buf);
        first = false;
      }
      f = root.openNextFile();
    }
  }
  server.sendContent("]");
}

void handleDownload() {
  String filename = server.arg("file");
  if (filename.length() == 0) { server.send(400, "text/plain", "Missing file parameter"); return; }
  String path = String(DATA_DIR) + "/" + filename;
  if (!LittleFS.exists(path)) { server.send(404, "text/plain", "File not found"); return; }
  File f = LittleFS.open(path, "r");
  if (!f) { server.send(500, "text/plain", "Could not open file"); return; }
  server.sendHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
  server.streamFile(f, "text/csv");
  f.close();
}

void handleDelete() {
  String filename = server.arg("file");
  if (filename.length() == 0) { server.send(400, "text/plain", "Missing file parameter"); return; }
  String path = String(DATA_DIR) + "/" + filename;
  server.send(200, "application/json", LittleFS.remove(path) ? "{\"ok\":true}" : "{\"ok\":false}");
}

void handleDeleteAll() {
  File root = LittleFS.open(DATA_DIR);
  if (root && root.isDirectory()) {
    File f = root.openNextFile();
    while (f) {
      if (!f.isDirectory()) LittleFS.remove(String(DATA_DIR) + "/" + f.name());
      f = root.openNextFile();
    }
  }
  totalSamples = 0; liveCount = 0; liveHead = 0; dayCounter = 0;
  server.send(200, "application/json", "{\"ok\":true}");
}

// ── Setup ──────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n[HELIOS] Data Logger v1.0 booting...");

  // LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("[FS] FATAL: LittleFS mount failed"); while (1) delay(1000);
  }
  Serial.printf("[FS] Mounted — Total: %uKB  Used: %uKB\n",
    (unsigned)(LittleFS.totalBytes() / 1024),
    (unsigned)(LittleFS.usedBytes()  / 1024));
  ensureDataDir();

  // GY-302 (BH1750)
  Wire.begin(I2C_SDA, I2C_SCL);
  configureBH1750();
  // Verify sensor is responding
  float testLux = lightMeter.readLightLevel();
  if (testLux < 0) {
    Serial.println("[BH1750] FATAL: GY-302 not responding — check wiring/address");
    while (1) delay(1000);
  }
  Serial.printf("[BH1750] GY-302 ready  (addr 0x%02X)  test reading: %.1f lux\n",
    BH1750_ADDR, testLux);

  // OV2640
  camera_config_t camCfg;
  camCfg.ledc_channel = LEDC_CHANNEL_0;
  camCfg.ledc_timer   = LEDC_TIMER_0;
  camCfg.pin_d0       = Y2_GPIO_NUM;  camCfg.pin_d1 = Y3_GPIO_NUM;
  camCfg.pin_d2       = Y4_GPIO_NUM;  camCfg.pin_d3 = Y5_GPIO_NUM;
  camCfg.pin_d4       = Y6_GPIO_NUM;  camCfg.pin_d5 = Y7_GPIO_NUM;
  camCfg.pin_d6       = Y8_GPIO_NUM;  camCfg.pin_d7 = Y9_GPIO_NUM;
  camCfg.pin_xclk     = XCLK_GPIO_NUM;
  camCfg.pin_pclk     = PCLK_GPIO_NUM;
  camCfg.pin_vsync    = VSYNC_GPIO_NUM;
  camCfg.pin_href     = HREF_GPIO_NUM;
  camCfg.pin_sccb_sda = SIOD_GPIO_NUM;
  camCfg.pin_sccb_scl = SIOC_GPIO_NUM;
  camCfg.pin_pwdn     = PWDN_GPIO_NUM;
  camCfg.pin_reset    = RESET_GPIO_NUM;
  camCfg.xclk_freq_hz = 20000000;
  camCfg.pixel_format = PIXFORMAT_RGB565;
  camCfg.frame_size   = FRAMESIZE_QVGA;
  camCfg.jpeg_quality = 12;
  camCfg.fb_count     = 1;
  camCfg.fb_location  = CAMERA_FB_IN_PSRAM;
  camCfg.grab_mode    = CAMERA_GRAB_LATEST;

  if (esp_camera_init(&camCfg) != ESP_OK) {
    Serial.println("[CAM] Init failed — continuing lux-only");
  } else {
    sensor_t *s = esp_camera_sensor_get();
    s->set_whitebal(s, 1); s->set_awb_gain(s, 1);
    s->set_exposure_ctrl(s, 1); s->set_aec2(s, 1);
    s->set_gain_ctrl(s, 1);
    s->set_brightness(s, 0); s->set_contrast(s, 0);
    s->set_saturation(s, 0); s->set_special_effect(s, 0);
    Serial.println("[CAM] OV2640 ready (RGB565 QVGA)");
  }

  // WiFi AP
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  IPAddress apIP(192, 168, 4, 1);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  Serial.printf("[WiFi] AP: %s  IP: 192.168.4.1\n", AP_SSID);

  // mDNS
  if (MDNS.begin(MDNS_HOSTNAME)) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("[mDNS] http://helios.local");
  }

  // Routes
  server.on("/",                          HTTP_GET,    handleRoot);
  server.on("/api/status",                HTTP_GET,    handleStatus);
  server.on("/api/live",                  HTTP_GET,    handleLive);
  server.on("/api/files",                 HTTP_GET,    handleFiles);
  server.on("/download",                  HTTP_GET,    handleDownload);
  server.on("/api/delete",                HTTP_DELETE, handleDelete);
  server.on("/api/deleteall",             HTTP_DELETE, handleDeleteAll);
  // Captive portal detection URLs
  server.on("/generate_204",              HTTP_GET,    handleCaptivePortal); // Android
  server.on("/connecttest.txt",           HTTP_GET,    handleCaptivePortal); // Windows
  server.on("/hotspot-detect.html",       HTTP_GET,    handleCaptivePortal); // Apple
  server.on("/library/test/success.html", HTTP_GET,    handleCaptivePortal); // Apple
  server.on("/ncsi.txt",                  HTTP_GET,    handleCaptivePortal); // Windows
  server.onNotFound(handleCaptivePortal);

  server.begin();
  Serial.println("[HELIOS] Boot complete — waiting for daylight (>20 lux)...");
}

// ── Loop ───────────────────────────────────────────────────────────────────
void loop() {
  server.handleClient();
  MDNS.update();

  uint32_t now = millis();
  if (now - lastSampleMs < SAMPLE_INTERVAL_MS) return;
  lastSampleMs = now;

  float lux = 0.0f;
  readBH1750(lux);

  // Start logging when lux crosses threshold
  if (!isLogging && lux >= LUX_START_THRESHOLD) {
    isLogging     = true;
    uptimeStartMs = now;
    openDayFile(dayCounter);
    Serial.printf("[LOG] Day %lu started — %.1f lux\n", dayCounter + 1, lux);
  }

  // Stop logging when lux drops below hysteresis threshold
  if (isLogging && lux < LUX_STOP_THRESHOLD) {
    isLogging = false;
    dayCounter++;
    if (dayCounter >= MAX_DAYS) dayCounter = 0;
    Serial.printf("[LOG] Day ended — %.1f lux  total: %lu samples\n", lux, totalSamples);
  }

  if (!isLogging) return;

  Sample s;
  s.uptime_ms      = now - uptimeStartMs;
  s.lux            = lux;
  s.irradiance_wm2 = lux * LUX_TO_WM2;
  s.blue_channel   = captureBlueChannel();

  writeSample(s);
  pushToLiveBuffer(s);

  Serial.printf("[SAMPLE] t=%lums  lux=%.1f  irr=%.3fW/m2  blue=%u\n",
    s.uptime_ms, s.lux, s.irradiance_wm2, s.blue_channel);
}
