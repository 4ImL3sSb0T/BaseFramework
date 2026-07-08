#include "Adafruit_ST7735.h"
#include "tft_port.h"
#include "Arduino.h"

static Adafruit_ST7735 tft(&SPI, PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);

extern "C" void tft_demo_task(void) {
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);

  tft.setCursor(0, 0);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.println("STM32H750");
  tft.print("TFT Hello!");

  tft.fillRect(10, 60, 30, 30, ST77XX_RED);
  tft.fillRect(50, 60, 30, 30, ST77XX_GREEN);
  tft.fillRect(90, 60, 30, 30, ST77XX_BLUE);

  tft.drawCircle(64, 120, 20, ST77XX_YELLOW);
}