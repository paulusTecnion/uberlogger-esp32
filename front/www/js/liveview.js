// Live data page: chart (uPlot via ulchart.js) whose legend shows the live values.
// The chart shows either the live history (collected in generic.js on every
// page, ~1 point/s per channel) or a CSV file opened from the computer.
//
// NOTE: the history buffer (dataPoints), its persistence and the per-poll
// accumulation live in generic.js so data keeps recording on every tab, not
// just while this page is open. This file only renders that shared buffer.

var lvView = null;            // ULChart.View
var lvMode = "live";          // "live" | "csv"
var lvCsv = null;             // parsed CSV dataset
var lvParse = null;           // running parse (cancelable)
var liveWindowMs = 5 * 60 * 1000;
var followLive = true;

$(document).ready(function () {
  // uPlot + the shared chart engine are only needed on this page
  $.ajax({ url: "js/uplot-1.6.30.min.js", dataType: "script", cache: true })
    .then(function () { return $.ajax({ url: "js/ulchart.js?version=" + page_version, dataType: "script", cache: true }); })
    .then(initLiveView);
});

function initLiveView() {
  lvView = new ULChart.View(document.getElementById("lv-chart"), {
    utc: false,
    liveLegend: true,   // legend shows the latest values unless hovering / pinned
    onPin: updatePinLabel,
    analogHeight: fitAnalogHeight,
    onUserView: pauseFollow,
    onInteract: pauseFollow,   // any press on the chart freezes it for inspection
    onReset: function () {
      if (lvMode === "live") { setLiveWindow(liveWindowMs); }
      else { var e = lvView.extent(); if (e) lvView.setView(e[0], e[1]); }
    },
  });

  $("#lv-windows button").on("click", function () { setLiveWindow(Number($(this).data("ms"))); });
  $("#lv-clear").on("click", clearLiveView);
  $("#lv-follow").on("click", function () { setLiveWindow(liveWindowMs); });
  $("#lv-back").on("click", backToLive);
  $("#lv-export").on("click", exportChartPng);
  $("#lv-file").on("change", function (e) {
    if (e.target.files[0]) openCsvFile(e.target.files[0]);
    e.target.value = "";
  });
  initDropOverlay();

  document.addEventListener("keydown", function (e) {
    if (!lvView || !isLiveViewVisible() || /INPUT|SELECT|TEXTAREA/.test(e.target.tagName)) return;
    if (lvView.key(e)) e.preventDefault();
  });

  liveTick();
  setInterval(liveTick, 1000);
}

function isLiveViewVisible() {
  var p = document.getElementById("panel-liveview");
  return !p || p.style.display !== "none";
}

function escapeHtml(s) {
  return String(s).replace(/[&<>"']/g, function (c) { return { "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]; });
}

// Analog chart height: whatever is left of the window below the chart's top after the
// analog legend, the digital lanes (+ legend) and a small margin, so the whole live view
// fits without scrolling on a normal screen. Phones: no taller than the screen is wide.
function fitAnalogHeight() {
  if (window.innerWidth < 560) return Math.round(Math.max(220, Math.min(window.innerHeight * 0.55, window.innerWidth * 0.95)));
  var rect = document.getElementById("lv-chart").getBoundingClientRect();
  var top = rect.top + window.scrollY;
  var analog = lvView && lvView.charts.filter(function (u) { return u._lanes === undefined; })[0];
  // everything in the chart block except the analog plot itself (legends, digital lanes, gaps), as rendered now
  var below = analog ? rect.height - analog.height : 30 + 13 + (44 + 6 * 22) + 30;
  below += 12 + 14;   // card padding + page margin
  return Math.round(Math.max(220, Math.min(720, window.innerHeight - top - below)));
}

function liveTick() {
  if (!isLiveViewVisible()) return;
  if (lvMode === "live") plotLive();
  updatePlotModeLabel();
}

function updatePinLabel(t) {
  var el = $("#lv-pin");
  if (t === null || t === undefined) { el.prop("hidden", true); return; }
  el.prop("hidden", false).html('<span class="dot pin"></span>Pinned ' + ULChart.fmtTime(t, lvMode === "csv").slice(11) +
    ' · <a href="javascript:void(0)" onclick="lvView.setPin(null)">release</a>');
}

// ---------------------------------------------------------------- live chart
var CATEGORY_ORDER = { TEMPERATURE: 0, ANALOG: 0, DIGITAL: 1 };

