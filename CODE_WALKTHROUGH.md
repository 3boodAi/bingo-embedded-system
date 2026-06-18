# Bingo Arcade Code Walkthrough

This file explains the project from A to Z. It is written for studying the code, not just using it.

## 1. Big Picture

The project has four main code areas:

| File / Folder | Job |
| --- | --- |
| `server.js` | Runs the Node.js server, lobby, game state, player cards, and hardware WebSocket. |
| `public/index.html` | Defines the screens players see in the browser. |
| `public/client.js` | Runs in each player's browser and talks to the server with Socket.IO. |
| `public/style.css` | Styles the browser interface. |
| `esp32u_bridge/esp32u_bridge.ino` | Runs on the ESP32U. Connects Wi-Fi/WebSocket and bridges messages to Arduino. |
| `arduino_uno_controller/arduino_uno_controller.ino` | Runs on Arduino Uno. Controls LCD, LEDs, DFPlayer, buzzer, buttons, and number drawing. |
| `audio/MP3/` | Holds the DFPlayer Mini sound files. |

## 2. Full Game Flow

1. Players open the website.
2. Browser connects to the Node.js server with Socket.IO.
3. Each player joins the lobby with a name.
4. Server gives each player a private Bingo card.
5. ESP32U connects to Wi-Fi and opens a hardware WebSocket to `/hardware`.
6. ESP32U tells Arduino whether the console is online.
7. Admin starts the game.
8. Server sends `start_game` to ESP32U.
9. ESP32U sends `START:<roundId>:<countdownMs>` to Arduino.
10. Arduino runs the countdown, buzzer, LCD, and LEDs.
11. Arduino draws numbers every few seconds.
12. Arduino displays and speaks each number.
13. Arduino sends `DRAW:<number>` to ESP32U.
14. ESP32U sends `{ "type": "drawn_number" }` to the server.
15. Server stores the drawn number, but browsers only see the count, not the number.
16. A player marks their card and clicks Claim Bingo.
17. Server validates the claim using the hidden drawn-number list.
18. If valid, server sends `game_over` to ESP32U.
19. ESP32U sends `WIN:<name>:<seat>:<ms>` to Arduino.
20. Arduino plays win effects on LCD, buzzer, LEDs, and DFPlayer.

## 3. Important Security Rule

The browser never receives the drawn numbers. It only receives `calledCount`.

That means players can see their own cards, but they must look at the physical LCD and listen to the physical speaker for the numbers. The server still validates every Bingo claim using the hidden `game.calledNumbers` array.

## 4. `server.js`

### Imports

```js
const express = require('express');
```

Loads Express, the web framework used to serve the website and HTTP routes.

```js
const http = require('http');
```

Loads Node's built-in HTTP module. Socket.IO and raw WebSockets need the same HTTP server.

```js
const path = require('path');
```

Loads path helpers so `public/` can be served safely across Windows/Linux.

```js
const { Server } = require('socket.io');
```

Loads Socket.IO's server class for browser real-time communication.

```js
const { WebSocketServer, WebSocket } = require('ws');
```

Loads the lower-level WebSocket server for the ESP32U hardware connection.

### Configuration

```js
const PORT = process.env.PORT || 3000;
```

Uses Render's `PORT` environment variable, or `3000` locally.

```js
const MAX_PLAYERS = Number(process.env.MAX_PLAYERS || 3);
```

Sets the lobby size. Default is 3 players.

```js
const CARD_SIZE = 5;
```

Each Bingo card is 5 by 5.

```js
const MAX_BALL = 75;
```

Numbers go from 1 to 75.

```js
const COUNTDOWN_MS = 4500;
```

Countdown phase lasts 4.5 seconds.

```js
const ALLOW_SOFTWARE_ONLY = process.env.ALLOW_SOFTWARE_ONLY === 'true';
```

When true, the server can start without hardware. In production this should stay false.

### Server Setup

```js
const app = express();
```

Creates the Express app.

```js
const server = http.createServer(app);
```

Wraps Express in a raw HTTP server so Socket.IO and WebSockets can attach to it.

```js
const io = new Server(server, { ... });
```

Creates the Socket.IO server for browser clients.

```js
cors: { origin: process.env.CORS_ORIGIN || '*', methods: ['GET', 'POST'] }
```

Allows browser connections. `*` is simple for this classroom project.

```js
const hardwareWss = new WebSocketServer({ noServer: true });
```

Creates a WebSocket server for hardware, but does not create a second HTTP port.

### HTTP Routes

```js
app.use(express.static(path.join(__dirname, 'public')));
```

Serves `public/index.html`, `client.js`, and `style.css`.

```js
app.get('/healthz', (_req, res) => res.json({ ok: true, state: game.phase }));
```

Health endpoint for Render or debugging.

```js
app.get('/api/status', (_req, res) => res.json(publicState()));
```

Returns public game state as JSON.

### Game State

```js
const game = { ... };
```

One object stores the whole live game.

| Field | Meaning |
| --- | --- |
| `phase` | Current state: `idle`, `lobby`, `countdown`, `playing`, `game_over`. |
| `roundId` | Unique ID for the current round. |
| `adminId` | Socket ID of the player allowed to start/reset. |
| `players` | `Map` of browser socket IDs to player objects. |
| `calledNumbers` | Hidden list of numbers drawn by the Arduino. |
| `startedAt` | Timestamp when active play begins. |
| `endedAt` | Timestamp when a win is accepted. |
| `winner` | Winner information after game over. |
| `hardware` | ESP32U connection status and socket. |
| `timers.countdown` | Server-side countdown timer ID. |

### Player Helpers

```js
function cleanName(name) { ... }
```

Converts a name to a safe short display string.

