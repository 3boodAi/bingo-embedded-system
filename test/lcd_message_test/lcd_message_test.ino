#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

void writeMessage() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Bingo Project");
  lcd.setCursor(0, 1);
  lcd.print("LCD is working");
}

void setupLcd() {
  lcd.init();
  lcd.backlight();
  writeMessage();
}

void setup() {
  setupLcd();
}

void loop() {
  static unsigned long lastRefresh = 0;

  if (millis() - lastRefresh >= 1000) {
    lastRefresh = millis();
    setupLcd();
  }
}
