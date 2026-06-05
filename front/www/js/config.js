document.querySelector("#configuration").onkeypress = checkEnter; // don't submit form on enter press

$(document).ready(function () {
  loadForm();
  document
    .getElementById("LOG_SAMPLE_RATE")
    .addEventListener("change", updateAverageSamplesField);
  updateAverageSamplesField(); // Call initially to set the correct state on load
});

function updateAverageSamplesField() {
  const logSampleRate = document.getElementById("LOG_SAMPLE_RATE").value;
  const averageSamplesField = document.getElementById("AVERAGE_SAMPLES");

  if (logSampleRate >= 0 && logSampleRate <= 4) {
    averageSamplesField.disabled = false;
  } else {
    averageSamplesField.disabled = true;
  }
}

function importConfigfile() {
  $("#file_import_config").click();
}

function toggleTriggerSettings() {
  const measurementMode = document.getElementById("EXT_TRIGGER_MODE").value;
  const triggerChannel = document.getElementById("EXT_TRIGGER_PIN");
  const debounceTime = document.getElementById("EXT_TRIGGER_DEBOUNCE_TIME");

  if (measurementMode == "0") {
    // Continuous measurement, disable trigger settings
    triggerChannel.disabled = true;
    debounceTime.disabled = true;
  } else {
    // Enable for other modes
    triggerChannel.disabled = false;
    debounceTime.disabled = false;
  }
}

function disableAIN(x) {
  var ntcSelect = document.getElementById("NTC" + x);
  var ainSelect = document.getElementById("AIN" + x + "_RANGE");

  // Get the selected value of NTCx dropdown
  var selectedValue = ntcSelect.options[ntcSelect.selectedIndex].value;

  // Disable/enable AINx dropdown based on the selected value of NTCx
  if (selectedValue === "1") {
    // "NTC" option is selected
    ainSelect.disabled = true;
  } else {
    ainSelect.disabled = false;
  }
}

function importConfigFileSelected() {
  var reader = new FileReader();
  var file = $("#file_import_config").prop("files")[0];

  console.log("Reading configuration file...");

  reader.onload = function (event) {
    var data = $.parseJSON(event.target.result);
    parseConfig(data);

    alert("Settings imported succesfully.");

    console.log("Done.");
  };

  reader.readAsText(file);
}

function loadForm() {
  // get FW version (and other status) into #config. Strip NTP_ENABLED /
  // NTP_LAST_SYNC first -- like getStatus() does -- so this populate can't
  // clobber the mode-driven NTP availability state set below.
  $.getJSON("ajax/getStatus", function (d) {
    delete d["NTP_ENABLED"];
    delete d["NTP_LAST_SYNC"];
    populateFields("#config", d);
  });

  // get other stuff
  console.log("Retrieving settings...");
  $.getJSON("ajax/getConfig", (data) => {
    // parse JSON data to form
    parseConfig(data);
    wifiConfigVisibilityUpdate();
    initConfigTabs();

    document.querySelector("#loading").style.display = "none";
    document.querySelector("#config").style.display = "block";

    setInterval(getStatus(), 2000);

    console.log("Done.");
  }).fail(function () {
    alert("Error: could not load settings. Reload page to try again.");
    console.log("Failed, could not retrieve settings.");
  });
}

function loadFormDefaults() {
  console.log("Retrieving defaults...");
  $.getJSON("ajax/getDefaultConfig", (data) => {
    // parse JSON data to form
    parseConfig(data);

    alert("Default settings loaded succesfully.");

    console.log("Done.");
  }).fail(function () {
    alert("Error: could not load default settings.");
    console.log("Failed, could not retrieve settings.");
  });
}