- `String(name || '')` prevents crashes if name is missing.
- `.trim()` removes extra spaces.
- `.replace(/[^\w .'-]/g, '')` removes unsafe/special characters.
- `.slice(0, 14)` keeps names short for the LCD/UI.
- `|| 'Player'` gives a fallback name.

```js
function publicState() { ... }
```

Returns state that every browser is allowed to see. It deliberately does not include `calledNumbers`.

```js
players: [...game.players.values()].map(...)
```

Turns the `Map` into an array for JSON.

```js
calledCount: game.calledNumbers.length
```

Browsers only know how many numbers were called.

```js
function playerState(socketId) { ... }
```

Returns private data for one player, including that player's card.

```js
function broadcastState() { ... }
```

Sends public lobby state to everyone, then sends each player their own private card state.

### Hardware Helper

```js
function sendHardware(payload) { ... }
```

Sends JSON to ESP32U if hardware is connected.

- Checks if socket exists.
- Checks if WebSocket is open.
- Sends `JSON.stringify(payload)`.
- Returns true or false.

### Round Lifecycle

```js
function clearCountdownTimer() { ... }
```

Stops an old countdown timer so a reset cannot leave a timer running.

```js
function resetRound(reason = 'reset') { ... }
```

Returns the game to lobby or idle.

- Clears countdown.
- Removes round data.
- Gives every player a new card.
- Tells hardware to reset.
- Tells browsers the round reset.

```js
function assignAdmin() { ... }
```

If the admin leaves, gives admin role to the first connected player.

```js
function nextSeat() { ... }
```

Finds the first open player seat from 1 to 3.

### Bingo Card Generation

```js
function createBingoCard() { ... }
```

Creates one 5x5 card.

```js
const ranges = [[1, 15], [16, 30], ...];
```

Standard Bingo columns:

- B: 1-15
- I: 16-30
- N: 31-45
- G: 46-60
- O: 61-75

```js
shuffle(values);
return values.slice(0, CARD_SIZE);
```

Randomizes a column and takes 5 numbers.

```js
index === 12 ? { value: 'FREE', free: true } : ...
```

The center square is free.

```js
function shuffle(values) { ... }
```

Uses the Fisher-Yates shuffle to randomize arrays.

### Starting The Game

```js
function allPlayersReady() { ... }
```

Returns true only if all 3 players joined and every player is ready.

```js
function startGame(socket) { ... }
```

Runs when admin clicks Start Game.

It checks:

- The requester is admin.
- No game is already running.
- Exactly 3 players joined.
- All players are ready.
- Hardware is connected unless software-only mode is enabled.

Then it:

- Sets phase to `countdown`.
- Creates a new `roundId`.
- Clears old called numbers and winner.
- Sends `start_game` to ESP32U.
- Emits `game_countdown` to browsers.
- Starts a server timer to move phase to `playing`.

### Claiming Bingo

```js
function claimBingo(socket, markedIndexes) { ... }
```

Runs when a browser clicks Claim Bingo.

It:

- Finds the player by socket ID.
- Rejects if the game is not playing.
- Calls `validateClaim`.
- If valid, sets game over state.
- Calculates win time.
- Sends winner to hardware.
- Emits winner to browsers.

### Win Validation

```js
function validateClaim(card, markedIndexes) { ... }
```

Checks whether a player's marked squares form a real winning line.

```js
const marked = new Set(...)
```

Builds a clean set of marked indexes.

```js
marked.add(12);
```

Center free square is always marked.

```js
const called = new Set(game.calledNumbers);
```

Uses the server's hidden called-number list.

```js
const allMarked = line.every(...)
```

Checks if the player marked the whole line.

```js
const allCalled = line.every(...)
```

Checks if every number in that line was actually drawn.

```js
function getWinningLines() { ... }
```

Creates all possible winning lines:

- 5 rows
- 5 columns
- 2 diagonals

### Hardware Message Handling

```js
function handleHardwareMessage(rawMessage) { ... }
```

Handles JSON sent by ESP32U.

```js
JSON.parse(rawMessage)
```

Turns text into a JavaScript object.

```js
message.type === 'hardware_hello'
```

Hardware announces itself. Server stores device ID and sends sync.

```js
message.type === 'drawn_number'
```

Hardware reports a drawn number. Server validates range, prevents duplicates, stores it, and only tells browsers the count.

```js
message.type === 'reset_requested'
```

Arduino system button requested reset.

```js
message.type === 'heartbeat'
```

ESP32U is alive. Server answers with current phase and time.

### Browser Socket.IO Events

```js
io.on('connection', (socket) => { ... });
```

Runs for each connected browser tab.

```js
socket.emit('lobby_state', publicState());
```

Immediately sends the current lobby state.

```js
socket.on('join_lobby', ...)
```

Adds a player to the lobby, creates their card, assigns seat/admin.

```js
socket.on('set_ready', ...)
```

Updates player ready status.

```js
socket.on('start_game', () => startGame(socket));
```

Lets admin try starting the game.

```js
socket.on('claim_bingo', ...)
```

Validates a Bingo claim.

```js
socket.on('reset_game', ...)
```

Lets admin reset the game.

```js
socket.on('disconnect', ...)
```

If player disconnects before the game, they are removed. During a game, they are marked disconnected so the round is not destroyed.

### Hardware WebSocket Upgrade

```js
server.on('upgrade', ...)
```

Handles raw WebSocket upgrade requests.

```js
if (url.pathname !== '/hardware') return;
```

Only `/hardware` is for ESP32U.

```js
hardwareWss.handleUpgrade(...)
```

Hands the connection to the hardware WebSocket server.

### Hardware WebSocket Connection

```js
hardwareWss.on('connection', (ws) => { ... });
```

Runs when ESP32U connects.

```js
if (game.hardware.socket && ...)
```

