#include "Bitmap.h"

#include <string.h>

namespace Bitmap {
namespace {

uint8_t g_img[MAX_ROWS * PRINT_WIDTH_BYTES];
int g_rows = 0;

// 5x7 font, column-major: each byte is one column, bit 0 is the top row.
// Only the glyphs the labels need.
struct Glyph {
  char c;
  uint8_t col[5];
};

const Glyph FONT[] = {
    {'0', {0x3E, 0x51, 0x49, 0x45, 0x3E}},
    {'1', {0x00, 0x42, 0x7F, 0x40, 0x00}},
    {'2', {0x42, 0x61, 0x51, 0x49, 0x46}},
    {'3', {0x21, 0x41, 0x45, 0x4B, 0x31}},
    {'4', {0x18, 0x14, 0x12, 0x7F, 0x10}},
    {'5', {0x27, 0x45, 0x45, 0x45, 0x39}},
    {'6', {0x3C, 0x4A, 0x49, 0x49, 0x30}},
    {'7', {0x01, 0x71, 0x09, 0x05, 0x03}},
    {'8', {0x36, 0x49, 0x49, 0x49, 0x36}},
    {'9', {0x06, 0x49, 0x49, 0x29, 0x1E}},
    {'.', {0x00, 0x60, 0x60, 0x00, 0x00}},
    {'-', {0x08, 0x08, 0x08, 0x08, 0x08}},
    {'s', {0x48, 0x54, 0x54, 0x54, 0x20}},
    {' ', {0x00, 0x00, 0x00, 0x00, 0x00}},
};
constexpr int FONT_COUNT = sizeof(FONT) / sizeof(FONT[0]);
constexpr int GLYPH_W = 5;
constexpr int GLYPH_H = 7;
constexpr int GLYPH_ADVANCE = GLYPH_W + 1;

const Glyph* findGlyph(char c) {
  for (int i = 0; i < FONT_COUNT; i++) {
    if (FONT[i].c == c) return &FONT[i];
  }
  return nullptr;
}

}  // namespace

void begin(int rows) {
  if (rows < 0) rows = 0;
  if (rows > MAX_ROWS) rows = MAX_ROWS;
  g_rows = rows;
  memset(g_img, 0, (size_t)rows * PRINT_WIDTH_BYTES);
}

int rows() { return g_rows; }
const uint8_t* data() { return g_img; }
size_t sizeBytes() { return (size_t)g_rows * PRINT_WIDTH_BYTES; }

void setPixel(int x, int y) {
  if (x < 0 || x >= PRINT_WIDTH_DOTS || y < 0 || y >= g_rows) return;
  // MSB-first within the byte: dot 0 is bit 7 of byte 0.
  g_img[(size_t)y * PRINT_WIDTH_BYTES + (x >> 3)] |= (uint8_t)(0x80 >> (x & 7));
}

void hSpan(int y, int x0, int x1) {
  if (x0 > x1) { const int t = x0; x0 = x1; x1 = t; }
  for (int x = x0; x <= x1; x++) setPixel(x, y);
}

void vSpan(int x, int y0, int y1) {
  if (y0 > y1) { const int t = y0; y0 = y1; y1 = t; }
  for (int y = y0; y <= y1; y++) setPixel(x, y);
}

void hDotted(int y, int x0, int x1, int step) {
  if (step < 1) step = 1;
  for (int x = x0; x <= x1; x += step) setPixel(x, y);
}

void vDotted(int x, int y0, int y1, int step) {
  if (step < 1) step = 1;
  for (int y = y0; y <= y1; y += step) setPixel(x, y);
}

int textWidth(const char* s) {
  return (int)strlen(s) * GLYPH_ADVANCE;
}

int drawText(int x, int y, const char* s) {
  for (const char* p = s; *p; p++) {
    const Glyph* g = findGlyph(*p);
    if (g) {
      for (int col = 0; col < GLYPH_W; col++) {
        const uint8_t bits = g->col[col];
        for (int row = 0; row < GLYPH_H; row++) {
          if (bits & (1 << row)) setPixel(x + col, y + row);
        }
      }
    }
    x += GLYPH_ADVANCE;
  }
  return x;
}

void drawTrace(const Bin* bins, int n, int32_t fullScaleCounts) {
  if (fullScaleCounts <= 0) fullScaleCounts = 1;
  const int halfWidth = (GRAPH_RIGHT - GRAPH_LEFT) / 2;

  for (int i = 0; i < n && i < g_rows; i++) {
    // Scale into dots either side of centre.
    int32_t lo = (int32_t)bins[i].min * halfWidth / fullScaleCounts;
    int32_t hi = (int32_t)bins[i].max * halfWidth / fullScaleCounts;

    const bool clipped = (lo < -halfWidth) || (hi > halfWidth);
    if (lo < -halfWidth) lo = -halfWidth;
    if (hi > halfWidth) hi = halfWidth;

    // Always draw at least one dot, so a quiet stretch reads as a continuous
    // baseline rather than a dashed one.
    int x0 = GRAPH_CENTER + (int)lo;
    int x1 = GRAPH_CENTER + (int)hi;
    if (x0 == x1) x1 = x0 + 1;
    hSpan(i, x0, x1);

    if (clipped) {
      // Marker in the left margin, at x=12-13 to clear the second labels
      // (which run x=0-10 for two digits) and the ticks (x=16-26). Without
      // this an over-range trace looks identical to one that merely reaches
      // the edge, and you would tune the pot the wrong way.
      setPixel(12, i);
      setPixel(13, i);
    }
  }
}

void drawGrid(int rows, int rowsPerSec) {
  if (rowsPerSec < 1) rowsPerSec = 1;
  const int halfWidth = (GRAPH_RIGHT - GRAPH_LEFT) / 2;

  // Centre line and the two half-scale gridlines, dotted so they read as
  // reference rather than as signal.
  for (int y = 0; y < rows; y += 3) {
    setPixel(GRAPH_CENTER, y);
  }
  for (int y = 0; y < rows; y += 6) {
    setPixel(GRAPH_CENTER - halfWidth / 2, y);
    setPixel(GRAPH_CENTER + halfWidth / 2, y);
  }

  // Per-second ticks in the left margin, longer and labelled every 5s.
  for (int sec = 0; sec * rowsPerSec < rows; sec++) {
    const int y = sec * rowsPerSec;
    if (sec % 5 == 0) {
      hSpan(y, 16, 26);
      char label[12];  // sized for any int, not just the 2 digits we expect
      snprintf(label, sizeof(label), "%d", sec);
      // Nudge the 7-row-tall label up so it centres on its tick, and keep it
      // inside the image at the very top.
      int ly = y - 3;
      if (ly < 0) ly = 0;
      if (ly + 7 > rows) ly = rows - 7;
      drawText(0, ly, label);
    } else {
      hSpan(y, 22, 26);
    }
  }
}

}  // namespace Bitmap