function parseConfig(data) {
  if (typeof data["WIFI_MODE"] !== "undefined") {
    wifiModeOriginal = String(data["WIFI_MODE"]);
  }
  populateFields("#configuration", data);
  populateFields("#channel_configuration", data["NTC_SELECT"]);
  populateFields("#channel_configuration", data["AIN_RANGE_SELECT"]);
  populateFields("#channel_configuration", data["AIN_ENABLED"]);
  populateFields("#channel_configuration", data["DIN_ENABLED"]);
  populateFields("#channel_configuration", data["AIN_CHANNEL_LABELS"]);
  populateFields("#channel_configuration", data["DIO_CHANNEL_LABELS"]);

  // Security mode selectors — initialised from _SET booleans returned by the server.
  // data-originally-set remembers the server state so setConfig() can avoid sending
  // unchanged passwords (which would trigger an unnecessary wifi_update_ap() call).
  var apAuthMode = document.getElementById("WIFI_AP_AUTH_MODE");
  if (apAuthMode) {
    var apSet = !!data["WIFI_PASSWORD_AP_SET"];
    apAuthMode.value = apSet ? "wpa2" : "open";
    apAuthMode.dataset.originallySet = apSet ? "true" : "false";
    updateApSecurityUI();
  }
  var staAuthMode = document.getElementById("WIFI_STA_AUTH_MODE");
  if (staAuthMode) {
    var staSet = !!data["WIFI_PASSWORD_SET"];
    staAuthMode.value = staSet ? "password" : "open";
    staAuthMode.dataset.originallySet = staSet ? "true" : "false";
    updateStaSecurityUI();
  }
  var webAuthMode = document.getElementById("WEB_AUTH_MODE");
  if (webAuthMode) {
    var webSet = !!data["WEB_PASSWORD_SET"];
    webAuthMode.value = webSet ? "password" : "none";
    webAuthMode.dataset.originallySet = webSet ? "true" : "false";
    updateWebSecurityUI();
  }

  // NTP enable select + server are populated by populateFields (NTP_ENABLED /
  // NTP_SERVER); reflect the enabled state in the UI.
  updateNtpUI();
}


// ── Security mode UI toggles ───────────────────────────────────────────────
function updateApSecurityUI() {
  var mode    = document.getElementById("WIFI_AP_AUTH_MODE");
  var section = document.getElementById("wifi_ap_password_section");
  if (mode && section) section.style.display = (mode.value === "wpa2") ? "block" : "none";
}

function updateStaSecurityUI() {
  var mode    = document.getElementById("WIFI_STA_AUTH_MODE");
  var section = document.getElementById("wifi_sta_password_section");
  if (mode && section) section.style.display = (mode.value === "password") ? "block" : "none";
}

function updateWebSecurityUI() {
  var mode    = document.getElementById("WEB_AUTH_MODE");
  var section = document.getElementById("web_password_section");
  if (mode && section) section.style.display = (mode.value === "password") ? "block" : "none";
}

// Show the NTP server/sync controls only when NTP is enabled.
function updateNtpUI() {
  var mode    = document.getElementById("NTP_ENABLED");
  var section = document.getElementById("ntp_settings_section");
  if (mode && section) section.style.display = (mode.value === "1") ? "block" : "none";
}

// True when the currently selected Wi-Fi connection mode is "Hotspot only" (0),
// in which case the device has no upstream and NTP cannot work.
function ntpWifiIsHotspotOnly() {
  return $("input[name='WIFI_MODE']:checked").val() === "0";
}

// NTP only works with an upstream connection (Hotspot+Client or Client mode).
// In hotspot-only mode, force the control to Disabled, grey out the NTP inputs
// and the Sync-now button, and show a notice explaining why.
function updateNtpModeAvailability() {
  var apOnly  = ntpWifiIsHotspotOnly();
  var enable  = document.getElementById("NTP_ENABLED");
  var server  = document.getElementById("NTP_SERVER");
  var syncBtn = document.getElementById("ntp_sync_now_btn");
  var notice  = document.getElementById("ntp_mode_notice");

  if (apOnly && enable) enable.value = "0"; // NTP can't run here -> show Disabled
  if (enable)  enable.disabled  = apOnly;
  if (server)  server.disabled  = apOnly;
  if (syncBtn) syncBtn.disabled = apOnly;
  if (notice)  notice.style.display = apOnly ? "block" : "none";

  updateNtpUI(); // reflect the (possibly forced) enabled/disabled section
}

