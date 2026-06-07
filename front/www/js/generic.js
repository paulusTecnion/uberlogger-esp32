var valuesData = [];
var alert_active_valueserr = false;
// Tracks whether the most recent getValues() poll succeeded, so the live-view
// status dot can show green (connected) / red (connection lost). Starts true to
// avoid a red flash before the first poll completes.
var liveConnected = true;
var fwUpdateInProgress = false;
var calibrating = false;
var calibCounter = 0;
const BYTES_PER_MB = 1024 * 1024;

// --- Live-view history -------------------------------------------------------
// Kept here (not in liveview.js) so it accumulates on EVERY page: getValues()
// polls on all tabs, so leaving the live view for File browser/Config/Manual no
// longer creates a gap. Polling is ~1 pt/s per channel. MAX_POINTS bounds both
// in-memory size and the localStorage footprint so the page is safe to leave
// running for a long time. ~21600 points = ~6 hours per channel.
var dataPoints = {};
const MAX_POINTS = 21600;
const HISTORY_STORAGE_KEY = "liveview.history.v1";
const SAVE_EVERY_MS = 10000;
let lastHistorySaveAt = 0;

// --- Channel labels ----------------------------------------------------------
// The live value table and chart legend show the user's configured channel
// labels (e.g. "Inlet (T1)") instead of the raw reading keys (T1, AIN5, DI1).
// Labels live in the config, not in getValues, so we fetch them from getConfig
// and refresh on page load and when switching to the live view (so renames
// saved on the Config page show up). channelLabels keeps them in config key
// form: AIN_CHAN_LABEL1..8 for analog/temperature channels, DIO_CHAN_LABEL1..6
// for digital channels.
var channelLabels = { ain: {}, dio: {} };

function loadChannelLabels() {
  $.getJSON("ajax/getConfig", function (cfg) {
    channelLabels = {
      ain: cfg && cfg["AIN_CHANNEL_LABELS"] ? cfg["AIN_CHANNEL_LABELS"] : {},
      dio: cfg && cfg["DIO_CHANNEL_LABELS"] ? cfg["DIO_CHANNEL_LABELS"] : {},
    };
  });
}

// Map a reading's category + channel to "Label (channel)", or the raw channel
// key when no label is set. T#/AIN# (same physical input) use the analog
// labels; DI# uses the digital labels.
function channelDisplayLabel(category, channel) {
  var m = String(channel).match(/(\d+)$/);
  if (!m) return channel;
  var i = m[1];
  var lbl =
    category === "DIGITAL"
      ? channelLabels.dio["DIO_CHAN_LABEL" + i]
      : channelLabels.ain["AIN_CHAN_LABEL" + i];
  if (lbl && String(lbl).trim() !== "") return lbl + " (" + channel + ")";
  return channel;
}

function storeDataPoint(category, channel, timestamp, value) {
  var inputTime = new Date(timestamp);
  if (inputTime.getFullYear() < 2000) return; // ignore pre-RTC-sync timestamps

  // The map key stays category.channel (stable id for history/persistence); the
  // trace name is the human label shown in the legend.
  var key = category + "." + channel;
  if (typeof dataPoints[key] == "undefined") {
    dataPoints[key] = {
      x: [],
      y: [],
      type: "scatter",
      name: channelDisplayLabel(category, channel),
    };
  }

  var series = dataPoints[key];
  series.x.push(inputTime);
  series.y.push(value);

  // Keep a bounded sliding window so memory/storage stay safe over long runs.
  if (series.x.length > MAX_POINTS) {
    series.x.splice(0, series.x.length - MAX_POINTS);
    series.y.splice(0, series.y.length - MAX_POINTS);
  }
}

// Pull every reading out of a getValues() response into the history buffer.
function accumulateReadings(data) {
  if (!data || !data["READINGS"] || typeof data["TIMESTAMP"] == "undefined") return;
  var ts = data["TIMESTAMP"];
  $.each(data["READINGS"], function (category, category_values) {
    $.each(category_values["VALUES"], function (channel, channel_value) {
      storeDataPoint(category, channel, ts, channel_value);
    });
  });
  saveDataPoints(false);
}

