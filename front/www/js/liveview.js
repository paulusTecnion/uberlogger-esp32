$( document ).ready(function() {
	$.ajax({
		url: "js/plotly-basic-2.22.0.min.js",
		dataType: "script",
		success: function(){
			setInterval(renderValueList, 1000);
		}
	});
});

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
	// parse JSON data to input list
	htmlstring=[];
	htmlstring+="<p>Timestamp of data: " + valuesData["TIMESTAMPSTR"] + "</p>";

	$.each(valuesData["READINGS"], function(category, category_values){
		htmlstring+="<div class='block greybox' style='font-size: smaller;'><h2 class='first'>" + sanitizeCategoryName(category) + "</h2>";
		htmlstring+="<table width='100%'>";
		htmlstring+="<tr><th>Input</th><th align='right'>" + category_values["UNITS"] + "</th></tr>";

		let n=0;
		$.each(category_values["VALUES"], function(channel, channel_value){
			n++;
			htmlstring+="<tr><td>" + channel + "</td>";

			if(typeof(channel_value) === "number"){
				if (channel.startsWith("T")) {
					// Round to one decimal place for Tx
					htmlstring += "<td align='right'>" + channel_value.toFixed(1) + "</td></tr>";
				} else if (channel.startsWith("DI")) {
					// Round to zero decimal places for DI
					htmlstring += "<td align='right'>" + channel_value.toFixed(0) + "</td></tr>";
				} else {
					// Default formatting for other cases
					htmlstring += "<td align='right'>" + parseFloat(channel_value).toFixed(3) + "</td></tr>";
				}
			}

			storeDataPoint(category, channel, valuesData["TIMESTAMP"], channel_value, category_values["UNITS"]);
		});

		if(n==0){
			htmlstring+="<tr><td>-</td><td align='right'>-</td></tr>";
		}

		htmlstring+="</table></div>";
	});

	$("#values>#valuelist").html(htmlstring);

	plotDataPoints();
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

	let config={
		responsive: true,
		displayModeBar: true,
		modeBarButtonsToRemove: ['lasso2d']
	}

	if(plot_drawn_state==0){
	let layout={
		datarevision: Number(new Date()),
		margin: {
				l: 75,
				r: 15,
				t: 45,
				pad: 0
		},
		title: {
			text:'Measured data',
			xref: 'paper',
			font: {
				size: 16
			},
			x: 0.5,
		},
		xaxis: {
			title: {
				text: 'Time',
				font: {
					size: 14
				}
			},
		}	
	};
	Plotly.newPlot(divPlot, dataPointsArray, layout, config);
	plot_drawn_state=1;
	
	}else{
	// we have a plot, do update of data only
	let layout={
		datarevision: Number(new Date())
	}
	Plotly.update(divPlot, dataPointsArray, layout, config);
	}
}
