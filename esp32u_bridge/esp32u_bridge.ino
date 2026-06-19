/*
  ESP32U bridge firmware

  Role:
  - Connects to Wi-Fi.
  - Connects to Render hardware WebSocket endpoint.
  - Talks to Arduino Uno over UART2.

  Runtime wiring:
  - Arduino D3 -> 1k -> junction -> ESP32U GPIO4
  - junction -> 2.2k -> GND
  - ESP32U GPIO17 -> Arduino D2
  - ESP32U GND -> Arduino GND
*/

// # Libraries
#include <Arduino.h>
#include <WebSocketsClient.h>
#include <WiFi.h>

// # Wi-Fi Settings
#if __has_include("wifi_config.h")
#include "wifi_config.h"
#else
const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
#endif

// # Render WebSocket Settings
const char* WS_HOST = "bingo-embedded-system.onrender.com";
const uint16_t WS_PORT = 443;
const char* WS_PATH = "/hardware";

// # UART And Timing Settings
const uint8_t ARDUINO_RX_PIN = 4;   // ESP32 receives from Arduino D3 through divider.
const uint8_t ARDUINO_TX_PIN = 17;  // ESP32 sends to Arduino D2.
const unsigned long HEARTBEAT_INTERVAL_MS = 15000;
const unsigned long WIFI_STATUS_INTERVAL_MS = 5000;
const unsigned long WIFI_RETRY_INTERVAL_MS = 20000;

// # Hardware Objects
WebSocketsClient webSocket;

// # Runtime State
String arduinoLine = "";
bool wsConnected = false;
unsigned long lastHeartbeatAt = 0;
unsigned long lastWifiStatusAt = 0;
unsigned long lastWifiRetryAt = 0;

// # Small JSON Helpers
String extractJsonString(const String& json, const String& key) {
  String pattern = "\"" + key + "\":";
  int keyIndex = json.indexOf(pattern);
  if (keyIndex < 0) return "";

  int quoteStart = json.indexOf('"', keyIndex + pattern.length());
  if (quoteStart < 0) return "";

  int quoteEnd = json.indexOf('"', quoteStart + 1);
  if (quoteEnd < 0) return "";

  return json.substring(quoteStart + 1, quoteEnd);
}

long extractJsonLong(const String& json, const String& key, long fallback) {
  String pattern = "\"" + key + "\":";
  int keyIndex = json.indexOf(pattern);
  if (keyIndex < 0) return fallback;

  int valueStart = keyIndex + pattern.length();
  while (valueStart < (int)json.length() && json[valueStart] == ' ') valueStart++;

  int valueEnd = valueStart;
  while (valueEnd < (int)json.length() && (isDigit(json[valueEnd]) || json[valueEnd] == '-')) {
    valueEnd++;
  }

  if (valueEnd == valueStart) return fallback;
  return json.substring(valueStart, valueEnd).toInt();
}

// # Messages To Server
void sendWs(String payload) {
  if (wsConnected) webSocket.sendTXT(payload);
}

void sendHardwareHello() {
  String deviceId = "esp32u-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  sendWs("{\"type\":\"hardware_hello\",\"deviceId\":\"" + deviceId + "\",\"version\":\"esp32u-bridge-1.0.0\"}");
}

void sendHeartbeat() {
  sendWs("{\"type\":\"heartbeat\",\"uptimeMs\":" + String(millis()) + "}");
}

// # Server WebSocket Events
void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_CONNECTED) {
    wsConnected = true;
    Serial.println("WebSocket online");
    Serial2.println("NET:ONLINE");
    sendHardwareHello();
    return;
  }

  if (type == WStype_DISCONNECTED) {
    wsConnected = false;
    Serial.println("WebSocket offline");
    Serial2.println("NET:OFFLINE");
    return;
  }

  if (type != WStype_TEXT) return;

  String message;
  message.reserve(length);
  for (size_t i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  String messageType = extractJsonString(message, "type");
  Serial.println(message);

  if (messageType == "sync") {
    Serial2.println("SYNC:" + extractJsonString(message, "phase"));
    return;
  }

  if (messageType == "start_game") {
    String roundId = extractJsonString(message, "roundId");
    long countdownMs = extractJsonLong(message, "countdownMs", 6000);
    Serial2.println("START:" + roundId + ":" + String(countdownMs));
    return;
  }

  if (messageType == "game_over") {
    String winnerName = extractJsonString(message, "winnerName");
    long winnerSeat = extractJsonLong(message, "winnerSeat", 1);
    long winTimeMs = extractJsonLong(message, "winTimeMs", 0);
    Serial2.println("WIN:" + winnerName + ":" + String(winnerSeat) + ":" + String(winTimeMs));
    return;
  }

  if (messageType == "reset") {
    Serial2.println("RESET");
  }
}

// # Messages From Arduino
void handleArduinoLine(const String& line) {
  Serial.print("Arduino: ");
  Serial.println(line);

  if (line.startsWith("DRAW:")) {
    int number = line.substring(5).toInt();
    sendWs("{\"type\":\"drawn_number\",\"number\":" + String(number) + "}");
    return;
  }

  if (line == "RESET_REQ") {
    sendWs("{\"type\":\"reset_requested\"}");
    return;
  }

  if (line == "HELLO") {
    if (wsConnected) {
      Serial2.println("NET:ONLINE");
    } else if (WiFi.status() == WL_CONNECTED) {
      Serial2.println("NET:OFFLINE");
    } else {
      Serial2.println("NET:WIFI_WAIT");
    }
    sendHardwareHello();
  }
}

// # Arduino UART Reader
void updateArduinoSerial() {
  while (Serial2.available()) {
    char c = (char)Serial2.read();
    if (c == '\r') continue;
    if (c == '\n') {
      arduinoLine.trim();
      if (arduinoLine.length() > 0) handleArduinoLine(arduinoLine);
      arduinoLine = "";
      continue;
    }

    if (arduinoLine.length() < 96) {
      arduinoLine += c;
    }
  }
}

// # Wi-Fi Watchdog
void connectWiFiIfNeeded() {
  if (WiFi.status() == WL_CONNECTED) return;

  const unsigned long now = millis();
  if (now - lastWifiStatusAt >= WIFI_STATUS_INTERVAL_MS) {
    lastWifiStatusAt = now;
    Serial.print("Wi-Fi waiting for SSID: ");
    Serial.print(WIFI_SSID);
    Serial.print(" status=");
    Serial.println(WiFi.status());
    Serial2.println("NET:WIFI_WAIT");
  }

  if (now - lastWifiRetryAt >= WIFI_RETRY_INTERVAL_MS) {
    lastWifiRetryAt = now;
    WiFi.disconnect(false);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }
}

// # Setup
void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, ARDUINO_RX_PIN, ARDUINO_TX_PIN);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  Serial.print("Connecting to Wi-Fi SSID: ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  webSocket.beginSSL(WS_HOST, WS_PORT, WS_PATH);
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
  webSocket.enableHeartbeat(15000, 3000, 2);

  Serial.println("ESP32U bridge booted");
  Serial2.println("NET:BOOT");
}

// # Main Loop
void loop() {
  connectWiFiIfNeeded();
  webSocket.loop();
  updateArduinoSerial();

  if (wsConnected && millis() - lastHeartbeatAt >= HEARTBEAT_INTERVAL_MS) {
    lastHeartbeatAt = millis();
    sendHeartbeat();
  }
}
