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
		console.log( "Done." );
	};

	reader.readAsText(file);
}


function loadForm(){
	console.log( "Retrieving settings..." );
	$.getJSON('ajax/getConfig', (data) => {
		// parse JSON data to form
    var frm="#configuration";
    populateFields(frm, data);

    document.querySelector("#loading").style.display = "none";
    document.querySelector("#config").style.display = "block";

    console.log( "Done." );
	})
	.fail(function() {
		console.log( "Failed, could not retrieve settings." );
	});	
};

function loadFormDefaults(){
	console.log( "Retrieving defaults..." );
	$.getJSON('ajax/getDefaultConfig', (data) => {
		// parse JSON data to form
    var frm="#configuration";

    populateFields(frm, data);

    console.log( "Done." );
	})
	.fail(function() {
		console.log( "Failed, could not retrieve settings." );
	});
};