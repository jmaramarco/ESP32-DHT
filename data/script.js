var gateway = `ws://${window.location.hostname}/ws`;
var websocket;

window.addEventListener('load', onLoad);

function initWebSocket() {
  console.log('Trying to open a WebSocket connection...');
  websocket = new WebSocket(gateway);
  websocket.onopen = onOpen;
  websocket.onclose = onClose;
  websocket.onmessage = onMessage;
}

function onOpen(event) {
  console.log('Connection opened');
}

function onClose(event) {
  console.log('Connection closed');
  setTimeout(initWebSocket, 2000);
}

function onMessage(event) {
  var data = JSON.parse(event.data);
  document.getElementById('temp').innerHTML = data.temperature;
  document.getElementById('hum').innerHTML = data.humidity;
}

function onLoad(event) {
  initWebSocket();
}