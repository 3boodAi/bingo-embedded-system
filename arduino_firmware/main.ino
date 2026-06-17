/*
  Bingo Game Arcade ESP32 firmware

  Arduino IDE libraries:
  - WebSockets by Markus Sattler
  - ArduinoJson by Benoit Blanchon
  - FastLED

  DFPlayer MicroSD layout:
  - /MP3/0001.mp3 through /MP3/0075.mp3 announce numbers 1-75
  - /MP3/0101.mp3 says "Player 1 won"
  - /MP3/0102.mp3 says "Player 2 won"
  - /MP3/0103.mp3 says "Player 3 won"
*/

#include <Arduino.h>
#include <ArduinoJson.h>
#include <FastLED.h>
#include <LiquidCrystal.h>
#include <LittleFS.h>
#include <WebSocketsClient.h>
#include <WiFi.h>

// ---------- Wi-Fi and server ----------
const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* WS_HOST = "bingo.render"; // Render custom domain or your-service.onrender.com
const uint16_t WS_PORT = 443;
const char* WS_PATH = "/hardware";
const bool WS_USE_SSL = true;

// ---------- ESP32 pin definitions ----------
const uint8_t LCD_RS = 14;
const uint8_t LCD_EN = 27;
const uint8_t LCD_D4 = 26;
const uint8_t LCD_D5 = 25;
const uint8_t LCD_D6 = 33;
const uint8_t LCD_D7 = 32;

const uint8_t BUZZER_PIN = 13;
const uint8_t LED_PIN = 23;
const uint16_t LED_COUNT = 30;

const uint8_t DFPLAYER_RX_PIN = 16; // ESP32 RX2, connect to DFPlayer TX
const uint8_t DFPLAYER_TX_PIN = 17; // ESP32 TX2, connect to DFPlayer RX through 1k resistor

const uint8_t SYSTEM_BUTTON_PIN = 18;
const uint8_t LEADERBOARD_BUTTON_PIN = 19;

// ---------- Game constants ----------
const uint8_t MAX_BALL = 75;
const unsigned long DRAW_INTERVAL_MS = 7000;
const unsigned long COUNTDOWN_MS = 4500;
const unsigned long WIFI_RETRY_MS = 5000;
const unsigned long HEARTBEAT_MS = 15000;
const unsigned long LONG_PRESS_MS = 5000;
const unsigned long DEBOUNCE_MS = 35;
const char* SCORE_FILE = "/scores.csv";

LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);
HardwareSerial dfSerial(2);
WebSocketsClient webSocket;
CRGB leds[LED_COUNT];

enum GamePhase {
  PHASE_IDLE,
  PHASE_COUNTDOWN,
  PHASE_PLAYING,
  PHASE_GAME_OVER,
  PHASE_LEADERBOARD
};

struct ButtonState {
  uint8_t pin;
  bool stablePressed;
  bool lastPhysicalPressed;
  bool longHandled;
  unsigned long changedAt;
  unsigned long pressedAt;
};

struct ScoreRecord {
  unsigned long ms;
  char name[15];
};

struct BeepStep {
  uint16_t frequency;
  unsigned long onMs;
  unsigned long offMs;
};

GamePhase phase = PHASE_IDLE;
GamePhase phaseBeforeLeaderboard = PHASE_IDLE;
ButtonState systemButton = {SYSTEM_BUTTON_PIN, false, false, false, 0, 0};
ButtonState leaderboardButton = {LEADERBOARD_BUTTON_PIN, false, false, false, 0, 0};

String roundId = "";
String lastWinnerName = "";
unsigned long lastWinnerTimeMs = 0;
unsigned long countdownStartedAt = 0;
unsigned long lastDrawAt = 0;
unsigned long lastWifiAttempt = 0;
unsigned long lastHeartbeatAt = 0;
unsigned long lastLedFrameAt = 0;
unsigned long lastLeaderboardFrameAt = 0;
unsigned long lastJackpotFrameAt = 0;
unsigned long buzzerStepStartedAt = 0;
unsigned long buzzerOffStartedAt = 0;

int drawBag[MAX_BALL];
uint8_t drawRemaining = 0;
uint8_t hue = 0;
uint8_t leaderboardIndex = 0;
bool wsConnected = false;
bool buzzerActive = false;
bool buzzerInGap = false;
uint8_t buzzerStepIndex = 0;