Closes the old hardware socket if a new console connects.

```js
game.hardware.connected = true;
```

Marks console online.

```js
ws.on('message', ...)
```

Routes ESP32U messages to `handleHardwareMessage`.

```js
ws.on('close', ...)
```

Marks console offline if it disconnects.

### Start Server

```js
server.listen(PORT, ...)
```

Starts the app.

## 5. `public/index.html`

HTML creates the visible screens. JavaScript later hides/shows them with the `active` class.

```html
<!doctype html>
```

Uses modern HTML.

```html
<html lang="en">
```

Declares English language for accessibility.

```html
<meta charset="utf-8">
```

Allows normal text characters.

```html
<meta name="viewport" content="width=device-width, initial-scale=1">
```

Makes the page responsive on phones.

```html
<link rel="stylesheet" href="/style.css">
```

Loads the CSS.

```html
<main class="app-shell">
```

Main wrapper around the app.

### Status Header

```html
<section class="topbar" aria-label="Arcade status">
```

Top status area.

```html
<span id="socket-status">
```

Browser connection indicator.

```html
<span id="hardware-status">
```

Physical console connection indicator.

### Join Screen

```html
<section id="join-screen" class="panel active">
```

First visible screen.

```html
<form id="join-form">
```

Player name form.

```html
<input id="player-name" ... maxlength="14" required>
```

Player name input. Max length matches the short-name design.

```html
<p id="join-message">
```

Area for join feedback.

### Lobby Screen

```html
<section id="lobby-screen" class="panel">
```

Screen used after joining.

```html
<div id="players-list">
```

JavaScript fills this with player rows.

```html
<input id="ready-toggle" type="checkbox">
```

Player ready control.

```html
<button id="start-button">
```

Admin start button.

```html
<button id="reset-lobby-button">
```

Admin reset button while in lobby.

### Countdown Screen

```html
<section id="countdown-screen" class="panel center" aria-live="polite">
```

Countdown screen. `aria-live` helps screen readers announce changes.

```html
<h2 id="countdown-value">3</h2>
```

JavaScript changes this to 3, 2, 1, GO.

### Game Screen

```html
<section id="game-screen" class="game-layout">
```

Active game view.

```html
<span id="timer-display">
```

Shows elapsed time.

```html
<span id="called-count">
```

Shows how many numbers were called, not which numbers.

```html
<div id="bingo-card">
```

JavaScript builds the 5x5 card here.

```html
<button id="claim-button">
```

Sends a Bingo claim.

### Winner Screen

```html
<section id="winner-screen">
```

Final result screen.

```html
<h2 id="winner-title">
```

Winner name goes here.

```html
<p id="winner-detail">
```

Win time goes here.

```html
<script src="/socket.io/socket.io.js"></script>
```

Loads Socket.IO browser client from the server.

```html
<script src="/client.js"></script>
```

Loads our browser app logic.

## 6. `public/client.js`

### Connection

```js
const socket = io();
```

Connects this browser tab to the Socket.IO server.

### Screen References

```js
const screens = { ... };
```

Stores each main screen DOM element so JavaScript can switch screens.

Each `document.getElementById(...)` finds one HTML element by ID.

### Browser State

```js
const state = { ... };
```

Stores live browser-only state.

| Field | Meaning |
| --- | --- |
| `lobby` | Latest public lobby state from server. |
| `player` | Private player state, including card. |
| `marked` | Set of card indexes this player marked. Starts with center free square. |
| `timer` | Active game timer interval ID. |
| `countdownTimer` | Countdown interval ID. |
| `countdownEndsAt` | Timestamp for countdown end. |

### Element References

```js
const elements = { ... };
```

Stores all important buttons, messages, and display areas.

This avoids repeating `document.getElementById` all over the file.

### Form And Button Events

```js
elements.playerName.value = localStorage.getItem('bingo.playerName') || '';
```

Restores the last used player name on this browser.

```js
elements.joinForm.addEventListener('submit', ...)
```

Runs when the Join form submits.

```js
event.preventDefault();
```

Stops the browser from refreshing the page.

```js
const playerName = elements.playerName.value.trim();
```

Reads and cleans the input.

```js
if (!playerName) return;
```

Does nothing if the input is empty.

```js
localStorage.setItem(...)
```

Saves the name for next time.

```js
socket.emit('join_lobby', { playerName });
```

Sends join request to the server.

```js
elements.readyToggle.addEventListener('change', ...)
```

Sends ready/unready status.

```js
elements.startButton.addEventListener('click', ...)
```

Asks the server to start. Server decides if allowed.

```js
elements.resetLobbyButton / resetGameButton
```

Ask the server to reset.

```js
elements.claimButton.addEventListener('click', ...)
```

Sends marked card indexes to the server for validation.

```js
elements.playAgainButton.addEventListener('click', ...)
```

Returns UI to lobby screen.

### Server Connection Events

```js
socket.on('connect', ...)
```

Shows browser WebSocket as connected.

```js
socket.on('disconnect', ...)
```

Shows reconnecting if connection drops.

### Game Events From Server

```js
socket.on('lobby_state', ...)
```

Receives public state, renders status/lobby, then chooses screen.

```js
socket.on('player_state', ...)
```

Receives this player's private card and ready/admin state.

```js
socket.on('game_countdown', ...)
```

Clears old marks, starts local countdown display, shows countdown screen.

```js
socket.on('game_started', ...)
```

Shows card, starts timer, shows game screen.

```js
socket.on('draw_progress', ...)
```

Updates only the number of called balls.

```js
socket.on('claim_rejected', ...)
```

Shows server rejection message.

```js
socket.on('game_over', ...)
```

Stops timers and shows winner screen.

```js
socket.on('round_reset', ...)
```

Clears local timers and marks after reset.

