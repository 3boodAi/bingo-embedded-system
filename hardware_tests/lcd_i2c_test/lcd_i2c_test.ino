#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
  Serial.begin(9600);

  lcd.init();
  lcd.backlight();
  lcd.clear();

  Serial.println("16x2 I2C LCD test started");
  Serial.println("LCD should show changing Bingo test text.");
}

void loop() {
  static int counter = 0;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Bingo LCD Test");
  lcd.setCursor(0, 1);
  lcd.print("Count: ");
  lcd.print(counter);

  Serial.print("LCD count: ");
  Serial.println(counter);

  counter++;
  delay(1000);
}
