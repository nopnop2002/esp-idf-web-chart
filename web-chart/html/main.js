//document.getElementById("datetime").innerHTML = "WebSocket is not connected";

var websocket = new WebSocket('ws://'+location.hostname+'/');
var meter1 = 0;
var meter2 = 0;
var meter3 = 0;

function pauseDatasetPush(name) {
	console.log('pauseDatasetPush');
	sendText(name);

	var button = document.getElementById("pauseDataset");
	button.style.backgroundColor = "lightblue";
}

function fixedDatasetPush(name) {
	console.log('fixedDatasetPush');
	sendText(name);

	var button = document.getElementById("fixedDataset");
	button.style.backgroundColor = "lightblue";
}


function resumeDatasetPush(name) {
	console.log('resumeDatasetPush');
	sendText(name);

	var button = document.getElementById("pauseDataset");
	button.style.backgroundColor = "";
	var button = document.getElementById("fixedDataset");
	button.style.backgroundColor = "";
}

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
	console.log("values=" + values);
	switch(values[0]) {
		case 'HEAD':
			console.log("HEAD values[1]=" + values[1]);
			//var h1 = document.getElementById( 'header' );
			//h1.textContent = values[1];
			break;

		case 'METER':
			console.log("METER values[1]=" + values[1]);
			console.log("METER values[2]=" + values[2]);
			console.log("METER values[3]=" + values[3]);
			console.log("config=" + Object.keys(config));
			console.log("config.data.datasets=" + Object.keys(config.data.datasets));
			console.log("config.data.datasets.length=", config.data.datasets.length);
			console.log("config.data.datasets[0]=" + Object.keys(config.data.datasets[0]));
			console.log("config.data.datasets[0].label=", config.data.datasets[0].label);
			console.log("config.data.datasets[0].backgroundColor=", config.data.datasets[0].backgroundColor);
			console.log("config.data.datasets[0].borderColor=", config.data.datasets[0].borderColor);
			if (values[1] != "") {
				config.data.datasets[0].label=values[1];
				config.data.datasets[0].backgroundColor = color(chartColors.red).alpha(0.5).rgbString();
				config.data.datasets[0].borderColor = chartColors.red;
				meter1 = 1;
			}
			if (values[2] != "") {
				config.data.datasets[1].label=values[2];
				config.data.datasets[1].backgroundColor = color(chartColors.green).alpha(0.5).rgbString();
				config.data.datasets[1].borderColor = chartColors.green;
				meter2 = 1;
			}
			if (values[3] != "") {
				config.data.datasets[2].label=values[3];
				config.data.datasets[2].backgroundColor = color(chartColors.blue).alpha(0.5).rgbString();
				config.data.datasets[2].borderColor = chartColors.blue;
				meter3 = 1;
			}
			window.myChart.update();

		case 'DATA':
			console.log("DATA values[1]=" + values[1]);
			var voltage1 = parseInt(values[1], 10);
			var now = Date.now();
			window.myChart.data.datasets[0].data.push({
				x: now,
				y: voltage1
			});
			if (meter2) {
				console.log("DATA values[2]=" + values[2]);
				var voltage2 = parseInt(values[2], 10);
				var now = Date.now();
				window.myChart.data.datasets[1].data.push({
					x: now,
					y: voltage2
				});
			}
			if (meter3) {
				console.log("DATA values[3]=" + values[3]);
				var voltage3 = parseInt(values[3], 10);
				var now = Date.now();
				window.myChart.data.datasets[2].data.push({
					x: now,
					y: voltage3
				});
			}
/*
			var counter = 1;
			window.myChart.data.datasets.forEach(function(dataset) {
				var val = parseInt(values[counter], 10);
				console.log("counter=%d val=%d", counter, val);
				dataset.data.push({
					x: now,
					//y: randomScalingFactor()
					y: val
				});
				counter++;
			});
*/

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