```js
socket.on('action_error', ...)
```

Shows start/reset errors in the correct screen message area.

### Screen Routing

```js
function routeForPhase() { ... }
```

Chooses which screen should be visible based on `state.lobby.phase`.

If there is no player, it shows Join.

If phase is:

- `countdown`: show countdown.
- `playing`: show game.
- `game_over`: show winner.
- anything else: show lobby.

### Lobby Rendering

```js
function renderStatus() { ... }
```

Updates hardware and called-count indicators.

```js
function renderLobby() { ... }
```

Builds the player seat list and admin controls.

```js
elements.playersList.innerHTML = '';
```

Clears old rows before rebuilding.

```js
for (let seat = 1; seat <= state.lobby.maxPlayers; seat += 1)
```

Creates one row per seat.

```js
state.lobby.players.find(...)
```

Finds who owns that seat.

```js
escapeHtml(player.name)
```

Prevents names from injecting HTML.

```js
const amAdmin = ...
```

Checks if this browser is the admin.

```js
elements.startButton.disabled = ...
```

Start button only enables when admin, full lobby, all ready, and hardware online.

### Bingo Card Rendering

```js
function renderCard(card) { ... }
```

Creates 25 card buttons.

```js
for (const [index, cell] of card.entries())
```

Loops through every card cell with its index.

```js
button.className = ...
```

Adds marked styling when needed.

```js
button.disabled = Boolean(cell.free);
```

Free square cannot be clicked.

```js
button.setAttribute('aria-pressed', ...)
```

Accessibility state for toggle buttons.

```js
button.addEventListener('click', ...)
```

Click toggles the marked state.

```js
function toggleCell(index, button) { ... }
```

Adds/removes the index from `state.marked` and updates the button.

### Bingo Claim Helpers

```js
function updateClaimButton() { ... }
```

Enables Claim only if the player has marked some possible line.

```js
function hasMarkedLine() { ... }
```

Checks rows, columns, and diagonals locally. This is only a UI helper. The server still does the real validation.

### Timers

```js
function startCountdownTicker() { ... }
```

Updates countdown text every 150 ms.

```js
const msLeft = Math.max(0, ...)
```

Never lets remaining time go below zero.

```js
Math.ceil(msLeft / 1000)
```

Shows 3, 2, 1 before GO.

```js
function startGameTimer(startedAt) { ... }
```

Updates elapsed time every 250 ms.

```js
function stopTimers() { ... }
```

Stops countdown and game timers.

### Small UI Helpers

```js
function showScreen(name) { ... }
```

Adds `active` class to one screen and removes it from others.

```js
function currentScreen() { ... }
```

Returns the name of the currently visible screen.

```js
function setStatus(element, text, kind) { ... }
```

Updates status-pill text and color class.

```js
function setMessage(element, text, isError = false) { ... }
```

Updates message text and optional error styling.

```js
function formatTime(ms) { ... }
```

Converts milliseconds into `MM:SS`.

```js
function escapeHtml(value) { ... }
```

Replaces unsafe HTML characters with safe entities.

## 7. `esp32u_bridge/esp32u_bridge.ino`

This file is the network bridge. It does not control the LCD or LEDs. It only connects the web server to Arduino.

### Header Comment

The top comment explains:

- ESP32U connects to Wi-Fi.
- ESP32U connects to Render WebSocket.
- ESP32U talks to Arduino over UART2.
- The voltage divider protects ESP32U GPIO4.

### Libraries

```cpp
#include <Arduino.h>
```

Loads Arduino functions like `setup`, `loop`, `millis`, and `String`.

```cpp
#include <WebSocketsClient.h>
```

Loads the WebSocket client library used to connect to Render.

```cpp
#include <WiFi.h>
```

Loads ESP32 Wi-Fi support.

### Wi-Fi Settings

```cpp
#if __has_include("wifi_config.h")
```

Checks whether a local private Wi-Fi file exists.

```cpp
#include "wifi_config.h"
```

Loads real Wi-Fi name/password from a local ignored file.

```cpp
#else
const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
#endif
```

Fallback placeholders for teammates who have not created `wifi_config.h`.

### Render WebSocket Settings

```cpp
const char* WS_HOST = "bingo-embedded-system.onrender.com";
```

The deployed server host.

```cpp
const uint16_t WS_PORT = 443;
```

HTTPS/WSS port.

```cpp
const char* WS_PATH = "/hardware";
```

The hardware WebSocket path on the Node server.

### UART And Timing Settings

```cpp
const uint8_t ARDUINO_RX_PIN = 4;
```

ESP32U receives Arduino messages on GPIO4.

```cpp
const uint8_t ARDUINO_TX_PIN = 17;
```

ESP32U sends messages to Arduino from GPIO17.

```cpp
const unsigned long HEARTBEAT_INTERVAL_MS = 15000;
```

ESP32U sends server heartbeat every 15 seconds.

```cpp
const unsigned long WIFI_STATUS_INTERVAL_MS = 5000;
```

If Wi-Fi is down, ESP32U tells Arduino every 5 seconds.

### Objects And State

```cpp
WebSocketsClient webSocket;
```

Creates the WebSocket client object.

```cpp
String arduinoLine = "";
```

Stores incoming UART characters until a full line arrives.

```cpp
bool wsConnected = false;
```

Tracks whether WebSocket is online.

```cpp
unsigned long lastHeartbeatAt = 0;
unsigned long lastWifiStatusAt = 0;
```

Timers using `millis()`.

### JSON Helpers

```cpp
String extractJsonString(...)
```

Finds a simple string value inside a JSON message.

This project uses small predictable JSON messages, so the bridge uses a tiny parser instead of a heavy JSON library.

```cpp
long extractJsonLong(...)
```

Finds a number value inside a JSON message.

