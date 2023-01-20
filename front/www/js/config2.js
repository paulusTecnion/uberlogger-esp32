// parameters
var form_state = 0;

// page viewer init
pageVisibilityUpdate();
handleButtonVisibility();

$( document ).ready(function() {
	loadForm();
});

setInterval(refreshStatus, 2000);
document.querySelector('#configuration').onkeypress = checkEnter; // don't submit form on enter press


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
		populateForm(frm, data);
		showFirstPage();
		console.log( "Done." );
	};

	reader.readAsText(file);
}

function checkEnter(e){
	e = e || event;
	var txtArea = /textarea/i.test((e.target || e.srcElement).tagName);
	return txtArea || (e.keyCode || e.which || e.charCode || 0) !== 13;
}

function populateForm(frm, data) {   
    $.each(data, function(key, value){
        // do some sanitizing of values
        if(key == "WIFI_TEST_STATUS"){
            switch(data){
                case 0:
                default:
                    value="N/A";
                    break;            

                case 1:
                    value="...";
                    break;            

                case 2:
                    value="success";
                    break;            

                case 3:
                    value="failed";
                    break;
            }
        }


        // process values
        var $ctrl = $('[name=' + key + ']', frm); 

        if($ctrl.is('select')){
            $("option",$ctrl).each(function(){
			    if (this.value==value){
				    this.selected=true;
			    }else{
				    this.selected=false;
			    }
            });

        } else if($ctrl.is('input')){

            switch($ctrl.attr("type"))  
            {  
                case "text" :   case "hidden":  case "textarea":
				    if(typeof(value) == "string"  || value == ""){
					    $ctrl.val(value);
				    }else{
	                    $ctrl.val(Number(value).toFixed(2));
				    }
                    break;
				    
                case "radio" :
                    $ctrl.each(function(){
						    if($(this).attr('value') == value){
							    $(this).prop("checked",true);
						    }else{
							    $(this).prop("checked",false);
						    }
				    });
                    break;
				    
			    case "checkbox":   
                    $ctrl.each(function(){
						    if($(this).attr('value') == value) {
						       $(this).prop("checked",value);
						    }else{
						       $(this).prop("checked",false);
						    }						   
					    });  
                    break;
            } 
        } else if($ctrl.is('span')){
		    if(typeof(value) == "string" || value == ""){
			    $ctrl.html(value);
		    }else{
			    $ctrl.html(Number(value).toFixed(2));   
		    }
	    }
    });  
};

function refreshStatus(){
	if(state[form_state]=="generic" || state[form_state]=="wifi_configuration"){
		console.log( "Retrieving status..." );
		$.getJSON('/ajax/getStatus', (data) => {
			// parse JSON data to form
			var frm="#configuration";
			populateForm(frm, data);
			console.log( "Done." );
		})
		.fail(function() {
			console.log( "Failed, could not retrieve status." );
		});		
	}
	
	if(state[form_state]=="wifi_configuration"){
		console.log( "Retrieving wifi networks..." );
		$.getJSON('/ajax/getWifiNetworks', (data) => {
			// parse JSON data to form
			var htmlstring="<table style='border-spacing: 4px;'>";
			var n=0;
			
			htmlstring+="<tr><th>SSID</th><th align='right'>RSSI (dB)</th></tr>";
			
			$.each(data, function(key, value){
				n++;
				
				if(value.RSSI < -80){
					var color="#cc3300";
				}else if(value.RSSI < -75){
					var color="#ff9900";
				}else{
					var color="#009900";
				}
				
				htmlstring+="<tr><td>" + value.SSID + "</td><td align='right' style='color:" + color + "'>" + value.RSSI + "</td><td><a href='javascript:undefined;' onclick='useWifiNetwork(\"" + value.SSID + "\");'><<<</a></tr>";
			});
			
			if(n==0){
				htmlstring+="<tr><td>-<td><td align='right'>-</td></tr>";
			}
			htmlstring+="</table>";

			var frm="#configuration";
			var $ctrl=$('[name=wifi_networks]', frm);
			$ctrl.html(htmlstring);

			console.log( "Done." );
		})
		.fail(function() {
			console.log( "Failed, could not retrieve networks." );
		});		
	}
};

function scanWifiNetworks(){
	// request a scan
	console.log( "Starting Wifi network scan..." );
	$.get('/ajax/scanWifiNetworks', (data) => {
		console.log( "Done." );
	})
	.fail(function() {
		console.log( "Failed, could not retrieve settings." );
	});

	var htmlstring="<table>";
	htmlstring+="<tr><th>WiFi</th><th align='right'>RSSI (dB)</th></tr>";
	htmlstring+="<tr><td>-<td><td align='right'>-</td></tr>";
	htmlstring+="</table>";

	var frm="#configuration";
	var $ctrl=$('[name=wifi_networks]', frm);
	$ctrl.html(htmlstring);

	console.log( "Done." );	
};