const BeepStep countdownBeeps[] = {
  {784, 500, 320},
  {784, 500, 320},
  {784, 500, 320},
  {1175, 1500, 0},
};
const uint8_t COUNTDOWN_BEEP_COUNT = sizeof(countdownBeeps) / sizeof(countdownBeeps[0]);

void showLcd(const String& row1, const String& row2 = "");
void setPhase(GamePhase nextPhase);
void connectWiFiIfNeeded();
void connectWebSocket();
void webSocketEvent(WStype_t type, uint8_t* payload, size_t length);
void handleServerMessage(const String& payload);
void sendJson(JsonDocument& doc);
void sendHardwareHello();
void sendHeartbeat();
void sendResetRequested();
void sendDrawnNumber(uint8_t number);
void updateButtons();
void handleButtonRelease(ButtonState& button, unsigned long heldMs);
void updateBuzzer();
void startCountdownBuzzer();
void stopBuzzer();
void updateLeds();
void updateCountdown();
void updateGameplay();
void refillDrawBag();
uint8_t drawNumber();
void playMp3Track(uint16_t track);
void sendDfCommand(uint8_t command, uint16_t parameter);
void saveScore(const String& name, unsigned long ms);
uint8_t loadScores(ScoreRecord* scores, uint8_t maxScores);
void sortScores(ScoreRecord* scores, uint8_t count);
void updateLeaderboard();
String formatTime(unsigned long ms);
String ballLabel(uint8_t number);
String compactName(const String& value);

void setup() {
  Serial.begin(115200);

  pinMode(SYSTEM_BUTTON_PIN, INPUT_PULLUP);
  pinMode(LEADERBOARD_BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  lcd.begin(16, 2);
  showLcd("Bingo Arcade", "Booting...");

  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, LED_COUNT);
  FastLED.setBrightness(96);
  FastLED.clear(true);

  dfSerial.begin(9600, SERIAL_8N1, DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);
  sendDfCommand(0x06, 25); // volume 0-30

  if (!LittleFS.begin(true)) {
    showLcd("LittleFS error", "Scores disabled");
  }

  randomSeed(esp_random());
  refillDrawBag();

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  connectWebSocket();
  setPhase(PHASE_IDLE);
}

void loop() {
  connectWiFiIfNeeded();
  webSocket.loop();

  if (wsConnected && millis() - lastHeartbeatAt >= HEARTBEAT_MS) {
    lastHeartbeatAt = millis();
    sendHeartbeat();
  }

  updateButtons();
  updateBuzzer();
  updateLeds();

  if (phase == PHASE_COUNTDOWN) updateCountdown();
  if (phase == PHASE_PLAYING) updateGameplay();
  if (phase == PHASE_LEADERBOARD) updateLeaderboard();
}

void setPhase(GamePhase nextPhase) {
  phase = nextPhase;

  if (nextPhase == PHASE_IDLE) {
    roundId = "";
    stopBuzzer();
    showLcd("Waiting for", "players...");
  } else if (nextPhase == PHASE_COUNTDOWN) {
    countdownStartedAt = millis();
    showLcd("Get ready", "3...");
    startCountdownBuzzer();
  } else if (nextPhase == PHASE_PLAYING) {
    lastDrawAt = millis() - DRAW_INTERVAL_MS + 1200;
    showLcd("Game started", "Watch numbers");
  } else if (nextPhase == PHASE_GAME_OVER) {
    showLcd(compactName(lastWinnerName), formatTime(lastWinnerTimeMs));
    tone(BUZZER_PIN, 988);
    buzzerActive = true;
    buzzerStepStartedAt = millis();
  } else if (nextPhase == PHASE_LEADERBOARD) {
    leaderboardIndex = 0;
    lastLeaderboardFrameAt = 0;
    updateLeaderboard();
  }
}

void connectWiFiIfNeeded() {
  if (WiFi.status() == WL_CONNECTED) return;
  if (millis() - lastWifiAttempt < WIFI_RETRY_MS) return;

  lastWifiAttempt = millis();
  showLcd("WiFi reconnect", WIFI_SSID);
  WiFi.disconnect(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void connectWebSocket() {
  if (WS_USE_SSL) {
    webSocket.beginSSL(WS_HOST, WS_PORT, WS_PATH);
  } else {
    webSocket.begin(WS_HOST, WS_PORT, WS_PATH);
  }
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
  webSocket.enableHeartbeat(15000, 3000, 2);
}

void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_CONNECTED) {
    wsConnected = true;
    showLcd("Server online", "Lobby ready");
    sendHardwareHello();
  } else if (type == WStype_DISCONNECTED) {
    wsConnected = false;
    if (phase != PHASE_PLAYING && phase != PHASE_COUNTDOWN) {
      showLcd("Server offline", "Reconnecting");
    }
  } else if (type == WStype_TEXT) {
    String message;
    message.reserve(length);
    for (size_t i = 0; i < length; i++) {
      message += (char)payload[i];
    }
    handleServerMessage(message);
  }
}