### Messages To Server

```cpp
void sendWs(String payload)
```

Sends text to the server only if WebSocket is connected.

```cpp
void sendHardwareHello()
```

Builds a unique device ID from the ESP32 MAC and sends `hardware_hello`.

```cpp
void sendHeartbeat()
```

Sends uptime so the server knows hardware is alive.

### Server WebSocket Events

```cpp
void webSocketEvent(WStype_t type, uint8_t* payload, size_t length)
```

Callback called by the WebSocket library.

```cpp
if (type == WStype_CONNECTED)
```

When connected:

- Set `wsConnected = true`.
- Print to USB serial.
- Tell Arduino `NET:ONLINE`.
- Send hardware hello to server.

```cpp
if (type == WStype_DISCONNECTED)
```

When disconnected:

- Set `wsConnected = false`.
- Tell Arduino `NET:OFFLINE`.

```cpp
if (type != WStype_TEXT) return;
```

Ignores binary or other message types.

```cpp
message.reserve(length);
```

Pre-allocates memory for the incoming message.

```cpp
for (...) message += (char)payload[i];
```

Converts raw bytes into an Arduino `String`.

```cpp
String messageType = extractJsonString(message, "type");
```

Reads the server message type.

```cpp
if (messageType == "sync")
```

Forwards current phase to Arduino as `SYNC:<phase>`.

```cpp
if (messageType == "start_game")
```

Extracts round ID and countdown, then sends `START:<roundId>:<countdownMs>` to Arduino.

```cpp
if (messageType == "game_over")
```

Extracts winner information and sends `WIN:<name>:<seat>:<ms>` to Arduino.

```cpp
if (messageType == "reset")
```

Sends `RESET` to Arduino.

### Messages From Arduino

```cpp
void handleArduinoLine(const String& line)
```

Handles full lines received from Arduino.

```cpp
if (line.startsWith("DRAW:"))
```

Arduino drew a number. ESP32U sends it to server as JSON.

```cpp
if (line == "RESET_REQ")
```

Arduino system button requested reset.

```cpp
if (line == "HELLO")
```

Arduino is asking for connection status, so ESP32U sends hardware hello again.

### Arduino UART Reader

```cpp
while (Serial2.available())
```

Reads all waiting UART bytes from Arduino.

```cpp
if (c == '\r') continue;
```

Ignores carriage returns.

```cpp
if (c == '\n')
```

Newline means a full command line arrived.

```cpp
if (arduinoLine.length() < 96)
```

Prevents a runaway long line from using too much memory.

### Wi-Fi Watchdog

```cpp
void connectWiFiIfNeeded()
```

Checks Wi-Fi status without blocking.

```cpp
if (WiFi.status() == WL_CONNECTED) return;
```

If Wi-Fi is already connected, do nothing.

```cpp
if (millis() - lastWifiStatusAt < WIFI_STATUS_INTERVAL_MS) return;
```

Only print/send status every 5 seconds.

```cpp
Serial2.println("NET:WIFI_WAIT");
```

Tells Arduino the network is not ready.

### Setup

```cpp
Serial.begin(115200);
```

USB debug serial speed.

```cpp
Serial2.begin(9600, SERIAL_8N1, ARDUINO_RX_PIN, ARDUINO_TX_PIN);
```

UART2 to Arduino at 9600 baud.

```cpp
WiFi.mode(WIFI_STA);
```

ESP32U acts as a Wi-Fi client, not an access point.

```cpp
WiFi.setAutoReconnect(true);
```

ESP32 should reconnect automatically if Wi-Fi drops.

```cpp
WiFi.persistent(false);
```

Avoids writing Wi-Fi settings to flash repeatedly.

```cpp
WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
```

Starts Wi-Fi connection.

```cpp
webSocket.beginSSL(...)
```

Connects to Render using secure WebSocket.

```cpp
webSocket.onEvent(webSocketEvent);
```

Registers the callback.

```cpp
webSocket.setReconnectInterval(5000);
```

Reconnects every 5 seconds if WebSocket drops.

```cpp
webSocket.enableHeartbeat(15000, 3000, 2);
```

WebSocket library-level heartbeat.

```cpp
Serial2.println("NET:BOOT");
```

Tells Arduino the ESP32U booted.

### Loop

```cpp
connectWiFiIfNeeded();
```

Checks Wi-Fi status.

```cpp
webSocket.loop();
```

Lets WebSocket library process network events.

```cpp
updateArduinoSerial();
```

Reads commands from Arduino.

```cpp
if (wsConnected && millis() - lastHeartbeatAt >= HEARTBEAT_INTERVAL_MS)
```

Sends heartbeat every 15 seconds without blocking.

## 8. `arduino_uno_controller/arduino_uno_controller.ino`

This file controls the physical arcade hardware.

### Header Comment

Explains:

- Arduino controls physical devices.
- Arduino talks to ESP32U through serial.
- ESP32U sends commands like `START`, `WIN`, `RESET`.
- Arduino sends commands like `HELLO`, `DRAW`, `RESET_REQ`.

### Libraries

```cpp
#include <Arduino.h>
```

Core Arduino functions.

```cpp
#include <FastLED.h>
```

Controls WS2812B LED strip.

```cpp
#include <LiquidCrystal_I2C.h>
```

Controls the 16x2 LCD with I2C backpack.

```cpp
#include <SoftwareSerial.h>
```

Creates extra serial ports on Arduino Uno pins.

```cpp
#include <Wire.h>
```

I2C communication for LCD.

### Pin Definitions

```cpp
const uint8_t ESP_RX_PIN = 2;
```

Arduino receives ESP32U TX on D2.

```cpp
const uint8_t ESP_TX_PIN = 3;
```

Arduino sends to ESP32U RX through voltage divider on D3.