function useWifiNetwork(name){
	var frm="#configuration";
	var $ctrl=$('[name=WIFI_SSID]', frm);
	$ctrl.val(name);
};

function testWifiNetwork(){
	var frm="#configuration";
	var ssid=$('[name=WIFI_SSID]', frm).val();
	var passwd=$('[name=WIFI_PASSWORD]', frm).val();
	
	$.post( "/ajax/testWifiNetwork", {"WIFI_SSID": ssid, "WIFI_PASSWORD": passwd});
};

function loadForm(){
	console.log( "Retrieving settings..." );
	$.getJSON('/ajax/getConfig', (data) => {
		// parse JSON data to form
		if('CONFIGURED' in data){
			var frm="#configuration";
			populateForm(frm, data);
			showFirstPage();
			scanWifiNetworks();
			console.log( "Done." );
		}else{
			console.log( "Failed, invalid config." );
		}
	})
	.fail(function() {
		console.log( "Failed, could not retrieve settings." );
	});	
};

function loadFormDefaults(){
	console.log( "Retrieving defaults..." );
	$.getJSON('/ajax/getDefaultConfig', (data) => {
		// parse JSON data to form
		if('CONFIGURED' in data){
			var frm="#configuration";
			populateForm(frm, data);
			showFirstPage();
			console.log( "Done." );
		}else{
			console.log( "Failed, invalid config." );
		}
	})
	.fail(function() {
		console.log( "Failed, could not retrieve settings." );
	});
};

/* Fan control buttons */
var update_speed_on_slider_change=1;

function fanOff(){
	update_speed_on_slider_change=0;
	var slider = document.getElementById("slider_fan_speed");
	slider.value=0;
	$.get("/ajax/setStatus?FAN-SPEED-REF=0&FAN-ENABLED-REF=0");
	setTimeout(function(){
				update_speed_on_slider_change=1;
				refreshStatus();
	}, 100);
};

function fanResume(){
	update_speed_on_slider_change=0;
	var slider = document.getElementById("slider_fan_speed");
	slider.value=0;
	// todo change to POST
	$.get("/ajax/setStatus?FAN-RESUME=1");
	setTimeout(function(){
				update_speed_on_slider_change=1;
				refreshStatus();
	}, 100);
};

function fanSpeed(){
	if(update_speed_on_slider_change==1){
		var slider = document.getElementById("slider_fan_speed");
		
		// todo change to POST
		$.get("/ajax/setStatus?FAN-SPEED-REF=" + slider.value + "&FAN-ENABLED-REF=1");
		setTimeout(refreshStatus, 100);
	}
}

/* Page control */
function pageVisibilityUpdate(){
	var mode=$("input[name=OPERATING_MODE]:checked").val();
	//console.log(mode);
	
	unhideAmbientcompensationParameters();
	unhideAdvancedParameters();
			
	switch(mode){
		case "0":
			state = ["generic", "wifi_configuration", "mqtt_configuration", "control_parameters", "configuration_done"];
			break;
			
		case "1":
			state = ["generic", "mqtt_slave_configuration", "wifi_configuration", "mqtt_configuration", "configuration_done"];
			break;
			
		case "2":
		default:
			state = ["generic", "control_parameters", "configuration_done"];
			break;
			
		case "3":
			state = ["generic", "wifi_configuration", "control_parameters", "configuration_done"];
			break;
	}
}

function showFirstPage(){
	document.querySelector("#loading").style.display = "none";
	document.querySelector("#"+state[form_state]).style.display = "block";

	handleButtonVisibility();
	pageVisibilityUpdate();
}

function nextPage(){
	document.querySelector("#"+state[form_state]).style.display = "none";
	form_state++;
	document.querySelector("#"+state[form_state]).style.display = "block";

	handleButtonVisibility();
	window.scrollTo(0, 0);	
}

function prevPage(){
	document.querySelector("#"+state[form_state]).style.display = "none";
	form_state--;
	document.querySelector("#"+state[form_state]).style.display = "block";

	handleButtonVisibility();
	window.scrollTo(0, 0);	
}

function handleButtonVisibility(){
	
	if(form_state >= state.length-1){
		document.querySelector("#btn_next").style.display = "none"; // hide "next" button
	}else{
		document.querySelector("#btn_next").style.display = "inline-block"; // show "next" button
	}
	
	if(form_state <= 0){
		document.querySelector("#btn_previous").style.display = "none"; // hide "previous" button
	}else{
		document.querySelector("#btn_previous").style.display = "inline-block"; // show "previous" button
	}
}

/* Advanced parameters visibility */
function unhideAdvancedParameters(){
	var show_parameters=$("input[name=unhide_advanced_parameters]:checked").val();
	
	if(show_parameters == "1"){
		document.querySelector("#advanced_parameters").style.display = "inline-block"; // hide parameters
	}else{
		document.querySelector("#advanced_parameters").style.display = "none"; // show parameters
	}
}

