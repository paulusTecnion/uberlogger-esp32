var valuesData = [];
var alert_active_valueserr = false;
var calibrating = false;
var calibCounter = 0;
const BYTES_PER_MB = 1024 * 1024;

// load correct page after document is ready and highlight correct item in menu
function loadPage() {
  const urlParams = new URLSearchParams(window.location.search);
  const page = urlParams.get("page");

  if (page == "" || page == undefined) {
    renderPage("liveview", page_version);
  } else {
    renderPage(page, page_version);
  }

  $("#menu_" + page).addClass("selected");

  getValues();
  setInterval(getValues, 1000);
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
  location.href = "index.html?page=" + page;
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
    valuesData = data;
    // parse JSON data to div

    // sanitize data
    let datetimestr = new Date(Number(valuesData["TIMESTAMP"]));
    // valuesData["TIMESTAMPSTR"] = formatDate(datetimestr);
    // Set the timestampstr to local time zone
    valuesData["TIMESTAMPSTR"] = datetimestr.toLocaleString([], {
      hour12: false,
    });
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
  }).fail(function () {
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