```cpp
const uint8_t LED_DATA_PIN = 6;
```

LED strip data pin.

```cpp
const uint8_t SYSTEM_BUTTON_PIN = 7;
```

Reset/reboot button.

```cpp
const uint8_t LEADERBOARD_BUTTON_PIN = 8;
```

Leaderboard button. Current Uno code shows unavailable.

```cpp
const uint8_t BUZZER_PIN = 9;
```

Buzzer output.

```cpp
const uint8_t DFPLAYER_TX_TO_ARDUINO_PIN = 10;
```

DFPlayer TX connects to Arduino D10.

```cpp
const uint8_t DFPLAYER_RX_FROM_ARDUINO_PIN = 11;
```

Arduino D11 sends commands to DFPlayer RX.

### Game And Timing Settings

```cpp
const uint8_t LCD_I2C_ADDRESS = 0x27;
```

LCD backpack address.

```cpp
const uint16_t LED_COUNT = 30;
```

Number of LEDs in strip.

```cpp
const uint8_t MAX_BALL = 75;
```

Bingo numbers 1 through 75.

```cpp
const unsigned long DRAW_INTERVAL_MS = 7000;
```

Draw a number every 7 seconds.

```cpp
const unsigned long DEFAULT_COUNTDOWN_MS = 4500;
```

Fallback countdown length.

```cpp
const unsigned long LONG_PRESS_MS = 5000;
```

System button long press threshold.

```cpp
const unsigned long DEBOUNCE_MS = 35;
```

Button debounce time to ignore electrical bouncing.

### Hardware Objects

```cpp
LiquidCrystal_I2C lcd(...)
```

Creates LCD object.

```cpp
SoftwareSerial espSerial(...)
```

Serial connection to ESP32U.

```cpp
SoftwareSerial dfSerial(...)
```

Serial connection to DFPlayer.

```cpp
CRGB leds[LED_COUNT];
```

LED color array used by FastLED.

### Game State Types

```cpp
enum Phase { ... };
```

Defines Arduino's state machine:

- `PHASE_IDLE`
- `PHASE_COUNTDOWN`
- `PHASE_PLAYING`
- `PHASE_GAME_OVER`

```cpp
struct ButtonState { ... };
```

Stores debounce and press timing for each button.

```cpp
struct BeepStep { ... };
```

Stores one buzzer step: frequency, on time, off time.

### Buzzer Sequence

```cpp
const BeepStep countdownBeeps[] = { ... };
```

Three short beeps at 784 Hz and one long higher beep at 1175 Hz.

```cpp
const uint8_t COUNTDOWN_BEEP_COUNT = ...
```

Calculates how many beep steps are in the array.

### Runtime State

These variables remember what is happening right now:

| Variable | Meaning |
| --- | --- |
| `phase` | Current Arduino state. |
| `systemButton` | Debounce state for reset button. |
| `leaderboardButton` | Debounce state for leaderboard button. |
| `espLine` | Incoming command buffer from ESP32U. |
| `roundId` | Current round ID from server. |
| `winnerName` | Winner name from server. |
| `countdownStartedAt` | When countdown began. |
| `countdownDurationMs` | Countdown length. |
| `lastDrawAt` | Last time a number was drawn. |
| `lastLedAt` | Last LED animation update. |
| `lastHelloAt` | Last HELLO sent to ESP32U. |
| `buzzerStepStartedAt` | Current beep start time. |
| `buzzerOffStartedAt` | Current beep gap start time. |
| `gameOverStartedAt` | Time game-over beep started. |
| `winnerTimeMs` | Winning time from server. |
| `winnerSeat` | Player seat 1-3. |
| `hue` | Rainbow animation color position. |
| `espOnline` | Whether ESP32U says network is online. |
| `buzzerActive` | Whether buzzer sequence is running. |
| `buzzerInGap` | Whether countdown buzzer is between beeps. |
| `buzzerStepIndex` | Current beep step index. |
| `drawBag` | Shuffled list of numbers. |
| `drawRemaining` | How many numbers remain in the bag. |

### Function List

The function declarations tell Arduino the functions exist before their full code appears.

### Setup

```cpp
Serial.begin(9600);
```

USB debug serial.

```cpp
espSerial.begin(9600);
```

Starts serial to ESP32U.

```cpp
dfSerial.begin(9600);
```

Starts serial to DFPlayer.

```cpp
pinMode(..., INPUT_PULLUP);
```

Buttons use internal pull-up resistors. Pressed means LOW.

```cpp
pinMode(BUZZER_PIN, OUTPUT);
```

Buzzer pin outputs sound signal.

```cpp
Wire.begin();
```

Starts I2C.

```cpp
Wire.setClock(50000);
```

Slows I2C to help unstable LCD wiring.

```cpp
delay(1000);
```

Waits for LCD hardware to power up. This is only in setup, before network loop matters.

```cpp
lcd.init();
lcd.backlight();
```

Initializes LCD and turns backlight on.

```cpp
FastLED.addLeds<WS2812B, LED_DATA_PIN, GRB>(...)
```

Configures WS2812B strip.

```cpp
FastLED.setBrightness(64);
```

Limits LED brightness.

```cpp
FastLED.clear(true);
```

Turns LEDs off immediately.

```cpp
sendDfCommand(0x06, 25);
```

Sets DFPlayer volume to 25.

```cpp
randomSeed(analogRead(A0));
```

Uses floating analog noise to randomize number drawing.

```cpp
refillDrawBag();
```

Prepares shuffled Bingo numbers.

```cpp
setPhase(PHASE_IDLE);
```

Starts in idle state.

```cpp
espSerial.println("HELLO");
```

Asks ESP32U to identify/sync.

### Main Loop

