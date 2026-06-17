#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>
#include <Wire.h>

const int ESP_RX_PIN = 2; // Arduino receives from ESP TX.
const int ESP_TX_PIN = 3; // Arduino sends to ESP RX through voltage divider.

LiquidCrystal_I2C lcd(0x27, 16, 2);
SoftwareSerial espSerial(ESP_RX_PIN, ESP_TX_PIN);

const unsigned long BAUD_RATES[] = {9600, 57600, 115200};
const byte BAUD_COUNT = sizeof(BAUD_RATES) / sizeof(BAUD_RATES[0]);

bool sendAtAndWait(const char* command, unsigned long timeoutMs) {
  while (espSerial.available()) espSerial.read();
  espSerial.println(command);

  unsigned long startedAt = millis();
  String response = "";

  while (millis() - startedAt < timeoutMs) {
    while (espSerial.available()) {
      char c = (char)espSerial.read();
      response += c;
      Serial.write(c);
      if (response.indexOf("OK") >= 0) return true;
    }
  }

  return false;
}

void setup() {
  Serial.begin(9600);
  Wire.begin();
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ESP AT Test");
  lcd.setCursor(0, 1);
  lcd.print("Wiring check");

  pinMode(ESP_RX_PIN, INPUT);
  pinMode(ESP_TX_PIN, OUTPUT);

  Serial.println("ESP8266 AT test started.");
  Serial.println("Open Serial Monitor at 9600 baud.");
  Serial.println("Trying 9600, 57600, and 115200 baud...");

  for (byte i = 0; i < BAUD_COUNT; i++) {
    unsigned long baud = BAUD_RATES[i];
    espSerial.begin(baud);
    delay(250);

    Serial.print("Trying ESP baud ");
    Serial.println(baud);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Trying baud");
    lcd.setCursor(0, 1);
    lcd.print(baud);

    if (sendAtAndWait("AT", 1500)) {
      Serial.print("ESP responded at ");
      Serial.println(baud);

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("ESP OK");
      lcd.setCursor(0, 1);
      lcd.print("Baud ");
      lcd.print(baud);

      sendAtAndWait("AT+GMR", 2500);
      return;
    }
  }

  Serial.println("No ESP response. Check power, EN/CH_PD, GND, RX/TX, and voltage divider.");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ESP no reply");
  lcd.setCursor(0, 1);
  lcd.print("Check wiring");
}

void loop() {
  while (Serial.available()) {
    espSerial.write(Serial.read());
  }

  while (espSerial.available()) {
    Serial.write(espSerial.read());
  }
}
