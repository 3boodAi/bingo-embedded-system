#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
  Serial.begin(9600);
  Wire.begin();
  Wire.setClock(50000);

  delay(3000);

  lcd.init();
  lcd.backlight();
  delay(500);
  lcd.clear();
  delay(100);

  lcd.setCursor(0, 0);
  lcd.print("Bingo LCD Test");
  lcd.setCursor(0, 1);
  lcd.print("Stable message");
}

void loop() {
  // Do not rewrite. If the text changes, the issue is electrical/noise.
}
