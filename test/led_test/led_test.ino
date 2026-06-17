#include <Arduino.h>

const int LED_RED = 9;
const int LED_GREEN = 10;
const int LED_BLUE = 11;

void setup() {
  Serial.begin(9600);

  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);

  Serial.println("LED test started");
  Serial.println("Red, green, and blue LEDs should blink one by one.");
}

void loop() {
  digitalWrite(LED_RED, HIGH);
  Serial.println("Red LED ON");
  delay(500);
  digitalWrite(LED_RED, LOW);

  digitalWrite(LED_GREEN, HIGH);
  Serial.println("Green LED ON");
  delay(500);
  digitalWrite(LED_GREEN, LOW);

  digitalWrite(LED_BLUE, HIGH);
  Serial.println("Blue LED ON");
  delay(500);
  digitalWrite(LED_BLUE, LOW);

  delay(500);
}