```cpp
updateEspSerial();
updateButtons();
updateBuzzer();
updateLeds();
```

Runs all active subsystems repeatedly.

```cpp
if (phase == PHASE_COUNTDOWN) updateCountdown();
```

Only countdown logic runs during countdown.

```cpp
if (phase == PHASE_PLAYING) updateGameplay();
```

Only draws numbers during gameplay.

```cpp
if (millis() - lastHelloAt >= 10000)
```

Sends `HELLO` every 10 seconds without blocking.

### Phase Changes

```cpp
void setPhase(Phase nextPhase)
```

Central state transition function.

In `PHASE_IDLE`:

- Clears round ID.
- Stops buzzer.
- Shows online/offline waiting text.

In `PHASE_COUNTDOWN`:

- Saves countdown start time.
- Shows `3...`.
- Starts buzzer sequence.

In `PHASE_PLAYING`:

- Refills draw bag.
- Sets first draw to happen soon.
- Shows game started message.

In `PHASE_GAME_OVER`:

- Shows winner and time.
- Plays winner MP3.
- Starts 4-second buzzer.

### ESP32U Serial Input

```cpp
espSerial.listen();
```

Because Uno has multiple SoftwareSerial objects, this selects ESP serial for reading.

```cpp
while (espSerial.available())
```

Reads all available characters.

```cpp
if (c == '\r') continue;
```

Ignores carriage return.

```cpp
if (c == '\n')
```

Newline means command is complete.

```cpp
if (espLine.length() < 96) espLine += c;
```

Protects memory from too-long commands.

### ESP32U Command Parser

```cpp
if (line == "NET:ONLINE")
```

Marks console online.

```cpp
if (line == "NET:OFFLINE" || line == "NET:WIFI_WAIT")
```

Marks console offline/waiting.

```cpp
if (line.startsWith("START:"))
```

Parses round ID and countdown time, then starts countdown.

```cpp
if (line.startsWith("WIN:"))
```

Parses winner name, seat, and time, then goes game over.

```cpp
if (line == "RESET")
```

Returns to idle.

### Button Handling

```cpp
const unsigned long now = millis();
```

Reads current time once for this update.

```cpp
ButtonState* buttons[] = { ... };
```

Lets one loop handle both buttons.

```cpp
bool physicalPressed = digitalRead(button->pin) == LOW;
```

Because pull-up buttons read LOW when pressed.

```cpp
if (physicalPressed != button->lastPhysicalPressed)
```

Detects a raw electrical change.

```cpp
if (now - button->changedAt < DEBOUNCE_MS) continue;
```

Waits for button to stabilize.

```cpp
if (button->stablePressed && button->pin == SYSTEM_BUTTON_PIN ...)
```

Detects long press on system button.

```cpp
asm volatile ("jmp 0");
```

Soft-restarts the Arduino sketch.

```cpp
handleButtonRelease(...)
```

Runs short-press action when button is released.

### Button Actions

```cpp
if (button.longHandled || heldMs >= LONG_PRESS_MS) return;
```

Prevents a long press from also triggering short press.

```cpp
if (button.pin == SYSTEM_BUTTON_PIN)
```

Short press resets the round.

```cpp
espSerial.println("RESET_REQ");
```

Asks server to reset through ESP32U.

```cpp
if (button.pin == LEADERBOARD_BUTTON_PIN && phase == PHASE_IDLE)
```

Leaderboard button only does something while idle.

### Buzzer Control

```cpp
void startCountdownBuzzer()
```

Starts the countdown beep sequence.

```cpp
tone(BUZZER_PIN, countdownBeeps[0].frequency);
```

Begins first beep.

```cpp
void stopBuzzer()
```

Stops tone and resets buzzer state.

```cpp
void updateBuzzer()
```

Advances buzzer timing using `millis()`.

For game over, it stops after 4 seconds.

For countdown, it alternates between beep-on and beep-gap states.

### LED Animations

```cpp
if (millis() - lastLedAt < 30) return;
```

Limits LED updates to about 33 frames per second.

In idle:

```cpp
beatsin8(14, 12, 110)
```

Creates smooth breathing brightness.

In countdown:

Uses amber pulsing.

In playing:

```cpp
fill_rainbow(leds, LED_COUNT, hue++, 7);
```

Creates moving rainbow.

In game over:

Random colors create jackpot flashing.

```cpp
FastLED.show();
```

Sends the LED buffer to the strip.

### Countdown State

```cpp
unsigned long elapsed = millis() - countdownStartedAt;
```

Calculates countdown progress.

Then LCD shows:

- `3...` for first second.
- `2...` for second second.
- `1...` for third second.
- `GO!` until countdown finishes.

After that:

```cpp
setPhase(PHASE_PLAYING);
```

Starts active gameplay.

### Gameplay Number Drawing

```cpp
if (millis() - lastDrawAt < DRAW_INTERVAL_MS) return;
```

Draws only every 7 seconds.

```cpp
uint8_t number = drawNumber();
```

Gets next shuffled number.

```cpp
showLcd("Current Number", ballLabel(number));
```

Displays B/I/N/G/O label and number.

```cpp
playMp3Track(number);
```

Plays `0001.mp3` through `0075.mp3`.

```cpp
espSerial.print("DRAW:");
espSerial.println(number);
```

Reports drawn number to ESP32U/server.

### Draw Bag

```cpp
void refillDrawBag()
```

Fills `drawBag` with 1 to 75 and shuffles it.

```cpp
for (uint8_t i = 0; i < MAX_BALL; i++) drawBag[i] = i + 1;
```

Creates ordered numbers.

```cpp
for (int i = MAX_BALL - 1; i > 0; i--)
```

Shuffles from end to beginning.

```cpp
drawRemaining = MAX_BALL;
```

Resets remaining count.

