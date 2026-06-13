#include "DisplayManager.h"

#include <LiquidCrystal_I2C.h>
#include <Wire.h>

namespace {
  LiquidCrystal_I2C lcd(0x27, 16, 2);
}

namespace DisplayManager {
  void begin() {
    lcd.init();
    lcd.backlight();
    setupContrast();
  }

  void setupContrast() {
  }

  void showMessage(String message) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(message);
  }

  void showNumber(int number) {
    showMessage("Drawn: " + String(number));
  }

  void showWinner(String winnerName) {
    showMessage("WIN: " + winnerName);
  }
}
