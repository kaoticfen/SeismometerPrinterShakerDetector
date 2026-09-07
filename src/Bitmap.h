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
// chunk boundary. At 30s and 20 rows/s that is 600 rows -- 28.8KB, held static
// so it never competes with the radios for heap.
namespace Bitmap {

constexpr int MAX_ROWS = WINDOW_SECONDS * PRINT_ROWS_PER_SEC;  // 600

// Clears the image and sets its height. Rows beyond `rows` are not printed.
void begin(int rows);

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
