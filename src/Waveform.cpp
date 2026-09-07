#include "Waveform.h"

#include <arduinoFFT.h>
#include <math.h>

#include "config.h"

namespace Waveform {
namespace {
constexpr int FFT_N = 256;

// Static so we never ask the allocator for 4KB while WiFi and BT are running.
// dominantFrequency() is only ever called from the print/stats path, which is
// single-threaded, so sharing these across calls is safe.
double g_re[FFT_N];
double g_im[FFT_N];
}  // namespace

int decimate(const int16_t* in, int n, int samplesPerBin, Bin* out, int maxBins) {
  if (samplesPerBin < 1) samplesPerBin = 1;
  int bins = n / samplesPerBin;
  if (bins > maxBins) bins = maxBins;

  for (int b = 0; b < bins; b++) {
    const int start = b * samplesPerBin;
    int16_t lo = in[start];
    int16_t hi = in[start];
    for (int i = 1; i < samplesPerBin; i++) {
      const int16_t v = in[start + i];
      if (v < lo) lo = v;
      if (v > hi) hi = v;
    }
    out[b].min = lo;
    out[b].max = hi;
  }
  return bins;
}

Stats analyse(const int16_t* vertical, const int16_t* horizontal, int n) {
  Stats s;
  s.samples = n;
  s.seconds = (float)n / (float)SAMPLE_RATE_HZ;
  if (n <= 0) return s;

  int32_t peak = 0;
  int32_t hpeak = 0;
  double sumSq = 0.0;

  for (int i = 0; i < n; i++) {
    const int32_t v = abs((int32_t)vertical[i]);
    if (v > peak) peak = v;
    sumSq += (double)vertical[i] * (double)vertical[i];

    const int32_t h = abs((int32_t)horizontal[i]);
    if (h > hpeak) hpeak = h;
  }

  s.peakMg = (float)peak * MPU_MG_PER_LSB;
  s.horizPeakMg = (float)hpeak * MPU_MG_PER_LSB;
  s.rmsMg = (float)sqrt(sumSq / (double)n) * MPU_MG_PER_LSB;
  return s;
}

float dominantFrequency(const int16_t* in, int n, float sampleRateHz) {
  if (n < FFT_N) return 0.0f;

  // Newest FFT_N samples.
  const int start = n - FFT_N;
  double mean = 0.0;
  for (int i = 0; i < FFT_N; i++) mean += in[start + i];
  mean /= FFT_N;

  for (int i = 0; i < FFT_N; i++) {
    // Remove the residual mean so the DC bin doesn't dominate the search.
    g_re[i] = (double)in[start + i] - mean;
    g_im[i] = 0.0;
  }

  ArduinoFFT<double> fft(g_re, g_im, FFT_N, sampleRateHz);
  fft.windowing(FFTWindow::Hamming, FFTDirection::Forward);
  fft.compute(FFTDirection::Forward);
  fft.complexToMagnitude();

  // Search from bin 2 up. Bins 0 and 1 hold DC and the residue of the 0.5Hz
  // high-pass corner, which would otherwise always win.
  int bestBin = 0;
  double bestMag = 0.0;
  for (int i = 2; i < FFT_N / 2; i++) {
    if (g_re[i] > bestMag) {
      bestMag = g_re[i];
      bestBin = i;
    }
  }
  if (bestBin == 0) return 0.0f;

  // Parabolic interpolation across the peak and its neighbours, which recovers
  // sub-bin resolution. Raw bins are 0.78Hz apart at 200Hz/256, too coarse to
  // distinguish a 2.0Hz walking cadence from 2.4Hz without this.
  const double y1 = g_re[bestBin - 1];
  const double y2 = g_re[bestBin];
  const double y3 = g_re[bestBin + 1];
  const double denom = y1 - 2.0 * y2 + y3;
  double delta = 0.0;
  if (fabs(denom) > 1e-9) delta = 0.5 * (y1 - y3) / denom;
  if (delta < -1.0 || delta > 1.0) delta = 0.0;

  return (float)((bestBin + delta) * sampleRateHz / FFT_N);
}

}  // namespace Waveform