// Empty the live-view history buffer and its persisted snapshot. The live value
// table keeps updating from new polls; this only wipes the accumulated history
// shown on the chart. The plot redraw is handled by clearLiveView() in
// liveview.js (it owns the Plotly state).
function clearDataPoints() {
  // Empty each series IN PLACE rather than replacing dataPoints, so the trace
  // objects Plotly already holds stay valid. New points then append to these
  // same arrays and render normally, and the chart/rangeslider stay alive.
  Object.keys(dataPoints).forEach(function (k) {
    dataPoints[k].x.length = 0;
    dataPoints[k].y.length = 0;
  });
  lastHistorySaveAt = 0;
  try {
    localStorage.removeItem(HISTORY_STORAGE_KEY);
  } catch (e) {
    /* ignore storage errors */
  }
}

// Compact, storage-friendly snapshot. All channels share one poll timestamp, so
// we store the timestamps once (t) and only per-channel values (ch). This keeps
// ~6h of ~22 channels comfortably under the localStorage quota.
function buildHistoryPayload() {
  let t = [];
  const ch = {};
  Object.keys(dataPoints).forEach(function (name) {
    const dp = dataPoints[name];
    if (!dp || !dp.y || dp.y.length === 0) return;
    ch[name] = dp.y;
    if (dp.x.length > t.length) t = dp.x; // longest series defines the axis
  });
  if (t.length === 0) return null;
  return {
    v: 1,
    t: t.map(function (d) {
      return d.getTime();
    }),
    ch: ch,
  };
}

function trimHistory(keep) {
  Object.keys(dataPoints).forEach(function (name) {
    const dp = dataPoints[name];
    if (dp.x.length > keep) {
      dp.x.splice(0, dp.x.length - keep);
      dp.y.splice(0, dp.y.length - keep);
    }
  });
}

// Persist to localStorage. Throttled to SAVE_EVERY_MS unless forced (on unload).
// Handles quota errors by trimming the oldest half and retrying once.
function saveDataPoints(force) {
  const now = Date.now();
  if (!force && now - lastHistorySaveAt < SAVE_EVERY_MS) return;
  lastHistorySaveAt = now;

  const payload = buildHistoryPayload();
  if (!payload) return;

  try {
    localStorage.setItem(HISTORY_STORAGE_KEY, JSON.stringify(payload));
  } catch (e) {
    console.warn("Live history save failed, trimming buffer:", e);
    trimHistory(Math.floor(MAX_POINTS / 2));
    const trimmed = buildHistoryPayload();
    try {
      if (trimmed) localStorage.setItem(HISTORY_STORAGE_KEY, JSON.stringify(trimmed));
    } catch (e2) {
      console.warn("Live history save failed after trim, clearing:", e2);
      localStorage.removeItem(HISTORY_STORAGE_KEY);
    }
  }
}

// Load a previously saved snapshot back into dataPoints. Timestamps are aligned
// to the tail of each series so channels that started part-way through line up.
function restoreDataPoints() {
  try {
    const raw = localStorage.getItem(HISTORY_STORAGE_KEY);
    if (!raw) return;
    const saved = JSON.parse(raw);
    if (!saved || !Array.isArray(saved.t) || !saved.ch) return;
    const t = saved.t;
    Object.keys(saved.ch).forEach(function (name) {
      const y = saved.ch[name];
      if (!Array.isArray(y) || y.length === 0) return;
      const offset = t.length - y.length;
      const xs = t.slice(offset < 0 ? 0 : offset).map(function (ms) {
        return new Date(ms);
      });
      dataPoints[name] = { x: xs, y: y.slice(), type: "scatter", name: name };
    });
  } catch (e) {
    console.warn("Could not restore live history:", e);
  }
}

