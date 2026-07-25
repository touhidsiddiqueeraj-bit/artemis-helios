/**
 * dashboard.h
 * Full web dashboard stored in program memory (PROGMEM).
 * Served at http://helios.local or http://192.168.4.1
 *
 * Features:
 *  - Live irradiance + blue channel chart (auto-refreshes)
 *  - Status panel (logging state, flash usage, sample count)
 *  - File browser with per-file download and delete
 *  - Wipe all data button
 *  - Responsive for phone use in the field
 */

#pragma once
#include <pgmspace.h>

const char DASHBOARD_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Helios Logger</title>
<style>
  /* ── Fonts ─────────────────────────────────────────────────────────── */
  @import url('https://fonts.googleapis.com/css2?family=Space+Mono:wght@400;700&family=DM+Sans:wght@300;400;500&display=swap');

  /* ── Design tokens ──────────────────────────────────────────────────── */
  :root {
    --bg:        #080E14;
    --surface:   #0D1620;
    --card:      #111C28;
    --border:    #1A2E42;
    --teal:      #0D9B9B;
    --teal-dim:  #0A6E6E;
    --teal-glow: rgba(13,155,155,0.15);
    --amber:     #E8A020;
    --amber-dim: rgba(232,160,32,0.12);
    --blue:      #4B9EFF;
    --blue-dim:  rgba(75,158,255,0.12);
    --green:     #1DB97A;
    --red:       #E84040;
    --text:      #C8DCE8;
    --text-dim:  #5A7A8E;
    --text-muted:#324858;
    --mono:      'Space Mono', monospace;
    --sans:      'DM Sans', sans-serif;
    --radius:    6px;
  }

  /* ── Reset ──────────────────────────────────────────────────────────── */
  *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }
  html { font-size: 14px; }
  body {
    background: var(--bg);
    color: var(--text);
    font-family: var(--sans);
    font-weight: 400;
    line-height: 1.5;
    min-height: 100vh;
  }

  /* ── Scrollbar ───────────────────────────────────────────────────────── */
  ::-webkit-scrollbar { width: 6px; }
  ::-webkit-scrollbar-track { background: var(--bg); }
  ::-webkit-scrollbar-thumb { background: var(--border); border-radius: 3px; }

  /* ── Layout ─────────────────────────────────────────────────────────── */
  .shell {
    max-width: 900px;
    margin: 0 auto;
    padding: 0 16px 48px;
  }

  /* ── Header ─────────────────────────────────────────────────────────── */
  header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 20px 0 18px;
    border-bottom: 1px solid var(--border);
    margin-bottom: 24px;
  }
  .logo {
    display: flex;
    align-items: center;
    gap: 12px;
  }
  .logo-icon {
    width: 36px; height: 36px;
    background: var(--teal);
    border-radius: 8px;
    display: flex; align-items: center; justify-content: center;
    font-size: 18px;
  }
  .logo-text {
    font-family: var(--mono);
    font-size: 15px;
    font-weight: 700;
    color: #fff;
    letter-spacing: 0.04em;
  }
  .logo-sub {
    font-size: 11px;
    color: var(--text-dim);
    letter-spacing: 0.06em;
    text-transform: uppercase;
    margin-top: 1px;
  }
  .header-right {
    display: flex;
    align-items: center;
    gap: 10px;
  }
  .status-pill {
    display: flex;
    align-items: center;
    gap: 7px;
    padding: 5px 12px;
    border-radius: 20px;
    border: 1px solid var(--border);
    font-family: var(--mono);
    font-size: 11px;
    letter-spacing: 0.05em;
    background: var(--surface);
    transition: border-color 0.3s, background 0.3s;
  }
  .status-pill.active {
    border-color: var(--green);
    background: rgba(29,185,122,0.08);
    color: var(--green);
  }
  .status-pill.idle {
    border-color: var(--text-muted);
    color: var(--text-dim);
  }
  .dot {
    width: 7px; height: 7px;
    border-radius: 50%;
    background: currentColor;
    flex-shrink: 0;
  }
  .dot.pulse {
    animation: pulse 1.4s ease-in-out infinite;
  }
  @keyframes pulse {
    0%, 100% { opacity: 1; transform: scale(1); }
    50%       { opacity: 0.4; transform: scale(0.7); }
  }

  /* ── Section label ───────────────────────────────────────────────────── */
  .section-label {
    font-family: var(--mono);
    font-size: 10px;
    letter-spacing: 0.12em;
    text-transform: uppercase;
    color: var(--text-dim);
    margin-bottom: 10px;
    display: flex;
    align-items: center;
    gap: 8px;
  }
  .section-label::after {
    content: '';
    flex: 1;
    height: 1px;
    background: var(--border);
  }

  /* ── Stats grid ──────────────────────────────────────────────────────── */
  .stats-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(140px, 1fr));
    gap: 10px;
    margin-bottom: 24px;
  }
  .stat-card {
    background: var(--card);
    border: 1px solid var(--border);
    border-radius: var(--radius);
    padding: 14px 16px;
    position: relative;
    overflow: hidden;
  }
  .stat-card::before {
    content: '';
    position: absolute;
    top: 0; left: 0; right: 0;
    height: 2px;
  }
  .stat-card.teal::before  { background: var(--teal); }
  .stat-card.amber::before { background: var(--amber); }
  .stat-card.blue::before  { background: var(--blue); }
  .stat-card.green::before { background: var(--green); }
  .stat-card.red::before   { background: var(--red); }

  .stat-label {
    font-size: 10px;
    letter-spacing: 0.08em;
    text-transform: uppercase;
    color: var(--text-dim);
    margin-bottom: 6px;
  }
  .stat-value {
    font-family: var(--mono);
    font-size: 22px;
    font-weight: 700;
    color: #fff;
    line-height: 1;
    margin-bottom: 4px;
  }
  .stat-value.teal  { color: var(--teal); }
  .stat-value.amber { color: var(--amber); }
  .stat-value.blue  { color: var(--blue); }
  .stat-value.green { color: var(--green); }
  .stat-sub {
    font-size: 10px;
    color: var(--text-muted);
  }

  /* ── Chart ───────────────────────────────────────────────────────────── */
  .chart-card {
    background: var(--card);
    border: 1px solid var(--border);
    border-radius: var(--radius);
    padding: 20px;
    margin-bottom: 24px;
  }
  .chart-header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    margin-bottom: 16px;
    flex-wrap: wrap;
    gap: 10px;
  }
  .chart-title {
    font-family: var(--mono);
    font-size: 12px;
    font-weight: 700;
    color: #fff;
    letter-spacing: 0.05em;
  }
  .chart-legend {
    display: flex;
    gap: 14px;
  }
  .legend-item {
    display: flex;
    align-items: center;
    gap: 6px;
    font-size: 11px;
    color: var(--text-dim);
  }
  .legend-dot {
    width: 8px; height: 8px;
    border-radius: 50%;
  }
  .chart-wrap {
    position: relative;
    height: 220px;
  }
  canvas { display: block; width: 100% !important; }

  .chart-range {
    display: flex;
    gap: 6px;
    margin-top: 10px;
    justify-content: flex-end;
  }
  .range-btn {
    background: none;
    border: 1px solid var(--border);
    border-radius: 4px;
    color: var(--text-dim);
    font-family: var(--mono);
    font-size: 10px;
    padding: 3px 8px;
    cursor: pointer;
    transition: all 0.15s;
  }
  .range-btn:hover, .range-btn.active {
    border-color: var(--teal);
    color: var(--teal);
    background: var(--teal-glow);
  }

  /* ── File browser ────────────────────────────────────────────────────── */
  .files-card {
    background: var(--card);
    border: 1px solid var(--border);
    border-radius: var(--radius);
    margin-bottom: 24px;
    overflow: hidden;
  }
  .files-header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 14px 20px;
    border-bottom: 1px solid var(--border);
  }
  .files-title {
    font-family: var(--mono);
    font-size: 12px;
    font-weight: 700;
    color: #fff;
    letter-spacing: 0.05em;
  }
  .files-empty {
    padding: 36px 20px;
    text-align: center;
    color: var(--text-muted);
    font-size: 13px;
    font-family: var(--mono);
  }
  .file-row {
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 11px 20px;
    border-bottom: 1px solid var(--border);
    transition: background 0.12s;
    gap: 12px;
  }
  .file-row:last-child { border-bottom: none; }
  .file-row:hover { background: rgba(255,255,255,0.02); }
  .file-name {
    font-family: var(--mono);
    font-size: 12px;
    color: var(--teal);
    flex: 1;
    min-width: 0;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
  }
  .file-size {
    font-family: var(--mono);
    font-size: 11px;
    color: var(--text-muted);
    white-space: nowrap;
  }
  .file-actions {
    display: flex;
    gap: 6px;
    flex-shrink: 0;
  }

  /* ── Buttons ─────────────────────────────────────────────────────────── */
  .btn {
    display: inline-flex;
    align-items: center;
    gap: 5px;
    border: none;
    border-radius: 4px;
    font-family: var(--mono);
    font-size: 11px;
    font-weight: 700;
    letter-spacing: 0.04em;
    cursor: pointer;
    padding: 6px 12px;
    transition: all 0.15s;
    text-decoration: none;
    white-space: nowrap;
  }
  .btn-teal {
    background: var(--teal);
    color: #fff;
  }
  .btn-teal:hover { background: #0bb5b5; }
  .btn-ghost {
    background: none;
    border: 1px solid var(--border);
    color: var(--text-dim);
  }
  .btn-ghost:hover {
    border-color: var(--teal);
    color: var(--teal);
    background: var(--teal-glow);
  }
  .btn-danger {
    background: none;
    border: 1px solid var(--border);
    color: var(--text-dim);
  }
  .btn-danger:hover {
    border-color: var(--red);
    color: var(--red);
    background: rgba(232,64,64,0.08);
  }
  .btn-sm { padding: 4px 8px; font-size: 10px; }

  /* ── Flash usage bar ─────────────────────────────────────────────────── */
  .flash-bar-wrap {
    background: var(--card);
    border: 1px solid var(--border);
    border-radius: var(--radius);
    padding: 16px 20px;
    margin-bottom: 24px;
  }
  .flash-bar-header {
    display: flex;
    justify-content: space-between;
    margin-bottom: 8px;
    font-size: 11px;
    color: var(--text-dim);
  }
  .flash-bar-track {
    height: 6px;
    background: var(--border);
    border-radius: 3px;
    overflow: hidden;
  }
  .flash-bar-fill {
    height: 100%;
    background: var(--teal);
    border-radius: 3px;
    transition: width 0.5s ease;
  }
  .flash-bar-fill.warn  { background: var(--amber); }
  .flash-bar-fill.crit  { background: var(--red); }

  /* ── Footer ──────────────────────────────────────────────────────────── */
  footer {
    text-align: center;
    font-size: 11px;
    color: var(--text-muted);
    font-family: var(--mono);
    letter-spacing: 0.05em;
    padding-top: 24px;
    border-top: 1px solid var(--border);
  }

  /* ── Toast ───────────────────────────────────────────────────────────── */
  .toast {
    position: fixed;
    bottom: 20px; right: 20px;
    background: var(--surface);
    border: 1px solid var(--teal);
    border-radius: var(--radius);
    padding: 10px 16px;
    font-family: var(--mono);
    font-size: 12px;
    color: var(--teal);
    opacity: 0;
    transform: translateY(8px);
    transition: all 0.25s;
    z-index: 999;
    pointer-events: none;
  }
  .toast.show {
    opacity: 1;
    transform: translateY(0);
  }
  .toast.error { border-color: var(--red); color: var(--red); }

  /* ── Responsive ──────────────────────────────────────────────────────── */
  @media (max-width: 480px) {
    .stats-grid { grid-template-columns: repeat(2, 1fr); }
    .chart-wrap { height: 180px; }
    .file-row { flex-wrap: wrap; }
  }
</style>
</head>
<body>
<div class="shell">

  <!-- HEADER -->
  <header>
    <div class="logo">
      <div class="logo-icon">☀</div>
      <div>
        <div class="logo-text">HELIOS</div>
        <div class="logo-sub">Data Logger v1.0</div>
      </div>
    </div>
    <div class="header-right">
      <div class="status-pill idle" id="statusPill">
        <div class="dot" id="statusDot"></div>
        <span id="statusText">IDLE</span>
      </div>
    </div>
  </header>

  <!-- STATS GRID -->
  <div class="section-label">Live Readings</div>
  <div class="stats-grid" id="statsGrid">
    <div class="stat-card teal">
      <div class="stat-label">Irradiance</div>
      <div class="stat-value teal" id="statIrr">—</div>
      <div class="stat-sub">W / m²</div>
    </div>
    <div class="stat-card amber">
      <div class="stat-label">Illuminance</div>
      <div class="stat-value amber" id="statLux">—</div>
      <div class="stat-sub">lux</div>
    </div>
    <div class="stat-card blue">
      <div class="stat-label">Sky Blue</div>
      <div class="stat-value blue" id="statBlue">—</div>
      <div class="stat-sub">0 – 255</div>
    </div>
    <div class="stat-card green">
      <div class="stat-label">Samples</div>
      <div class="stat-value" id="statSamples">—</div>
      <div class="stat-sub">total logged</div>
    </div>
    <div class="stat-card">
      <div class="stat-label">Day</div>
      <div class="stat-value" id="statDay">—</div>
      <div class="stat-sub">of 14</div>
    </div>
    <div class="stat-card">
      <div class="stat-label">Uptime</div>
      <div class="stat-value" id="statUptime" style="font-size:16px">—</div>
      <div class="stat-sub">ms offset</div>
    </div>
  </div>

  <!-- FLASH USAGE -->
  <div class="section-label">Flash Storage</div>
  <div class="flash-bar-wrap">
    <div class="flash-bar-header">
      <span id="flashLabel">— KB used of — KB</span>
      <span id="flashPct">—%</span>
    </div>
    <div class="flash-bar-track">
      <div class="flash-bar-fill" id="flashFill" style="width:0%"></div>
    </div>
  </div>

  <!-- CHART -->
  <div class="section-label">Time Series</div>
  <div class="chart-card">
    <div class="chart-header">
      <div class="chart-title">Irradiance &amp; Sky Blue Channel</div>
      <div class="chart-legend">
        <div class="legend-item">
          <div class="legend-dot" style="background:var(--teal)"></div>
          Irradiance (W/m²)
        </div>
        <div class="legend-item">
          <div class="legend-dot" style="background:var(--blue)"></div>
          Blue channel
        </div>
      </div>
    </div>
    <div class="chart-wrap">
      <canvas id="chart"></canvas>
    </div>
    <div class="chart-range">
      <button class="range-btn active" onclick="setRange(60)"  id="r60" >1 h</button>
      <button class="range-btn"        onclick="setRange(180)" id="r180">3 h</button>
      <button class="range-btn"        onclick="setRange(360)" id="r360">6 h</button>
      <button class="range-btn"        onclick="setRange(0)"   id="rAll">All</button>
    </div>
  </div>

  <!-- FILES -->
  <div class="section-label">Logged Files</div>
  <div class="files-card">
    <div class="files-header">
      <div class="files-title">CSV Archive</div>
      <button class="btn btn-danger btn-sm" onclick="deleteAll()">⚠ Wipe All</button>
    </div>
    <div id="fileList"><div class="files-empty">No files yet.</div></div>
  </div>

  <footer>Helios-Artemis · Leading University Sylhet · 2026</footer>
</div>

<!-- TOAST -->
<div class="toast" id="toast"></div>

<script>
// ═══════════════════════════════════════════════════════════════════
//  CHART SETUP  (vanilla canvas — no library dependency)
// ═══════════════════════════════════════════════════════════════════
const canvas  = document.getElementById('chart');
const ctx     = canvas.getContext('2d');
let chartData = [];
let rangeCount = 60;  // number of samples to display

function setRange(n) {
  rangeCount = n;
  ['r60','r180','r360','rAll'].forEach(id => {
    document.getElementById(id).classList.remove('active');
  });
  const map = {60:'r60', 180:'r180', 360:'r360', 0:'rAll'};
  document.getElementById(map[n]).classList.add('active');
  drawChart();
}

function drawChart() {
  const w = canvas.parentElement.clientWidth;
  const h = canvas.parentElement.clientHeight;
  canvas.width  = w;
  canvas.height = h;

  const data = rangeCount === 0
    ? chartData
    : chartData.slice(-rangeCount);

  if (data.length === 0) {
    ctx.fillStyle = '#324858';
    ctx.font = '12px Space Mono, monospace';
    ctx.textAlign = 'center';
    ctx.fillText('No data yet — waiting for daylight', w / 2, h / 2);
    return;
  }

  const PAD  = { top: 10, right: 12, bottom: 28, left: 52 };
  const cw   = w - PAD.left - PAD.right;
  const ch   = h - PAD.top  - PAD.bottom;

  // Max values for scaling
  const maxIrr  = Math.max(...data.map(d => d.irr), 0.01);
  const maxBlue = 255;

  // Grid
  ctx.clearRect(0, 0, w, h);
  ctx.strokeStyle = '#1A2E42';
  ctx.lineWidth   = 1;
  for (let i = 0; i <= 4; i++) {
    const y = PAD.top + (ch / 4) * i;
    ctx.beginPath();
    ctx.moveTo(PAD.left, y);
    ctx.lineTo(PAD.left + cw, y);
    ctx.stroke();
    // Y axis label (irradiance)
    const val = (maxIrr * (1 - i / 4)).toFixed(2);
    ctx.fillStyle = '#5A7A8E';
    ctx.font = '9px Space Mono, monospace';
    ctx.textAlign = 'right';
    ctx.fillText(val, PAD.left - 4, y + 3);
  }

  // X axis labels
  ctx.fillStyle = '#5A7A8E';
  ctx.font = '9px Space Mono, monospace';
  ctx.textAlign = 'center';
  const xStep = Math.max(1, Math.floor(data.length / 6));
  for (let i = 0; i < data.length; i += xStep) {
    const x  = PAD.left + (i / (data.length - 1 || 1)) * cw;
    const ms = data[i].t;
    const min = Math.floor(ms / 60000);
    const label = min < 60
      ? `${min}m`
      : `${Math.floor(min/60)}h${min%60 > 0 ? (min%60)+'m' : ''}`;
    ctx.fillText(label, x, h - 6);
  }

  // Helper: map data point to canvas coords
  const px = (i)   => PAD.left + (i / (data.length - 1 || 1)) * cw;
  const pyIrr  = (v) => PAD.top  + ch - (v / maxIrr)  * ch;
  const pyBlue = (v) => PAD.top  + ch - (v / maxBlue) * ch;

  // Irradiance fill
  ctx.beginPath();
  ctx.moveTo(px(0), pyIrr(data[0].irr));
  data.forEach((d, i) => ctx.lineTo(px(i), pyIrr(d.irr)));
  ctx.lineTo(px(data.length - 1), PAD.top + ch);
  ctx.lineTo(px(0), PAD.top + ch);
  ctx.closePath();
  const grad = ctx.createLinearGradient(0, PAD.top, 0, PAD.top + ch);
  grad.addColorStop(0,   'rgba(13,155,155,0.30)');
  grad.addColorStop(1,   'rgba(13,155,155,0.02)');
  ctx.fillStyle = grad;
  ctx.fill();

  // Irradiance line
  ctx.beginPath();
  ctx.strokeStyle = '#0D9B9B';
  ctx.lineWidth   = 2;
  ctx.lineJoin    = 'round';
  data.forEach((d, i) => {
    i === 0 ? ctx.moveTo(px(i), pyIrr(d.irr)) : ctx.lineTo(px(i), pyIrr(d.irr));
  });
  ctx.stroke();

  // Blue channel line
  ctx.beginPath();
  ctx.strokeStyle = '#4B9EFF';
  ctx.lineWidth   = 1.5;
  ctx.setLineDash([4, 3]);
  data.forEach((d, i) => {
    i === 0 ? ctx.moveTo(px(i), pyBlue(d.blue)) : ctx.lineTo(px(i), pyBlue(d.blue));
  });
  ctx.stroke();
  ctx.setLineDash([]);

  // Latest value dot (irradiance)
  if (data.length > 0) {
    const last = data[data.length - 1];
    const lx = px(data.length - 1);
    const ly = pyIrr(last.irr);
    ctx.beginPath();
    ctx.arc(lx, ly, 4, 0, Math.PI * 2);
    ctx.fillStyle   = '#0D9B9B';
    ctx.fill();
    ctx.strokeStyle = '#080E14';
    ctx.lineWidth   = 2;
    ctx.stroke();
  }
}

// ═══════════════════════════════════════════════════════════════════
//  API POLLING
// ═══════════════════════════════════════════════════════════════════
let lastUptimeMs = null;

async function fetchStatus() {
  try {
    const r = await fetch('/api/status');
    const d = await r.json();

    // Status pill
    const pill = document.getElementById('statusPill');
    const dot  = document.getElementById('statusDot');
    const txt  = document.getElementById('statusText');
    if (d.logging) {
      pill.className = 'status-pill active';
      dot.classList.add('pulse');
      txt.textContent = 'LOGGING';
    } else {
      pill.className = 'status-pill idle';
      dot.classList.remove('pulse');
      txt.textContent = 'IDLE';
    }

    // Stats
    document.getElementById('statSamples').textContent = d.total_samples.toLocaleString();
    document.getElementById('statDay').textContent     = d.day;
    if (d.latest) {
      document.getElementById('statIrr').textContent   = d.latest.irradiance_wm2.toFixed(3);
      document.getElementById('statLux').textContent   = d.latest.lux.toFixed(1);
      document.getElementById('statBlue').textContent  = d.latest.blue_channel;
      document.getElementById('statUptime').textContent = d.latest.uptime_ms.toLocaleString();
      lastUptimeMs = d.latest.uptime_ms;
    }

    // Flash bar
    const pct  = d.fs_total_kb > 0 ? (d.fs_used_kb / d.fs_total_kb * 100) : 0;
    document.getElementById('flashLabel').textContent =
      `${d.fs_used_kb} KB used of ${d.fs_total_kb} KB`;
    document.getElementById('flashPct').textContent  = pct.toFixed(1) + '%';
    const fill = document.getElementById('flashFill');
    fill.style.width = pct + '%';
    fill.className   = 'flash-bar-fill' + (pct > 85 ? ' crit' : pct > 65 ? ' warn' : '');

  } catch(e) { /* connection lost during sample? ignore */ }
}

async function fetchLive() {
  try {
    const count = rangeCount === 0 ? 360 : rangeCount;
    const r = await fetch(`/api/live?count=${count}`);
    const data = await r.json();
    chartData = data;
    drawChart();
  } catch(e) {}
}

async function fetchFiles() {
  try {
    const r = await fetch('/api/files');
    const files = await r.json();
    const list  = document.getElementById('fileList');

    if (!files || files.length === 0) {
      list.innerHTML = '<div class="files-empty">No files yet — waiting for first daylight trigger.</div>';
      return;
    }

    list.innerHTML = files.map(f => `
      <div class="file-row">
        <div class="file-name">${f.name}</div>
        <div class="file-size">${formatBytes(f.size)}</div>
        <div class="file-actions">
          <a class="btn btn-teal btn-sm" href="/download?file=${encodeURIComponent(f.name)}" download>↓ CSV</a>
          <button class="btn btn-danger btn-sm" onclick="deleteFile('${encodeURIComponent(f.name)}')">✕</button>
        </div>
      </div>
    `).join('');
  } catch(e) {}
}

// ═══════════════════════════════════════════════════════════════════
//  ACTIONS
// ═══════════════════════════════════════════════════════════════════
async function deleteFile(name) {
  if (!confirm(`Delete ${name}? This cannot be undone.`)) return;
  try {
    const r = await fetch(`/api/delete?file=${encodeURIComponent(name)}`, { method: 'DELETE' });
    const d = await r.json();
    if (d.ok) { toast('File deleted'); fetchFiles(); }
    else       toast('Delete failed', true);
  } catch(e) { toast('Error', true); }
}

async function deleteAll() {
  if (!confirm('Wipe ALL logged data? This cannot be undone.')) return;
  try {
    const r = await fetch('/api/deleteall', { method: 'DELETE' });
    const d = await r.json();
    if (d.ok) {
      chartData = [];
      drawChart();
      toast('All data wiped');
      fetchFiles();
      fetchStatus();
    } else toast('Wipe failed', true);
  } catch(e) { toast('Error', true); }
}

// ═══════════════════════════════════════════════════════════════════
//  UTILITIES
// ═══════════════════════════════════════════════════════════════════
function formatBytes(b) {
  if (b < 1024)       return b + ' B';
  if (b < 1024*1024)  return (b/1024).toFixed(1) + ' KB';
  return (b/1048576).toFixed(2) + ' MB';
}

let toastTimer = null;
function toast(msg, isError = false) {
  const el = document.getElementById('toast');
  el.textContent = msg;
  el.className   = 'toast show' + (isError ? ' error' : '');
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => { el.className = 'toast'; }, 2500);
}

// ═══════════════════════════════════════════════════════════════════
//  INIT + POLLING INTERVALS
// ═══════════════════════════════════════════════════════════════════
window.addEventListener('resize', drawChart);

// Initial draw (empty)
drawChart();

// Poll status every 5s, live data every 10s, files every 30s
fetchStatus();
fetchLive();
fetchFiles();

setInterval(fetchStatus, 5000);
setInterval(fetchLive,   10000);
setInterval(fetchFiles,  30000);
</script>
</body>
</html>
)rawhtml";