// Turn the shared dataPoints buffer into a column dataset for the chart.
function buildLiveDataset() {
  var keys = Object.keys(dataPoints).filter(function (k) {
    return dataPoints[k].x.length && (!liveChannelKeys || liveChannelKeys.indexOf(k) >= 0);
  });
  keys.sort(function (a, b) {
    var ca = a.split(".")[0], cb = b.split(".")[0];
    var na = Number((a.match(/(\d+)$/) || [0, 0])[1]), nb = Number((b.match(/(\d+)$/) || [0, 0])[1]);
    return (CATEGORY_ORDER[ca] || 0) - (CATEGORY_ORDER[cb] || 0) || na - nb || a.localeCompare(b);
  });
  var longest = null;
  keys.forEach(function (k) { if (!longest || dataPoints[k].x.length > longest.length) longest = dataPoints[k].x; });
  var n0 = longest ? longest.length : 0;
  // polls arrive ~1/s: a jump of more than 10 s is a pause, insert an empty row there
  var breaks = [];
  for (var i = 1; i < n0; i++) if (longest[i].getTime() - longest[i - 1].getTime() > 10000) breaks.push(i);
  var n = n0 + breaks.length, t = new Float64Array(n), rowOf = new Int32Array(n0);
  for (var r = 0, b = 0, i2 = 0; i2 < n0; i2++) {
    if (b < breaks.length && breaks[b] === i2) { t[r] = (longest[i2 - 1].getTime() + longest[i2].getTime()) / 2000; r++; b++; }
    t[r] = longest[i2].getTime() / 1000;
    rowOf[i2] = r++;
  }
  var D = { n: n, t: t, cols: [], scales: [], decimals: [], names: [], types: [], pyramids: [], gaps: null, gapCount: 0 };
  keys.forEach(function (k) {
    var dot = k.indexOf("."), category = k.slice(0, dot), channel = k.slice(dot + 1);
    var y = dataPoints[k].y, col = new Float64Array(n).fill(NaN), off = n0 - y.length;
    for (var j = 0; j < y.length; j++) col[rowOf[off + j]] = Number(y[j]);
    var type = category === "DIGITAL" ? "digital" : category === "TEMPERATURE" ? "temp" : "volt";
    D.cols.push(col);
    D.scales.push(1);
    D.decimals.push(type === "temp" ? 1 : type === "digital" ? 0 : 3);
    D.names.push(typeof channelDisplayLabel === "function" ? channelDisplayLabel(category, channel) : channel);
    D.types.push(type);
    D.pyramids.push([]);
  });
  return D;
}

function liveWindowRange(D) {
  var latest = D.t[D.n - 1], start = liveWindowMs > 0 ? latest - liveWindowMs / 1000 : D.t[0];
  return [Math.max(D.t[0], start), latest];
}

function plotLive() {
  if (!lvView || !isLiveViewVisible()) return;
  var D = buildLiveDataset();
  if (!D.n) return;
  lvView.opts.utc = false;
  if (followLive) lvView.setData(D, liveWindowRange(D));
  else lvView.setData(D);
}

function pauseFollow() {
  if (lvMode === "live" && followLive) { followLive = false; updatePlotModeLabel(); }
}

function setLiveWindow(ms) {
  liveWindowMs = ms;
  followLive = true;
  if (lvView) lvView.setPin(null);   // back to live releases a pinned moment
  $("#lv-windows button").removeClass("active").filter('[data-ms="' + ms + '"]').addClass("active");
  if (lvMode === "csv") backToLive();
  else plotLive();
  updatePlotModeLabel();
}

// The follow button doubles as status: "● Live" (following, inactive), "▶ Back to live"
// (zoomed/panned, click to follow the newest data again) or "Connection lost".
function updatePlotModeLabel() {
  var el = document.getElementById("lv-follow");
  if (!el) return;
  var connected = typeof liveConnected === "undefined" ? true : liveConnected;
  var state = !connected ? "lost" : followLive ? "live" : "paused";
  if (el.getAttribute("data-state") === state) return;
  el.setAttribute("data-state", state);
  el.disabled = state !== "paused";
  el.innerHTML = state === "lost" ? '<span class="dot bad"></span>Connection lost'
    : state === "live" ? '<span class="dot ok"></span>Live'
    : "&#9654; Back to live";
}

// Empty the history buffer + saved snapshot, then re-seed with the latest
// reading so every channel keeps one live point.
function clearLiveView() {
  if (typeof clearDataPoints === "function") clearDataPoints();
  if (typeof accumulateReadings === "function" && valuesData && valuesData["READINGS"]) accumulateReadings(valuesData);
  followLive = true;
  plotLive();
  updatePlotModeLabel();
}

// Export the chart as shown (live or CSV) as a PNG with a timestamped name.
function exportChartPng() {
  if (!lvView || !lvView.charts.length) return;
  var now = new Date(), p2 = function (v) { return String(v).padStart(2, "0"); };
  var stamp = now.getFullYear() + "-" + p2(now.getMonth() + 1) + "-" + p2(now.getDate()) + "_" + p2(now.getHours()) + "-" + p2(now.getMinutes()) + "-" + p2(now.getSeconds());
  var title, name;
  if (lvMode === "csv" && lvCsv) {
    title = "Uberlogger · " + lvCsv.fileName;
    name = lvCsv.fileName.replace(/\.csv$/i, "") + "_" + stamp + ".png";
  } else {
    title = "Uberlogger · Live data";
    name = "uberlogger_live_" + stamp + ".png";
  }
  lvView.exportPng(title, name).catch(function (e) { $("#lv-error").text(e.message); });
}

