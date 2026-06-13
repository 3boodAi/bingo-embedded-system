#include "WiFiManager.h"

#include <SD.h>
#include <SPI.h>
#include <SoftwareSerial.h>
#include <WiFiEsp.h>

namespace {
  const int CS_PIN = 4;

  SoftwareSerial espSerial(2, 3);

  char ssid[] = "YOUR_SSID";
  char pass[] = "YOUR_PASSWORD";
  char server[] = "192.168.1.100";
  int port = 3000;

  WiFiEspClient client;

  unsigned long lastDrawTime = 0;
  const unsigned long DRAW_INTERVAL = 7000;
  bool gameActive = false;

  WiFiManager::Event noneEvent() {
    WiFiManager::Event event;
    event.type = WiFiManager::EVENT_NONE;
    event.number = -1;
    event.winnerName = "";
    event.winTime = "";
    return event;
  }

  WiFiManager::Event numberDrawnEvent(int number) {
    WiFiManager::Event event = noneEvent();
    event.type = WiFiManager::EVENT_NUMBER_DRAWN;
    event.number = number;
    return event;
  }

  WiFiManager::Event winnerEvent(String winnerName, String winTime) {
    WiFiManager::Event event = noneEvent();
    event.type = WiFiManager::EVENT_WINNER;
    event.winnerName = winnerName;
    event.winTime = winTime;
    return event;
  }

  void connectNetwork() {
    Serial.println("Initializing ESP8266...");
    WiFi.init(&espSerial);

    if (WiFi.status() == WL_NO_SHIELD) {
      Serial.println("ESP8266 shield not present");
      while (true) {
      }
    }

    Serial.println("Connecting to Wi-Fi...");
    while (WiFi.status() != WL_CONNECTED) {
      WiFi.begin(ssid, pass);
    }

    Serial.println("Connected to Wi-Fi!");
    Serial.println("Connecting to server...");

    if (client.connect(server, port)) {
      Serial.println("Connected to server successfully");
      gameActive = true;
    } else {
      Serial.println("Server connection failed");
    }
  }

  void initializeSDCard() {
    if (!SD.begin(CS_PIN)) {
      Serial.println("SD card initialization failed!");
    } else {
      Serial.println("SD card initialized successfully.");
    }
  }

  WiFiManager::Event handleIncomingPayload(String line) {
    line.trim();

    if (!line.startsWith("WINNER:")) {
      return noneEvent();
    }

    gameActive = false;

    int firstColon = line.indexOf(':');
    int secondColon = line.indexOf(':', firstColon + 1);

    if (firstColon == -1 || secondColon == -1) {
      return noneEvent();
    }

    String winnerName = line.substring(firstColon + 1, secondColon);
    String winTime = line.substring(secondColon + 1);

    return winnerEvent(winnerName, winTime);
  }
}

namespace WiFiManager {
  void begin() {
    espSerial.begin(9600);
    initializeSDCard();
    connectNetwork();
  }

  Event update() {
    if (gameActive && (millis() - lastDrawTime >= DRAW_INTERVAL)) {
      lastDrawTime = millis();

      int drawnNumber = random(0, 100);

      if (client.connected()) {
        client.print("NUM:");
        client.println(drawnNumber);
      }

      return numberDrawnEvent(drawnNumber);
    }

    if (client.available()) {
      String line = client.readStringUntil('\n');
      return handleIncomingPayload(line);
    }

    return noneEvent();
  }

  void saveScoreToSD(String name, String time) {
  }

  bool isGameActive() {
    return gameActive;
  }
}