void handleServerMessage(const String& payload) {
  StaticJsonDocument<768> doc;
  DeserializationError error = deserializeJson(doc, payload);
  if (error) return;

  const char* type = doc["type"] | "";

  if (strcmp(type, "sync") == 0) {
    const char* state = doc["phase"] | "idle";
    if (strcmp(state, "playing") == 0) {
      setPhase(PHASE_PLAYING);
    } else if (strcmp(state, "countdown") == 0) {
      setPhase(PHASE_COUNTDOWN);
    } else if (strcmp(state, "game_over") == 0) {
      JsonObject winner = doc["winner"];
      if (!winner.isNull()) {
        lastWinnerName = String((const char*)(winner["name"] | "Winner"));
        lastWinnerTimeMs = winner["winTimeMs"] | 0;
      }
      setPhase(PHASE_GAME_OVER);
    } else if (phase != PHASE_LEADERBOARD) {
      setPhase(PHASE_IDLE);
    }
    return;
  }

  if (strcmp(type, "start_game") == 0) {
    roundId = String((const char*)doc["roundId"]);
    refillDrawBag();
    setPhase(PHASE_COUNTDOWN);
    return;
  }

  if (strcmp(type, "game_over") == 0) {
    lastWinnerName = String((const char*)doc["winnerName"]);
    lastWinnerTimeMs = doc["winTimeMs"] | 0;
    uint8_t winnerSeat = doc["winnerSeat"] | 1;
    playMp3Track(100 + winnerSeat);
    saveScore(lastWinnerName, lastWinnerTimeMs);
    setPhase(PHASE_GAME_OVER);
    return;
  }

  if (strcmp(type, "reset") == 0) {
    setPhase(PHASE_IDLE);
  }
}

void sendJson(JsonDocument& doc) {
  if (!wsConnected) return;
  String output;
  serializeJson(doc, output);
  webSocket.sendTXT(output);
}

void sendHardwareHello() {
  StaticJsonDocument<192> doc;
  doc["type"] = "hardware_hello";
  doc["deviceId"] = String("esp32-") + String((uint32_t)ESP.getEfuseMac(), HEX);
  doc["version"] = "1.0.0";
  sendJson(doc);
}

void sendHeartbeat() {
  StaticJsonDocument<128> doc;
  doc["type"] = "heartbeat";
  doc["uptimeMs"] = millis();
  sendJson(doc);
}

void sendResetRequested() {
  StaticJsonDocument<96> doc;
  doc["type"] = "reset_requested";
  sendJson(doc);
}

void sendDrawnNumber(uint8_t number) {
  StaticJsonDocument<160> doc;
  doc["type"] = "drawn_number";
  doc["roundId"] = roundId;
  doc["number"] = number;
  sendJson(doc);
}

void updateButtons() {
  const unsigned long now = millis();
  ButtonState* buttons[] = {&systemButton, &leaderboardButton};

  for (ButtonState* button : buttons) {
    bool physicalPressed = digitalRead(button->pin) == LOW;
    if (physicalPressed != button->lastPhysicalPressed) {
      button->lastPhysicalPressed = physicalPressed;
      button->changedAt = now;
    }

    if (now - button->changedAt < DEBOUNCE_MS) continue;
    if (physicalPressed == button->stablePressed) {
      if (button->stablePressed && button->pin == SYSTEM_BUTTON_PIN && !button->longHandled &&
          now - button->pressedAt >= LONG_PRESS_MS) {
        button->longHandled = true;
        showLcd("Rebooting", "ESP32...");
        ESP.restart();
      }
      continue;
    }

    button->stablePressed = physicalPressed;
    if (button->stablePressed) {
      button->pressedAt = now;
      button->longHandled = false;
    } else {
      handleButtonRelease(*button, now - button->pressedAt);
    }
  }
}

