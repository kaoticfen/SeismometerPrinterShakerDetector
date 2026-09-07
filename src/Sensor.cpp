#include "Sensor.h"

#include <Wire.h>
#include <math.h>

#include "Filters.h"

namespace Sensor {
namespace {

// MPU6050 registers
constexpr uint8_t MPU_ADDR         = 0x68;  // AD0 tied low
constexpr uint8_t REG_SMPLRT_DIV   = 0x19;
constexpr uint8_t REG_CONFIG       = 0x1A;
constexpr uint8_t REG_ACCEL_CONFIG = 0x1C;
constexpr uint8_t REG_ACCEL_XOUT_H = 0x3B;
constexpr uint8_t REG_PWR_MGMT_1   = 0x6B;
constexpr uint8_t REG_WHO_AM_I     = 0x75;

// Static storage, deliberately not heap. See RingBuffer.h.
RingBuffer<BUFFER_SAMPLES> g_vertical;
RingBuffer<BUFFER_SAMPLES> g_horizontal;

// A mutex, not a portMUX critical section. Snapshotting copies up to 6000
// samples, and taskENTER_CRITICAL disables interrupts for the duration -- doing
// that for a copy this long upsets the WiFi and Bluetooth stacks. A mutex only
// blocks the sampling task, which can afford to wait one period.
SemaphoreHandle_t g_lock = nullptr;

class Lock {
 public:
  Lock() { if (g_lock) xSemaphoreTake(g_lock, portMAX_DELAY); }
  ~Lock() { if (g_lock) xSemaphoreGive(g_lock); }
};

HighPass g_hpX, g_hpY, g_hpZ;

volatile bool     g_healthy = false;
volatile float    g_latestMg = 0.0f;
volatile uint32_t g_samples = 0;
volatile uint32_t g_errors = 0;

void writeReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

bool readAccel(int16_t& ax, int16_t& ay, int16_t& az) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(REG_ACCEL_XOUT_H);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)MPU_ADDR, 6, (int)true) != 6) return false;

  ax = (int16_t)((Wire.read() << 8) | Wire.read());
  ay = (int16_t)((Wire.read() << 8) | Wire.read());
  az = (int16_t)((Wire.read() << 8) | Wire.read());
  return true;
}

void samplingTask(void*) {
  const TickType_t period = pdMS_TO_TICKS(1000 / SAMPLE_RATE_HZ);
  TickType_t last = xTaskGetTickCount();

  for (;;) {
    int16_t ax, ay, az;
    if (readAccel(ax, ay, az)) {
      // High-pass in counts. The filter is linear so scaling to mg can wait
      // until display, which keeps the ring buffers at int16.
      const float zh = g_hpZ.process((float)az);
      const float xh = g_hpX.process((float)ax);
      const float yh = g_hpY.process((float)ay);
      const float hmag = sqrtf(xh * xh + yh * yh);

      // Clamp before narrowing: a hard knock can push the high-passed value
      // past int16 range, and letting that wrap would print a full-scale spike
      // in the wrong direction.
      const int32_t zi = (int32_t)lroundf(constrain(zh, -32767.0f, 32767.0f));
      const int32_t hi = (int32_t)lroundf(constrain(hmag, 0.0f, 32767.0f));

      {
        Lock l;
        g_vertical.push((int16_t)zi);
        g_horizontal.push((int16_t)hi);
      }

      g_latestMg = (float)zi * MPU_MG_PER_LSB;
      g_samples++;
    } else {
      g_errors++;
    }

    // vTaskDelayUntil rather than vTaskDelay: this compensates for the time the
    // I2C read itself took, so the sample clock does not drift. That matters --
    // the printed time axis is derived from SAMPLE_RATE_HZ, so drift here shows
    // up as second labels that disagree with a stopwatch.
    vTaskDelayUntil(&last, period);
  }
}

}  // namespace