// --- Single-page tab navigation ---------------------------------------------
// The main menu tabs are mounted as panels under #render and switched by
// hide/show (no full page reload), so polling and the live chart stay alive and
// jQuery/Plotly are downloaded only once. Each panel is mounted lazily on first
// visit and its JS loads exactly once, which sidesteps re-entrancy issues
// (duplicate intervals/listeners, const redeclaration).
//
// fwupdate is intentionally NOT a SPA tab: it is reached from the Config page
// and ends in a device reboot, so it keeps using a full page load.
const MENU_PAGES = ["liveview", "log", "config", "home"];
var loadedPanels = {}; // page -> jQuery Deferred for its mount (fragment + JS)
var currentPage = null;

// load correct page after document is ready and highlight correct item in menu
function loadPage() {
  // Restore history saved by a previous page load before anything renders, and
  // flush it on exit so a refresh / tab switch keeps the accumulated live data.
  restoreDataPoints();
  loadChannelLabels(); // fetch channel labels for the live value table + legend
  window.addEventListener("beforeunload", function () {
    saveDataPoints(true);
  });
  document.addEventListener("visibilitychange", function () {
    if (document.visibilityState === "hidden") saveDataPoints(true);
  });

  const urlParams = new URLSearchParams(window.location.search);
  let page = urlParams.get("page");
  if (page == "" || page == undefined) page = "liveview";

  if (MENU_PAGES.indexOf(page) === -1) {
    // Non-tab page (e.g. fwupdate): keep the classic single-render behaviour.
    renderPage(page, page_version);
    $("#menu_" + page).addClass("selected");
  } else {
    showPage(page, true);
    if (history.replaceState) history.replaceState({ page: page }, "", "?page=" + page);
    window.addEventListener("popstate", function (e) {
      var p = (e.state && e.state.page) || "liveview";
      if (MENU_PAGES.indexOf(p) !== -1) showPage(p, true);
    });
  }

  getValues();
  window.valuesInterval = setInterval(function () {
    if (sessionStorage.getItem('fwFlashInProgress')) return;
    getValues();
  }, 1000);
}

// Mount a tab panel once: fetch its HTML fragment into a hidden panel div, then
// load its JS (which self-initialises exactly once). Returns a Deferred.
function mountPanel(page) {
  if (loadedPanels[page]) return loadedPanels[page];

  var d = $.Deferred();
  loadedPanels[page] = d;

  $("#render").append(
    '<div class="page-panel" id="panel-' + page + '" style="display:none"></div>'
  );

  $.get("html/" + page + ".html?version=" + page_version)
    .done(function (data) {
      $("#panel-" + page).html(data);
      // home has no JS; load the others (panel is in the DOM so init can run).
      $.getScript("js/" + page + ".js?version=" + page_version)
        .always(function () {
          d.resolve();
        });
    })
    .fail(function () {
      d.resolve(); // still show whatever mounted
    });

  return d;
}

// Switch the visible tab. fromHistory suppresses the pushState (used on initial
// load and on browser back/forward).
function showPage(page, fromHistory) {
  // Refresh channel labels when entering the live view so labels renamed on the
  // Config page are reflected in the value table and chart legend.
  if (page === "liveview") loadChannelLabels();
  mountPanel(page).always(function () {
    $("#render > .page-panel").hide();
    $("#panel-" + page).show();

    $("[id^='menu_']").removeClass("selected");
    $("#menu_" + page).addClass("selected");
    $("#menu_" + page + "_mobile").addClass("selected");

    currentPage = page;
    if (!fromHistory && history.pushState) {
      history.pushState({ page: page }, "", "?page=" + page);
    }

    // The panel may have been mounted/updated while hidden (0 size); nudge
    // responsive plots (Plotly) to recompute now that it is visible.
    window.dispatchEvent(new Event("resize"));
  });
}

function renderPage(page, page_version) {
  // get HTML of page
  $.get("html/" + page + ".html?version=" + page_version, (data) => {
    $("#render").html(data);

    // load JS of page
    $.getScript("js/" + page + ".js?version=" + page_version);
  });
}

function gotoPage(page, version) {
  // Close the mobile hamburger menu after a selection (SPA nav no longer
  // reloads the page, so it would otherwise stay open).
  var menu = document.getElementById("menu_items");
  if (menu) menu.classList.remove("show");

  if (MENU_PAGES.indexOf(page) === -1) {
    location.href = "index.html?page=" + page; // fwupdate etc. -> full reload
    return;
  }
  showPage(page, false);
}