// ---------------------------------------------------------------- CSV viewing
// Warn before opening a file that probably needs more memory than this device's browser allows.
function openCsvFile(file) {
  if (!lvView) return;
  ULChart.estimateCsvMemory(file).then(function (est) {
    if (est.bytes > est.budget) {
      var msg = file.name + " (" + ULChart.fmtBytes(file.size) + ", about " + Math.round(est.rows).toLocaleString() +
        " rows) needs about " + ULChart.fmtBytes(est.bytes) + " of memory to view.\n\n" +
        (est.mobile ? "On a phone or tablet the browser may close the page. " : "This may be more than your browser allows. ") +
        "Viewing it on a computer, or a smaller (split) file, is safer.\n\nOpen anyway?";
      if (!window.confirm(msg)) return;
    }
    loadCsvFile(file);
  }).catch(function () { loadCsvFile(file); });
}

function loadCsvFile(file) {
  if (typeof currentPage !== "undefined" && currentPage !== "liveview" && typeof showPage === "function") showPage("liveview", false);
  if (lvParse) lvParse.cancel();
  $("#lv-error").text("");
  $("#lv-progress").prop("hidden", false).children().css("width", "0");
  $("#lv-csv-info").text("Loading " + file.name + " (" + ULChart.fmtBytes(file.size) + ")…");
  showCsvControls(true);
  lvParse = ULChart.parseCsvFile(file, function (p) { $("#lv-progress > div").css("width", (p * 100).toFixed(1) + "%"); });
  lvParse.then(function (D) {
    lvParse = null;
    $("#lv-progress").prop("hidden", true);
    lvCsv = D;
    lvMode = "csv";
    lvView.opts.utc = true;
    lvView.opts.liveLegend = false;
    lvView.setPin(null);
    lvView.setData(D, [D.t[0], D.t[D.n - 1]]);
    var dur = D.t[D.n - 1] - D.t[0], rate = D.n > 1 && dur > 0 ? (D.n - 1) / dur : 0;
    $("#lv-csv-info").html(
      "<b>" + escapeHtml(D.fileName) + "</b> · " + ULChart.fmtBytes(D.fileSize) + " · " + D.n.toLocaleString() + " rows · " +
      ULChart.fmtDur(dur) + " · ≈" + rate.toFixed(rate < 10 ? 2 : 0) + " Hz · times in UTC · " +
      (D.gapCount ? '<span class="error-text">' + D.gapCount.toLocaleString() + " timestamp gap" + (D.gapCount > 1 ? "s" : "") + " (red lines)</span>" : "no timestamp gaps")
    );
  }).catch(function (err) {
    lvParse = null;
    $("#lv-progress").prop("hidden", true);
    $("#lv-error").text("Could not open " + file.name + ": " + err.message);
    if (lvMode === "live") showCsvControls(false);
  });
}

function showCsvControls(on) {
  $("#lv-live-controls").prop("hidden", on);
  $("#lv-csv-controls").prop("hidden", !on);
}

function backToLive() {
  if (lvParse) { lvParse.cancel(); lvParse = null; $("#lv-progress").prop("hidden", true); }
  lvMode = "live";
  lvCsv = null;
  followLive = true;
  lvView.opts.liveLegend = true;
  lvView.setPin(null);
  showCsvControls(false);
  $("#lv-error").text("");
  plotLive();
  updatePlotModeLabel();
}

// Full-page "Drop the CSV file here" overlay while a file is dragged over the window.
function initDropOverlay() {
  var overlay = document.getElementById("drop-overlay");
  document.body.appendChild(overlay); // fixed overlay must not live in a hidden panel
  var depth = 0, timer = 0;
  var isFileDrag = function (e) { return e.dataTransfer && Array.prototype.indexOf.call(e.dataTransfer.types, "Files") >= 0; };
  var show = function (on) {
    overlay.classList.toggle("show", on);
    if (on) document.getElementById("drop-overlay-sub").textContent = lvCsv ? "Replaces " + lvCsv.fileName : "Uberlogger CSV file";
  };
  document.addEventListener("dragenter", function (e) { if (!isFileDrag(e)) return; e.preventDefault(); depth++; show(true); });
  document.addEventListener("dragover", function (e) {
    if (!isFileDrag(e)) return;
    e.preventDefault();
    e.dataTransfer.dropEffect = "copy";
    show(true);
    clearTimeout(timer);
    timer = setTimeout(function () { depth = 0; show(false); }, 400); // drag cancelled
  });
  document.addEventListener("dragleave", function (e) { if (!isFileDrag(e)) return; depth = Math.max(0, depth - 1); if (!depth) show(false); });
  document.addEventListener("drop", function (e) {
    if (!isFileDrag(e)) return;
    e.preventDefault();
    depth = 0;
    show(false);
    var f = e.dataTransfer.files[0];
    if (f) openCsvFile(f);
  });
}
