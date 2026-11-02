/*
 * Uberlogger chart engine (shared by the live view and the CSV viewer).
 *
 * - ULChart.parseCsvFile(file, onProgress): parses an Uberlogger CSV in a Web
 *   Worker. Values are stored exactly as 32-bit fixed-point integers (the way
 *   the logger writes them) with a min/max pyramid, so multi-GB files stay fast.
 * - new ULChart.View(container, opts): uPlot based chart with an analog panel
 *   (V left, degC right) and a digital panel (one lane per input). Draws at most
 *   ~2 points per pixel per channel (min/max decimation) at any zoom level.
 *
 * A dataset is { n, t: Float64Array (unix seconds), cols: [Int32Array|Float64Array],
 * scales: [divisor per column], decimals: [...], names: [...], types: ["volt"|"temp"|"digital"],
 * pyramids: [[{B, mn, mx}]], gaps: Float64Array (pairs), gapCount }.
 * Needs uPlot (js/uplot-*.min.js).
 */
(function (global) {
  "use strict";

  var MISSING = -2147483648;
  var POW10 = [1, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9];
  var COLORS = ["#2563eb", "#dc2626", "#16a34a", "#d97706", "#7c3aed", "#0891b2", "#db2777", "#65a30d"];
  var DCOLORS = ["#0f766e", "#b45309", "#7c3aed", "#be123c", "#1d4ed8", "#4d7c0f"];

  // ---------------------------------------------------------------- CSV parser (Web Worker)
  var WORKER_SRC = function () {
    "use strict";
    var POW10 = [1, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 1e10, 1e11, 1e12, 1e13, 1e14, 1e15];
    var LEVELS = [64, 4096, 262144];
    var MISSING = -2147483648;

    self.onmessage = function (e) {
      parse(e.data.file).catch(function (err) {
        self.postMessage({ type: "error", message: String((err && err.message) || err) });
      });
    };

    async function parse(file) {
      var t0 = Date.now(), size = file.size;
      var reader = file.stream().pipeThrough(new TextDecoderStream()).getReader();
      var rest = "", bytes = 0, lastProgress = 0;
      var names = null, sep = ",", dec = ".", ncol = 0;
      var cap = 0, n = 0, t = null, cols = null, decimals = null;
      var dayKey = "", dayMs = 0, bad = 0, skipped = 0;

      function grow(newCap) {
        var nt = new Float64Array(newCap); nt.set(t.subarray(0, n)); t = nt;
        cols = cols.map(function (c) { var nc = new Int32Array(newCap); nc.set(c.subarray(0, n)); return nc; });
        cap = newCap;
      }

      function line(s, a, b) {
        if (b <= a) return;
        if (names === null) {
          var h = s.slice(a, b);
          sep = h.indexOf(";") >= 0 ? ";" : ",";
          names = h.split(sep).map(function (x) { return x.trim(); });
          ncol = names.length - 1;
          return;
        }
        var c0 = s.charCodeAt(a);
        if (c0 < 48 || c0 > 57) { skipped++; return; }
        if (t === null) {
          var first = s.slice(a, b);
          if (sep === ";" && first.indexOf(",") >= 0) dec = ",";
          cap = Math.ceil(size / (b - a + 2) * 1.1) + 1024;
          t = new Float64Array(cap);
          cols = [];
          for (var q = 0; q < ncol; q++) cols.push(new Int32Array(cap));
          decimals = new Array(ncol).fill(-1);
        }
        if (n >= cap) grow(Math.ceil(cap * 1.5));

        var key = s.substr(a, 10);
        if (key !== dayKey) {
          dayKey = key;
          dayMs = Date.UTC(+key.substr(0, 4), +key.substr(5, 2) - 1, +key.substr(8, 2));
        }
        var d2 = function (i) { return (s.charCodeAt(i) - 48) * 10 + (s.charCodeAt(i + 1) - 48); };
        var p = a + 20, ms = 0, c;
        while (p < b && (c = s.charCodeAt(p)) >= 48 && c <= 57) { ms = ms * 10 + (c - 48); p++; }
        t[n] = (dayMs + ((d2(a + 11) * 60 + d2(a + 14)) * 60 + d2(a + 17)) * 1000 + ms) / 1000;

        var sepc = sep.charCodeAt(0), decc = dec.charCodeAt(0);
        for (var k = 0; k < ncol; k++) {
          if (p >= b || s.charCodeAt(p) !== sepc) { cols[k][n] = MISSING; bad++; continue; }
          p++;
          var neg = false, ip = 0, fp = 0, fd = 0, digits = 0;
          if (s.charCodeAt(p) === 45) { neg = true; p++; }
          while (p < b && (c = s.charCodeAt(p)) >= 48 && c <= 57) { ip = ip * 10 + (c - 48); p++; digits++; }
          if (p < b && s.charCodeAt(p) === decc) {
            p++;
            while (p < b && (c = s.charCodeAt(p)) >= 48 && c <= 57) { if (fd < 15) { fp = fp * 10 + (c - 48); fd++; } p++; digits++; }
          }
          if (!digits) { cols[k][n] = MISSING; bad++; continue; }
          if (decimals[k] < 0) decimals[k] = Math.min(fd, 9); // fixed by the column's first value
          var dk = decimals[k];
          var v = ip * POW10[dk] + (fd <= dk ? fp * POW10[dk - fd] : Math.round(fp / POW10[fd - dk]));
          if (v > 2147483647) { cols[k][n] = MISSING; bad++; continue; }
          cols[k][n] = neg ? -v : v;
        }
        n++;
      }

      for (;;) {
        var r = await reader.read();
        if (r.done) break;
        bytes += r.value.length;
        var chunk = rest + r.value, start = 0, idx;
        while ((idx = chunk.indexOf("\n", start)) !== -1) {
          var end = idx;
          if (end > start && chunk.charCodeAt(end - 1) === 13) end--;
          line(chunk, start, end);
          start = idx + 1;
        }
        rest = chunk.slice(start);
        var now = Date.now();
        if (now - lastProgress > 100) { lastProgress = now; self.postMessage({ type: "progress", value: Math.min(1, bytes / size) }); }
      }
      if (rest.length) line(rest, 0, rest.charCodeAt(rest.length - 1) === 13 ? rest.length - 1 : rest.length);
      if (!names || n === 0) throw new Error("No data rows found. Is this an Uberlogger CSV file?");

      t = t.subarray(0, n);
      cols = cols.map(function (c) { return c.subarray(0, n); });
      decimals = decimals.map(function (d) { return Math.max(0, d); });

      var pyramids = cols.map(function (col) {
        var lv = [], srcMin = col, srcMax = col, srcN = n, prevB = 1;
        for (var li = 0; li < LEVELS.length; li++) {
          var B = LEVELS[li], step = B / prevB, m = Math.ceil(srcN / step);
          if (m < 2) break;
          var mn = new Int32Array(m), mx = new Int32Array(m);
          for (var j = 0; j < m; j++) {
            var lo = Infinity, hi = -Infinity, e2 = Math.min(srcN, (j + 1) * step);
            for (var i = j * step; i < e2; i++) {
              var av = srcMin[i];
              if (av === MISSING) continue;
              if (av < lo) lo = av;
              if (srcMax[i] > hi) hi = srcMax[i];
            }
            if (lo === Infinity) { mn[j] = MISSING; mx[j] = MISSING; } else { mn[j] = lo; mx[j] = hi; }
          }
          lv.push({ B: B, mn: mn, mx: mx });
          srcMin = mn; srcMax = mx; srcN = m; prevB = B;
        }
        return lv;
      });

      var stats = cols.map(function (col, k) {
        var lo = Infinity, hi = -Infinity, lv = pyramids[k], top = lv.length ? lv[lv.length - 1] : null;
        var a1 = top ? top.mn : col, a2 = top ? top.mx : col;
        for (var j = 0; j < a1.length; j++) { if (a1[j] === MISSING) continue; if (a1[j] < lo) lo = a1[j]; if (a2[j] > hi) hi = a2[j]; }
        return { min: lo / POW10[decimals[k]], max: hi / POW10[decimals[k]] };
      });
      var types = names.slice(1).map(function (name, k) {
        if (decimals[k] === 0 && stats[k].min >= 0 && stats[k].max <= 1) return "digital";
        if (decimals[k] === 1) return "temp";
        return "volt";
      });

      var sample = [], stepI = Math.max(1, Math.floor(n / 20000));
      for (var i2 = 1; i2 < n && sample.length < 20000; i2 += stepI) sample.push(t[i2] - t[i2 - 1]);
      sample.sort(function (x, y) { return x - y; });
      var med = sample.length ? sample[sample.length >> 1] : 0;
      var gaps = [], gapCount = 0;
      if (med > 0) {
        var lim = med * 1.75;
        for (var i3 = 1; i3 < n; i3++) {
          var dt = t[i3] - t[i3 - 1];
          if (dt > lim || dt < 0) { gapCount++; if (gaps.length < 100000) gaps.push(t[i3 - 1], t[i3]); }
        }
      }
      var gapArr = new Float64Array(gaps);

      var transfer = [t.buffer, gapArr.buffer];
      cols.forEach(function (c) { transfer.push(c.buffer); });
      pyramids.forEach(function (lv) { lv.forEach(function (l) { transfer.push(l.mn.buffer, l.mx.buffer); }); });
      self.postMessage({
        type: "done", names: names.slice(1), n: n, t: t, cols: cols, pyramids: pyramids, decimals: decimals,
        scales: decimals.map(function (d) { return POW10[d]; }), types: types,
        gaps: gapArr, gapCount: gapCount, medianDt: med, bad: bad, skipped: skipped, parseMs: Date.now() - t0,
      }, transfer);
    }
  };

  // Estimate the memory needed to view a CSV (from its first 64 kB) and a budget
  // for this device. The viewer stores 8 bytes (time) + 4 bytes per column per row.
  function estimateCsvMemory(file) {
    return file.slice(0, 65536).text().then(function (txt) {
      var lines = txt.split("\n").filter(function (l) { return l.length; });
      var data = lines.filter(function (l) { return /^\d/.test(l); });
      var head = lines[0] || "", ncol = Math.max(1, head.split(head.indexOf(";") >= 0 ? ";" : ",").length - 1);
      var avg = data.length > 1 ? (data.slice(0, -1).join("\n").length + data.length - 1) / (data.length - 1) : 100;
      var rows = file.size / Math.max(10, avg);
      var need = rows * (8 + 4 * ncol) * 1.15;   // + over-allocation and pyramids
      var mobile = window.matchMedia && window.matchMedia("(pointer: coarse)").matches && Math.min(screen.width, screen.height) < 900;
      var budget = mobile ? 400e6 : 2.5e9;
      if (navigator.deviceMemory) budget = Math.min(budget, navigator.deviceMemory * 1e9 * (mobile ? 0.15 : 0.3));
      return { rows: rows, bytes: need, budget: budget, mobile: !!mobile };
    });
  }

  var workerUrl = null;
  function parseCsvFile(file, onProgress) {
    if (!workerUrl) {
      var src = "(" + WORKER_SRC.toString() + ")();";
      workerUrl = URL.createObjectURL(new Blob([src], { type: "text/javascript" }));
    }
    var w = new Worker(workerUrl);
    var p = new Promise(function (resolve, reject) {
      w.onmessage = function (e) {
        var m = e.data;
        if (m.type === "progress") { if (onProgress) onProgress(m.value); }
        else if (m.type === "error") { w.terminate(); reject(new Error(m.message)); }
        else if (m.type === "done") { w.terminate(); m.fileName = file.name; m.fileSize = file.size; resolve(m); }
      };
      w.onerror = function (e) { w.terminate(); reject(new Error("Parser error: " + e.message)); };
    });
    p.cancel = function () { w.terminate(); };
    w.postMessage({ file: file });
    return p;
  }

  // ---------------------------------------------------------------- helpers
  function pad(v, w) { return String(v).padStart(w, "0"); }
  function fmtTime(ts, utc) {
    if (!isFinite(ts)) return "—";
    var d = new Date(Math.round(ts * 1000));
    if (utc) {
      return d.getUTCFullYear() + "-" + pad(d.getUTCMonth() + 1, 2) + "-" + pad(d.getUTCDate(), 2) + " " +
        pad(d.getUTCHours(), 2) + ":" + pad(d.getUTCMinutes(), 2) + ":" + pad(d.getUTCSeconds(), 2) + "." + pad(d.getUTCMilliseconds(), 3);
    }
    return d.getFullYear() + "-" + pad(d.getMonth() + 1, 2) + "-" + pad(d.getDate(), 2) + " " +
      pad(d.getHours(), 2) + ":" + pad(d.getMinutes(), 2) + ":" + pad(d.getSeconds(), 2) + "." + pad(d.getMilliseconds(), 3);
  }
  function fmtBytes(b) { return b > 1e9 ? (b / 1e9).toFixed(2) + " GB" : b > 1e6 ? (b / 1e6).toFixed(1) + " MB" : (b / 1e3).toFixed(0) + " kB"; }
  function fmtDur(s) {
    if (s >= 60) s = Math.round(s); // whole seconds, so 899.98 s reads "15 min 0 s", not "14 min 60 s"
    var h = Math.floor(s / 3600), m = Math.floor(s % 3600 / 60), x = s % 60;
    return (h ? h + " h " : "") + (h || m ? m + " min " : "") + x.toFixed(h || m ? 0 : 1) + " s";
  }
  // exact text of a stored fixed-point value, e.g. -126774848 with 6 decimals -> "-126.774848"
  function fmtFixed(v, dec) {
    if (v === MISSING) return "—";
    if (!dec) return String(v);
    var neg = v < 0, a = String(Math.abs(v)).padStart(dec + 1, "0");
    return (neg ? "-" : "") + a.slice(0, a.length - dec) + "." + a.slice(a.length - dec);
  }
  function lowerBound(a, x) { var lo = 0, hi = a.length; while (lo < hi) { var m = (lo + hi) >> 1; if (a[m] < x) lo = m + 1; else hi = m; } return lo; }
  function cssVar(el, name, fallback) { var v = getComputedStyle(el).getPropertyValue(name).trim(); return v || fallback; }

  // min/max of column k over raw index range [a, b), using the pyramid when present
  function rangeMinMax(D, k, a, b, out) {
    var lo = Infinity, hi = -Infinity, col = D.cols[k], lv = (D.pyramids && D.pyramids[k]) || [];
    (function rec(a, b, level) {
      if (a >= b) return;
      while (level >= 0) {
        var B = lv[level].B, s = Math.ceil(a / B) * B, e = Math.floor(b / B) * B;
        if (e > s) {
          rec(a, s, level - 1);
          var mn = lv[level].mn, mx = lv[level].mx;
          for (var j = s / B; j < e / B; j++) { if (mn[j] === MISSING) continue; if (mn[j] < lo) lo = mn[j]; if (mx[j] > hi) hi = mx[j]; }
          rec(e, b, level - 1);
          return;
        }
        level--;
      }
      for (var i = a; i < b; i++) { var v = col[i]; if (v === MISSING || v !== v) continue; if (v < lo) lo = v; if (v > hi) hi = v; }
    })(a, b, lv.length - 1);
    var sc = D.scales[k];
    if (lo === Infinity) { out[0] = out[1] = NaN; } else { out[0] = lo / sc; out[1] = hi / sc; }
  }

  // plot data for [min, max]: raw points when zoomed in, per-pixel min/max otherwise
  function buildData(D, colIdx, min, max, width) {
    var t = D.t, n = D.n;
    var i0 = Math.max(0, lowerBound(t, min) - 1), i1 = Math.min(n, lowerBound(t, max) + 1);
    var N = Math.max(0, i1 - i0), W = Math.max(50, Math.floor(width));
    if (N <= W * 3) {
      return [t.subarray(i0, i1)].concat(colIdx.map(function (k) {
        // plain array: uPlot draws a gap at null (NaN would be joined over)
        var col = D.cols[k], sc = D.scales[k], y = new Array(N);
        for (var i = 0; i < N; i++) { var v = col[i0 + i]; y[i] = v === MISSING || v !== v ? null : v / sc; }
        return y;
      }));
    }
    var x = new Float64Array(W * 2), ys = colIdx.map(function () { return new Array(W * 2); }), mm = [0, 0];
    for (var j = 0; j < W; j++) {
      var a = i0 + Math.floor(j * N / W), b = i0 + Math.floor((j + 1) * N / W);
      x[2 * j] = t[a];
      x[2 * j + 1] = t[(a + b - 1) >> 1];
      for (var s = 0; s < colIdx.length; s++) {
        rangeMinMax(D, colIdx[s], a, b, mm);
        ys[s][2 * j] = mm[0] === mm[0] ? mm[0] : null;
        ys[s][2 * j + 1] = mm[1] === mm[1] ? mm[1] : null;
      }
    }
    return [x].concat(ys);
  }

  function valueText(D, k, i) {
    var v = D.cols[k][i];
    if (D.cols[k] instanceof Int32Array) return fmtFixed(v, D.decimals[k]);
    return v === v ? v.toFixed(D.decimals[k]) : "—";
  }

  // ---------------------------------------------------------------- chart view
  // opts: { utc: bool (axis/legend in UTC), onUserView(min,max), onReset(), analogHeight(): px }
  function View(container, opts) {
    this.el = container;
    this.opts = opts || {};
    this.D = null;
    this.charts = [];
    this.view = null;
    this.signature = "";
    this.updating = false;
    this.pinned = null;             // unix seconds of a clicked (pinned) sample, or null
    this.el.classList.add("ulchart");
    this.analogEl = document.createElement("div");
    this.digitalEl = document.createElement("div");
    this.analogEl.className = "ulchart-panel";
    this.digitalEl.className = "ulchart-panel ulchart-digital";
    this.el.appendChild(this.analogEl);
    this.el.appendChild(this.digitalEl);
    var self = this, timer = 0;
    this._onResize = function () {
      clearTimeout(timer);
      timer = setTimeout(function () { self.resize(); }, 100);
    };
    window.addEventListener("resize", this._onResize);
    // axis/grid colours come from CSS variables: rebuild when the theme changes
    this._onTheme = function () { if (self.D) self.build(); };
    window.addEventListener("ul-theme", this._onTheme);
    if (window.matchMedia) {
      var mq = window.matchMedia("(prefers-color-scheme: dark)");
      if (mq.addEventListener) mq.addEventListener("change", this._onTheme);
    }
  }

  View.prototype.destroy = function () {
    this.charts.forEach(function (u) { u.destroy(); });
    this.charts = [];
    window.removeEventListener("resize", this._onResize);
    this.el.innerHTML = "";
  };

  View.prototype.extent = function () {
    var D = this.D;
    return D && D.n ? [D.t[0], D.t[D.n - 1]] : null;
  };

  // Show a dataset. Charts are only rebuilt when the set of channels changes, so
  // a live view can call this every second without flicker or losing legend toggles.
  View.prototype.setData = function (D, view) {
    var sig = D.names.join("|") + "#" + D.types.join("|") + "#" + (this.opts.utc ? 1 : 0);
    this.D = D;
    if (view) this.view = view.slice();
    else if (!this.view) this.view = this.extent();
    if (sig !== this.signature || !this.charts.length) {
      this.signature = sig;
      this.build();
    } else {
      this.refresh();
    }
  };

  View.prototype.width = function () {
    var w = this.el.clientWidth;
    return Math.max(280, w - 2);
  };

  View.prototype.build = function () {
    var self = this, D = this.D, utc = !!this.opts.utc;
    this.charts.forEach(function (u) { u.destroy(); });
    this.charts = [];
    this.analogEl.innerHTML = "";
    this.digitalEl.innerHTML = "";
    if (!D || !D.n) return;
    this._narrow = this.width() < 560;

    var analog = [], digital = [];
    D.types.forEach(function (ty, k) { (ty === "digital" ? digital : analog).push(k); });
    var hasTemp = analog.some(function (k) { return D.types[k] === "temp"; });
    var hasVolt = analog.some(function (k) { return D.types[k] === "volt"; });
    var narrow = this.width() < 560;
    var leftSize = narrow ? 46 : (hasVolt ? 64 : 56), rightSize = hasVolt && hasTemp ? (narrow ? 40 : 56) : 0;
    var vLabel = narrow ? null : "Volt", cLabel = narrow ? null : "°C", gutterLabel = narrow ? null : "\u00a0";
    this.analogEl.style.display = analog.length ? "" : "none";
    this.digitalEl.style.display = digital.length ? "" : "none";

    var muted = cssVar(this.el, "--ul-muted", "#5d6676"), grid = cssVar(this.el, "--ul-grid", "#e7eaf0");
    var axisStyle = function () { return { stroke: muted, grid: { stroke: grid, width: 1 }, ticks: { stroke: grid, width: 1 } }; };
    var sync = uPlot.sync("ul-" + Math.random().toString(36).slice(2));
    var tzDate = utc ? function (ts) { return uPlot.tzDate(new Date(ts * 1e3), "Etc/UTC"); } : function (ts) { return new Date(ts * 1e3); };
    var xAxis = Object.assign(axisStyle(), {
      values: [
        [3600 * 24 * 365, "{YYYY}", null, null, null, null, null, null, 1],
        [3600 * 24, "{MM}-{DD}", "\n{YYYY}", null, null, null, null, null, 1],
        [3600, "{HH}:{mm}", "\n{YYYY}-{MM}-{DD}", null, "\n{MM}-{DD}", null, null, null, 1],
        [60, "{HH}:{mm}", "\n{YYYY}-{MM}-{DD}", null, "\n{MM}-{DD}", null, null, null, 1],
        // second ticks: only {HH}:{mm} under the first tick, a date there runs into the
        // next minute label (the legend shows the full date and time)
        [1, ":{ss}", "\n{HH}:{mm}", null, "\n{MM}-{DD} {HH}:{mm}", null, "\n{HH}:{mm}", null, 1],
        [0.001, ":{ss}.{fff}", "\n{HH}:{mm}", null, "\n{MM}-{DD} {HH}:{mm}", null, "\n{HH}:{mm}", null, 1],
      ],
    });
    // Legend values: the sample under the cursor, else the pinned sample, else
    // (live view) the latest sample. Always the raw value, not a decimated one.
    var xSeries = {
      label: utc ? "Time (UTC)" : "Time",
      value: function (u) { var i = self.legendIdx(u); return i >= 0 ? fmtTime(self.D.t[i], utc) : "—"; },
    };
    var rawValue = function (k) {
      return function (u) { var i = self.legendIdx(u); return i >= 0 ? valueText(self.D, k, i) : "—"; };
    };
    var common = function () {
      return {
        tzDate: tzDate,
        cursor: {
          sync: { key: sync.key },
          drag: { x: true, y: false, setScale: true, dist: 5 },   // a click never zooms
          bind: { dblclick: function () { return function () { self.setPin(null); self.reset(); return null; }; } },
        },
        hooks: {
          setScale: [function (u, key) { if (key === "x" && !self.updating) self.userView(u.scales.x.min, u.scales.x.max); }],
          setSeries: [function () { if (!self.updating) self.refresh(); }],
          draw: [function (u) { self.drawGaps(u); self.drawPin(u); }],
        },
      };
    };

    var w = this.width();
    if (analog.length) {
      var scales = { x: { time: true, auto: false } }, axes = [xAxis];
      if (hasVolt) { scales.V = { auto: false }; axes.push(Object.assign(axisStyle(), { scale: "V", label: vLabel, size: leftSize })); }
      if (hasTemp) {
        scales.C = { auto: false };
        axes.push(Object.assign(axisStyle(), { scale: "C", label: cLabel, side: hasVolt ? 1 : 3, size: hasVolt ? rightSize : leftSize, grid: { show: !hasVolt, stroke: grid } }));
      }
      var o = Object.assign(common(), {
        width: w,
        height: this.opts.analogHeight ? this.opts.analogHeight() : 360,
        scales: scales, axes: axes,
        series: [xSeries].concat(analog.map(function (k, i) {
          return {
            label: D.names[k] + (D.types[k] === "temp" ? " (°C)" : " (V)"),
            scale: D.types[k] === "temp" ? "C" : "V",
            stroke: COLORS[i % COLORS.length], width: 1.25, points: { show: false },
            value: rawValue(k),
          };
        })),
      });
      var ua = new uPlot(o, buildData(D, analog, this.view[0], this.view[1], w), this.analogEl);
      ua._cols = analog;
      sync.sub(ua);
      this.attachPanZoom(ua);
      this.charts.push(ua);
    }
    if (digital.length) {
      var L = 1.5, H = digital.length * L, dscales = { x: { time: true, auto: false } }, lanes = {};
      digital.forEach(function (k, i) {
        var base = (digital.length - 1 - i) * L;
        lanes["d" + i] = [-0.25 - base, H - 0.25 - base];
        dscales["d" + i] = { auto: false };
      });
      var gutter = function (side, size) {
        return { scale: "d0", side: side, size: size, label: gutterLabel, values: function (u, sp) { return sp.map(function () { return ""; }); }, grid: { show: false }, ticks: { show: false } };
      };
      var od = Object.assign(common(), {
        width: w,
        height: 66 + digital.length * 22,   // the time axis below takes ~66 px; 22 px per lane
        scales: dscales,
        axes: [xAxis, gutter(3, leftSize)].concat(rightSize ? [gutter(1, rightSize)] : []),
        series: [xSeries].concat(digital.map(function (k, i) {
          return {
            label: D.names[k], scale: "d" + i,
            stroke: DCOLORS[i % DCOLORS.length], width: 1.25, points: { show: false },
            paths: uPlot.paths.stepped({ align: 1 }),
            value: rawValue(k),
          };
        })),
      });
      od.hooks.draw.push(function (u) { self.drawLaneLabels(u); });
      var ud = new uPlot(od, buildData(D, digital, this.view[0], this.view[1], w), this.digitalEl);
      ud._cols = digital;
      ud._lanes = lanes;
      sync.sub(ud);
      this.attachPanZoom(ud);
      this.charts.push(ud);
    }
    this.refresh();
    // heights may depend on the rendered legends (analogHeight measures them): settle once
    var self2 = this;
    requestAnimationFrame(function () { self2.resize(); });
  };

  View.prototype.resize = function () {
    if (!this.charts.length || !this.el.offsetParent) return;
    if ((this.width() < 560) !== this._narrow) { this.build(); return; }
    var w = this.width(), self = this;
    this.charts.forEach(function (u) {
      var h = (u._lanes === undefined && self.opts.analogHeight) ? self.opts.analogHeight() : u.height;
      if (u.width !== w || u.height !== h) u.setSize({ width: w, height: h });
    });
    this.refresh();
  };

  // clamp + apply a view; does not notify (used by the owner, e.g. live follow)
  View.prototype.setView = function (min, max) {
    var ext = this.extent();
    if (!ext) return;
    var span = Math.max(max - min, 0.001);
    if (ext[1] > ext[0]) {
      span = Math.min(span, Math.max(ext[1] - ext[0], 0.001));
      min = Math.max(ext[0], Math.min(min, ext[1] - span));
    }
    this.view = [min, min + span];
    this.refresh();
  };

  View.prototype.userView = function (min, max) {
    this.setView(min, max);
    if (this.opts.onUserView) this.opts.onUserView(this.view[0], this.view[1]);
  };

  View.prototype.reset = function () {
    if (this.opts.onReset) this.opts.onReset();
    else { var e = this.extent(); if (e) this.setView(e[0], e[1]); }
  };

  View.prototype.refresh = function () {
    var D = this.D, self = this;
    if (!D || !this.charts.length || !this.view) return;
    this.updating = true;
    try {
      this.charts.forEach(function (u) {
        var data = buildData(D, u._cols, self.view[0], self.view[1], u.bbox.width / devicePixelRatio || self.width());
        u.batch(function () {
          u.setData(data, false);
          u.setScale("x", { min: self.view[0], max: self.view[1] });
          var ranges = {};
          u.series.forEach(function (s, i) {
            if (i === 0 || !s.show || (s.scale !== "V" && s.scale !== "C")) return;
            var arr = data[i], r = ranges[s.scale] || [Infinity, -Infinity];
            for (var j = 0; j < arr.length; j++) { var v = arr[j]; if (v === null) continue; if (v < r[0]) r[0] = v; if (v > r[1]) r[1] = v; }
            ranges[s.scale] = r;
          });
          Object.keys(ranges).forEach(function (key) {
            var lo = ranges[key][0], hi = ranges[key][1];
            if (!isFinite(lo)) { lo = 0; hi = 1; }
            var span = hi - lo, p = span > 0 ? span * 0.06 : Math.max(Math.abs(hi) * 0.01, key === "C" ? 0.5 : 0.01);
            u.setScale(key, { min: lo - p, max: hi + p });
          });
          if (u._lanes) Object.keys(u._lanes).forEach(function (key) { u.setScale(key, { min: u._lanes[key][0], max: u._lanes[key][1] }); });
        });
      });
    } finally { this.updating = false; }
    this.updateLegends();
  };

  // nearest raw sample index for a time
  View.prototype.indexAt = function (x) {
    var D = this.D, i = lowerBound(D.t, x);
    if (i >= D.n) return D.n - 1;
    if (i > 0 && x - D.t[i - 1] <= D.t[i] - x) i--;
    return i;
  };

  // which sample the legend shows: cursor > pinned > latest (live view only)
  View.prototype.legendIdx = function (u) {
    var D = this.D;
    if (!D || !D.n) return -1;
    if (u.cursor.left >= 0) return this.indexAt(u.posToVal(u.cursor.left, "x"));
    if (this.pinned !== null) return this.indexAt(this.pinned);
    return this.opts.liveLegend ? D.n - 1 : -1;
  };

  // re-evaluate legend values of charts the mouse is not over (live updates, pin changes)
  View.prototype.updateLegends = function () {
    this.charts.forEach(function (u) { if (u.cursor.left < 0) u.setLegend({ idx: 0 }, false); });
  };

  View.prototype.setPin = function (t) {
    this.pinned = t;
    this.charts.forEach(function (u) { u.redraw(false, false); });
    this.updateLegends();
    if (this.opts.onPin) this.opts.onPin(t);
  };

  View.prototype.drawPin = function (u) {
    if (this.pinned === null || !this.view || this.pinned < this.view[0] || this.pinned > this.view[1]) return;
    var ctx = u.ctx, x = Math.round(u.valToPos(this.pinned, "x", true)) + 0.5, dpr = devicePixelRatio;
    ctx.save();
    ctx.strokeStyle = cssVar(this.el, "--accent", "#f26b3a");
    ctx.lineWidth = 1.5 * dpr;
    ctx.setLineDash([5 * dpr, 4 * dpr]);
    ctx.beginPath();
    ctx.moveTo(x, u.bbox.top);
    ctx.lineTo(x, u.bbox.top + u.bbox.height);
    ctx.stroke();
    ctx.restore();
  };

  View.prototype.drawGaps = function (u) {
    var g = this.D && this.D.gaps;
    if (!g || !g.length || !this.view) return;
    var ctx = u.ctx, top = u.bbox.top, height = u.bbox.height;
    ctx.save();
    ctx.strokeStyle = cssVar(this.el, "--ul-gap", "#dc2626");
    ctx.lineWidth = Math.max(1, devicePixelRatio);
    ctx.beginPath();
    var start = lowerBound(g, this.view[0]);
    start -= start % 2;
    var lastPx = -1, drawn = 0;
    for (var i = start; i < g.length && drawn < 5000; i += 2) {
      if (g[i] > this.view[1]) break;
      var px = Math.round(u.valToPos(g[i + 1], "x", true));
      if (px === lastPx) continue;
      lastPx = px; drawn++;
      ctx.moveTo(px + 0.5, top);
      ctx.lineTo(px + 0.5, top + height);
    }
    ctx.stroke();
    ctx.restore();
  };

  View.prototype.drawLaneLabels = function (u) {
    var ctx = u.ctx, dpr = devicePixelRatio, el = this.el;
    ctx.save();
    ctx.font = (12 * dpr) + "px system-ui, sans-serif";
    ctx.textAlign = "right";
    ctx.textBaseline = "middle";
    u.series.forEach(function (s, i) {
      if (i === 0) return;
      ctx.fillStyle = s.show ? (s._stroke || "#333") : cssVar(el, "--ul-muted", "#999");
      ctx.fillText(s.label, u.bbox.left - 8 * dpr, u.valToPos(0.5, s.scale, true));
    });
    ctx.restore();
  };

  View.prototype.attachPanZoom = function (u) {
    var self = this, over = u.over;
    // touch: two fingers pinch-zoom / pan; one finger keeps scrolling the page, a tap pins
    over.style.touchAction = "pan-y";
    var pinch = null;
    var touchInfo = function (e) {
      var r = over.getBoundingClientRect(), a = e.touches[0], b = e.touches[1];
      return { mid: (a.clientX + b.clientX) / 2 - r.left, dist: Math.max(10, Math.abs(a.clientX - b.clientX)) };
    };
    over.addEventListener("touchstart", function (e) {
      if (e.touches.length !== 2 || !self.view) return;
      e.preventDefault();
      if (self.opts.onInteract) self.opts.onInteract();
      var ti = touchInfo(e);
      pinch = { v: self.view.slice(), x: u.posToVal(ti.mid, "x"), dist: ti.dist, w: over.clientWidth };
    }, { passive: false });
    over.addEventListener("touchmove", function (e) {
      if (!pinch || e.touches.length !== 2) return;
      e.preventDefault();
      var ti = touchInfo(e), span = (pinch.v[1] - pinch.v[0]) * pinch.dist / ti.dist;
      var min = pinch.x - (ti.mid / pinch.w) * span;   // keep the touched time under the fingers
      self.userView(min, min + span);
    }, { passive: false });
    over.addEventListener("touchend", function (e) { if (e.touches.length < 2) pinch = null; });

    // click (no drag) pins the sample under the cursor; clicking again releases it
    var down = null;
    over.addEventListener("mousedown", function (e) {
      // pressing the chart (click, hold, drag) means "let me look at this": tell the owner to stop following
      if (self.opts.onInteract) self.opts.onInteract();
      down = e.button === 0 && !e.shiftKey ? { x: e.clientX, y: e.clientY, t: Date.now() } : null;
    });
    over.addEventListener("mouseup", function (e) {
      if (!down || Math.abs(e.clientX - down.x) > 4 || Math.abs(e.clientY - down.y) > 4 || Date.now() - down.t > 500) { down = null; return; }
      down = null;
      var rect = over.getBoundingClientRect(), x = u.posToVal(e.clientX - rect.left, "x");
      var tol = (self.view[1] - self.view[0]) / Math.max(1, rect.width) * 6;   // ~6 px
      self.setPin(self.pinned !== null && Math.abs(x - self.pinned) < tol ? null : self.D.t[self.indexAt(x)]);
    });
    over.addEventListener("wheel", function (e) {
      if (!self.view) return;
      e.preventDefault();
      var rect = over.getBoundingClientRect(), xv = u.posToVal(e.clientX - rect.left, "x"), f = e.deltaY < 0 ? 0.8 : 1.25;
      self.userView(xv - (xv - self.view[0]) * f, xv + (self.view[1] - xv) * f);
    }, { passive: false });
    over.addEventListener("mousedown", function (e) {
      if (!(e.shiftKey || e.button === 1) || !self.view) return;
      e.preventDefault(); e.stopPropagation();
      var x0 = e.clientX, v0 = self.view.slice(), pxPerSec = over.clientWidth / (v0[1] - v0[0]);
      var move = function (ev) { var dt = (ev.clientX - x0) / pxPerSec; self.userView(v0[0] - dt, v0[1] - dt); };
      var up = function () { window.removeEventListener("mousemove", move); window.removeEventListener("mouseup", up); };
      window.addEventListener("mousemove", move);
      window.addEventListener("mouseup", up);
    }, true);
  };

  // Save the visible chart(s) as a PNG: title line, the charts as drawn and their
  // legends (channel colours, names and the values the legend currently shows).
  View.prototype.exportPng = function (title, filename) {
    var self = this, charts = this.charts.filter(function (u) { return u.root.offsetParent; });
    if (!charts.length) return Promise.reject(new Error("Nothing to export"));
    var dpr = devicePixelRatio || 1, pad = 16 * dpr, W = Math.max.apply(null, charts.map(function (u) { return u.ctx.canvas.width; })) + 2 * pad;
    var bg = cssVar(this.el, "--surface", "#ffffff"), fg = cssVar(this.el, "--text", "#1d2330"), muted = cssVar(this.el, "--muted", "#5d6676");
    var font = function (px, bold) { return (bold ? "600 " : "") + px * dpr + "px system-ui, -apple-system, Segoe UI, Roboto, sans-serif"; };
    var measure = document.createElement("canvas").getContext("2d");

    // lay out each chart's legend as wrapped "■ label: value" entries
    var legends = charts.map(function (u) {
      var items = [];
      u.series.forEach(function (s, i) {
        if (i === 0 || !s.show) return;
        var v = s.value(u, null, i, null);
        items.push({ color: s._stroke || fg, text: s.label + (v && v !== "—" ? ": " + v : "") });
      });
      measure.font = font(13);
      var rows = [[]], x = pad, maxX = W - pad;
      items.forEach(function (it) {
        var w = 16 * dpr + measure.measureText(it.text).width + 18 * dpr;
        if (x + w > maxX && rows[rows.length - 1].length) { rows.push([]); x = pad; }
        it.x = x; rows[rows.length - 1].push(it); x += w;
      });
      return rows;
    });
    var titleH = 44 * dpr, rowH = 22 * dpr, gap = 10 * dpr;
    var H = titleH + charts.reduce(function (h, u, k) { return h + u.ctx.canvas.height + legends[k].length * rowH + gap; }, 0) + pad;

    var c = document.createElement("canvas");
    c.width = W; c.height = H;
    var ctx = c.getContext("2d");
    ctx.fillStyle = bg; ctx.fillRect(0, 0, W, H);
    ctx.textBaseline = "middle";
    ctx.fillStyle = fg; ctx.font = font(16, true); ctx.fillText(title, pad, 22 * dpr);
    ctx.fillStyle = muted; ctx.font = font(12);
    var range = fmtTime(this.view[0], this.opts.utc).slice(0, 19) + "  –  " + fmtTime(this.view[1], this.opts.utc).slice(0, 19) + (this.opts.utc ? " UTC" : "");
    ctx.textAlign = "right"; ctx.fillText(range, W - pad, 22 * dpr); ctx.textAlign = "left";
    var y = titleH;
    charts.forEach(function (u, k) {
      ctx.drawImage(u.ctx.canvas, pad, y);
      y += u.ctx.canvas.height;
      ctx.font = font(13);
      legends[k].forEach(function (row) {
        var cy = y + rowH / 2;
        row.forEach(function (it) {
          ctx.fillStyle = it.color; ctx.fillRect(it.x, cy - 5 * dpr, 10 * dpr, 10 * dpr);
          ctx.fillStyle = fg; ctx.fillText(it.text, it.x + 16 * dpr, cy);
        });
        y += rowH;
      });
      y += gap;
    });
    return new Promise(function (resolve, reject) {
      c.toBlob(function (blob) {
        if (!blob) { reject(new Error("PNG export failed")); return; }
        var a = document.createElement("a");
        a.href = URL.createObjectURL(blob);
        a.download = filename;
        document.body.appendChild(a);
        a.click();
        setTimeout(function () { URL.revokeObjectURL(a.href); a.remove(); }, 1000);
        resolve(blob);
      }, "image/png");
    });
  };

  // keyboard pan/zoom on the view (call from a keydown listener)
  View.prototype.key = function (e) {
    if (!this.view) return false;
    var v = this.view, span = v[1] - v[0], mid = (v[0] + v[1]) / 2;
    if (e.key === "ArrowLeft") this.userView(v[0] - span * 0.2, v[1] - span * 0.2);
    else if (e.key === "ArrowRight") this.userView(v[0] + span * 0.2, v[1] + span * 0.2);
    else if (e.key === "+" || e.key === "=") this.userView(mid - span * 0.4, mid + span * 0.4);
    else if (e.key === "-" || e.key === "_") this.userView(mid - span * 0.625, mid + span * 0.625);
    else if (e.key === "0" || e.key === "Home") this.reset();
    else return false;
    return true;
  };

  global.ULChart = {
    MISSING: MISSING,
    parseCsvFile: parseCsvFile,
    estimateCsvMemory: estimateCsvMemory,
    View: View,
    fmtTime: fmtTime, fmtBytes: fmtBytes, fmtDur: fmtDur, fmtFixed: fmtFixed,
    rangeMinMax: rangeMinMax, buildData: buildData,
  };
})(window);
