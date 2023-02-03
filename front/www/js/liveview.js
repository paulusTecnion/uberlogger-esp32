$.getScript("js/plotly-2.16.1-basic.min.js");

setInterval(renderValueList, 1000);

var dataPoints={};
var dataPointsArray=[];
var plot_drawn_state=0;

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
      let datetimestr = new Date(Number(data["TIMESTAMP"]));

      let htmlstring=[];
      htmlstring+="<p>Timestamp of data: " + datetimestr + "</p>";

      $.each(data["READINGS"], function(category, category_values){

        htmlstring+="<div class='block'><h3 class='first'>" + sanitizeCategoryName(category) + "</h3>";
        htmlstring+="<table>";
        htmlstring+="<tr><th>Input</th><th align='right'>" + category_values["UNITS"] + "</th></tr>";

        let n=0;  
        $.each(category_values["VALUES"], function(channel, channel_value){
          n++;
          htmlstring+="<tr><td>" + channel + "</td><td align='right'>" + channel_value + "</td></tr>";
          storeDataPoint(category, channel, data["TIMESTAMP"], channel_value, category_values["UNITS"]);
        });

        if(n==0){
          htmlstring+="<tr><td>-<td><td align='right'>-</td></tr>";
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
  const divPlot="plotly";
  
  // convert JSON to array
  const dataPointsArray=Object.values(dataPoints);
  let layout={
    datarevision: Number(new Date())
  };

  if(plot_drawn_state==0){
    Plotly.newPlot(divPlot, dataPointsArray, layout);
    plot_drawn_state=1;
  }else{
    // we have a plot, do update of data only
    Plotly.update(divPlot, dataPointsArray, layout);
  }
}