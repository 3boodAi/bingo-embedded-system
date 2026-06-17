#include <Arduino.h>

#include "AudioManager.h"
#include "DisplayManager.h"
#include "LEDManager.h"
#include "WiFiManager.h"

void setup() {
  Serial.begin(9600);

  DisplayManager::begin();
  AudioManager::begin();
  LEDManager::begin();
  randomSeed(analogRead(0));
  WiFiManager::begin();
}

void loop() {
  WiFiManager::Event event = WiFiManager::update();

  if (event.type == WiFiManager::EVENT_NUMBER_DRAWN) {
    DisplayManager::showNumber(event.number);
    AudioManager::playDrawSound();
  }

  if (event.type == WiFiManager::EVENT_WINNER) {
    DisplayManager::showWinner(event.winnerName);
    AudioManager::playWinSound();
    LEDManager::flashWinningLEDs();
    WiFiManager::saveScoreToSD(event.winnerName, event.winTime);
  }
}
