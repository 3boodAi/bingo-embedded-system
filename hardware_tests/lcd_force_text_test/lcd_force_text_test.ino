#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

LiquidCrystal_I2C lcd27(0x27, 16, 2);
LiquidCrystal_I2C lcd3f(0x3F, 16, 2);

void writeMessage(LiquidCrystal_I2C& lcd, const char* addressText, int counter) {
  lcd.init();
  lcd.backlight();
  delay(50);
  lcd.clear();
  delay(20);
  lcd.setCursor(0, 0);
  lcd.print("BINGO LCD OK");
  lcd.setCursor(0, 1);
  lcd.print(addressText);
  lcd.print(" Count ");
  lcd.print(counter);
}

void setup() {
  Serial.begin(9600);
  Wire.begin();
  delay(1000);
  Serial.println("Force LCD text test started.");
}

void loop() {
  static int counter = 0;

  writeMessage(lcd27, "0x27", counter);
  writeMessage(lcd3f, "0x3F", counter);

  Serial.print("Force text count: ");
  Serial.println(counter);

  counter++;
  delay(1000);
}
