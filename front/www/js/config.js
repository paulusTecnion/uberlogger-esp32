document.querySelector("#configuration").onkeypress = checkEnter; // don't submit form on enter press

$(document).ready(function () {
  loadForm();
});

function importConfigfile() {
  $("#file_import_config").click();
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
  // get FW version
  queryData("getStatus", "#config");

  // get other stuff
  console.log("Retrieving settings...");
  $.getJSON("ajax/getConfig", (data) => {
    // parse JSON data to form
    parseConfig(data);
    wifiConfigVisibilityUpdate();

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
  populateFields("#configuration", data);
  populateFields("#channel_configuration", data["NTC_SELECT"]);
  populateFields("#channel_configuration", data["AIN_RANGE_SELECT"]);
  populateFields("#channel_configuration", data["AIN_ENABLED"]);
  populateFields("#channel_configuration", data["DIN_ENABLED"]);
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

  if (show_parameters == "1") {
    document.querySelector("#wifi_client_settings").style.display =
      "inline-block"; // hide parameters
  } else {
    document.querySelector("#wifi_client_settings").style.display = "none"; // show parameters
  }
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

  // merge input to config struct
  let config = {
    LOG_SAMPLE_RATE: input["LOG_SAMPLE_RATE"],
    ADC_RESOLUTION: input["ADC_RESOLUTION"],
    LOG_MODE: input["LOG_MODE"],
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
    WIFI_CHANNEL: input["WIFI_CHANNEL"],
    WIFI_MODE: input["WIFI_MODE"],
    WIFI_PASSWORD: input["WIFI_PASSWORD"],
    WIFI_SSID: input["WIFI_SSID"],
    TIMESTAMP: Number(new Date()),
  };

  $.ajax({
    method: "POST",
    url: "ajax/setConfig",
    data: JSON.stringify(config),

    processData: false,
    dataType: "json",
    contentType: "application/json",

    success: function (response) {
      if (response["resp"] == "ack") {
        alert("Settings saved succesfully.");
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
