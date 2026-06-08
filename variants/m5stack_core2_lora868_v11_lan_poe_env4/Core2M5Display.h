#pragma once

#include <helpers/ui/DisplayDriver.h>
#include <M5Core2.h>

class Core2M5Display : public DisplayDriver {
  bool _isOn = false;
  uint16_t _color = TFT_WHITE;
  int _cursor_x = 0;
  int _cursor_y = 0;

  uint16_t toColor(Color c) const;

public:
  Core2M5Display() : DisplayDriver(320, 240) {}

  bool isOn() override { return _isOn; }
  void turnOn() override;
  void turnOff() override;
  void clear() override;
  bool begin();
  void startFrame(Color bkg = DARK) override;
  void setTextSize(int sz) override;
  void setColor(Color c) override;
  void setCursor(int x, int y) override;
  void print(const char* str) override;
  void fillRect(int x, int y, int w, int h) override;
  void drawRect(int x, int y, int w, int h) override;
  void drawXbm(int x, int y, const uint8_t* bits, int w, int h) override;
  uint16_t getTextWidth(const char* str) override;
  void endFrame() override;
};
