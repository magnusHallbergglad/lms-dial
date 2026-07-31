#include <M5Unified.h>

long dialPosition = 0;

void drawDialPosition() {
  M5.Display.fillScreen(BLACK);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(WHITE, BLACK);
  M5.Display.setTextSize(2);
  M5.Display.drawString("lms-dial", M5.Display.width() / 2,
                        M5.Display.height() / 2 - 36);

  M5.Display.setTextSize(4);
  M5.Display.drawNumber(dialPosition, M5.Display.width() / 2,
                        M5.Display.height() / 2 + 14);
}

void setup() {
  auto config = M5.config();
  M5.begin(config);

  M5.Display.setRotation(0);
  drawDialPosition();
}

void loop() {
  M5.update();

  const long newPosition = M5.Dial.Encoder.read();
  if (newPosition != dialPosition) {
    dialPosition = newPosition;
    drawDialPosition();
  }

  if (M5.BtnA.wasPressed()) {
    dialPosition = 0;
    M5.Dial.Encoder.write(dialPosition);
    drawDialPosition();
  }
}
