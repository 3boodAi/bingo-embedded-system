/*
  Arduino Uno arcade controller firmware

  Role:
  - Controls LCD, WS2812B LEDs, DFPlayer Mini, buzzer, and buttons.
  - Talks to ESP32U bridge over SoftwareSerial:
      Arduino D2 <- ESP32U GPIO17
      Arduino D3 -> ESP32U GPIO4 through voltage divider
  - Uses short line protocol:
      From ESP: NET:ONLINE, NET:OFFLINE, START:<roundId>:<countdownMs>, WIN:<name>:<seat>:<ms>, RESET
      To ESP:   HELLO, DRAW:<number>, RESET_REQ
*/

// # Libraries
#include <Arduino.h>
#include <FastLED.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>
#include <Wire.h>

// # Pin Definitions
const uint8_t ESP_RX_PIN = 2; // Arduino receives from ESP TX.
const uint8_t ESP_TX_PIN = 3; // Arduino sends to ESP RX through voltage divider.
const uint8_t LED_DATA_PIN = 6;
const uint8_t SYSTEM_BUTTON_PIN = 7;
const uint8_t LEADERBOARD_BUTTON_PIN = 8;
const uint8_t BUZZER_PIN = 9;
const uint8_t DFPLAYER_TX_TO_ARDUINO_PIN = 10;
const uint8_t DFPLAYER_RX_FROM_ARDUINO_PIN = 11;

// # Game And Timing Settings
const uint8_t LCD_I2C_ADDRESS = 0x27;
const uint16_t LED_COUNT = 30;
const uint8_t MAX_BALL = 75;
const unsigned long DRAW_INTERVAL_MS = 7000;
const unsigned long DEFAULT_COUNTDOWN_MS = 6000;
const unsigned long LONG_PRESS_MS = 5000;
const unsigned long DEBOUNCE_MS = 35;
const uint16_t TRACK_BINGO = 200;
const uint16_t TRACK_WIN_PLAYER_1 = 201;
const uint16_t TRACK_GOOD_GAME = 204;
const uint16_t TRACK_GOOD_LUCK = 205;
const uint16_t TRACK_COUNTDOWN = 210;

// # Hardware Objects
LiquidCrystal_I2C lcd(LCD_I2C_ADDRESS, 16, 2);
SoftwareSerial espSerial(ESP_RX_PIN, ESP_TX_PIN);
SoftwareSerial dfSerial(DFPLAYER_TX_TO_ARDUINO_PIN, DFPLAYER_RX_FROM_ARDUINO_PIN);
CRGB leds[LED_COUNT];

// # Game State Types
enum Phase {
  PHASE_IDLE,
  PHASE_COUNTDOWN,
  PHASE_PLAYING,
  PHASE_GAME_OVER,
};

struct ButtonState {
  uint8_t pin;
  bool stablePressed;
  bool lastPhysicalPressed;
  bool longHandled;
  unsigned long changedAt;
  unsigned long pressedAt;
};

struct BeepStep {
  uint16_t frequency;
  unsigned long onMs;
  unsigned long offMs;
};

// # Buzzer Sequence
const BeepStep countdownBeeps[] = {
  {784, 220, 780},
  {784, 220, 780},
  {784, 220, 780},
  {784, 220, 780},
  {784, 220, 780},
  {1175, 900, 0},
};
const uint8_t COUNTDOWN_BEEP_COUNT = sizeof(countdownBeeps) / sizeof(countdownBeeps[0]);

// # Runtime State
Phase phase = PHASE_IDLE;
ButtonState systemButton = {SYSTEM_BUTTON_PIN, false, false, false, 0, 0};
ButtonState leaderboardButton = {LEADERBOARD_BUTTON_PIN, false, false, false, 0, 0};

String espLine = "";
String roundId = "";
String winnerName = "";
unsigned long countdownStartedAt = 0;
unsigned long countdownDurationMs = DEFAULT_COUNTDOWN_MS;
unsigned long lastDrawAt = 0;
unsigned long lastLedAt = 0;
unsigned long lastHelloAt = 0;
unsigned long buzzerStepStartedAt = 0;
unsigned long buzzerOffStartedAt = 0;
unsigned long gameOverStartedAt = 0;
unsigned long winnerTimeMs = 0;
uint8_t winnerSeat = 1;
uint8_t hue = 0;
bool espOnline = false;
bool buzzerActive = false;
bool buzzerInGap = false;
uint8_t buzzerStepIndex = 0;
uint8_t drawBag[MAX_BALL];
uint8_t drawRemaining = 0;

