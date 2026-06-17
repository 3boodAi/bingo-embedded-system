#include <Arduino.h>
#include <U8g2lib.h>

const int GLCD_CLOCK_PIN = 13;
const int GLCD_DATA_PIN = 11;
const int GLCD_CS_PIN = 10;
const int GLCD_RESET_PIN = 8;

U8G2_ST7920_128X64_1_SW_SPI display(
  U8G2_R0,
  GLCD_CLOCK_PIN,
  GLCD_DATA_PIN,
  GLCD_CS_PIN,
  GLCD_RESET_PIN
);

void drawTestScreen(unsigned int counter) {
  int boxX = (counter * 4) % 104;

  display.setFont(u8g2_font_6x10_tf);
  display.drawFrame(0, 0, 128, 64);
  display.drawStr(6, 12, "Bingo GLCD Test");
  display.drawStr(6, 25, "128x64 ST7920");
  display.drawStr(6, 38, "No I2C module");

  display.drawLine(0, 0, 127, 63);
  display.drawLine(127, 0, 0, 63);
  display.drawBox(boxX + 6, 47, 18, 10);

  display.setCursor(6, 62);
  display.print("Count ");
  display.print(counter);
}

void setup() {
  display.begin();
  display.setContrast(255);
}

void loop() {
  static unsigned int counter = 0;

  display.firstPage();
  do {
    if (counter % 4 == 0) {
      display.drawBox(0, 0, 128, 64);
    } else if (counter % 4 == 1) {
      display.setFont(u8g2_font_6x10_tf);
      display.drawFrame(0, 0, 128, 64);
      display.drawStr(6, 16, "FULL SCREEN TEST");
      display.drawStr(6, 32, "Adjust contrast");
      display.drawStr(6, 48, "Check PSB = GND");
    } else {
      drawTestScreen(counter);
    }
  } while (display.nextPage());

  counter++;
  delay(1000);
}
