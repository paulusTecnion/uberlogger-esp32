$.getScript("js/plotly-basic-2.18.2.min.js");

setInterval(renderValueList, 250);

var dataPoints={};
var dataPointsArray=[];
var plot_drawn_state=0;
const divPlot="plotly";

function sanitizeCategoryName(category){
  switch(category){
    case "ANALOG":
      return("Analog");
      break;

    case "DIGITAL":
      return("Digital");
      break;

    case "TEMPERATURE":
      return("Temperature");
      break;
    
    default:
      return(category);
  }
}

function renderValueList(){
    const query='getValues';

    $.getJSON('./ajax/' + query, (data) => {
      // parse JSON data to input list
      data['TIMESTAMP'] = Date.now();
      let datetimestr = new Date(Number(data["TIMESTAMP"]));

      let htmlstring=[];
      htmlstring+="<p>Timestamp of data: " + datetimestr + "</p>";

      $.each(data["READINGS"], function(category, category_values){

        htmlstring+="<div class='block' style='width: 150px;'><h3 class='first'>" + sanitizeCategoryName(category) + "</h3>";
        htmlstring+="<table>";
        htmlstring+="<tr><th>Input</th><th align='right'>" + category_values["UNITS"] + "</th></tr>";

        let n=0;
        $.each(category_values["VALUES"], function(channel, channel_value){
          n++;
          htmlstring+="<tr><td>" + channel + "</td>";

          if(typeof(channel_value) === "number"){
            htmlstring+="<td align='right'>" + parseFloat(channel_value).toFixed(3) + "</td></tr>";
          }else{
            htmlstring+="<td align='right'>" + channel_value + "</td></tr>";
          }

          storeDataPoint(category, channel, data["TIMESTAMP"], channel_value, category_values["UNITS"]);
        });

        if(n==0){
          htmlstring+="<tr><td>-</td><td align='right'>-</td></tr>";
        }

        htmlstring+="</table></div>";
      });

      $("#values>#valuelist").html(htmlstring);

      plotDataPoints();

    })
    .fail(function() {
      console.log("Data query failed.");
    });		
  
}


function storeDataPoint(category, channel, timestamp, value, unit){
  if(typeof(dataPoints[category + "." + channel])=="undefined"){
    // add channel
    dataPoints[category + "." + channel]={
      x:[],
      y:[],
      type:'scatter',
      name: category + "." + channel
    };
  }

  dataPoints[category + "." + channel]["x"].push(new Date(timestamp));
  dataPoints[category + "." + channel]["y"].push(value);
}


function plotDataPoints(){
  const dataPointsArray=Object.values(dataPoints);

  let layout={
    datarevision: Number(new Date()),
	  margin:{
		  l: 30,
		  r: 10,
		  t: 30,
		  pad: 0
  	}
  };

  let config={
	  responsive: true
  }

  if(plot_drawn_state==0){
    Plotly.newPlot(divPlot, dataPointsArray, layout, config);
    plot_drawn_state=1;
  }else{
    // we have a plot, do update of data only
    Plotly.update(divPlot, dataPointsArray, layout, config);
  }
}
