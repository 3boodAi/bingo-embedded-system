const { WebSocket } = require('ws');

const target = process.env.HARDWARE_WS_URL || 'ws://localhost:3000/hardware';
const ws = new WebSocket(target);

let phase = 'idle';
let drawTimer = null;
const bag = [];

function refillBag() {
  bag.length = 0;
  for (let value = 1; value <= 75; value += 1) bag.push(value);
  for (let index = bag.length - 1; index > 0; index -= 1) {
    const swapIndex = Math.floor(Math.random() * (index + 1));
    [bag[index], bag[swapIndex]] = [bag[swapIndex], bag[index]];
  }
}

function send(payload) {
  if (ws.readyState === WebSocket.OPEN) ws.send(JSON.stringify(payload));
}

function stopDrawing() {
  clearInterval(drawTimer);
  drawTimer = null;
}

function startDrawing() {
  stopDrawing();
  refillBag();
  phase = 'playing';
  drawTimer = setInterval(() => {
    if (!bag.length) refillBag();
    const number = bag.pop();
    console.log(`Drawn number: ${number}`);
    send({ type: 'drawn_number', number });
  }, Number(process.env.FAKE_DRAW_MS || 2500));
}

ws.on('open', () => {
  console.log(`Fake ESP32 connected to ${target}`);
  send({ type: 'hardware_hello', deviceId: 'fake-esp32', version: 'simulator' });
  setInterval(() => send({ type: 'heartbeat', uptimeMs: Date.now() }), 15000);
});

ws.on('message', (raw) => {
  const message = JSON.parse(raw.toString());
  if (message.type === 'start_game') {
    phase = 'countdown';
    console.log('Countdown started');
    setTimeout(startDrawing, message.countdownMs || 4500);
  }
  if (message.type === 'game_over' || message.type === 'reset') {
    phase = message.type;
    stopDrawing();
    console.log(`Round stopped: ${message.type}`);
  }
});

ws.on('close', () => {
  stopDrawing();
  console.log(`Fake ESP32 disconnected while ${phase}`);
});

ws.on('error', (error) => {
  console.error(error.message);
});
