#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

LiquidCrystal_I2C lcd27(0x27, 16, 2);
LiquidCrystal_I2C lcd3f(0x3F, 16, 2);

void setup() {
  Wire.begin();
  lcd27.init();
  lcd3f.init();
}

void loop() {
  lcd27.backlight();
  lcd3f.backlight();
  delay(1000);

  lcd27.noBacklight();
  lcd3f.noBacklight();
  delay(1000);
}
