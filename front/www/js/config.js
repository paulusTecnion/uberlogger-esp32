document.querySelector("#configuration").onkeypress = checkEnter; // don't submit form on enter press

$(document).ready(function () {
  loadForm();
});

function importConfigfile() {
  $("#file_import_config").click();
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
        value = "Failed to connect";
        break;

      case 3:
        value = "Connected";
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

function setConfig() {
  let input_all = getFormDataAsJsonObject($("#configuration"));
  let input_numbers = getFormDataAsJsonObject(
    $("#configuration .json-as-number")
  );
  let input_bools = getFormDataAsJsonObject($("#configuration .json-as-bool"));

  input = fixInputFieldNumbers(input_all, input_numbers, input_bools);

  // merge input to config struct
  let config = {
    WIFI_SSID: input["WIFI_SSID"],
    WIFI_PASSWORD: input["WIFI_PASSWORD"],
    WIFI_MODE: input["WIFI_MODE"],
    LOG_SAMPLE_RATE: input["LOG_SAMPLE_RATE"],
    ADC_RESOLUTION: input["ADC_RESOLUTION"],
    LOG_MODE: input["LOG_MODE"],

    NTC_SELECT: {
      NTC0: input["NTC0"],
      NTC1: input["NTC1"],
      NTC2: input["NTC2"],
      NTC3: input["NTC3"],
      NTC4: input["NTC4"],
      NTC5: input["NTC5"],
      NTC6: input["NTC6"],
      NTC7: input["NTC7"],
    },
    AIN_RANGE_SELECT: {
      AIN0: input["AIN0"],
      AIN1: input["AIN1"],
      AIN2: input["AIN2"],
      AIN3: input["AIN3"],
      AIN4: input["AIN4"],
      AIN5: input["AIN5"],
      AIN6: input["AIN6"],
      AIN7: input["AIN7"],
    },
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
