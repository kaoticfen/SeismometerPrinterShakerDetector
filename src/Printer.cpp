#include "Printer.h"

#include "Bitmap.h"
#include "BtLink.h"
#include "config.h"

namespace Printer {
namespace {

bool send(const uint8_t* d, size_t n) { return BtLink::write(d, n); }

bool sendStr(const char* s) {
  return BtLink::write((const uint8_t*)s, strlen(s));
}

}  // namespace

void init() {
  const uint8_t reset[] = {0x1B, 0x40};  // ESC @
  send(reset, sizeof(reset));
  delay(50);

  // ESC 7 n1 n2 n3 -- heating dots, heat time, heat interval.
  // Defaults on these units are tuned for text; a solid raster needs a longer
  // heat time or the graph comes out grey and patchy. These values are the
  // usual safe compromise: dark enough to read, not so hot that the head
  // scorches or the print slows to a crawl.
  const uint8_t heat[] = {0x1B, 0x37, 7, 0x50, 0x02};
  send(heat, sizeof(heat));
  delay(20);

  const uint8_t align[] = {0x1B, 0x61, 0};  // left align
  send(align, sizeof(align));
}

void text(const char* s) { sendStr(s); }

void textln(const char* s) {
  sendStr(s);
  sendStr("\n");
}

void feed(int lines) {
  if (lines <= 0) return;
  const uint8_t cmd[] = {0x1B, 0x64, (uint8_t)lines};  // ESC d n
  send(cmd, sizeof(cmd));
}

void setBold(bool on) {
  const uint8_t cmd[] = {0x1B, 0x45, (uint8_t)(on ? 1 : 0)};
  send(cmd, sizeof(cmd));
}

void setCenter(bool on) {
  const uint8_t cmd[] = {0x1B, 0x61, (uint8_t)(on ? 1 : 0)};
  send(cmd, sizeof(cmd));
}

void hr() { textln("--------------------------------"); }

bool printBitmap() {
  const int totalRows = Bitmap::rows();
  const uint8_t* img = Bitmap::data();
  if (totalRows <= 0) return true;

  int row = 0;
  while (row < totalRows) {
    int chunk = totalRows - row;
    if (chunk > RASTER_CHUNK_ROWS) chunk = RASTER_CHUNK_ROWS;

    // GS v 0 m xL xH yL yH
    //   m  = 0 (normal density)
    //   xL/xH = bytes per row  (48)
    //   yL/yH = rows in this chunk
    const uint8_t header[] = {
        0x1D, 0x76, 0x30, 0x00,
        (uint8_t)(PRINT_WIDTH_BYTES & 0xFF), (uint8_t)(PRINT_WIDTH_BYTES >> 8),
        (uint8_t)(chunk & 0xFF), (uint8_t)(chunk >> 8),
    };
    if (!send(header, sizeof(header))) return false;
    if (!send(img + (size_t)row * PRINT_WIDTH_BYTES,
              (size_t)chunk * PRINT_WIDTH_BYTES)) {
      return false;
    }

    // The printer's input buffer is only a few KB and neither SPP nor BLE gives
    // us real flow control, so pace by hand. Without this the image arrives
    // faster than the head can print it, the buffer overruns, and rows are
    // dropped -- which looks like a corrupted graph, not like a comms error.
    delay(RASTER_CHUNK_PAD_MS);
    row += chunk;
  }
  return true;
}

bool printSeismogram(const Bin* bins, int nBins, int32_t fullScaleCounts,
                     const Stats& stats, uint32_t eventNumber) {
  init();

  setCenter(true);
  setBold(true);
  textln("SEISMOGRAPH");
  setBold(false);
  setCenter(false);
  hr();

  char line[64];
  const uint32_t t = millis() / 1000;
  snprintf(line, sizeof(line), "record #%lu  t+%luh%02lum%02lus",
           (unsigned long)eventNumber, (unsigned long)(t / 3600),
           (unsigned long)((t / 60) % 60), (unsigned long)(t % 60));
  textln(line);

  snprintf(line, sizeof(line), "window   %.1f s", stats.seconds);
  textln(line);
  snprintf(line, sizeof(line), "fullscale+/-%.1f mg",
           (double)fullScaleCounts * MPU_MG_PER_LSB);
  textln(line);
  // The left-margin labels count 0..N down the page, which is ambiguous
  // without this: the top of the graph is the oldest sample, not "now".
  textln("time flows down, oldest top");
  hr();

  // Graph. One printed row per bin.
  if (!Bitmap::begin(nBins)) {
    textln("(out of memory for graph)");
    feed(3);
    return false;
  }
  Bitmap::drawGrid(nBins, PRINT_ROWS_PER_SEC);
  Bitmap::drawTrace(bins, nBins, fullScaleCounts);
  const bool imageOk = printBitmap();
  Bitmap::end();
  if (!imageOk) return false;

  hr();
  snprintf(line, sizeof(line), "peak     %.1f mg", stats.peakMg);
  textln(line);
  snprintf(line, sizeof(line), "rms      %.2f mg", stats.rmsMg);
  textln(line);
  snprintf(line, sizeof(line), "horiz pk %.1f mg", stats.horizPeakMg);
  textln(line);
  if (stats.dominantHz > 0.0f) {
    snprintf(line, sizeof(line), "dom freq %.2f Hz", stats.dominantHz);
    textln(line);
  }
  hr();

  feed(3);
  return true;
}

bool printTestPage() {
  init();

  setCenter(true);
  setBold(true);
  textln("SEISMO TEST PAGE");
  setBold(false);
  setCenter(false);
  hr();
  textln("If you can read this, the");
  textln("Bluetooth link and ESC/POS");
  textln("text path are both working.");
  hr();
  textln("Raster test below. Expect a");
  textln("clean wedge with sharp edges");
  textln("and a dotted centre line.");
  feed(1);

  // A wedge, plus the grid the real graph uses. Ragged or missing rows here
  // mean the chunk pacing needs to be slower (raise RASTER_CHUNK_PAD_MS);
  // a grey or patchy fill means the heat settings need raising.
  const int rows = 120;
  if (!Bitmap::begin(rows)) {
    textln("(out of memory for raster)");
    feed(3);
    return false;
  }
  Bitmap::drawGrid(rows, PRINT_ROWS_PER_SEC);
  const int halfWidth = (GRAPH_RIGHT - GRAPH_LEFT) / 2;
  for (int y = 0; y < rows; y++) {
    const int w = halfWidth * y / rows;
    Bitmap::hSpan(y, GRAPH_CENTER - w, GRAPH_CENTER + w);
  }
  const bool imageOk = printBitmap();
  Bitmap::end();
  if (!imageOk) return false;

  hr();
  textln("width  384 dots / 48 bytes");
  char line[64];
  snprintf(line, sizeof(line), "chunk  %d rows, %d ms pad",
           RASTER_CHUNK_ROWS, RASTER_CHUNK_PAD_MS);
  textln(line);
  snprintf(line, sizeof(line), "link   %s", BtLink::statusText());
  textln(line);
  feed(3);
  return true;
}

}  // namespace Printer
