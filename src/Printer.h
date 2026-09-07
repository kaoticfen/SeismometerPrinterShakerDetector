#pragma once
#include <Arduino.h>

#include "Waveform.h"

// ESC/POS driver for the Netum NT-1809 (58mm, 203dpi, 384 dots wide).
namespace Printer {

void init();          // ESC @ plus the density/heat settings
void text(const char* s);
void textln(const char* s);
void feed(int lines);
void setBold(bool on);
void setCenter(bool on);
void hr();            // a full-width rule of dashes

// Sends the Bitmap module's current image as chunked GS v 0 rasters.
// Returns false if the link dropped mid-image.
bool printBitmap();

// A full seismogram: header, graph, stats footer.
bool printSeismogram(const Bin* bins, int nBins, int32_t fullScaleCounts,
                     const Stats& stats, uint32_t eventNumber);

// Text plus a raster test pattern. Run this first -- it exercises the entire
// ESC/POS path without involving the sensor, so a failure here is unambiguously
// the link or the printer rather than the DSP.
bool printTestPage();

}  // namespace Printer
