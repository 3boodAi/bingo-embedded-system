#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

void writeStableText() {
  lcd.clear();
  delay(10);
  lcd.setCursor(0, 0);
  lcd.print("BINGO READY     ");
  lcd.setCursor(0, 1);
  lcd.print("LCD TEST OK     ");
}

void setup() {
  Serial.begin(9600);
  Wire.begin();
  Wire.setClock(25000);

  delay(2500);
  lcd.begin(16, 2);
  lcd.backlight();
  lcd.noCursor();
  lcd.noBlink();
  lcd.leftToRight();
  lcd.noAutoscroll();
  writeStableText();

  Serial.println("LCD begin reset test running.");
}

void loop() {
  // Refresh slowly in case the LCD was left in a bad state.
  delay(5000);
  lcd.begin(16, 2);
  lcd.backlight();
  writeStableText();
}