// # Function List
void showLcd(const String& row1, const String& row2 = "");
void setPhase(Phase nextPhase);
void updateEspSerial();
void handleEspLine(const String& line);
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
String ballLabel(uint8_t number);
String formatTime(unsigned long ms);
String compactName(const String& value);

// # Setup
void setup() {
  Serial.begin(9600);
  espSerial.begin(9600);
  dfSerial.begin(9600);

  pinMode(SYSTEM_BUTTON_PIN, INPUT_PULLUP);
  pinMode(LEADERBOARD_BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);

  Wire.begin();
  Wire.setClock(50000);
  delay(1000);
  lcd.init();
  lcd.backlight();

  FastLED.addLeds<WS2812B, LED_DATA_PIN, GRB>(leds, LED_COUNT);
  FastLED.setBrightness(64);
  FastLED.clear(true);

  sendDfCommand(0x06, 25);
  randomSeed(analogRead(A0));
  refillDrawBag();
  setPhase(PHASE_IDLE);

  espSerial.println("HELLO");
}

// # Main Loop
void loop() {
  updateEspSerial();
  updateButtons();
  updateBuzzer();
  updateLeds();

  if (phase == PHASE_COUNTDOWN) updateCountdown();
  if (phase == PHASE_PLAYING) updateGameplay();

  if (millis() - lastHelloAt >= 10000) {
    lastHelloAt = millis();
    espSerial.println("HELLO");
  }
}

// # Phase Changes
void setPhase(Phase nextPhase) {
  phase = nextPhase;

  if (nextPhase == PHASE_IDLE) {
    roundId = "";
    stopBuzzer();
    showLcd(espOnline ? "Console online" : "Console offline", "Waiting players");
  } else if (nextPhase == PHASE_COUNTDOWN) {
    countdownStartedAt = millis();
    showLcd("Get ready", "5...");
    playMp3Track(TRACK_COUNTDOWN);
    startCountdownBuzzer();
  } else if (nextPhase == PHASE_PLAYING) {
    refillDrawBag();
    lastDrawAt = millis() - DRAW_INTERVAL_MS + 1200;
    showLcd("Game started", "Watch number");
  } else if (nextPhase == PHASE_GAME_OVER) {
    showLcd(compactName(winnerName), formatTime(winnerTimeMs));
    playMp3Track(TRACK_WIN_PLAYER_1 + winnerSeat - 1);
    tone(BUZZER_PIN, 988);
    buzzerActive = true;
    gameOverStartedAt = millis();
  }
}

// # ESP32U Serial Input
void updateEspSerial() {
  espSerial.listen();
  while (espSerial.available()) {
    char c = (char)espSerial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      espLine.trim();
      if (espLine.length() > 0) handleEspLine(espLine);
      espLine = "";
      continue;
    }
    if (espLine.length() < 96) espLine += c;
  }
}

// # ESP32U Command Parser
void handleEspLine(const String& line) {
  if (line == "NET:ONLINE") {
    espOnline = true;
    if (phase == PHASE_IDLE) setPhase(PHASE_IDLE);
    return;
  }

  if (line == "NET:OFFLINE" || line == "NET:WIFI_WAIT") {
    espOnline = false;
    if (phase == PHASE_IDLE) setPhase(PHASE_IDLE);
    return;
  }

  if (line.startsWith("START:")) {
    int firstColon = line.indexOf(':');
    int secondColon = line.indexOf(':', firstColon + 1);
    roundId = line.substring(firstColon + 1, secondColon);
    countdownDurationMs = line.substring(secondColon + 1).toInt();
    if (countdownDurationMs == 0) countdownDurationMs = DEFAULT_COUNTDOWN_MS;
    setPhase(PHASE_COUNTDOWN);
    return;
  }

  if (line.startsWith("WIN:")) {
    int p1 = line.indexOf(':');
    int p2 = line.indexOf(':', p1 + 1);
    int p3 = line.indexOf(':', p2 + 1);
    winnerName = line.substring(p1 + 1, p2);
    winnerSeat = line.substring(p2 + 1, p3).toInt();
    winnerTimeMs = line.substring(p3 + 1).toInt();
    if (winnerSeat < 1 || winnerSeat > 3) winnerSeat = 1;
    setPhase(PHASE_GAME_OVER);
    return;
  }

  if (line == "RESET") {
    setPhase(PHASE_IDLE);
  }
}

