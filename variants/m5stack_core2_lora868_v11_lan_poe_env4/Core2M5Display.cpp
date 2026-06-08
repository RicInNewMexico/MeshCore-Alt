#include "Core2M5Display.h"
#include <Arduino.h>

#ifdef RED
#undef RED
#endif
#ifdef GREEN
#undef GREEN
#endif
#ifdef BLUE
#undef BLUE
#endif
#ifdef YELLOW
#undef YELLOW
#endif
#ifdef ORANGE
#undef ORANGE
#endif

namespace {
bool s_core2_initialized = false;
constexpr uint8_t kCore2BrightnessMax = 255;

void enforceCore2DisplayPower() {
  M5.Axp.SetLcdVoltage(2800);
  M5.Axp.SetVibration(false);
  M5.Axp.SetLed(0);
  M5.Axp.SetSpkEnable(false);
}
}

uint16_t Core2M5Display::toColor(Color c) const {
  switch (c) {
    case DARK:
      return TFT_BLACK;
    case LIGHT:
      return TFT_WHITE;
    case RED:
      return TFT_RED;
    case GREEN:
      return TFT_GREEN;
    case BLUE:
      return TFT_BLUE;
    case YELLOW:
      return TFT_YELLOW;
    case ORANGE:
      return TFT_ORANGE;
    default:
      return TFT_WHITE;
  }
}

bool Core2M5Display::begin() {
  if (!s_core2_initialized) {
    M5.begin(true, true, true, false, kMBusModeOutput, false);
    M5.Axp.SetCHGCurrent(AXP192::kCHG_190mA);
    M5.Axp.SetVibration(false);
    M5.Axp.SetLed(0);
    M5.Axp.SetSpkEnable(false);
    s_core2_initialized = true;
  }
  M5.update();
  enforceCore2DisplayPower();
  M5.Axp.ScreenBreath(kCore2BrightnessMax);
  M5.Lcd.setRotation(1);
  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.setTextSize(1);
  Serial.printf("LCD: M5Core2 init bl=%u\n", static_cast<unsigned>(kCore2BrightnessMax));
  setColor(LIGHT);
  turnOn();
  return true;
}

void Core2M5Display::turnOn() {
  enforceCore2DisplayPower();
  M5.Axp.ScreenBreath(kCore2BrightnessMax);
  _isOn = true;
}

void Core2M5Display::turnOff() {
  M5.Axp.SetVibration(false);
  _isOn = false;
}

void Core2M5Display::clear() {
  M5.Lcd.fillScreen(TFT_BLACK);
}

void Core2M5Display::startFrame(Color bkg) {
  M5.Lcd.startWrite();
  M5.Lcd.fillScreen(toColor(bkg));
  setColor(LIGHT);
}

void Core2M5Display::setTextSize(int sz) {
  M5.Lcd.setTextSize(sz);
}

void Core2M5Display::setColor(Color c) {
  _color = toColor(c);
  M5.Lcd.setTextColor(_color);
}

void Core2M5Display::setCursor(int x, int y) {
  _cursor_x = x;
  _cursor_y = y;
  M5.Lcd.setCursor(x, y);
}

void Core2M5Display::print(const char* str) {
  M5.Lcd.setCursor(_cursor_x, _cursor_y);
  M5.Lcd.print(str);
  _cursor_x = M5.Lcd.getCursorX();
  _cursor_y = M5.Lcd.getCursorY();
}

void Core2M5Display::fillRect(int x, int y, int w, int h) {
  M5.Lcd.fillRect(x, y, w, h, _color);
}

void Core2M5Display::drawRect(int x, int y, int w, int h) {
  M5.Lcd.drawRect(x, y, w, h, _color);
}

void Core2M5Display::drawXbm(int x, int y, const uint8_t* bits, int w, int h) {
  M5.Lcd.drawXBitmap(x, y, bits, w, h, _color);
}

uint16_t Core2M5Display::getTextWidth(const char* str) {
  return M5.Lcd.textWidth(str);
}

void Core2M5Display::endFrame() {
  M5.Lcd.endWrite();
}
