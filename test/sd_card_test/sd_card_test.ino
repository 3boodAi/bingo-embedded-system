#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

const int CS_PIN = 4;

void setup() {
  Serial.begin(9600);
  while (!Serial) {
  }

  Serial.println("MicroSD card module test started");
  Serial.println("Testing write and read on test.txt");

  if (!SD.begin(CS_PIN)) {
    Serial.println("SD initialization failed");
    return;
  }

  Serial.println("SD initialized");

  if (SD.exists("test.txt")) {
    SD.remove("test.txt");
  }

  File testFile = SD.open("test.txt", FILE_WRITE);
  if (!testFile) {
    Serial.println("Could not open test.txt for writing");
    return;
  }

  testFile.println("Bingo SD test OK");
  testFile.close();
  Serial.println("Write OK");

  testFile = SD.open("test.txt");
  if (!testFile) {
    Serial.println("Could not open test.txt for reading");
    return;
  }

  Serial.println("Read result:");
  while (testFile.available()) {
    Serial.write(testFile.read());
  }
  testFile.close();

  Serial.println();
  Serial.println("SD test complete");
}

void loop() {
}
