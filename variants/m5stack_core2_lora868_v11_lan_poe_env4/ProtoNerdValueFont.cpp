#include "ProtoNerdValueFont.h"

#include "ProtoNerdValueFontData.h"

namespace proto_nerd_value_font {

using proto_nerd_font::kBitmap;
using proto_nerd_font::kBytesPerGlyph;
using proto_nerd_font::kBytesPerRow;
using proto_nerd_font::kCellHeight;
using proto_nerd_font::kCellWidth;
using proto_nerd_font::kFirst;
using proto_nerd_font::kLast;

static uint8_t normalizeChar(char c) {
  uint8_t uc = static_cast<uint8_t>(c);
  if (uc < kFirst || uc > kLast) {
    return static_cast<uint8_t>('?');
  }
  return uc;
}

int textWidth(const char* text, uint8_t scale) {
  if (!text || !*text) {
    return 0;
  }
  int count = 0;
  for (const char* p = text; *p; ++p) {
    ++count;
  }
  return count * static_cast<int>(kCellWidth) * static_cast<int>(scale);
}

void drawText(M5Display& lcd, int x, int y, const char* text, uint16_t fg, uint16_t bg, uint8_t scale) {
  if (!text || !*text) {
    return;
  }

  const int cell_w = static_cast<int>(kCellWidth) * static_cast<int>(scale);
  const int cell_h = static_cast<int>(kCellHeight) * static_cast<int>(scale);

  int cx = x;
  for (const char* p = text; *p; ++p) {
    uint8_t ch = normalizeChar(*p);
    uint16_t glyph_index = static_cast<uint16_t>(ch - kFirst);
    uint32_t off = static_cast<uint32_t>(glyph_index) * static_cast<uint32_t>(kBytesPerGlyph);

    lcd.fillRect(cx, y, cell_w, cell_h, bg);

    for (uint8_t row = 0; row < kCellHeight; ++row) {
      for (uint8_t col = 0; col < kCellWidth; ++col) {
        uint32_t byte_idx = off + static_cast<uint32_t>(row) * static_cast<uint32_t>(kBytesPerRow) + (col >> 3);
        uint8_t bit = static_cast<uint8_t>(0x80 >> (col & 7));
        if (kBitmap[byte_idx] & bit) {
          if (scale == 1) {
            lcd.drawPixel(cx + col, y + row, fg);
          } else {
            lcd.fillRect(cx + static_cast<int>(col) * scale, y + static_cast<int>(row) * scale, scale, scale, fg);
          }
        }
      }
    }

    cx += cell_w;
  }
}

void drawTextTransparent(M5Display& lcd, int x, int y, const char* text, uint16_t fg, uint8_t scale) {
  if (!text || !*text) {
    return;
  }

  const int cell_w = static_cast<int>(kCellWidth) * static_cast<int>(scale);

  int cx = x;
  for (const char* p = text; *p; ++p) {
    uint8_t ch = normalizeChar(*p);
    uint16_t glyph_index = static_cast<uint16_t>(ch - kFirst);
    uint32_t off = static_cast<uint32_t>(glyph_index) * static_cast<uint32_t>(kBytesPerGlyph);

    for (uint8_t row = 0; row < kCellHeight; ++row) {
      for (uint8_t col = 0; col < kCellWidth; ++col) {
        uint32_t byte_idx = off + static_cast<uint32_t>(row) * static_cast<uint32_t>(kBytesPerRow) + (col >> 3);
        uint8_t bit = static_cast<uint8_t>(0x80 >> (col & 7));
        if (kBitmap[byte_idx] & bit) {
          if (scale == 1) {
            lcd.drawPixel(cx + col, y + row, fg);
          } else {
            lcd.fillRect(cx + static_cast<int>(col) * scale, y + static_cast<int>(row) * scale, scale, scale, fg);
          }
        }
      }
    }

    cx += cell_w;
  }
}

}  // namespace proto_nerd_value_font