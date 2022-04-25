//document.getElementById("datetime").innerHTML = "WebSocket is not connected";

var websocket = new WebSocket('ws://'+location.hostname+'/');
var meter1 = 0;
var meter2 = 0;
var meter3 = 0;


/*
function sendText(name) {
	console.log('sendText');
	var data = {};
	data["id"] = name;
	console.log('data=', data);
	json_data = JSON.stringify(data);
	console.log('json_data=' + json_data);
	websocket.send(json_data);
}
*/

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
				document.getElementById("label1").innerText = values[1] + " [mV]";
				meter1 = 1;
			} 
			if (values[2] != "") {
				document.getElementById("label2").innerText = values[2] + " [mV]";
				meter2 = 1;
			} else {
				document.getElementById("label2").style.display ="none";
				document.getElementById("chart2").style.display ="none";
			}
			if (values[3] != "") {
				document.getElementById("label3").innerText = values[3] + " [mV]";
				meter3 = 1;
			} else {
				document.getElementById("label3").style.display ="none";
				document.getElementById("chart3").style.display ="none";
			}
			break;

		case 'DATA':
			console.log("DATA values[1]=" + values[1]);
			var timeVal = getChartTime();
			var voltage1 = parseInt(values[1], 10);
			// Set line color
			var catname = "category1";
			chart1.getVisibleLayers()[0].className = "layer " + catname;
			chart1.push([ {time: timeVal, y: voltage1} ]);
			if (meter2) {
				console.log("DATA values[2]=" + values[2]);
				var voltage2 = parseInt(values[2], 10);
				// Set line color
				var catname = "category2";
				chart2.getVisibleLayers()[0].className = "layer " + catname;
				chart2.push([ {time: timeVal, y: voltage2} ]);
			}
			if (meter3) {
				console.log("DATA values[3]=" + values[3]);
				var voltage3 = parseInt(values[3], 10);
				// Set line color
				var catname = "category3";
				chart3.getVisibleLayers()[0].className = "layer " + catname;
				chart3.push([ {time: timeVal, y: voltage3} ]);
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
