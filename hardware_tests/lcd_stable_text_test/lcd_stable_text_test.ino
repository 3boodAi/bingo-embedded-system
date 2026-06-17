#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
  Serial.begin(9600);
  Wire.begin();
  Wire.setClock(50000); // Slower I2C is more tolerant of long/loose jumper wires.

  delay(1000);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  delay(100);

  lcd.setCursor(0, 0);
  lcd.print("BINGO LCD TEST");
  lcd.setCursor(0, 1);
  lcd.print("Count: 0000");
}

void loop() {
  static int counter = 0;

  lcd.setCursor(7, 1);
  if (counter < 1000) lcd.print("0");
  if (counter < 100) lcd.print("0");
  if (counter < 10) lcd.print("0");
  lcd.print(counter);

  counter++;
  if (counter > 9999) counter = 0;
  delay(1000);
}
