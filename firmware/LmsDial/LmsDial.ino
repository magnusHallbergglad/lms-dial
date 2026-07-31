#include <M5Dial.h>

long dialPosition = 0;

void drawDialPosition() {
  M5Dial.Display.fillScreen(BLACK);
  M5Dial.Display.setTextDatum(middle_center);
  M5Dial.Display.setTextColor(WHITE, BLACK);
  M5Dial.Display.setTextSize(2);
  M5Dial.Display.drawString("lms-dial", M5Dial.Display.width() / 2,
                            M5Dial.Display.height() / 2 - 36);

  M5Dial.Display.setTextSize(4);
  M5Dial.Display.drawNumber(dialPosition, M5Dial.Display.width() / 2,
                            M5Dial.Display.height() / 2 + 14);
}

void setup() {
  auto config = M5.config();
  M5Dial.begin(config, true, false);  // Enable encoder, disable RFID.

  M5Dial.Display.setRotation(0);
  drawDialPosition();
}

void loop() {
  M5Dial.update();

  const long newPosition = M5Dial.Encoder.read();
  if (newPosition != dialPosition) {
    dialPosition = newPosition;
    drawDialPosition();
  }

  if (M5Dial.BtnA.wasPressed()) {
    dialPosition = 0;
    M5Dial.Encoder.write(dialPosition);
    drawDialPosition();
  }
}