void handleButtonRelease(ButtonState& button, unsigned long heldMs) {
  if (button.longHandled || heldMs >= LONG_PRESS_MS) return;

  if (button.pin == SYSTEM_BUTTON_PIN) {
    sendResetRequested();
    setPhase(PHASE_IDLE);
    return;
  }

  if (button.pin == LEADERBOARD_BUTTON_PIN) {
    if (phase == PHASE_LEADERBOARD) {
      setPhase(phaseBeforeLeaderboard);
    } else if (phase == PHASE_IDLE) {
      phaseBeforeLeaderboard = phase;
      setPhase(PHASE_LEADERBOARD);
    }
  }
}

void startCountdownBuzzer() {
  buzzerActive = true;
  buzzerInGap = false;
  buzzerStepIndex = 0;
  buzzerStepStartedAt = millis();
  tone(BUZZER_PIN, countdownBeeps[0].frequency);
}

void stopBuzzer() {
  noTone(BUZZER_PIN);
  digitalWrite(BUZZER_PIN, LOW);
  buzzerActive = false;
  buzzerInGap = false;
  buzzerStepIndex = 0;
}

void updateBuzzer() {
  if (!buzzerActive) return;
  const unsigned long now = millis();

  if (phase == PHASE_GAME_OVER) {
    if (now - buzzerStepStartedAt >= 4000) stopBuzzer();
    return;
  }

  if (buzzerStepIndex >= COUNTDOWN_BEEP_COUNT) {
    stopBuzzer();
    return;
  }

  const BeepStep& step = countdownBeeps[buzzerStepIndex];
  if (!buzzerInGap && now - buzzerStepStartedAt >= step.onMs) {
    noTone(BUZZER_PIN);
    buzzerInGap = true;
    buzzerOffStartedAt = now;
    return;
  }

  if (buzzerInGap && now - buzzerOffStartedAt >= step.offMs) {
    buzzerStepIndex++;
    if (buzzerStepIndex >= COUNTDOWN_BEEP_COUNT) {
      stopBuzzer();
      return;
    }
    buzzerInGap = false;
    buzzerStepStartedAt = now;
    tone(BUZZER_PIN, countdownBeeps[buzzerStepIndex].frequency);
  }
}

void updateLeds() {
  const unsigned long now = millis();
  if (now - lastLedFrameAt < 24) return;
  lastLedFrameAt = now;

  if (phase == PHASE_IDLE || phase == PHASE_LEADERBOARD) {
    uint8_t brightness = beatsin8(14, 18, 150);
    fill_solid(leds, LED_COUNT, CHSV(118, 190, brightness));
  } else if (phase == PHASE_COUNTDOWN) {
    fill_solid(leds, LED_COUNT, CHSV(32, 230, beatsin8(28, 60, 220)));
  } else if (phase == PHASE_PLAYING) {
    fill_rainbow(leds, LED_COUNT, hue++, 7);
  } else if (phase == PHASE_GAME_OVER) {
    if (now - lastJackpotFrameAt >= 70) {
      lastJackpotFrameAt = now;
      for (uint16_t i = 0; i < LED_COUNT; i++) {
        leds[i] = CHSV(random8(), 255, random8(80, 255));
      }
    }
  }

  FastLED.show();
}

void updateCountdown() {
  unsigned long elapsed = millis() - countdownStartedAt;
  if (elapsed < 1000) {
    showLcd("Get ready", "3...");
  } else if (elapsed < 2000) {
    showLcd("Get ready", "2...");
  } else if (elapsed < 3000) {
    showLcd("Get ready", "1...");
  } else if (elapsed < COUNTDOWN_MS) {
    showLcd("Get ready", "GO!");
  } else {
    setPhase(PHASE_PLAYING);
  }
}

void updateGameplay() {
  if (millis() - lastDrawAt < DRAW_INTERVAL_MS) return;
  lastDrawAt = millis();

  uint8_t number = drawNumber();
  showLcd("Current Number", ballLabel(number));
  playMp3Track(number);
  sendDrawnNumber(number);

  if (drawRemaining == 0) refillDrawBag();
}

void refillDrawBag() {
  for (uint8_t i = 0; i < MAX_BALL; i++) {
    drawBag[i] = i + 1;
  }
  for (int i = MAX_BALL - 1; i > 0; i--) {
    int j = random(i + 1);
    int temp = drawBag[i];
    drawBag[i] = drawBag[j];
    drawBag[j] = temp;
  }
  drawRemaining = MAX_BALL;
}

