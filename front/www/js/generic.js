var valuesData = [];
var alert_active_valueserr = false;

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

//
function getValues() {
  query = "getValues";

  $.getJSON("./ajax/" + query, (data) => {
    valuesData = data;
    // parse JSON data to div

    // sanitize data
    let datetimestr = new Date(Number(valuesData["TIMESTAMP"]));
    valuesData["TIMESTAMPSTR"] = formatDate(datetimestr);
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
        value = 'Failed to connect. Press "Save Wi-Fi settings" to try again.';
        break;

      case 3:
        value = "Connected";
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

      // case 9:
      // 	valuesData["LOGGER_STATE"]="CALIBRATION";
      // 	break;
    }

    if (valuesData["LOGGER_STATE"] == "IDLE") {
      $("#btn_logger_start").removeAttr("disabled");
    } else {
      $("#btn_logger_start").attr("disabled", true);
    }
    if (valuesData["LOGGER_STATE"] == "LOGGING") {
      $("#btn_logger_stop").removeAttr("disabled");
    } else {
      $("#btn_logger_stop").attr("disabled", true);
    }

    populateFields("#topstatus", valuesData);
  }).fail(function () {
    if (alert_active_valueserr == false) {
      alert_active_valueserr = true;
      alert("Error: could not update values.");
      alert_active_valueserr = false;
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
