#include <Arduino.h>

const int BUZZER_PIN = 8;

void setup() {
  Serial.begin(9600);
  pinMode(BUZZER_PIN, OUTPUT);

  Serial.println("Buzzer test started");
  Serial.println("The buzzer should beep repeatedly.");
}

void loop() {
  Serial.println("Beep");
  tone(BUZZER_PIN, 1000);
  delay(300);
  noTone(BUZZER_PIN);
  delay(700);
}
