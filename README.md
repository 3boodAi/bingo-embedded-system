# Bingo Game Arcade

Physical Bingo arcade console with a browser lobby, an Arduino Uno hardware controller, and an ESP32U Wi-Fi bridge. The current demo mode starts with 1 player.

## Final Architecture

- `server.js` runs the Node.js web server.
- `public/` contains the browser player interface.
- `arduino_uno_controller/` is uploaded to the Arduino Uno. It controls the LCD, LEDs, DFPlayer Mini, buzzer, and buttons.
- `esp32u_bridge/` is uploaded to the ESP32U. It connects to Wi-Fi, talks to Render, and forwards messages between the server and Arduino.
- `audio/MP3/` contains the DFPlayer Mini audio files for numbers, countdown, and winner announcements.

The old ESP-01S plan, one-board ESP32 firmware, simulator, and hardware test sketches were removed so this repo only shows the build we are actually wiring.

## Run The Web Server

```bash
npm install
npm start
```

Open `http://localhost:3000`.

The player count is controlled by `MAX_PLAYERS`. It is currently set to `1` in `render.yaml` for single-player testing. Change it back to `3` when the team wants the full three-player rule again.

Render uses `render.yaml`:

```yaml
buildCommand: npm install
startCommand: npm start
```

## Firmware Upload Order

1. Upload `arduino_uno_controller/arduino_uno_controller.ino` to the Arduino Uno.
2. Upload `esp32u_bridge/esp32u_bridge.ino` to the ESP32U.
3. Keep the ESP-01S disconnected. It is no longer part of the final build.

The ESP32U Wi-Fi password is stored in `esp32u_bridge/wifi_config.h` on this computer. That file is ignored by Git. If another teammate clones the repo, they should copy `esp32u_bridge/wifi_config.example.h` to `esp32u_bridge/wifi_config.h` and fill in their own Wi-Fi details.

## Arduino Uno Wiring

| Part | Connect To |
| --- | --- |
| LCD I2C SDA | Arduino A4 |
| LCD I2C SCL | Arduino A5 |
| LCD VCC | Arduino 5V |
| LCD GND | Breadboard GND |
| LED strip DIN | Arduino D6 |
| LED strip 5V | External 5V or Arduino 5V for short strip |
| LED strip GND | Breadboard GND |
| System button | Arduino D7 to GND |
| Leaderboard button | Arduino D8 to GND |
| Buzzer + | Arduino D9 |
| Buzzer - | GND |
| DFPlayer TX | Arduino D10 |
| DFPlayer RX | Arduino D11 through 1k resistor |
| DFPlayer VCC | 5V |
| DFPlayer GND | GND |
| Speaker + / - | DFPlayer SPK1 / SPK2 |

## ESP32U To Arduino Bridge Wiring

| Signal | Connect To |
| --- | --- |
| Arduino D3 TX | 1k resistor, then divider junction |
| Divider junction | ESP32U GPIO4 |
| Divider junction | 2.2k resistor to GND |
| ESP32U GPIO17 | Arduino D2 RX |
| ESP32U GND | Arduino/Breadboard GND |

The 1k and 2.2k resistors protect the ESP32U receive pin by reducing Arduino 5V serial down near 3.3V.

## Audio Files

Copy the repo `audio/MP3` folder to the MicroSD card root so the card has:

- `/MP3/0000.mp3` through `/MP3/0099.mp3` for clean number-only announcements.
- `/MP3/0200.mp3` for "Bingo".
- `/MP3/0201.mp3`, `/MP3/0202.mp3`, `/MP3/0203.mp3` for Player 1, 2, and 3 winner announcements.
- `/MP3/0204.mp3` for "Good game".
- `/MP3/0205.mp3` for "Good luck".
- `/MP3/0210.mp3` for the "5, 4, 3, 2, 1, go" countdown.

## Study Order

Start with `CODE_WALKTHROUGH.md` for the full feature and code explanation.

1. Read `server.js` to learn the game state machine and WebSocket rules.
2. Read `public/index.html`, `public/client.js`, and `public/style.css` to learn the player UI.
3. Read `esp32u_bridge/esp32u_bridge.ino` to learn Wi-Fi and server communication.
4. Read `arduino_uno_controller/arduino_uno_controller.ino` to learn the physical arcade behavior.