bool begin() {
  g_lock = xSemaphoreCreateMutex();
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(REG_WHO_AM_I);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)MPU_ADDR, 1, (int)true) != 1) return false;
  const uint8_t who = Wire.read();
  // Genuine MPU6050 reports 0x68. Clones commonly report 0x70, 0x72 or 0x98 and
  // are otherwise register-compatible, so accept anything that answered.
  Serial.printf("[sensor] WHO_AM_I = 0x%02X\n", who);

  writeReg(REG_PWR_MGMT_1, 0x80);  // device reset
  delay(100);
  writeReg(REG_PWR_MGMT_1, 0x01);  // wake, clock from X gyro PLL (stabler than
                                   // the internal 8MHz oscillator)
  delay(10);

  // DLPF_CFG=4 -> accel bandwidth 21Hz, delay 8.5ms. Well above the 0.5-15Hz
  // band that footsteps and leg-shake occupy, and it keeps the 1kHz internal
  // rate from aliasing into our 200Hz sampling.
  writeReg(REG_CONFIG, 0x04);

  // With DLPF enabled the gyro output rate is 1kHz, so DIV=4 gives 200Hz.
  writeReg(REG_SMPLRT_DIV, (1000 / SAMPLE_RATE_HZ) - 1);

  writeReg(REG_ACCEL_CONFIG, 0x00);  // +/-2g, finest resolution

  g_hpX.begin(HPF_CORNER_HZ, SAMPLE_RATE_HZ);
  g_hpY.begin(HPF_CORNER_HZ, SAMPLE_RATE_HZ);
  g_hpZ.begin(HPF_CORNER_HZ, SAMPLE_RATE_HZ);

  g_healthy = true;
  return true;
}

void startTask() {
  // Core 1. The Arduino loop, WiFi and Bluetooth all live on core 0, and
  // leaving them there is what keeps the sample clock clean.
  xTaskCreatePinnedToCore(samplingTask, "sampler", 4096, nullptr, 3, nullptr, 1);
}

bool healthy() { return g_healthy; }

float latestVerticalMg() { return g_latestMg; }

int snapshotVertical(int n, int16_t* out) {
  Lock l;
  return g_vertical.copyNewest(n, out);
}

int snapshotHorizontal(int n, int16_t* out) {
  Lock l;
  return g_horizontal.copyNewest(n, out);
}

int snapshotBins(int nSamples, int samplesPerBin, Bin* outBins, int maxBins,
                 Stats* outStats) {
  if (samplesPerBin < 1) samplesPerBin = 1;

  Lock l;

  if (nSamples > g_vertical.size()) nSamples = g_vertical.size();
  int bins = nSamples / samplesPerBin;
  if (bins > maxBins) bins = maxBins;
  // Only the samples that actually land in a bin are considered, so the stats
  // describe exactly the span that gets printed.
  const int used = bins * samplesPerBin;

  int32_t peak = 0, hpeak = 0;
  double sumSq = 0.0;

  for (int b = 0; b < bins; b++) {
    int16_t lo = INT16_MAX, hi = INT16_MIN;
    for (int i = 0; i < samplesPerBin; i++) {
      // age counts back from the newest sample; bin 0 is the oldest printed.
      const int age = used - 1 - (b * samplesPerBin + i);
      const int16_t v = g_vertical.recent(age);
      if (v < lo) lo = v;
      if (v > hi) hi = v;

      const int32_t a = abs((int32_t)v);
      if (a > peak) peak = a;
      sumSq += (double)v * (double)v;

      const int32_t h = abs((int32_t)g_horizontal.recent(age));
      if (h > hpeak) hpeak = h;
    }
    outBins[b].min = lo;
    outBins[b].max = hi;
  }

  if (outStats) {
    outStats->samples = used;
    outStats->seconds = (float)used / (float)SAMPLE_RATE_HZ;
    outStats->peakMg = (float)peak * MPU_MG_PER_LSB;
    outStats->horizPeakMg = (float)hpeak * MPU_MG_PER_LSB;
    outStats->rmsMg = used > 0
                          ? (float)sqrt(sumSq / (double)used) * MPU_MG_PER_LSB
                          : 0.0f;
    outStats->dominantHz = 0.0f;  // filled in by the caller, needs the FFT
  }
  return bins;
}

int available() {
  Lock l;
  return g_vertical.size();
}

uint32_t sampleCount() { return g_samples; }
uint32_t errorCount() { return g_errors; }

}  // namespace Sensor