// # Button Handling
void updateButtons() {
  const unsigned long now = millis();
  ButtonState* buttons[] = {&systemButton, &leaderboardButton};

  for (uint8_t i = 0; i < 2; i++) {
    ButtonState* button = buttons[i];
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
        showLcd("Restart Arduino", "Release button");
        asm volatile ("jmp 0");
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

// # Button Actions
void handleButtonRelease(ButtonState& button, unsigned long heldMs) {
  if (button.longHandled || heldMs >= LONG_PRESS_MS) return;

  if (button.pin == SYSTEM_BUTTON_PIN) {
    espSerial.println("RESET_REQ");
    setPhase(PHASE_IDLE);
    return;
  }

  if (button.pin == LEADERBOARD_BUTTON_PIN && phase == PHASE_IDLE) {
    showLcd("Leaderboard", "Unavailable Uno");
  }
}

// # Buzzer Control
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
    if (now - gameOverStartedAt >= 4000) stopBuzzer();
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

// # LED Animations
void updateLeds() {
  if (millis() - lastLedAt < 30) return;
  lastLedAt = millis();

  if (phase == PHASE_IDLE) {
    uint8_t brightness = beatsin8(14, 12, 110);
    fill_solid(leds, LED_COUNT, CHSV(118, 180, brightness));
  } else if (phase == PHASE_COUNTDOWN) {
    fill_solid(leds, LED_COUNT, CHSV(32, 230, beatsin8(26, 60, 180)));
  } else if (phase == PHASE_PLAYING) {
    fill_rainbow(leds, LED_COUNT, hue++, 7);
  } else {
    for (uint16_t i = 0; i < LED_COUNT; i++) {
      leds[i] = CHSV(random8(), 255, random8(40, 180));
    }
  }

  FastLED.show();
}

// # Countdown State
void updateCountdown() {
  unsigned long elapsed = millis() - countdownStartedAt;
  if (elapsed < 1000) {
    showLcd("Get ready", "5...");
  } else if (elapsed < 2000) {
    showLcd("Get ready", "4...");
  } else if (elapsed < 3000) {
    showLcd("Get ready", "3...");
  } else if (elapsed < 4000) {
    showLcd("Get ready", "2...");
  } else if (elapsed < 5000) {
    showLcd("Get ready", "1...");
  } else if (elapsed < countdownDurationMs) {
    showLcd("Get ready", "GO!");
  } else {
    setPhase(PHASE_PLAYING);
  }
}

// # Gameplay Number Drawing
void updateGameplay() {
  if (millis() - lastDrawAt < DRAW_INTERVAL_MS) return;
  lastDrawAt = millis();

  uint8_t number = drawNumber();
  showLcd("Current Number", ballLabel(number));
  playMp3Track(number);
  espSerial.print("DRAW:");
  espSerial.println(number);
}

// # Draw Bag
void refillDrawBag() {
  for (uint8_t i = 0; i < MAX_BALL; i++) drawBag[i] = i + 1;

  for (int i = MAX_BALL - 1; i > 0; i--) {
    int j = random(i + 1);
    uint8_t temp = drawBag[i];
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

// # DFPlayer Mini
void playMp3Track(uint16_t track) {
  sendDfCommand(0x12, track);
}

void sendDfCommand(uint8_t command, uint16_t parameter) {
  dfSerial.listen();
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
  espSerial.listen();
}

// # LCD Output
void showLcd(const String& row1, const String& row2) {
  static String previousRow1 = "";
  static String previousRow2 = "";

  String nextRow1 = row1.substring(0, 16);
  String nextRow2 = row2.substring(0, 16);
  while (nextRow1.length() < 16) nextRow1 += " ";
  while (nextRow2.length() < 16) nextRow2 += " ";

  if (nextRow1 == previousRow1 && nextRow2 == previousRow2) return;
  previousRow1 = nextRow1;
  previousRow2 = nextRow2;

  lcd.setCursor(0, 0);
  lcd.print(nextRow1);
  lcd.setCursor(0, 1);
  lcd.print(nextRow2);
}

// # Text Formatting Helpers
String ballLabel(uint8_t number) {
  char letter = 'B';
  if (number >= 16 && number <= 30) letter = 'I';
  else if (number >= 31 && number <= 45) letter = 'N';
  else if (number >= 46 && number <= 60) letter = 'G';
  else if (number >= 61) letter = 'O';

  char buffer[8];
  snprintf(buffer, sizeof(buffer), "%c-%02u", letter, number);
  return String(buffer);
}

String formatTime(unsigned long ms) {
  unsigned long totalSeconds = ms / 1000;
  char buffer[10];
  snprintf(buffer, sizeof(buffer), "%02lu:%02lu", totalSeconds / 60, totalSeconds % 60);
  return String(buffer);
}

String compactName(const String& value) {
  String output = value;
  output.trim();
  if (output.length() == 0) output = "Player";
  if (output.length() > 16) output = output.substring(0, 16);
  return output;
}
