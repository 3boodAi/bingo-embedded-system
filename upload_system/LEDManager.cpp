#include "LEDManager.h"

#include <Arduino.h>

namespace {
  const int LED_RED = 9;
  const int LED_GREEN = 10;
  const int LED_BLUE = 11;

  void setVictoryLEDs(int state) {
    digitalWrite(LED_RED, state);
    digitalWrite(LED_GREEN, state);
    digitalWrite(LED_BLUE, state);
  }
}

namespace LEDManager {
  void begin() {
    pinMode(LED_RED, OUTPUT);
    pinMode(LED_GREEN, OUTPUT);
    pinMode(LED_BLUE, OUTPUT);
    setVictoryLEDs(LOW);
  }

  void flashWinningLEDs() {
    for (int i = 0; i < 6; i++) {
      setVictoryLEDs(HIGH);
      delay(200);
      setVictoryLEDs(LOW);
      delay(200);
    }
  }
}
