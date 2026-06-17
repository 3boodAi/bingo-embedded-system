#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

LiquidCrystal_I2C lcd27(0x27, 16, 2);
LiquidCrystal_I2C lcd3f(0x3F, 16, 2);

void setupLcd(LiquidCrystal_I2C& lcd) {
  lcd.init();
  lcd.backlight();
  lcd.clear();
}

void printToLcd(LiquidCrystal_I2C& lcd, int counter, const char* addressText) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("LCD OK ");
  lcd.print(addressText);
  lcd.setCursor(0, 1);
  lcd.print("Count: ");
  lcd.print(counter);
}

void setup() {
  Serial.begin(9600);
  Wire.begin();
  delay(250);

  setupLcd(lcd27);
  setupLcd(lcd3f);

  Serial.println("Dual-address LCD test started.");
  Serial.println("Trying LCD addresses 0x27 and 0x3F.");
}

void loop() {
  static int counter = 0;

  setupLcd(lcd27);
  printToLcd(lcd27, counter, "0x27");

  setupLcd(lcd3f);
  printToLcd(lcd3f, counter, "0x3F");

  Serial.print("LCD count: ");
  Serial.println(counter);

  counter++;
  delay(1000);
}
