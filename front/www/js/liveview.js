$(document).ready(function () {
  $.ajax({
    url: "js/plotly-basic-2.22.0.min.js",
    dataType: "script",
    success: function () {
      setInterval(renderValueList, 1000);
    },
  });
});

// NOTE: the history buffer (dataPoints), its persistence and the per-poll
// accumulation live in generic.js so data keeps recording on every tab, not
// just while this page is open. This file only renders that shared buffer.

var plot_drawn_state = 0;
const divPlot = "plotly";

// Live-tail window controls. By default the chart shows the most recent
// LIVE_WINDOW and follows new data; the user can pick another window or
// zoom/pan to inspect the full retained history (double-click resumes live).
var liveWindowMs = 5 * 60 * 1000; // 5 minutes
var followLive = true;
var suppressRelayout = false; // guards our own programmatic relayouts
// Plotly preserves pan/zoom/rangeslider state while this token is unchanged.
// Clearing bumps it once so the stale view is dropped and the chart re-anchors.
var uiRevision = "liveview";

function sanitizeCategoryName(category) {
  switch (category) {
    case "ANALOG":
      return "Analog";
    case "DIGITAL":
      return "Digital";
    case "TEMPERATURE":
      return "Temperature";
    default:
      return category;
  }
}

function renderValueList() {
  // Skip rendering while the live-view tab is hidden (another tab is active).
  // Data still accumulates in generic.js, so nothing is lost; this just avoids
  // table/Plotly work on a non-visible panel.
  var lvPanel = document.getElementById("panel-liveview");
  if (lvPanel && lvPanel.style.display === "none") return;

  // Build the current-values table from the latest poll. Accumulation into the
  // history buffer happens in generic.js (getValues), so it is not done here.
  htmlstring = [];
  htmlstring += "<p>Timestamp of data: " + valuesData["TIMESTAMPSTR"] + "</p>";
  $.each(valuesData["READINGS"], function (category, category_values) {
    htmlstring +=
      "<div class='block greybox' style='font-size: smaller;'><h2 class='first'>" +
      sanitizeCategoryName(category) +
      "</h2>";
    htmlstring += "<table class='table-compact'>";
    htmlstring +=
      "<tr><th>Input</th><th align='right'>" +
      category_values["UNITS"] +
      "</th></tr>";

    let n = 0;
    $.each(category_values["VALUES"], function (channel, channel_value) {
      n++;
      // Show the configured label ("Inlet (T1)") when set, else the raw key.
      // Long labels wrap inside the value card via CSS (no truncation here, so
      // the "(channel)" suffix is never clipped).
      let displayChannel =
        typeof channelDisplayLabel === "function"
          ? channelDisplayLabel(category, channel)
          : channel;

      htmlstring += "<tr><td>" + displayChannel + "</td>";

      if (typeof channel_value === "number") {
        if (category === "TEMPERATURE") {
          htmlstring +=
            "<td align='right'>" + channel_value.toFixed(1) + "</td></tr>";
        } else if (category === "DIGITAL") {
          htmlstring +=
            "<td align='right'>" + channel_value.toFixed(0) + "</td></tr>";
        } else {
          htmlstring +=
            "<td align='right'>" +
            parseFloat(channel_value).toFixed(3) +
            "</td></tr>";
        }
      }
    });

    if (n == 0) {
      htmlstring += "<tr><td>-</td><td align='right'>-</td></tr>";
    }

    htmlstring += "</table></div>";
  });

  $("#values>#valuelist").html(htmlstring);

  plotDataPoints();
  updatePlotModeLabel(); // refresh live/connection dot every poll
}

// Most recent timestamp across the buffer (the live edge of the chart).
function getLatestTimestamp() {
  if (typeof valuesData !== "undefined" && valuesData && valuesData["TIMESTAMP"]) {
    var ts = Number(valuesData["TIMESTAMP"]);
    if (ts >= 946684800000) return ts; // >= year 2000
  }
  var latest = null;
  Object.keys(dataPoints).forEach(function (name) {
    var x = dataPoints[name].x;
    if (x.length) {
      var t = x[x.length - 1].getTime();
      if (latest === null || t > latest) latest = t;
    }
  });
  return latest;
}

