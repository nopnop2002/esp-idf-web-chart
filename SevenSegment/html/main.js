//document.getElementById("datetime").innerHTML = "WebSocket is not connected";

var websocket = new WebSocket('ws://'+location.hostname+'/');
var meter1 = 0;
var meter2 = 0;
var meter3 = 0;
var segment1 = new SevenSegmentDisplay("canvas1");
var segment2 = new SevenSegmentDisplay("canvas2");
var segment3 = new SevenSegmentDisplay("canvas3");
var gpio1 = "";
var gpio2 = "";
var gpio3 = "";
var uint = 1;

function sendText(name) {
	console.log('sendText');
	var data = {};
	data["id"] = name;
	console.log('data=', data);
	json_data = JSON.stringify(data);
	console.log('json_data=' + json_data);
	websocket.send(json_data);
}

window.onload = function() {
    console.log("onload");
	// ColorScheme is 1 to 6
    segment1.ColorScheme = 2;
    segment1.DecimalPointType = 2;
    segment1.NumberOfDecimalPlaces = 0;
    segment1.NumberOfDigits = 4;

    segment2.ColorScheme = 3;
    segment2.DecimalPointType = 2;
    segment2.NumberOfDecimalPlaces = 0;
    segment2.NumberOfDigits = 4;

    segment3.ColorScheme = 4;
    segment3.DecimalPointType = 2;
    segment3.NumberOfDecimalPlaces = 0;
    segment3.NumberOfDigits = 4;
};

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
			console.log("METER values[1]=" + values[1]);
			console.log("METER values[2]=" + values[2]);
			console.log("METER values[3]=" + values[3]);
			if (values[1] == "") {
				document.getElementById("label1").style.display = "none";
				document.getElementById("canvas1").style.display = "none";
			} else {
				meter1 = 1;
				gpio1 = values[1];
				document.getElementById("label1").style.display = "inline-block";
				document.getElementById("label1").innerText = gpio1 + " [mV]";
				document.getElementById("canvas1").style.display = "inline-block";
			}
			if (values[2] == "") {
				document.getElementById("label2").style.display = "none";
				document.getElementById("canvas2").style.display = "none";
			} else {
				meter2 = 1;
				gpio2 = values[2];
				document.getElementById("label2").style.display = "inline-block";
				document.getElementById("label2").innerText = gpio2 + " [mV]";
				document.getElementById("canvas2").style.display = "inline-block";
			}
			if (values[3] == "") {
				document.getElementById("label3").style.display = "none";
				document.getElementById("canvas3").style.display = "none";
			} else {
				meter3 = 1;
				gpio3 = values[3];
				document.getElementById("label3").style.display = "inline-block";
				document.getElementById("label3").innerText = gpio3 + " [mV]";
				document.getElementById("canvas3").style.display = "inline-block";
			}
			break;

		case 'UNIT':
			console.log("UNIT");
			uint = 1000;
    		segment1.NumberOfDecimalPlaces = 3;
    		segment2.NumberOfDecimalPlaces = 3;
    		segment3.NumberOfDecimalPlaces = 3;
			document.getElementById("label1").innerText = gpio1 + " [V]";
			document.getElementById("label2").innerText = gpio2 + " [V]";
			document.getElementById("label3").innerText = gpio3 + " [V]";
			break;

		case 'DATA':
			console.log("DATA values[1]=" + values[1]);
			var voltage1 = parseInt(values[1], 10);
			segment1.Value = voltage1 / uint;
			if (meter2) {
				console.log("DATA values[2]=" + values[2]);
				var voltage2 = parseInt(values[2], 10);
				segment2.Value = voltage2 / uint;
			}
			if (meter3) {
				console.log("DATA values[3]=" + values[3]);
				var voltage3 = parseInt(values[3], 10);
				segment3.Value = voltage3 / uint;
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
