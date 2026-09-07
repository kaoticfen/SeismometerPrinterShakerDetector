#pragma once
#include <math.h>

// One-pole high-pass. Removes the 1g gravity bias and the slow thermal drift of
// the MPU6050, leaving only the AC vibration we want to plot.
//
// Difference equation:  y[n] = a * (y[n-1] + x[n] - x[n-1])
// with a = RC / (RC + dt), RC = 1 / (2*pi*fc).
//
// Note this settles rather than jumping: after a step (say you rotate the board)
// the output decays toward zero with time constant RC, which for fc = 0.5Hz is
// about 0.3s. That decay is visible on the trace and is expected, not a bug.
class HighPass {
 public:
  void begin(float cornerHz, float sampleRateHz) {
    const float rc = 1.0f / (2.0f * (float)M_PI * cornerHz);
    const float dt = 1.0f / sampleRateHz;
    a_ = rc / (rc + dt);
    reset();
  }

  void reset() {
    prevIn_ = 0.0f;
    prevOut_ = 0.0f;
    primed_ = false;
  }

  float process(float x) {
    if (!primed_) {
      // Seed from the first sample so we don't emit one huge transient equal to
      // the full 1g gravity vector on the first call.
      prevIn_ = x;
      prevOut_ = 0.0f;
      primed_ = true;
      return 0.0f;
    }
    const float y = a_ * (prevOut_ + x - prevIn_);
    prevIn_ = x;
    prevOut_ = y;
    return y;
  }

 private:
  float a_ = 0.99f;
  float prevIn_ = 0.0f;
  float prevOut_ = 0.0f;
  bool primed_ = false;
};