```cpp
uint8_t drawNumber()
```

Returns one number and decreases `drawRemaining`.

### DFPlayer Mini

```cpp
void playMp3Track(uint16_t track)
```

Plays a track number.

```cpp
sendDfCommand(0x12, track);
```

DFPlayer command `0x12` means play MP3 folder track.

```cpp
dfSerial.listen();
```

Switches SoftwareSerial listening to DFPlayer before sending.

```cpp
uint8_t buffer[10] = { ... };
```

Builds the 10-byte DFPlayer command packet.

```cpp
uint16_t checksum = 0;
```

Calculates DFPlayer checksum.

```cpp
dfSerial.write(buffer, sizeof(buffer));
```

Sends command to DFPlayer.

```cpp
espSerial.listen();
```

Switches listening back to ESP32U.

### LCD Output

```cpp
static String previousRow1 = "";
static String previousRow2 = "";
```

Remembers previous LCD text to avoid rewriting the same text constantly.

```cpp
String nextRow1 = row1.substring(0, 16);
```

LCD row is only 16 characters.

```cpp
while (nextRow1.length() < 16) nextRow1 += " ";
```

Pads row with spaces to erase old leftover characters.

```cpp
if (nextRow1 == previousRow1 && nextRow2 == previousRow2) return;
```

Skips unnecessary LCD writes.

```cpp
lcd.setCursor(0, 0);
lcd.print(nextRow1);
```

Writes first row.

```cpp
lcd.setCursor(0, 1);
lcd.print(nextRow2);
```

Writes second row.

### Text Formatting Helpers

```cpp
String ballLabel(uint8_t number)
```

Converts a number into `B-01`, `I-20`, `N-40`, etc.

```cpp
String formatTime(unsigned long ms)
```

Converts milliseconds to `MM:SS`.

```cpp
String compactName(const String& value)
```

Trims winner name and limits it to 16 LCD characters.

## 9. `public/style.css`

CSS does not change game logic, but it makes the interface usable.

### Theme Variables

`:root` defines reusable colors and shadow values. Any rule can use them with `var(--name)`.

### Base Elements

`* { box-sizing: border-box; }` makes sizing easier because padding/border stay inside the element width.

`body` sets dark background, text color, and font.

`button`, `input`, `button:hover`, and `button:disabled` define common form control behavior.

### App Layout

`.app-shell` limits the app width and centers it.

### Header And Status

`.topbar` lays out the title and status pills.

`.status-pill`, `.status-pill.ok`, and `.status-pill.warn` show connection state colors.

### Screens

`.panel` and `.game-layout` are hidden by default.

`.panel.active` and `.game-layout.active` are shown.

This is how `showScreen()` switches screens.

### Join Form

`.join-form` and `.form-row` control the player name form layout.

### Lobby

`.players-list`, `.player-row`, and `.seat` style the three player slots.

`.lobby-actions` and `.ready-toggle` style ready/start/reset controls.

### Game Screen

`.game-layout`, `.game-info`, and `.metrics` organize active gameplay.

### Bingo Card

`.bingo-card` creates a 5-column grid.

`.bingo-cell` styles each card square.

`.bingo-cell.marked` gives marked cells a bright gradient.

### Messages And Winner

`#countdown-value` makes countdown huge.

`.message` and `.message.error` style normal/error feedback.

`.winner-detail` styles final win details.

`[hidden]` forces hidden elements to disappear.

### Mobile Layout

The `@media (max-width: 680px)` block changes layout for phones:

- Header stacks vertically.
- Form becomes one column.
- Player row becomes narrower.
- Bingo card fills width.

## 10. Audio Assets

DFPlayer expects files in `/MP3` on the MicroSD card.

| File Range | Meaning |
| --- | --- |
| `0001.mp3` to `0075.mp3` | Spoken number announcements. |
| `0101.mp3` | Player 1 won. |
| `0102.mp3` | Player 2 won. |
| `0103.mp3` | Player 3 won. |

Arduino calls:

```cpp
playMp3Track(number);
```

For drawn numbers.

```cpp
playMp3Track(100 + winnerSeat);
```

For winner files.

## 11. Message Protocol Summary

### Server To ESP32U

| JSON Type | Meaning |
| --- | --- |
| `sync` | Server sends current phase. |
| `start_game` | Start countdown/game. |
| `game_over` | Announce winner. |
| `reset` | Reset hardware state. |
| `heartbeat_ack` | Reply to ESP32U heartbeat. |

### ESP32U To Server

| JSON Type | Meaning |
| --- | --- |
| `hardware_hello` | ESP32U connected and identifies itself. |
| `drawn_number` | Arduino drew a number. |
| `reset_requested` | Hardware button requested reset. |
| `heartbeat` | ESP32U is alive. |

### ESP32U To Arduino

| Line | Meaning |
| --- | --- |
| `NET:ONLINE` | WebSocket connected. |
| `NET:OFFLINE` | WebSocket disconnected. |
| `NET:WIFI_WAIT` | Wi-Fi not connected yet. |
| `START:<roundId>:<countdownMs>` | Start countdown. |
| `WIN:<name>:<seat>:<ms>` | Show winner. |
| `RESET` | Return to idle. |

### Arduino To ESP32U

| Line | Meaning |
| --- | --- |
| `HELLO` | Arduino asks bridge/server to sync. |
| `DRAW:<number>` | Arduino drew a number. |
| `RESET_REQ` | System button requested reset. |

## 12. Things To Remember While Studying

- The server is the judge.
- The Arduino is the physical game master.
- The ESP32U is the internet bridge.
- The browser is only a player interface.
- `millis()` is used for non-blocking firmware timing.
- Browsers never receive drawn numbers.
- The ESP-01S and old `arduino_firmware` path are not part of the final build anymore.
