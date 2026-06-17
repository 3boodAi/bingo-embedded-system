#include "AudioManager.h"

#include <Arduino.h>
#include <SoftwareSerial.h>

namespace {
  const int BUZZER_PIN = 8;
  const int DFPLAYER_RX_PIN = 5;
  const int DFPLAYER_TX_PIN = 6;
  const int BINGO_TRACK = 1;
  const int WINNER_TRACK = 2;

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

  void playTrack(int trackNumber) {
    sendDfPlayerCommand(0x03, trackNumber);
  }
}

namespace AudioManager {
  void begin() {
    pinMode(BUZZER_PIN, OUTPUT);
    dfPlayerSerial.begin(9600);
    delay(1000);
    sendDfPlayerCommand(0x06, 20);
  }

  void playDrawSound() {
    playBingoSound();
  }

  void playBingoSound() {
    playTrack(BINGO_TRACK);
  }

  void playWinSound() {
    playTrack(WINNER_TRACK);
  }
}