uint8_t drawNumber() {
  if (drawRemaining == 0) refillDrawBag();
  drawRemaining--;
  return drawBag[drawRemaining];
}

void playMp3Track(uint16_t track) {
  sendDfCommand(0x12, track);
}

void sendDfCommand(uint8_t command, uint16_t parameter) {
  uint8_t buffer[10] = {
    0x7E,
    0xFF,
    0x06,
    command,
    0x00,
    highByte(parameter),
    lowByte(parameter),
    0x00,
    0x00,
    0xEF
  };

  uint16_t checksum = 0;
  for (uint8_t i = 1; i < 7; i++) checksum += buffer[i];
  checksum = 0 - checksum;
  buffer[7] = highByte(checksum);
  buffer[8] = lowByte(checksum);

  dfSerial.write(buffer, sizeof(buffer));
}

void saveScore(const String& name, unsigned long ms) {
  File file = LittleFS.open(SCORE_FILE, FILE_APPEND);
  if (!file) return;

  String safeName = compactName(name);
  safeName.replace(",", " ");
  file.print(ms);
  file.print(",");
  file.println(safeName);
  file.close();
}

uint8_t loadScores(ScoreRecord* scores, uint8_t maxScores) {
  File file = LittleFS.open(SCORE_FILE, FILE_READ);
  if (!file) return 0;

  uint8_t count = 0;
  while (file.available() && count < maxScores) {
    String line = file.readStringUntil('\n');
    line.trim();
    int comma = line.indexOf(',');
    if (comma <= 0) continue;

    scores[count].ms = line.substring(0, comma).toInt();
    String name = compactName(line.substring(comma + 1));
    name.toCharArray(scores[count].name, sizeof(scores[count].name));
    count++;
  }
  file.close();
  sortScores(scores, count);
  return count;
}

void sortScores(ScoreRecord* scores, uint8_t count) {
  for (uint8_t i = 0; i < count; i++) {
    for (uint8_t j = i + 1; j < count; j++) {
      if (scores[j].ms < scores[i].ms) {
        ScoreRecord temp = scores[i];
        scores[i] = scores[j];
        scores[j] = temp;
      }
    }
  }
}

void updateLeaderboard() {
  if (millis() - lastLeaderboardFrameAt < 2500 && lastLeaderboardFrameAt != 0) return;
  lastLeaderboardFrameAt = millis();

  ScoreRecord scores[32];
  uint8_t count = loadScores(scores, 32);
  if (count == 0) {
    showLcd("Leaderboard", "No wins saved");
    return;
  }

  uint8_t topCount = count < 10 ? count : 10;
  if (leaderboardIndex >= topCount) leaderboardIndex = 0;

  String row1 = "#" + String(leaderboardIndex + 1) + " " + String(scores[leaderboardIndex].name);
  String row2 = formatTime(scores[leaderboardIndex].ms);
  showLcd(row1, row2);
  leaderboardIndex++;
}

void showLcd(const String& row1, const String& row2) {
  static String previousRow1 = "";
  static String previousRow2 = "";
  String nextRow1 = row1.substring(0, 16);
  String nextRow2 = row2.substring(0, 16);

  if (nextRow1 == previousRow1 && nextRow2 == previousRow2) return;
  previousRow1 = nextRow1;
  previousRow2 = nextRow2;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(nextRow1);
  lcd.setCursor(0, 1);
  lcd.print(nextRow2);
}

String formatTime(unsigned long ms) {
  unsigned long totalSeconds = ms / 1000;
  unsigned int minutes = totalSeconds / 60;
  unsigned int seconds = totalSeconds % 60;
  char buffer[17];
  snprintf(buffer, sizeof(buffer), "%02u:%02u", minutes, seconds);
  return String(buffer);
}

String ballLabel(uint8_t number) {
  char letter = 'B';
  if (number >= 16 && number <= 30) letter = 'I';
  else if (number >= 31 && number <= 45) letter = 'N';
  else if (number >= 46 && number <= 60) letter = 'G';
  else if (number >= 61) letter = 'O';

  char buffer[17];
  snprintf(buffer, sizeof(buffer), "%c-%02u", letter, number);
  return String(buffer);
}

String compactName(const String& value) {
  String output = value;
  output.trim();
  if (output.length() == 0) output = "Player";
  if (output.length() > 14) output = output.substring(0, 14);
  return output;
}
