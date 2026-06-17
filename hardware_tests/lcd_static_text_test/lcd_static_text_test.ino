#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
  Wire.begin();
  Wire.setClock(50000);
  delay(1000);

  lcd.init();
  lcd.backlight();
  lcd.clear();
  delay(100);

  lcd.setCursor(0, 0);
  lcd.print("BINGO READY");
  lcd.setCursor(0, 1);
  lcd.print("LCD IS STABLE");
}

void loop() {
  // Intentionally empty. This test does not rewrite the LCD.
}
