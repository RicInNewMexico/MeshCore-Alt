#pragma once

#include <Arduino.h>
#include <M5Core2.h>

namespace proto_nerd_font {

int textWidth(const char* text, uint8_t scale = 1);
void drawText(M5Display& lcd, int x, int y, const char* text, uint16_t fg, uint16_t bg, uint8_t scale = 1);
void drawTextTransparent(M5Display& lcd, int x, int y, const char* text, uint16_t fg, uint8_t scale = 1);

}  // namespace proto_nerd_font
