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
      let truncatedChannel =
        channel.length > 4 ? channel.substring(0, 4) + ".." : channel;

      htmlstring += "<tr><td>" + truncatedChannel + "</td>";

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
  if (!followLive) {
    el.innerHTML =
      " &nbsp;&#10073;&#10073; paused (zoomed) &ndash; pick a window or double-click to resume live";
  } else if (liveWindowMs > 0) {
    el.innerHTML = " &nbsp;&#9679; live";
  } else {
    el.innerHTML = " &nbsp;&#9679; live (all retained, max ~6h)";
  }
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
  const dataPointsArray = Object.values(dataPoints);

  let config = {
    responsive: true,
    displayModeBar: true,
    modeBarButtonsToRemove: ["lasso2d"],
  };

  if (plot_drawn_state == 0) {
    let layout = {
      datarevision: Number(new Date()),
      uirevision: "liveview", // preserve the user's zoom across data updates
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
    let layout = { datarevision: Number(new Date()), uirevision: "liveview" };
    Plotly.update(divPlot, dataPointsArray, layout, config);
    applyFollow();
  }
}
