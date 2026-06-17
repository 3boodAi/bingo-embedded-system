#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>

namespace WiFiManager {
  enum EventType {
    EVENT_NONE,
    EVENT_NUMBER_DRAWN,
    EVENT_WINNER
  };

  struct Event {
    EventType type;
    int number;
    String winnerName;
    String winTime;
  };

  void begin();
  Event update();
  void saveScoreToSD(String name, String time);
  bool isGameActive();
}

#endif