function unhideAmbientcompensationParameters(){
	var show_parameters=$("input[name=ENABLE_AMBIENTCOMPENSATION]:checked").val();
	
	if(show_parameters == "1"){
		document.querySelector("#ambientcompensation_parameters").style.display = "inline-block"; // hide parameters
	}else{
		document.querySelector("#ambientcompensation_parameters").style.display = "none"; // show parameters
	}
}

/* Input box sanity */
let max_length=32;

function filterFieldLettersNumbersHyphen(e) {
  let t = e.target;
  let badValues = /[^\w\d-]/gi;
  t.value = t.value.replace(badValues, '').substring(0,max_length);
}

function filterFieldNumbers(e) {
  let t = e.target;
  t.value = t.value.substring(0,max_length);
  t.value = t.value.replace(",",".");
    
  t.value = Number(t.value).toFixed(2);
  
  let valid = /^\d{1,3}(\.(\d{1,2}))?$/; // only numbers in format xxx.xx or xxx are allowed
  
  if (!valid.test(t.value)) {
    t.value = "0.0";
  }

}

function filterFieldIntegers(e) {
  filterFieldNumbers(e);

  let t = e.target;
  t.value = Number(t.value).toFixed(0);
  
  let valid = /^\d{1,5}$/; // only numbers in format xxxxxx are allowed
  if (!valid.test(t.value)) {
    t.value = "0";
  }

}

function filterFieldPercentage(e) {
  filterFieldNumbers(e);

  let t = e.target;
  t.value = Number(t.value).toFixed(0);

  let valid = /^\d{1,3}$/; // only numbers in format xxxxxx are allowed

  if (!valid.test(t.value)) {
    t.value = "0";
  }
}

function filterFieldIPAddressInline(e) {
  let t = e.target;
  let badValues = /[^\d.]/gi;
  t.value = t.value.replace(badValues, '').substring(0,max_length);
}

function filterFieldIPAddressOnBlur(e) {
  let t = e.target;
  let badValues = /[^\d.]/gi;
  let valid = /^([01]?\d\d?|2[0-4]\d|25[0-5])\.([01]?\d\d?|2[0-4]\d|25[0-5])\.([01]?\d\d?|2[0-4]\d|25[0-5])\.([01]?\d\d?|2[0-4]\d|25[0-5])$/;
  t.value = t.value.replace(badValues, '').substring(0,max_length);
  if (!valid.test(t.value)) {
    t.value = "0.0.0.0";
  }
}

function filterMaxLength(e) {
  let t = e.target;
  t.value = t.value.substring(0,max_length);
}

function filterMaxLengthExtra(e) {
  let t = e.target;
  t.value = t.value.substring(0, 2*max_length);
}

document.getElementById('NODE_NAME').addEventListener('input', filterFieldLettersNumbersHyphen);

document.getElementById('WIFI_SSID').addEventListener('input', filterMaxLength);
document.getElementById('WIFI_PASSWORD').addEventListener('input', filterMaxLengthExtra);

document.getElementById('MASTER_NODE_NAME').addEventListener('input', filterFieldLettersNumbersHyphen);

document.getElementById('MQTT_SERVER').addEventListener('input', filterFieldIPAddressInline);
document.getElementById('MQTT_SERVER').addEventListener('blur', filterFieldIPAddressOnBlur);

document.getElementById('MQTT_USERNAME').addEventListener('input', filterMaxLength);
document.getElementById('MQTT_PASSWORD').addEventListener('input', filterMaxLengthExtra);

document.getElementById('HEATING_FAN_ENABLE_DELTA_T').addEventListener('blur', filterFieldNumbers);
document.getElementById('COOLING_FAN_ENABLE_DELTA_T').addEventListener('blur', filterFieldNumbers);
document.getElementById('ENABLE_T_HYSTERESIS').addEventListener('blur', filterFieldNumbers);
document.getElementById('HEATING_LOWER_TEMPERATURE').addEventListener('blur', filterFieldNumbers);
document.getElementById('HEATING_UPPER_TEMPERATURE').addEventListener('blur', filterFieldNumbers);
document.getElementById('HEATING_SPEED_LIMIT').addEventListener('blur', filterFieldPercentage);
document.getElementById('HEATING_SPEED_BOOST_LIMIT').addEventListener('blur', filterFieldPercentage);
document.getElementById('COOLING_SPEED').addEventListener('blur', filterFieldPercentage);
document.getElementById('COOLING_SPEED_BOOST').addEventListener('blur', filterFieldPercentage);
document.getElementById('FAN_OFF_DELAY').addEventListener('blur', filterFieldIntegers);

document.getElementById('AMBIENTCOMPENSATION_TARGET').addEventListener('blur', filterFieldNumbers);
document.getElementById('AMBIENTCOMPENSATION_KP').addEventListener('blur', filterFieldNumbers);
document.getElementById('AMBIENTCOMPENSATION_KI').addEventListener('blur', filterFieldNumbers);