function toggleMenu() {
  var menu = document.getElementById("menu_items");
  if (menu.classList.contains("show")) {
    menu.classList.remove("show");
  } else {
    menu.classList.add("show");
  }
}

function ejectCard() {
  $.ajax({
    method: "POST",
    url: "ajax/sdcardUnmount",

    success: function (response) {
      if (response["resp"] == "ack") {
        alert("SD card unmounted. You can eject the card now.");
        console.log("Card unmounted, response=" + response["responseText"]);
      } else {
        alert(
          "Error: cannot eject card while logger. Response: " +
            response["reason"] +
            "."
        );
        console.log(
          "SDCARD Ejection failed, response=" + response["responseText"]
        );
      }
    },

    error: function (response) {
      alert(
        "Error: could not eject SD card, response=" + response["responseText"]
      );
      console.log("Failed, response=" + response["responseText"]);
    },
  });
}

function loggerStart() {
  let input = { ACTION: "START" };

  $.ajax({
    method: "POST",
    url: "ajax/loggerStart",
    data: JSON.stringify(input),

    processData: false,
    dataType: "json",
    contentType: "application/json",

    success: function (response) {
      if (response["resp"] == "ack") {
        //alert("Logger started.");

        $("#start_logging_button").attr("onclick", "loggerStop()");
      } else {
        alert(
          "Error: could not start logger, response=" + response["reason"] + "."
        );
        console.log("Failed, response=" + response["responseText"]);
      }
    },

    error: function (response) {
      alert(
        "Error: could not start logger, response=" + response["responseText"]
      );
      console.log("Failed, response=" + response["responseText"]);
    },
  });
}

function tryFileBrowserRefresh() {
  try {
    filebrowserRefresh("/");
  } catch (error) {}
}

function loggerStop() {
  let input = { ACTION: "STOP" };

  $.ajax({
    method: "POST",
    url: "ajax/loggerStop",
    data: JSON.stringify(input),

    processData: false,
    dataType: "json",
    contentType: "application/json",

    success: function (response) {
      if (response["resp"] == "ack") {
        //alert("Logger stopped.");
        setTimeout(tryFileBrowserRefresh, 2000);

        $("#start_logging_button").attr("onclick", "loggerStart()");
      } else {
        alert(
          "Error: could not stop logger, response=" + response["reason"] + "."
        );
        console.log("Failed, response=" + response["responseText"]);
      }
    },

    error: function (response) {
      alert(
        "Error: could not stop logger, response=" + response["responseText"]
      );
      console.log("Failed, response=" + response["responseText"]);
    },
  });
}

