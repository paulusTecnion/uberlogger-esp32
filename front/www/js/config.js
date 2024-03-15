document.querySelector("#configuration").onkeypress = checkEnter; // don't submit form on enter press

$(document).ready(function () {
  loadForm();
});

function importConfigfile() {
  $("#file_import_config").click();
}

function disableAIN(x) {
  var ntcSelect = document.getElementById("NTC" + x);
  var ainSelect = document.getElementById("AIN" + x);

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
        console.log("Time synchronized, response=" + JSON.stringify(response));
      } else {
        alert(
          "Error: could not synchronize time, error=" + response["reason"] + "."
        );
        console.log("Failed, response=" + JSON.stringify(response));
      }
    },

    error: function (response) {
      alert(
        "Error: could not synchronize time, response=" +
          JSON.stringify(response)
      );
      console.log("Failed, response=" + JSON.stringify(response));
    },
  });
}

function startCalibration() {
  ret = confirm(
    "Please disconnect all input wires from the Uberlogger and set the DIP switches of ALL channels to ANALOG before starting the calibration. You can leave the range settings as they are. Continue?"
  );

  if (ret == true) {
    query = "calibrate";

    $.getJSON("./ajax/" + query, (data) => {
      if (data["resp"] == "ack") {
        alert("Calibration in progress...");
      } else {
        alert("Error: calibration failed: " + data["reason"]);
      }
    }).fail(function (response) {
      alert("Error: calibration failed: " + response);
    });
  }
}

function validateIntegerInput(element) {
  // Remove any non-digits or leading zeros
  element.value = element.value.replace(/[^\d]/g, '').replace(/^0+/g, '');
  
  // Convert the cleaned input back to a number and round down if necessary
  const intValue = parseInt(element.value, 10);
  if (!isNaN(intValue)) {
      element.value = intValue;
  } else {
      element.value = ''; // Clear the field if the result is not a number
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
          return ;
      }
      if (!/^[a-zA-Z0-9_-]*$/.test(fileNamePrefix)) {
          alert("File name prefix should not contain spaces or special characters.");
          return ;
      }
  
      // Validate file split size
      var fileSplitSize = input_all["FILE_SPLIT_SIZE"];
      var sizeUnit = input_all["FILE_SIZE_SPLIT_SIZE_UNIT"];
  
      // Convert file size to GiB based on the selected unit
      var maxSizeKiB = 4194304; // Maximum size in GiB
      var fileSizeInKiB;
      if (sizeUnit === "0") { // KiB
          fileSizeInKiB = fileSplitSize;
      } else if (sizeUnit === "1") { // MiB
          fileSizeInKiB = fileSplitSize * 1024;
      } else if (sizeUnit === "2") { // GiB
          fileSizeInKiB = fileSplitSize * 1024 * 1024;
      }
  
      if (fileSizeInKiB < 200)
      {
        alert("File split size should be minimum of 200 KiB.");
        return;
      }

      if (fileSizeInKiB > maxSizeKiB) {
          alert("File split size should not exceed 4 GiB.");
          return;
      }
  


  // merge input to config struct
  let config = {
    
    LOG_SAMPLE_RATE: input["LOG_SAMPLE_RATE"],
    ADC_RESOLUTION: input["ADC_RESOLUTION"],
    LOG_MODE: input["LOG_MODE"],
    FILE_NAME_PREFIX: input["FILE_NAME_PREFIX"],
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
      AIN1: input["AIN1"],
      AIN2: input["AIN2"],
      AIN3: input["AIN3"],
      AIN4: input["AIN4"],
      AIN5: input["AIN5"],
      AIN6: input["AIN6"],
      AIN7: input["AIN7"],
      AIN8: input["AIN8"],
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
        console.log("Failed, response=" + JSON.stringify(response));
      }
    },

    error: function (response) {
      alert(
        "Error: could not save settings, response=" + JSON.stringify(response)
      );
      console.log("Failed, response=" + JSON.stringify(response));
    },
  });
}

function getFormDataAsJsonObject(object) {
  let array = {};
  let data = object.serializeArray();

  $.map(data, function (x) {
    array[x["name"]] = x["value"];
  });

  return array;
}
