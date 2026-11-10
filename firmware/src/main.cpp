#include <M5Unified.h>

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.fillScreen(TFT_WHITE);
  M5.Display.setTextColor(TFT_BLACK);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextSize(3);
  M5.Display.drawString("Patogochi", 160, 100);
  M5.Display.setTextSize(2);
  M5.Display.drawString("quack!", 160, 150);
}

void loop() {
  M5.update();
  delay(50);
}
