/*
 * SD_Test.ino
 * 
 * Standalone Micro SD Card Test Script for Arduino Uno.
 * This sketch initializes the SD card via SPI, writes a test message to 'test.txt',
 * and reads it back to the Serial Monitor to verify hardware wiring.
 * 
 * --- SPI Pin Connections (Arduino Uno / Nano) ---
 * MOSI - Pin 11
 * MISO - Pin 12
 * SCK  - Pin 13
 * CS   - Pin 4 (Can be changed below)
 */


#include <SPI.h>
#include <SD.h>

const int chipSelect = 4; // Update this if your CS pin is connected elsewhere

void setup() {
  // Open serial communications
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  Serial.println("Initializing SD card...");

  // Initialize the SD card
  if (!SD.begin(chipSelect)) {
    Serial.println("Initialization failed!");
    Serial.println("1. Is a card inserted?");
    Serial.println("2. Is your wiring correct? (MOSI:11, MISO:12, SCK:13, CS:4)");
    Serial.println("3. Did you change the chipSelect pin to match your module?");
    return;
  }
  Serial.println("Initialization done.");

  // Delete the file if it exists so we can start fresh
  if (SD.exists("test.txt")) {
    SD.remove("test.txt");
  }

  // Open the file for writing
  File testFile = SD.open("test.txt", FILE_WRITE);

  // if the file opened okay, write to it:
  if (testFile) {
    Serial.print("Writing to test.txt...");
    testFile.println("Hardware Test Successful");
    // close the file:
    testFile.close();
    Serial.println("done.");
  } else {
    // if the file didn't open, print an error:
    Serial.println("error opening test.txt");
    return;
  }

  // re-open the file for reading:
  testFile = SD.open("test.txt");
  if (testFile) {
    Serial.println("test.txt content:");
    
    // read from the file until there's nothing else in it:
    while (testFile.available()) {
      Serial.write(testFile.read());
    }
    // close the file:
    testFile.close();
    Serial.println("\n--- SD Card Test Complete ---");
  } else {
    // if the file didn't open, print an error:
    Serial.println("error opening test.txt for reading");
  }
}

void loop() {
  // nothing happens after setup
}