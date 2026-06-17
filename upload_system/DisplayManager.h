#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>

namespace DisplayManager {
  void begin();
  void setupContrast();
  void showMessage(String message);
  void showNumber(int number);
  void showWinner(String winnerName);
}

#endif
