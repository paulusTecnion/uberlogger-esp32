document.querySelector('#configuration').onkeypress = checkEnter; // don't submit form on enter press

$( document ).ready(function() {
	loadForm();
});


function importConfigfile() {
	$('#file_import_config').click();
}

function importConfigFileSelected(){
	var reader = new FileReader();
	var file = $("#file_import_config").prop("files")[0];
	var frm="#configuration";

	console.log( "Reading configuration file..." );

	reader.onload = function(event) {
		var data = $.parseJSON(event.target.result);
		populateFields(frm, data);
		showFirstPage();

		alert("Settings imported succesfully.");

		console.log( "Done." );
	};

	reader.readAsText(file);
}

function loadForm(){
	// get FW version
	queryData('getStatus', '#config');  

	// get other stuff
	console.log( "Retrieving settings..." );
	$.getJSON('ajax/getConfig', (data) => {
		// parse JSON data to form
		parseConfig(data);

		document.querySelector("#loading").style.display = "none";
		document.querySelector("#config").style.display = "block";

		setInterval(getStatus(), 2000);

		console.log( "Done." );
	})
	.fail(function() {
		alert("Error: could not load settings. Reload page to try again.");
		console.log( "Failed, could not retrieve settings." );
	});	
};

function loadFormDefaults(){
	console.log( "Retrieving defaults..." );
	$.getJSON('ajax/getDefaultConfig', (data) => {
		// parse JSON data to form
		parseConfig(data);

		alert("Default settings loaded succesfully.");

		console.log( "Done." );
	})
	.fail(function() {
		alert("Error: could not load default settings.");
		console.log( "Failed, could not retrieve settings." );
	});
};

function parseConfig(data){
	
	populateFields("#configuration", data);

	populateFields("#channel_configuration", data["NTC_SELECT"]);
	populateFields("#channel_configuration", data["AIN_RANGE_SELECT"]);

}

function syncTime() {
	$.post("ajax/setConfig", { "TIMESTAMP": Math.floor(Number(new Date())/1000) }, function( data ) {
		let datetimestr = new Date(Number(data["TIMESTAMP"]));
		alert("Time synchronized succesfully.");
		console.log( "Time set to " + datetimestr);	
	}, "json")
	.fail(function() {
		alert("Error: could not synchronize time.");
		console.log( "Failed, could not sync time." );
	});

}


function testWifiNetwork(){
	var frm="#wifi_configuration";
	var ssid=$('[name=WIFI_SSID]', frm).val();
	var passwd=$('[name=WIFI_PASSWORD]', frm).val();
	
	$.post( "/testWifiNetwork", {"WIFI_SSID": ssid, "WIFI_PASSWORD": passwd}, (data) => {
		setTimeout(getStatus, 100);
		setTimeout(getStatus, 3000);
	});
};

function getStatus(){
  parent="#wifi_configuration";
  query="getStatus";

	$.getJSON('./ajax/' + query, (data) => {
		// parse JSON data to div

    // sanitize data
		switch(data["WIFI_TEST_STATUS"]){
			case 0:
			default:
				value="N/A";
				break;			

			case 1:
				value="...";
				break;			

			case 2:
				value="OK";
				break;			

			case 3:
				value="failed";
				break;
		}
		data["WIFI_TEST_STATUS"]=value;

		if(data["WIFI_MODE"] == 0){
			// hotspot mode, enable dialog
			document.querySelector("#WIFI_TEST_BTN").style.display = "block";
		}else{
			document.querySelector("#WIFI_TEST_BTN").style.display = "none";
		}

    // populate form
		populateFields(parent, data);
	})
	.fail(function() {
    alert("Error: could not update status.");
		console.log("Data query failed.");
	});		

}



function configSubmit() {
	var input = getFormDataAsJsonObject($("#configuration")); 

	$.post("ajax/setConfig", JSON.stringify(input), function( data ) {
		alert("Settings saved.");
	})
	.fail(function() {
		alert("Error: could not save settings.");
		console.log( "Failed, could not save settings." );
	});
}

function getFormDataAsJsonObject(object) {
	let array = {};
	let data=object.serializeArray();

	$.map(data, function(x) {
		array[x['name']] = x['value'];
	});

	return array;
}