//
function getValues() {
  query = "getValues";

  $.getJSON("./ajax/" + query, (data) => {
    sessionStorage.removeItem('fwFlashInProgress');
    valuesData = data;
    // parse JSON data to div

    // sanitize data
    let datetimestr = new Date(Number(valuesData["TIMESTAMP"]));
    // valuesData["TIMESTAMPSTR"] = formatDate(datetimestr);
    // Set the timestampstr to local time zone
    valuesData["TIMESTAMPSTR"] = datetimestr.toLocaleString([], {
      hour12: false,
    });
    // Accumulate readings into the history buffer on every poll, regardless of
    // which page is showing. Done before the fields below are reformatted (this
    // only reads READINGS + TIMESTAMP, which are left untouched).
    accumulateReadings(valuesData);

    valuesData["SD_CARD_FREE_SPACE"] =
      (valuesData["SD_CARD_FREE_SPACE"] / BYTES_PER_MB).toFixed(3) + " MB";

    // sanitize data
    switch (data["WIFI_TEST_STATUS"]) {
      case 0:
      default:
        value = "N/A";
        break;

      case 1:
        value = "Connecting";

        break;

      case 2:
        value =
          'Failed to connect &#10060;. Press "Save Wi-Fi settings" to try again.';

        break;

      case 3:
        value = "Connected &#9989;";

        break;
    }
    data["WIFI_TEST_STATUS"] = value;

    // populate form
    populateFields(parent, data);

    switch (valuesData["SD_CARD_STATUS"]) {
      default:
      case 0:
        valuesData["SD_CARD_STATUS"] = "EJECTED";
        break;

      case 1:
        valuesData["SD_CARD_STATUS"] = "UNMOUNTED";
        break;

      case 2:
        valuesData["SD_CARD_STATUS"] = "MOUNTED";
        break;
    }

    switch (valuesData["ERRORCODE"]) {
      case 0:
        valuesData["ERRORCODE"] = "No error";

        break;

      case 128:
        valuesData["ERRORCODE"] = "Mount error";
        valuesData["SD_CARD_STATUS"] = "Mount errror";
        break;

      case 512:
        valuesData["ERRORCODE"] = "No free space";
        break;

      case 4096:
        valuesData["ERRORCODE"] = "Max file size reached";
        break;

      default:
        break;
    }

    switch (valuesData["LOGGER_STATE"]) {
      case 0:
        valuesData["LOGGER_STATE"] = "INIT";
        break;

      default:
      case 1:
        valuesData["LOGGER_STATE"] = "IDLE";
        break;

      case 2:
        valuesData["LOGGER_STATE"] = "LOGGING";
        break;

      case 3:
      case 4:
        valuesData["LOGGER_STATE"] = "SETTINGS";
        break;

      case 6:
        valuesData["LOGGER_STATE"] = "ERROR";
        break;

      case 8:
        valuesData["LOGGER_STATE"] = "FW UPDATE";
        break;

      case 10:
        valuesData["LOGGER_STATE"] = "Calibrating";
        $("#calibrationStart").attr("disabled", true);
        calibrating = true;
        calibCounter = 0;
        break;
      case 11:
        valuesData["LOGGER_STATE"] = "Formatting SD";
        break;
      case 14:
        valuesData["LOGGER_STATE"] = "Waiting for trigger";
        break;

      // case 9:
      // 	valuesData["LOGGER_STATE"]="CALIBRATION";
      // 	break;
    }

    if (valuesData["LOGGER_STATE"] == "Calibrating") {
      $("#calibStatus").html("Calibrating...");
    } else if (calibrating == true && calibCounter < 5) {
      $("#calibStatus").html("Calibration done &checkmark;");
      $("#calibrationStart").attr("disabled", false);
      calibCounter++;
    } else {
      $("#calibStatus").html("");
    }

    // Disable the start button when
    if (
      valuesData["LOGGER_STATE"] == "IDLE" &&
      valuesData["SD_CARD_STATUS"] == "MOUNTED"
    ) {
      $("#btn_logger_start").removeAttr("disabled");
      $("#start_logging_button").attr("onclick", "loggerStart();");
      $("#start_logging_button").html(
        '<span class="button-icon">&#9658;</span><span class="button-text">Start logging</span>'
      );
    } else {
      $("#btn_logger_start").attr("disabled", true);
    }

    if (
      valuesData["LOGGER_STATE"] == "IDLE" &&
      valuesData["SD_CARD_STATUS"] == "EJECTED"
    ) {
      $("#start_logging_button").attr("onclick", "");
      $("#start_logging_button").html(
        '<span class="button-text">No sd card</span>'
      );
    }

    if (valuesData["LOGGER_STATE"] == "LOGGING") {
      $("#btn_logger_stop").removeAttr("disabled");
      $("#start_logging_button").attr("onclick", "loggerStop();");
      $("#start_logging_button").html(
        '<span class="button-icon">&#9209;</span><span class="button-text">Stop logging</span>'
      );
    } else {
      $("#btn_logger_stop").attr("disabled", true);
    }

    // Enable Refresh file list, format and unmount buttons in case SD card status is mounted.
    if (valuesData["SD_CARD_STATUS"] == "MOUNTED") {
      $("#start_logging_button").removeAttr("disabled");
      $("#btn_refresh_sdcard").removeAttr("disabled");
      $("#btn_format_sdcard").removeAttr("disabled");
      $("#btn_unmount_sdcard").removeAttr("disabled");
      $("#btn_unmount_sdcard_top").removeAttr("disabled");
    } else {
      $("#filelist").html(
        "File browser is not available when logging or when no SD card is inserted."
      );
      $("#start_logging_button").attr("disabled", true);
      $("#btn_refresh_sdcard").attr("disabled", true);
      $("#btn_format_sdcard").attr("disabled", true);
      $("#btn_unmount_sdcard").attr("disabled", true);
      $("#btn_unmount_sdcard_top").attr("disabled", true);
    }
    populateFields("#topstatus", valuesData);
    alert_active_valueserr = false;
    liveConnected = true;
  }).fail(function () {
    liveConnected = false;
    if (alert_active_valueserr == false) {
      alert_active_valueserr = true;
      alert("Error: could not update values.");
    }
    console.log("Data query failed.");
  });
}

