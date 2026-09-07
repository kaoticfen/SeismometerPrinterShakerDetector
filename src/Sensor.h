#pragma once
#include <Arduino.h>

#include "RingBuffer.h"
#include "Waveform.h"
#include "config.h"

// MPU6050 sampling at SAMPLE_RATE_HZ from a task pinned to core 1, so that WiFi
// and Bluetooth work on core 0 cannot add jitter to the sample clock.
//
// Two traces are kept:
//   vertical   - signed Z axis, high-passed. This is the seismogram.
//   horizontal - sqrt(x^2 + y^2) of the high-passed X/Y, unsigned. Feeds stats.
//
// Both are stored as int16 raw counts, not mg, so a 30s window costs 12KB each
// instead of 24KB as floats. Convert with countsToMg() at the point of display.
namespace Sensor {

bool begin();

// Starts the 200Hz sampling task on core 1. Call after begin().
void startTask();

// True once the MPU6050 has answered on I2C and the task is running.
bool healthy();

// Most recent vertical sample in mg, for the LED and the live view.
float latestVerticalMg();

// Snapshots the newest `n` samples of each trace in chronological order.
// Takes the buffer lock, so a print cannot tear across a ring wrap mid-copy.
// Returns the number of samples actually written.
//
// Intended for small n (the FFT window, the live view). The print path should
// use snapshotBins() instead -- copying a full 30s window through here would
// cost 12KB per trace.
int snapshotVertical(int n, int16_t* out);
int snapshotHorizontal(int n, int16_t* out);

// Decimates the newest `nSamples` into min/max bins and computes the stats in a
// single locked pass straight off the ring. This is what the print path wants:
// it avoids a 24KB intermediate copy of the raw window, which matters on a chip
// already hosting both radios.
//
// `outStats` may be null. Returns the number of bins written.
int snapshotBins(int nSamples, int samplesPerBin, Bin* outBins, int maxBins,
                 Stats* outStats);

// How many samples are currently buffered, capped at BUFFER_SAMPLES.
int available();

inline float countsToMg(int16_t counts) {
  return (float)counts * MPU_MG_PER_LSB;
}

// Total samples since boot, and samples dropped because an I2C read failed.
uint32_t sampleCount();
uint32_t errorCount();

}  // namespace Sensor