// Tracks the NTP enabled state that is actually saved on the device (from the
// status poll), as opposed to the unsaved <select> value the user may be
// editing. "Sync now" only works once the setting is saved and active.
var ntpDeviceEnabled = false;

// The WIFI_MODE value the device booted with (from getConfig). A mode change
// reboots the device to apply it (a live switch strands the web UI), so
// setConfig() compares against this to show the reboot/reconnect notice.
var wifiModeOriginal = null;

// Render the NTP last-sync status (called from the status poll). enabled and
// lastSync (epoch seconds, 0 = never) come from /ajax/getStatus.
function updateNtpStatus(enabled, lastSync) {
  ntpDeviceEnabled = Number(enabled) === 1;
  var el = document.getElementById("ntp_last_sync_status");
  if (!el) return;
  // Don't clobber a transient "syncing…" message with the periodic poll until
  // a real sync timestamp arrives.
  if (el.dataset.syncing === "1" && !(Number(lastSync) > 0)) return;
  el.dataset.syncing = "";
  if (Number(enabled) !== 1) {
    el.textContent = "disabled";
  } else if (Number(lastSync) > 0) {
    el.textContent = "last sync: " + new Date(Number(lastSync) * 1000).toLocaleString([], { hour12: false });
  } else {
    el.textContent = "waiting for first sync…";
  }
}

// "Sync now" button: ask the device to poll its NTP server immediately.
function ntpSyncNow() {
  // NTP needs an upstream connection; it cannot work in hotspot-only mode.
  if (ntpWifiIsHotspotOnly()) {
    alert("Automatic time sync is unavailable in Hotspot-only mode. Switch the connection mode to 'Hotspot + Client' or 'Client mode' (and save) to use it.");
    return;
  }
  var mode = document.getElementById("NTP_ENABLED");
  // If the dropdown shows Enabled but that isn't saved on the device yet, the
  // poll would fail with a confusing error. Tell the user to save first.
  if (mode && mode.value === "1" && !ntpDeviceEnabled) {
    alert("Save your settings first. Click 'Save' to enable automatic time sync, then use 'Sync now'.");
    return;
  }
  if (!ntpDeviceEnabled) {
    alert("Automatic time sync is disabled. Enable it and save your settings first.");
    return;
  }

  var el = document.getElementById("ntp_last_sync_status");
  $.ajax({
    method: "POST",
    url: "ajax/ntpSync",
    success: function (response) {
      if (response && response["resp"] == "ack") {
        if (el) {
          el.dataset.syncing = "1";
          el.textContent = "syncing…";
        }
        // Refresh the status a few times so the new sync time shows up.
        setTimeout(getStatus, 1500);
        setTimeout(getStatus, 4000);
      } else {
        alert("Could not sync: " + (response ? response["reason"] : "no response") +
              ".\n\nIf you just enabled NTP, click 'Save' first, then try 'Sync now'.");
      }
    },
    error: function () {
      alert("Could not request NTP sync. If you just enabled automatic time sync, " +
            "click 'Save' to apply the setting first, then try 'Sync now' again. " +
            "Also make sure the device is connected to Wi-Fi.");
    },
  });
}

function testWifiNetwork() {
  var frm = "#wifi_configuration";
  var ssid = $("[name=WIFI_SSID]", frm).val();
  var passwd = $("[name=WIFI_PASSWORD]", frm).val();

  $.post(
    "./ajax/testWifiNetwork",
    { WIFI_SSID: ssid, WIFI_PASSWORD: passwd },
    (data) => {
      setTimeout(getStatus, 100);
      setTimeout(getStatus, 3000);
    }
  );
}