function getEarliestTimestamp() {
  var earliest = null;
  Object.keys(dataPoints).forEach(function (name) {
    var x = dataPoints[name].x;
    if (x.length) {
      var t = x[0].getTime();
      if (earliest === null || t < earliest) earliest = t;
    }
  });
  return earliest;
}

// Slide the x-axis to the live window (or full extent when liveWindowMs == 0).
function applyFollow() {
  if (!followLive || plot_drawn_state === 0) return;
  var latest = getLatestTimestamp();
  if (latest === null) return;

  var range;
  if (liveWindowMs > 0) {
    var earliest = getEarliestTimestamp();
    var start = latest - liveWindowMs;
    if (earliest !== null && earliest > start) start = earliest; // less data than window
    range = [new Date(start), new Date(latest)];
  } else {
    var first = getEarliestTimestamp();
    range = [new Date(first === null ? latest - 1000 : first), new Date(latest)];
  }

  // Hold the guard until the relayout (and its plotly_relayout event, which can
  // fire asynchronously) has completed, so our own range update is not mistaken
  // for a manual zoom.
  suppressRelayout = true;
  var p = Plotly.relayout(divPlot, { "xaxis.range": range });
  if (p && typeof p.then === "function") {
    p.then(function () {
      suppressRelayout = false;
    });
  } else {
    suppressRelayout = false;
  }
}

// Pick a live window from the control buttons and resume following.
function setLiveWindow(ms) {
  liveWindowMs = ms;
  followLive = true;
  applyFollow();
  updatePlotModeLabel();
}

// Manual zoom/pan pauses following; double-click (autorange) resumes it.
function onPlotRelayout(ev) {
  if (suppressRelayout) return;
  if (ev["xaxis.autorange"] === true) {
    followLive = true;
    applyFollow();
    updatePlotModeLabel();
    return;
  }
  if ("xaxis.range" in ev || "xaxis.range[0]" in ev) {
    followLive = false;
    updatePlotModeLabel();
  }
}

function updatePlotModeLabel() {
  var el = document.getElementById("plotmode");
  if (!el) return;
  // The status dot reflects the live connection: green when the last poll
  // succeeded, red when it failed (set by getValues in generic.js).
  var connected = typeof liveConnected === "undefined" ? true : liveConnected;
  var dot =
    '<span style="color:' +
    (connected ? "#2ecc40" : "#ff4136") +
    '">&#9679;</span>';

  if (!connected) {
    el.innerHTML = " &nbsp;" + dot + " connection lost";
  } else if (!followLive) {
    el.innerHTML =
      " &nbsp;&#10073;&#10073; paused (zoomed) &ndash; pick a window or double-click to resume live";
  } else if (liveWindowMs > 0) {
    el.innerHTML = " &nbsp;" + dot + " live";
  } else {
    el.innerHTML = " &nbsp;" + dot + " live (all retained, max ~6h)";
  }
}

// Empty the history buffer + persisted snapshot and blank the chart. The trace
// objects are kept (emptied in place by clearDataPoints), so new polls keep
// appending to them and the chart/rangeslider stay live.
function clearLiveView() {
  if (typeof clearDataPoints === "function") clearDataPoints();
  // Re-seed each series with the latest reading so every channel keeps one live
  // point. Without this the traces are momentarily empty and Plotly drops their
  // legend entries, so the signal labels vanish until the next poll (~1s later).
  if (
    typeof accumulateReadings === "function" &&
    typeof valuesData !== "undefined" &&
    valuesData &&
    valuesData["READINGS"]
  ) {
    accumulateReadings(valuesData);
  }
  followLive = true;
  // New uirevision token => Plotly discards the stale pan/zoom/rangeslider view
  // for this redraw, then keeps preserving the (reset) view on later ticks.
  uiRevision = "liveview-" + Number(new Date());
  if (plot_drawn_state === 1 && window.Plotly) {
    Plotly.update(divPlot, Object.values(dataPoints), {
      datarevision: Number(new Date()),
      uirevision: uiRevision,
    });
    applyFollow();
  }
  updatePlotModeLabel();
}

