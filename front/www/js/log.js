$( document ).ready(function() {
    filebrowserRefresh();

    getStatus();

    setInterval(getStatus, 5000);    
});

function getStatus(){
  parent="#log";
  query="getStatus";

	$.getJSON('./ajax/' + query, (data) => {
		// parse JSON data to div

    // sanitize data
    let datetimestr = new Date(Number(data["TIMESTAMP"]));
    data["TIMESTAMPSTR"] = String(datetimestr);

    switch(data["LOGGER_STATE"]){
      default:
      case 0:
        data["LOGGER_STATE"]="INIT";
        break;

      case 1:
        data["LOGGER_STATE"]="IDLE";
        break;

      case 2:
        data["LOGGER_STATE"]="LOGGING";
        break;
        
      case 3:
        data["LOGGER_STATE"]="SETTINGS";
         break;
    }

    if(data["LOGGER_STATE"] == "IDLE"){
      $("#btn_logger_start").removeAttr("disabled");
    }else{
      $("#btn_logger_start").attr("disabled", true);
    }
    if(data["LOGGER_STATE"] == "LOGGING"){
      $("#btn_logger_stop").removeAttr("disabled");
    }else{
      $("#btn_logger_stop").attr("disabled", true);
    }

    
    // populate form
		populateFields(parent, data);
	})
	.fail(function() {
    alert("Error: could not update status.");
		console.log("Data query failed.");
	});		

}

function filebrowserRefresh(){
  parent="#filelist";
  query="getFileList";

	$.getJSON('./ajax/' + query, (data) => {
    let htmlstring=[];
    htmlstring+="<table width='100%'>";
    htmlstring+="<tr><th width='60%'>Name</th><th width='20%'>Size</th><th width='20%'>Action</th></tr>"
    htmlstring=buildFileTree(data["root"], htmlstring, 1, "");
    htmlstring+="</table>";

    $("#filelist").html(htmlstring);

    console.log("Data query done.");
	})
	.fail(function() {
    alert("Error: could not get list of SD-card files.");
		console.log("Data query failed.");
	});		

}

function filebrowserFormat(){
  $.getJSON('./ajax/filebrowserFormat', (data) => {
    // to do: implement a proper response sequence
    alert("SD-card now formatting.");

	})
	.fail(function() {
    alert("Error: could not format SD-card.");
		console.log("Data query failed.");
	});		

}



function buildFileTree(data, htmlstring, depth, path){

  $.each(data, function(key, value){
    if(value["TYPE"] == "FILE"){
      htmlstring+="<tr>";
      htmlstring+="<td style='padding-left: " + depth * 10 + "px;'>" + value["NAME"] + "</td>";
      htmlstring+="<td>" + value["SIZE"] + " MB</td>";
      htmlstring+="<td><a href='/ajax/getFile?file=" + encodeURIComponent(path + "/" + value["NAME"]) + "'>download</a></td>";
      htmlstring+="</tr>";
    }else if(value["TYPE"] == "DIRECTORY"){
      htmlstring+="<tr>";
      htmlstring+="<td style='padding-left: " + depth * 10 + "px;'><b><i>" + value["NAME"] + "</i></b></td>";
      htmlstring+="<td><i>(directory)</i></td>";
      htmlstring+="<td></td>";
      htmlstring+="</tr>";
      
      htmlstring=buildFileTree(value, htmlstring, depth + 1, path + "/" + value["NAME"]);
    }
  });
  
  return(htmlstring);

}