function wifiConfigVisibilityUpdate() {
  var show_parameters = $("input[name='WIFI_MODE']:checked").val();

  // Show client (STA) settings for both Hotspot+Client (1) and Client-only (2)
  if (show_parameters == "1" || show_parameters == "2") {
    document.querySelector("#wifi_client_settings").style.display =
      "inline-block";
  } else {
    document.querySelector("#wifi_client_settings").style.display = "none";
  }

  // NTP availability depends on the connection mode.
  updateNtpModeAvailability();
}

function getStatus() {
  parent = "#wifi_configuration";
  query = "getStatus";

  $.getJSON("./ajax/" + query, (data) => {
    // parse JSON data to div

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
        value = "Failed to connect &#10060;";
        break;

      case 3:
        value = "Connected &#9989;";
        break;
    }
    data["WIFI_TEST_STATUS"] = value;

    // Render NTP status by id, then drop the keys so populateFields doesn't
    // overwrite the NTP_ENABLED <select> the user may be editing.
    updateNtpStatus(data["NTP_ENABLED"], data["NTP_LAST_SYNC"]);
    delete data["NTP_ENABLED"];
    delete data["NTP_LAST_SYNC"];

    // populate form
    populateFields(parent, data);
  }).fail(function () {
    alert("Error: could not update status.");
    console.log("Data query failed.");
  });
}

function syncTime() {
  let input = { TIMESTAMP: Number(new Date()) };

  $.ajax({
    method: "POST",
    url: "ajax/setTime",
    data: JSON.stringify(input),

    processData: false,
    dataType: "json",
    contentType: "application/json",

    success: function (response) {
      if (response["resp"] == "ack") {
        alert(
          "Time synchronized succesfully to " +
            new Date(input["TIMESTAMP"]) +
            "."
        );
        console.log("Time synchronized, response=" + response["responseText"]);
      } else {
        alert(
          "Error: could not synchronize time, error=" +
            response["responseText"] +
            "."
        );
        console.log("Failed, response=" + response["responseText"]);
      }
    },

    error: function (response) {
      alert(
        "Error: could not synchronize time, response=" +
          //response["responseText"]
          response["responseText"]
      );
      console.log("Failed, response=" + response["responseText"]);
    },
  });
}

function startCalibration() {
  ret = confirm(
    "Please disconnect all input wires from the Uberlogger and set the DIP switches of ALL channels to ANALOG before starting the calibration. You can leave the range settings as they are. Continue?"
  );

  if (ret == true) {
    query = "calibrate";

    $.ajax({
      method: "POST",
      url: "ajax/calibrate",
      processData: false,
      dataType: "json",
      contentType: "application/json",

      success: function (response) {
        if (response["resp"] == "ack") {
          alert("Calibration in progress...");
        } else {
          alert("Error: calibration failed: " + response["responseText"]);
          console.log("Failed, response=" + response["responseText"]);
        }
      },

      error: function (response) {
        alert("Error: calibration failed: " + response["responseText"]);
        console.log("Failed, response=" + response["responseText"]);
      },
    });
  }
}

function validateIntegerInput(element) {
  // Remove any non-digits or leading zeros
  element.value = element.value.replace(/[^\d]/g, "").replace(/^0+/g, "");

  // Convert the cleaned input back to a number and round down if necessary
  const intValue = parseInt(element.value, 10);
  if (!isNaN(intValue)) {
    element.value = intValue;
  } else {
    element.value = ""; // Clear the field if the result is not a number
  }

  if (value < input.min) {
    input.value = input.min;
  } else if (value > input.max) {
    input.value = input.max;
  }
}

function validateChannelNames(channelNames) {
  const validPattern = /^[a-zA-Z0-9_-]*$/;
  for (let key in channelNames) {
    if (channelNames.hasOwnProperty(key)) {
      if (!validPattern.test(channelNames[key])) {
        alert(
          `Invalid characters in ${key}. Only letters, numbers, underscores, and hyphens are allowed.`
        );
        return false;
      }
    }
  }
  return true;
}