// On narrow screens the default right-hand vertical legend (22 series) crushes
// the plot. Move it below the chart and drop the redundant title (the page
// already has a "Live data" heading) so the plot gets the full width.
function isNarrowPlot() {
  return window.innerWidth < 760;
}

function applyResponsiveLayout() {
  if (plot_drawn_state !== 1 || !window.Plotly) return;
  var upd = isNarrowPlot()
    ? {
        "legend.orientation": "h",
        "legend.x": 0,
        "legend.xanchor": "left",
        "legend.y": -0.3,
        "legend.yanchor": "top",
        "legend.font.size": 9,
        "title.text": "",
        "margin.t": 20,
        "margin.r": 10,
        "margin.l": 50,
      }
    : {
        "legend.orientation": "v",
        "legend.x": 1.02,
        "legend.xanchor": "left",
        "legend.y": 1,
        "legend.yanchor": "auto",
        "legend.font.size": 11,
        "title.text": "Measured data",
        "margin.t": 45,
        "margin.r": 15,
        "margin.l": 75,
      };
  Plotly.relayout(divPlot, upd); // legend/title/margin keys only -> not a zoom
}

function plotDataPoints() {
  // Keep each trace's legend name in sync with the latest channel labels, so
  // labels that load after the first poll (or change on a config save) show up.
  Object.keys(dataPoints).forEach(function (key) {
    var dot = key.indexOf(".");
    if (dot > 0 && typeof channelDisplayLabel === "function") {
      dataPoints[key].name = channelDisplayLabel(key.slice(0, dot), key.slice(dot + 1));
    }
  });
  const dataPointsArray = Object.values(dataPoints);

  let config = {
    responsive: true,
    displayModeBar: true,
    // Mouse-wheel zoom on the chart. A wheel-zoom changes xaxis.range, which
    // fires plotly_relayout -> onPlotRelayout pauses live-follow (same as a
    // drag-zoom); double-click autoranges and resumes following.
    scrollZoom: true,
    modeBarButtonsToRemove: ["lasso2d"],
  };

  if (plot_drawn_state == 0) {
    // Don't draw an empty plot: Plotly.update (used on later ticks) can't add
    // traces, so a newPlot with 0 traces would leave the chart permanently
    // empty. Wait until the first poll has populated at least one series.
    if (dataPointsArray.length === 0) return;
    let layout = {
      datarevision: Number(new Date()),
      uirevision: uiRevision, // preserve the user's zoom across data updates
      margin: { l: 75, r: 15, t: 45, pad: 0 },
      title: {
        text: "Measured data",
        xref: "paper",
        font: { size: 16 },
        x: 0.5,
      },
      xaxis: {
        title: { text: "Time", font: { size: 14 } },
        type: "date",
        rangeslider: { visible: true, thickness: 0.08 },
      },
    };
    Plotly.newPlot(divPlot, dataPointsArray, layout, config);
    plot_drawn_state = 1;
    document.getElementById(divPlot).on("plotly_relayout", onPlotRelayout);
    window.addEventListener("resize", applyResponsiveLayout);
    updatePlotModeLabel();
    applyResponsiveLayout();
    applyFollow();
  } else {
    // We have a plot; the trace objects are shared with Plotly, so just bump
    // datarevision to redraw the appended points, then re-apply the window.
    let layout = { datarevision: Number(new Date()), uirevision: uiRevision };
    Plotly.update(divPlot, dataPointsArray, layout, config);
    applyFollow();
  }
}
