//document.getElementById("datetime").innerHTML = "WebSocket is not connected";

var websocket = new WebSocket('ws://'+location.hostname+'/');

var data1 = [
	{
	domain: { x: [0, 1], y: [0, 1] },
	title: { text: "" },
	value: 0,
	type: "indicator",
	mode: "gauge+number",
    gauge: {
    	axis: { range: [null, 3000] },
    	steps: [
        	{ range: [0, 2000], color: "cyan" },
        	{ range: [2000, 3000], color: "royalblue" }
    	],
    	}
	}
];

var data2 = data1;
var data3 = data1;

var layout = { width: 400, height: 300, margin: { t: 0, b: 0 } };

Plotly.newPlot('gauge1', data1, layout);
Plotly.newPlot('gauge2', data2, layout);
Plotly.newPlot('gauge3', data3, layout);

var meter1 = 0;
var meter2 = 0;
var meter3 = 0;
var gpio1 = "";
var gpio2 = "";
var gpio3 = "";


function sendText(name) {
	console.log('sendText');
	var data = {};
	data["id"] = name;
	console.log('data=', data);
	json_data = JSON.stringify(data);
	console.log('json_data=' + json_data);
	websocket.send(json_data);
}

websocket.onopen = function(evt) {
	console.log('WebSocket connection opened');
	var data = {};
	data["id"] = "init";
	console.log('data=', data);
	json_data = JSON.stringify(data);
	console.log('json_data=' + json_data);
	websocket.send(json_data);
	//document.getElementById("datetime").innerHTML = "WebSocket is connected!";
}

websocket.onmessage = function(evt) {
	var msg = evt.data;
	console.log("msg=" + msg);
	var values = msg.split('\4'); // \4 is EOT
	//console.log("values=" + values);
	switch(values[0]) {
		case 'HEAD':
			console.log("HEAD values[1]=" + values[1]);
			var h1 = document.getElementById( 'header' );
			h1.textContent = values[1];
			break;

		case 'METER':
			//console.log("gauge1=" + Object.keys(gauge1.options));
			//console.log("gauge1.options.units=" + gauge1.options.units);
			console.log("METER values[1]=" + values[1]);
			console.log("METER values[2]=" + values[2]);
			console.log("METER values[3]=" + values[3]);
			if (values[1] != "") {
				meter1 = 1;
				gpio1 = values[1];
				document.getElementById("gauge1").style.display = "inline-block";
			}
			if (values[2] != "") {
				meter2 = 1;
				gpio2 = values[2];
				document.getElementById("gauge2").style.display = "inline-block";
			}
			if (values[3] != "") {
				meter3 = 1;
				gpio3 = values[3];
				document.getElementById("gauge3").style.display = "inline-block";
			}
			break;

		case 'DATA':
			console.log("DATA values[1]=" + values[1]);
			var voltage1 = parseInt(values[1], 10);
			var data1_update = {
				title: { text: gpio1 },
				value: voltage1,
			};
			Plotly.update('gauge1', data1_update, layout)

			var voltage2 = 0;
			var voltage3 = 0;
			if (meter2) {
				console.log("DATA values[2]=" + values[2]);
				voltage2 = parseInt(values[2], 10);
				var data2_update = {
					title: { text: gpio2 },
					value: voltage2,
				};
				Plotly.update('gauge2', data2_update, layout)
			}
			if (meter3) {
				console.log("DATA values[3]=" + values[3]);
				voltage3 = parseInt(values[3], 10);
				var data3_update = {
					title: { text: gpio3 },
					value: voltage3,
				};
				Plotly.update('gauge3', data3_update, layout)
			}
			break;

		default:
			break;
	}
}

websocket.onclose = function(evt) {
	console.log('Websocket connection closed');
	//document.getElementById("datetime").innerHTML = "WebSocket closed";
}

websocket.onerror = function(evt) {
	console.log('Websocket error: ' + evt);
	//document.getElementById("datetime").innerHTML = "WebSocket error????!!!1!!";
}