// Shown after saving a Wi-Fi mode change: the device reboots to apply it.
// newMode is the REST value being saved (0=Hotspot-only, 1=Hotspot+Client,
// 2=Client-only). We only auto-reload when the device will still be reachable
// at the address the browser is currently using; if the mode change drops that
// network (e.g. you're on the router IP and switch to Hotspot-only, or you're
// on the hotspot and switch to Client-only) polling the same address can never
// succeed, so instead we tell the user where to reconnect.
function showWifiRebootNotice(newMode) {
  if (document.getElementById("reboot_overlay")) return;

  // The device hotspot is always 192.168.4.x; any other host means we reached
  // the device over the client (STA) network (the router-assigned IP).
  var onHotspot = /^192\.168\.4\./.test(location.hostname);
  var newHasAP = newMode === 0 || newMode === 1; // Hotspot-only or Hotspot+Client
  var newHasSTA = newMode === 1 || newMode === 2; // Hotspot+Client or Client-only
  var stillReachable = onHotspot ? newHasAP : newHasSTA;

  var instructions, showStatus;
  if (stillReachable) {
    instructions = "This page will reload automatically when the device is back.";
    showStatus = true;
  } else if (newHasAP) {
    // Was on the client network; device drops STA and keeps only its hotspot.
    instructions =
      "This device is leaving your network. Connect your computer to the device " +
      "Wi-Fi hotspot (<b>Uberlogger-…</b>), then open " +
      "<a href='http://192.168.4.1' style='color:#7fd1ff;'>http://192.168.4.1</a>.";
    showStatus = false;
  } else {
    // Client-only: the hotspot turns off; the device gets an IP from your router.
    instructions =
      "The device hotspot is switching off. Reconnect your computer to your main " +
      "Wi-Fi network, then open the device's new address (check your router for its IP).";
    showStatus = false;
  }

  var html =
    '<div id="reboot_overlay" style="position:fixed;inset:0;background:rgba(0,0,0,0.85);' +
    'color:#fff;z-index:99999;display:flex;align-items:center;justify-content:center;' +
    'text-align:center;padding:24px;font-size:16px;">' +
    '<div style="max-width:480px;line-height:1.5;">' +
    '<h2 style="margin-top:0;">Applying new network mode…</h2>' +
    "<p>The device is restarting (about 10&nbsp;seconds).</p>" +
    "<p>" + instructions + "</p>" +
    (showStatus
      ? '<p id="reboot_status" style="opacity:0.85;">Waiting for the device to come back…</p>' +
        '<p><button type="button" onclick="location.reload()" ' +
        'style="padding:8px 16px;font-size:15px;cursor:pointer;">Reload now</button></p>'
      : "") +
    "</div></div>";
  $("body").append(html);

  if (!showStatus) return; // different network — nothing to poll for, no auto-reload

  var tries = 0;
  function poll() {
    tries++;
    $.ajax({ url: "ajax/getConfig", method: "GET", timeout: 3000, cache: false })
      .done(function () {
        $("#reboot_status").text("Device is back — reloading…");
        setTimeout(function () { location.reload(); }, 600);
      })
      .fail(function () {
        if (tries < 40) {
          setTimeout(poll, 1500);
        } else {
          $("#reboot_status").text(
            "Still unreachable. If you changed networks, reconnect Wi-Fi and reload."
          );
        }
      });
  }
  // Give the device time to actually go down before we start polling, so a
  // stale success from the still-up server doesn't reload too early. The
  // reboot path waits ~3s before restarting, then boot takes a few seconds.
  setTimeout(poll, 6000);
}

