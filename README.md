# Bingo Game Arcade

Physical three-player Bingo arcade console with a Node.js lobby server, browser player UI, and ESP32 game-master firmware.

## Architecture

- **Node.js server:** Express static hosting, Socket.IO for browser players, raw WebSocket endpoint at `/hardware` for the ESP32.
- **Browser UI:** Players join the single physical arcade lobby, mark their own cards, and claim Bingo. Drawn numbers are never sent to browsers.
- **ESP32 firmware:** Controls the 16x2 parallel LCD, active buzzer, WS2812B LED strip, DFPlayer Mini, two buttons, Wi-Fi, WebSocket reconnects, and LittleFS leaderboard storage.

## Important Design Decisions

- The server validates claims against the hidden called-number list, so browser clients cannot win by marking uncalled numbers.
- The ESP32 is the visible game master: it draws numbers, displays them on the LCD, plays MP3 announcements, and sends only the drawn value to the server for validation.
- DFPlayer Mini cannot synthesize arbitrary names. The firmware plays slot-based winner clips: `0101.mp3`, `0102.mp3`, and `0103.mp3`. The LCD still shows the winner's actual browser name.
- A standard 5x5 Bingo card is used with a free center square and row/column/diagonal win lines.
- The firmware intentionally uses `millis()` scheduling throughout; no `delay()` calls are used.

## Web Server

```bash
npm install
npm start
```

Open `http://localhost:3000`.

Render uses `render.yaml`:

```yaml
buildCommand: npm install
startCommand: npm start
```

If you need to demo without hardware, run:

```bash
npm run sim:hardware
```

Useful environment variables:

- `PORT`: server port, defaults to `3000`.
- `MAX_PLAYERS`: defaults to `3`.
- `ALLOW_SOFTWARE_ONLY=true`: allows starting without ESP32 hardware connected.
- `HARDWARE_WS_URL`: simulator target, defaults to `ws://localhost:3000/hardware`.

## ESP32 Firmware

Open the `arduino_firmware` sketch folder in Arduino IDE. The full firmware is in `main.ino`; `arduino_firmware.ino` is the small entry tab Arduino tooling expects.

Install these libraries:

- WebSockets by Markus Sattler
- ArduinoJson by Benoit Blanchon
- FastLED

Select an ESP32 board package and update these constants at the top of `main.ino`:

```cpp
const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* WS_HOST = "bingo.render";
const uint16_t WS_PORT = 443;
const bool WS_USE_SSL = true;
```

For local testing, use your computer's LAN IP, port `3000`, and `WS_USE_SSL = false`.

## DFPlayer Mini Files

Place files in the MicroSD `/MP3` folder:

- `0001.mp3` through `0075.mp3`: spoken number announcements.
- `0101.mp3`: "Player 1 won".
- `0102.mp3`: "Player 2 won".
- `0103.mp3`: "Player 3 won".

The repo keeps source/sample audio under `audio/MP3`.

## Repo Layout

- `server.js`: Node.js app and WebSocket game coordinator.
- `public/`: browser player UI.
- `arduino_firmware/`: production ESP32 firmware sketch.
- `audio/MP3/`: DFPlayer Mini MP3 source files.
- `hardware_tests/`: standalone wiring and peripheral test sketches.
- `tools/`: local development helpers, including the fake ESP32 simulator.

## Hardware Buttons

- **System button:** short press resets the current round; long press for 5+ seconds soft-reboots the ESP32.
- **Leaderboard button:** while idle, toggles the LCD Top 10 fastest wins from LittleFS.
