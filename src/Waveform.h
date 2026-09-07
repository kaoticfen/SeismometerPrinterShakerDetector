#pragma once
#include <Arduino.h>

// One decimated time bin: the extremes of the samples that fell into it.
struct Bin {
  int16_t min;
  int16_t max;
};

struct Stats {
  float peakMg = 0.0f;      // largest absolute vertical excursion
  float rmsMg = 0.0f;       // RMS of the vertical trace
  float horizPeakMg = 0.0f; // largest horizontal magnitude
  float dominantHz = 0.0f;  // strongest spectral peak
  int   samples = 0;
  float seconds = 0.0f;
};

namespace Waveform {

// Min/max envelope decimation: each output bin spans `samplesPerBin` input
// samples and records their minimum and maximum.
//
// This is what seismograph drums and audio waveform views do, and it matters
// here. Averaging 10 samples would flatten a footstep -- a sharp bipolar
// impulse averages close to zero -- so peaks would silently vanish from the
// printout. Min/max preserves the full excursion of every bin.
//
// Returns the number of bins written.
int decimate(const int16_t* in, int n, int samplesPerBin, Bin* out, int maxBins);

// Peak, RMS and horizontal peak over the supplied traces.
Stats analyse(const int16_t* vertical, const int16_t* horizontal, int n);

// Strongest frequency component of `in`, via a 256-point FFT at the tail of the
// window. Returns 0 if there are too few samples or the spectrum is flat.
//
// Uses the newest 256 samples rather than the whole window because that is the
// part the user just caused by walking past; averaging a spectrum over 30s of
// mostly-silence buries the event.
float dominantFrequency(const int16_t* in, int n, float sampleRateHz);

}  // namespace Waveform