function setConfig() {
  let input_all = getFormDataAsJsonObject($("#configuration"));
  let input_numbers = getFormDataAsJsonObject(
    $("#configuration .json-as-number")
  );
  let input_bools = getFormDataAsJsonObject($("#configuration .json-as-bool"));

  input = fixInputFieldNumbers(input_all, input_numbers, input_bools);

  // Validate file name prefix
  var fileNamePrefix = input_all["FILE_NAME_PREFIX"];
  if (fileNamePrefix.length > 70) {
    alert("File name prefix cannot be longer than 70 characters.");
    return;
  }
  if (!/^[a-zA-Z0-9_-]*$/.test(fileNamePrefix)) {
    alert("File name prefix should not contain spaces or special characters.");
    return;
  }

  // Validate channel names
  let ainChannelLabels = {
    AIN1: String(input["AIN_CHAN_LABEL1"]),
    AIN2: String(input["AIN_CHAN_LABEL2"]),
    AIN3: String(input["AIN_CHAN_LABEL3"]),
    AIN4: String(input["AIN_CHAN_LABEL4"]),
    AIN5: String(input["AIN_CHAN_LABEL5"]),
    AIN6: String(input["AIN_CHAN_LABEL6"]),
    AIN7: String(input["AIN_CHAN_LABEL7"]),
    AIN8: String(input["AIN_CHAN_LABEL8"]),
  };

  let dioChannelLabels = {
    DIO1: String(input["DIO_CHAN_LABEL1"]),
    DIO2: String(input["DIO_CHAN_LABEL2"]),
    DIO3: String(input["DIO_CHAN_LABEL3"]),
    DIO4: String(input["DIO_CHAN_LABEL4"]),
    DIO5: String(input["DIO_CHAN_LABEL5"]),
    DIO6: String(input["DIO_CHAN_LABEL6"]),
  };

  if (
    !validateChannelNames(ainChannelLabels) ||
    !validateChannelNames(dioChannelLabels)
  ) {
    return;
  }

  // Validate file split size
  var fileSplitSize = input_all["FILE_SPLIT_SIZE"];
  var sizeUnit = input_all["FILE_SIZE_SPLIT_SIZE_UNIT"];

  // Convert file size to GiB based on the selected unit
  var maxSizeKiB = 4194304; // Maximum size in GiB
  var fileSizeInKiB;
  if (sizeUnit === "0") {
    // KiB
    fileSizeInKiB = fileSplitSize;
  } else if (sizeUnit === "1") {
    // MiB
    fileSizeInKiB = fileSplitSize * 1024;
  } else if (sizeUnit === "2") {
    // GiB
    fileSizeInKiB = fileSplitSize * 1024 * 1024;
  }

  if (fileSizeInKiB < 200) {
    alert("File split size should be minimum of 200 KB.");
    return;
  }

  if (fileSizeInKiB > maxSizeKiB) {
    alert("File split size should not exceed 4 GB.");
    return;
  }

  // Merge input to config struct
  let config = {
    LOG_SAMPLE_RATE: input["LOG_SAMPLE_RATE"],
    ADC_RESOLUTION: input["ADC_RESOLUTION"],
    AVERAGE_SAMPLES: input["AVERAGE_SAMPLES"],
    LOG_MODE: input["LOG_MODE"],
    EXT_TRIGGER_MODE: input["EXT_TRIGGER_MODE"],
    EXT_TRIGGER_PIN: input["EXT_TRIGGER_PIN"],
    EXT_TRIGGER_DEBOUNCE_TIME: input["EXT_TRIGGER_DEBOUNCE_TIME"],
    FILE_DECIMAL_CHAR: input["FILE_DECIMAL_CHAR"],
    FILE_NAME_MODE: input["FILE_NAME_MODE"],
    FILE_NAME_PREFIX: input["FILE_NAME_PREFIX"],
    FILE_SEPARATOR_CHAR: input["FILE_SEPARATOR_CHAR"],
    FILE_SPLIT_SIZE: input["FILE_SPLIT_SIZE"],
    FILE_SPLIT_SIZE_UNIT: input["FILE_SPLIT_SIZE_UNIT"],
    NTC_SELECT: {
      NTC1: input["NTC1"],
      NTC2: input["NTC2"],
      NTC3: input["NTC3"],
      NTC4: input["NTC4"],
      NTC5: input["NTC5"],
      NTC6: input["NTC6"],
      NTC7: input["NTC7"],
      NTC8: input["NTC8"],
    },
    AIN_RANGE_SELECT: {
      AIN1_RANGE: input["AIN1_RANGE"],
      AIN2_RANGE: input["AIN2_RANGE"],
      AIN3_RANGE: input["AIN3_RANGE"],
      AIN4_RANGE: input["AIN4_RANGE"],
      AIN5_RANGE: input["AIN5_RANGE"],
      AIN6_RANGE: input["AIN6_RANGE"],
      AIN7_RANGE: input["AIN7_RANGE"],
      AIN8_RANGE: input["AIN8_RANGE"],
    },
    AIN_ENABLED: {
      AIN1_ENABLE: input["AIN1_ENABLE"],
      AIN2_ENABLE: input["AIN2_ENABLE"],
      AIN3_ENABLE: input["AIN3_ENABLE"],
      AIN4_ENABLE: input["AIN4_ENABLE"],
      AIN5_ENABLE: input["AIN5_ENABLE"],
      AIN6_ENABLE: input["AIN6_ENABLE"],
      AIN7_ENABLE: input["AIN7_ENABLE"],
      AIN8_ENABLE: input["AIN8_ENABLE"],
    },
    DIN_ENABLED: {
      DIN1_ENABLE: input["DIN1_ENABLE"],
      DIN2_ENABLE: input["DIN2_ENABLE"],
      DIN3_ENABLE: input["DIN3_ENABLE"],
      DIN4_ENABLE: input["DIN4_ENABLE"],
      DIN5_ENABLE: input["DIN5_ENABLE"],
      DIN6_ENABLE: input["DIN6_ENABLE"],
    },
    AIN_CHANNEL_LABELS: ainChannelLabels,
    DIO_CHANNEL_LABELS: dioChannelLabels,
    WIFI_CHANNEL: input["WIFI_CHANNEL"],
    WIFI_SSID_HIDDEN: input["WIFI_SSID_HIDDEN"],
    WIFI_MODE: input["WIFI_MODE"],
    WIFI_SSID: input["WIFI_SSID"],
    TIMESTAMP: Number(new Date()),
  };

  // Password fields — only include in the POST when the user actually changed something.
  // Sending WIFI_PASSWORD_AP triggers wifi_update_ap() server-side, which reconfigures
  // the WiFi AP and can drop the current HTTP connection before the response is sent.
  // Sending unchanged passwords caused the save to silently fail.
  var apAuthMode = document.getElementById("WIFI_AP_AUTH_MODE");
  if (apAuthMode) {
    var apOriginallySet = (apAuthMode.dataset.originallySet === "true");
    if (apAuthMode.value === "open" && apOriginallySet) {
      config["WIFI_PASSWORD_AP"] = ""; // user switched from password → open: explicit clear
    } else if (apAuthMode.value === "wpa2") {
      var apPwd = $("[name=WIFI_PASSWORD_AP]", "#configuration").val();
      if (apPwd) config["WIFI_PASSWORD_AP"] = apPwd; // new password typed
    }
    // else: already open and staying open → omit (no wifi_update_ap() call)
  }

  var staAuthMode = document.getElementById("WIFI_STA_AUTH_MODE");
  if (staAuthMode) {
    var staOriginallySet = (staAuthMode.dataset.originallySet === "true");
    if (staAuthMode.value === "open" && staOriginallySet) {
      config["WIFI_PASSWORD"] = ""; // user switched from password → open: explicit clear
    } else if (staAuthMode.value === "password") {
      var staPwd = $("[name=WIFI_PASSWORD]", "#configuration").val();
      if (staPwd) config["WIFI_PASSWORD"] = staPwd;
    }
  }

  var webAuthMode = document.getElementById("WEB_AUTH_MODE");
  if (webAuthMode) {
    var webOriginallySet = (webAuthMode.dataset.originallySet === "true");
    if (webAuthMode.value === "none" && webOriginallySet) {
      config["WEB_PASSWORD"] = ""; // user switched from password → none: explicit clear
    } else if (webAuthMode.value === "password") {
      var webPwd = $("[name=WEB_PASSWORD]", "#configuration").val();
      if (webPwd) config["WEB_PASSWORD"] = webPwd;
    }
  }

  var ntpEnabledSel = document.getElementById("NTP_ENABLED");
  if (ntpEnabledSel) {
    // NTP cannot work in hotspot-only mode (no upstream), so persist it off
    // when that mode is selected, regardless of the (greyed) select's value.
    config["NTP_ENABLED"] = ntpWifiIsHotspotOnly() ? 0 : Number(ntpEnabledSel.value);
    var ntpServer = $("[name=NTP_SERVER]", "#configuration").val();
    config["NTP_SERVER"] = (ntpServer && ntpServer.trim()) ? ntpServer.trim() : "pool.ntp.org";
  }

  $.ajax({
    method: "POST",
    url: "ajax/setConfig",
    data: JSON.stringify(config),

    processData: false,
    dataType: "json",
    contentType: "application/json",

    success: function (response) {
      if (response["resp"] == "ack") {
        // A Wi-Fi mode change reboots the device to apply it. Show the
        // reboot/reconnect notice instead of the normal "saved" alert.
        var modeChanged =
          wifiModeOriginal !== null &&
          typeof config["WIFI_MODE"] !== "undefined" &&
          String(config["WIFI_MODE"]) !== String(wifiModeOriginal);
        if (modeChanged) {
          showWifiRebootNotice(Number(config["WIFI_MODE"]));
          return;
        }
        // The device has now applied the settings, including NTP. Reflect the
        // saved NTP enabled state immediately so "Sync now" works without a
        // page refresh (the periodic status poll does not run on this page).
        if (typeof config["NTP_ENABLED"] !== "undefined") {
          ntpDeviceEnabled = Number(config["NTP_ENABLED"]) === 1;
        }
        setTimeout(getStatus, 300);
        alert("Settings saved successfully.");
      } else {
        alert(
          "Error: could not save settings, response=" + response["reason"] + "."
        );
        console.log("Failed, response=" + response["responseText"]);
      }
    },

    error: function (response) {
      alert(
        "Error: could not save settings, response=" + response["responseText"]
      );
      console.log("Failed, response=" + response["responseText"]);
    },
  });
}

