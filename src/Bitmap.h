#pragma once
#include <Arduino.h>

#include "Waveform.h"
#include "config.h"

// Monochrome raster image, 384 dots wide, in the bit order ESC/POS GS v 0
// expects: one row after another, 48 bytes per row, MSB is the leftmost dot,
// and a set bit means a black dot.
//
// The whole graph is rendered into one buffer rather than streamed row by row
// because the second labels are 7 rows tall and would otherwise straddle a
// chunk boundary. At 30s and 20 rows/s that is 600 rows -- 28.8KB.
//
// That buffer is heap-allocated for the duration of a print rather than held
// static, and the ESP32 has only just enough DRAM for that to matter: with the
// WiFi AP and Bluedroid both up there is ~23KB of heap left, and opening an
// RFCOMM channel needs more than that. Bluedroid does not fail such an
// allocation cleanly either -- it asserts in vQueueDelete on its own cleanup
// path. Both print paths open the link before calling begin(), so the image is
// allocated only once the connection is established and the stack has taken
// what it needs.
namespace Bitmap {

constexpr int MAX_ROWS = WINDOW_SECONDS * PRINT_ROWS_PER_SEC;  // 600

// Allocates and clears the image. Returns false if the heap could not provide
// it, in which case nothing else here draws anything. Call end() when done.
bool begin(int rows);

// Releases the image. Safe to call when nothing is allocated.
void end();

int rows();
const uint8_t* data();
size_t sizeBytes();

// A set bit is a black dot. Out-of-range coordinates are ignored.
void setPixel(int x, int y);
void hSpan(int y, int x0, int x1);              // inclusive
void vSpan(int x, int y0, int y1);              // inclusive
void hDotted(int y, int x0, int x1, int step);
void vDotted(int x, int y0, int y1, int step);

// 5x7 digits, '0'-'9', plus '.', 's', '-' and ' '. Anything else prints blank.
// Returns the x just past the string, so calls can be chained.
int drawText(int x, int y, const char* s);
int textWidth(const char* s);

// Renders the decimated trace: for each bin a vertical run from its min to its
// max, deflected either side of the centre line. `fullScaleCounts` maps to the
// half-width of the graph area.
//
// Bins that exceed full scale are clipped to the graph edge and flagged with a
// marker in the margin, so a clipped trace is visibly different from one that
// merely happens to touch the edge.
void drawTrace(const Bin* bins, int n, int32_t fullScaleCounts);

// Centre line, half-scale gridlines, per-second ticks and 5s labels.
void drawGrid(int rows, int rowsPerSec);

}  // namespace Bitmap