// assign to a form to avoid submit on enter press
function checkEnter(e) {
  e = e || event;
  var txtArea = /textarea/i.test((e.target || e.srcElement).tagName);
  return txtArea || (e.keyCode || e.which || e.charCode || 0) !== 13;
}

function queryData(query, parent) {
  $.getJSON("./ajax/" + query, (data) => {
    // parse JSON data to div
    populateFields(parent, data);
  }).fail(function () {
    console.log("Data query failed.");
  });
}

function populateFields(parent, data) {
  // populates children in a parent object with values from data
  // name of children need to match with keys of data, values will be the values belonging to that key

  $.each(data, function (key, value) {
    // sanitizing of value can be done here

    // process values
    var $ctrl = $("[name=" + key + "]", parent);

    if ($ctrl.is("select")) {
      $("option", $ctrl).each(function () {
        if (this.value == value) {
          this.selected = true;
        } else {
          this.selected = false;
        }

        const event = new Event("change", { bubbles: true });
        $ctrl[0].dispatchEvent(event);
      });
    } else if ($ctrl.is("input")) {
      switch ($ctrl.attr("type")) {
        case "text":
        case "hidden":
        case "textarea":
          if (typeof value == "string" || value == "") {
            $ctrl.val(value);
          } else {
            $ctrl.val(Number(value).toFixed(2));
          }

          break;

        case "number":
          $ctrl.val(Number(value));
          break;

        case "radio":
          $ctrl.each(function () {
            if ($(this).attr("value") == value) {
              $(this).prop("checked", true);
            } else {
              $(this).prop("checked", false);
            }
          });
          break;

        case "checkbox":
          $ctrl.each(function () {
            if ($(this).attr("value") == value || value == true) {
              $(this).prop("checked", value);
            } else {
              $(this).prop("checked", false);
            }
          });
          break;
      }
    } else if ($ctrl.is("span")) {
      if (typeof value == "string" || value == "") {
        $ctrl.html(value);
      } else {
        $ctrl.html(Number(value).toFixed(2));
      }
    } else if ($ctrl.is("a")) {
      $ctrl[0].innerHTML = value;
      $ctrl[0].setAttribute("href", "http://" + value);
    }
  });
}

function formatDate(date) {
  var year = date.getFullYear();
  var month = date.getMonth() + 1;
  var day = date.getDate();
  var hour = date.getHours();
  var min = date.getMinutes();
  var sec = date.getSeconds();

  month = (month < 10 ? "0" : "") + month;
  day = (day < 10 ? "0" : "") + day;
  hour = (hour < 10 ? "0" : "") + hour;
  min = (min < 10 ? "0" : "") + min;
  sec = (sec < 10 ? "0" : "") + sec;

  var str = year + "-" + month + "-" + day + " " + hour + ":" + min + ":" + sec;

  return str;
}

function fixInputFieldNumbers(input_all, input_numbers, input_bools) {
  // input = all fields to save as object
  // input_numbers = all fields in input to save as number in object
  // input_bools = all fields in input to save as boolean in object

  let input_new = input_all;

  $.each(input_numbers, function (key, value) {
    input_new[key] = Number(value);
  });

  $.each(input_bools, function (key, value) {
    if (value == 0) {
      value = false;
    } else {
      value = true;
    }

    input_new[key] = Boolean(value);
  });

  return input_new;
}