// ── Tab navigation ────────────────────────────────────────────────────────
function initConfigTabs() {
  // Use delegated click so it works even if tabs are re-rendered.
  $(document).off("click.configtab").on("click.configtab", ".tab-btn", function () {
    var tab = $(this).data("tab");
    $(".tab-btn").removeClass("active");
    $(this).addClass("active");
    $(".tab-pane").removeClass("active");
    $("#tab-" + tab).addClass("active");
    try { sessionStorage.setItem("config_tab", tab); } catch (e) {}
  });

  // Restore the last active tab (default: channels)
  var lastTab = "channels";
  try { lastTab = sessionStorage.getItem("config_tab") || "channels"; } catch (e) {}
  $('[data-tab="' + lastTab + '"]').trigger("click");
}

function getFormDataAsJsonObject(form) {
  let jsonObject = {};
  // Handle regular input fields, unchecked checkboxes, and numeric conversions
  form.find("input, select, textarea").each(function () {
    let value = this.value;

    // Check if value is numeric and convert if so
    if (!isNaN(value) && value.trim() !== "") {
      value = +value; // Unary plus operator converts string to number if possible
    }

    if (this.type === "checkbox") {
      jsonObject[this.name] = this.checked; // Directly set boolean value for checkboxes
    } else if (this.type === "radio") {
      if (this.checked) {
        // Only add if the radio button is checked
        jsonObject[this.name] = value;
      }
    } else {
      jsonObject[this.name] = value; // Set converted numeric value or original value
    }
  });
  return jsonObject;
}
