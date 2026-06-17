#include <Arduino.h>
#include <SoftwareSerial.h>

const int DFPLAYER_RX_PIN = 5;
const int DFPLAYER_TX_PIN = 6;

SoftwareSerial dfPlayerSerial(DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);

void sendDfPlayerCommand(byte command, int parameter) {
  byte commandBuffer[10] = {
    0x7E,
    0xFF,
    0x06,
    command,
    0x00,
    highByte(parameter),
    lowByte(parameter),
    0x00,
    0x00,
    0xEF
  };

  int checksum = 0;
  for (int i = 1; i < 7; i++) {
    checksum += commandBuffer[i];
  }

  checksum = -checksum;
  commandBuffer[7] = highByte(checksum);
  commandBuffer[8] = lowByte(checksum);

  for (int i = 0; i < 10; i++) {
    dfPlayerSerial.write(commandBuffer[i]);
  }
}

void setup() {
  Serial.begin(9600);
  dfPlayerSerial.begin(9600);

  Serial.println("DFPlayer Mini test started");
  Serial.println("Put 0001.mp3 on the DFPlayer microSD card.");
  Serial.println("Track 1 should play every 5 seconds.");

  sendDfPlayerCommand(0x0C, 0);
  delay(2000);
  sendDfPlayerCommand(0x06, 30);
  delay(500);
}

void loop() {
  Serial.println("Playing track 1");
  sendDfPlayerCommand(0x03, 1);
  delay(5000);
}
