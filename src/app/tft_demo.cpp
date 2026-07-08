#include "Adafruit_ST7735.h"
#include "buffered_display.h"
#include "tft_port.h"
#include "Arduino.h"

static Adafruit_ST7735 tft_dev(&SPI, PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);
static BufferedDisplay tft(&tft_dev);

extern "C" void tft_demo_task(void) {
  tft_dev.initR(INITR_BLACKTAB);
  tft_dev.setRotation(1);

  // ====== 所有绘制操作走帧缓冲 ======
  tft.fillScreen(ST77XX_BLACK);

  // 文字
  tft.setCursor(0, 0);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.println("STM32H750");
  tft.print("Buffered!");

  // 几何图形
  tft.fillRect(10, 60, 30, 30, ST77XX_RED);
  tft.fillRect(50, 60, 30, 30, ST77XX_GREEN);
  tft.fillRect(90, 60, 30, 30, ST77XX_BLUE);
  tft.drawCircle(64, 120, 20, ST77XX_YELLOW);

  // ====== 一次性刷到屏幕 ======
  tft.flush();

  // ====== 之后再改部分区域 ======
  tft.fillRect(40, 40, 48, 14, ST77XX_BLACK);       // 清除旧文字（帧缓冲内）
  tft.setCursor(42, 42);
  tft.setTextColor(ST77XX_CYAN);
  tft.setTextSize(1);
  tft.print("FrameBuf OK");
  tft.flush();                                        // 只改动部分也可全刷

  // 更多动画效果演示：
  for (int r = 0; r < 80; r += 5) {
    tft.drawCircle(64, 120, r, ST77XX_MAGENTA);
    tft.flush();
    HAL_Delay(50);
  }
}
