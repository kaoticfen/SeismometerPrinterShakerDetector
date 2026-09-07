// ESP32 seismograph -> Netum NT-1809 thermal printer.
//
// Samples an MPU6050 at 200Hz on core 1, keeps a 30s rolling window, and prints
// it as a seismogram on a button press. A SoftAP web view shows the live trace
// so the gain can be set before spending paper.
//
// Controls:
//   short press  print the last WINDOW_SECONDS
//   long press   print a test page (exercises the link without the sensor)
//   pot          graph full scale, log-mapped 5mg..500mg
//   LED          lit while the signal exceeds half full scale

#include <Arduino.h>
#include <math.h>

#include "Bitmap.h"
#include "BtLink.h"
#include "Printer.h"
#include "Sensor.h"
#include "Waveform.h"
#include "WebUI.h"
#include "config.h"

namespace {

Bin g_bins[Bitmap::MAX_ROWS];
int16_t g_fftBuf[256];

uint32_t g_eventCount = 0;
float g_fullScaleMg = 100.0f;

// Set from the web handler, serviced in loop(). Printing takes several seconds
// and must not run inside the HTTP task -- that would stall the web server and
// trip the client's timeout mid-print.
volatile bool g_printPending = false;

// ---------------------------------------------------------------------------
// Pot -> full scale
// ---------------------------------------------------------------------------
float readFullScaleMg() {
  // Average a few reads; the ESP32 SAR ADC is noisy enough that a single
  // sample makes the displayed full scale jitter by several mg.
  uint32_t acc = 0;
  for (int i = 0; i < 8; i++) acc += analogRead(PIN_POT);
  const float norm = (float)acc / 8.0f / 4095.0f;

  // Log mapping: linear would spend most of the pot's travel in the range
  // above 250mg, which nothing in an office ever reaches.
  const float lo = logf(FULLSCALE_MIN_MG);
  const float hi = logf(FULLSCALE_MAX_MG);
  return expf(lo + norm * (hi - lo));
}

int32_t fullScaleCounts() {
  return (int32_t)(g_fullScaleMg / MPU_MG_PER_LSB);
}

// ---------------------------------------------------------------------------
// Printing
// ---------------------------------------------------------------------------
bool doPrintWindow() {
  if (!BtLink::connected() && !BtLink::connect()) {
    Serial.println("[print] printer not connected");
    return false;
  }

  const int wantSamples = WINDOW_SECONDS * SAMPLE_RATE_HZ;
  Stats st;
  const int bins = Sensor::snapshotBins(wantSamples, SAMPLES_PER_ROW, g_bins,
                                        Bitmap::MAX_ROWS, &st);
  if (bins <= 0) {
    Serial.println("[print] nothing buffered yet");
    return false;
  }

  const int got = Sensor::snapshotVertical(256, g_fftBuf);
  st.dominantHz =
      Waveform::dominantFrequency(g_fftBuf, got, (float)SAMPLE_RATE_HZ);

  g_eventCount++;
  Serial.printf("[print] %d bins, peak %.1f mg, rms %.2f mg, %.2f Hz\n", bins,
                st.peakMg, st.rmsMg, st.dominantHz);

  const bool ok = Printer::printSeismogram(g_bins, bins, fullScaleCounts(), st,
                                           g_eventCount);
  if (!ok) Serial.println("[print] link dropped mid-image");
  return ok;
}

bool requestPrintFromWeb() {
  if (g_printPending) return false;
  if (!BtLink::connected()) return false;
  g_printPending = true;
  return true;
}

// ---------------------------------------------------------------------------
// Button: short press prints, long press runs the test page
// ---------------------------------------------------------------------------
class Button {
 public:
  void begin() { pinMode(PIN_BUTTON, INPUT_PULLUP); }

  // Returns 1 for a short press, 2 for a long one, 0 otherwise.
  int poll() {
    const bool down = digitalRead(PIN_BUTTON) == LOW;
    const uint32_t now = millis();
    int event = 0;

    if (down != raw_) {
      raw_ = down;
      changedAt_ = now;
    } else if (now - changedAt_ >= BUTTON_DEBOUNCE_MS && down != stable_) {
      stable_ = down;
      if (down) {
        pressedAt_ = now;
        longFired_ = false;
      } else if (!longFired_) {
        event = 1;  // released before the long-press threshold
      }
    }

    // Fire the long press while still held, so it is obvious it registered
    // rather than leaving you wondering how long to hold.
    if (stable_ && !longFired_ && now - pressedAt_ >= BUTTON_LONGPRESS_MS) {
      longFired_ = true;
      event = 2;
    }
    return event;
  }

 private:
  bool raw_ = false, stable_ = false, longFired_ = false;
  uint32_t changedAt_ = 0, pressedAt_ = 0;
};

Button g_button;

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n[boot] seismograph starting");

  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);
  g_button.begin();

  analogReadResolution(12);
  // 11dB attenuation gives the full 0-3.3V span, which is what the pot swings.
  analogSetPinAttenuation(PIN_POT, ADC_11db);

  if (!Sensor::begin()) {
    Serial.println("[boot] MPU6050 not responding -- check SDA/SCL and AD0");
    // Keep going: the web UI and the printer test page are still useful for
    // diagnosing, and they are how you would confirm the rest of the rig works.
  }
  Sensor::startTask();

  WebUI::begin(requestPrintFromWeb);
  BtLink::begin();

  Serial.printf("[boot] ready. AP \"%s\", link %s\n", AP_SSID,
                BtLink::statusText());
}

void loop() {
  BtLink::poll();
  WebUI::poll();

  // Pot, sampled at a leisurely rate.
  static uint32_t lastPot = 0;
  if (millis() - lastPot > 100) {
    lastPot = millis();
    // Light smoothing on top of the averaging in readFullScaleMg, so the
    // displayed value settles instead of flickering on the last digit.
    g_fullScaleMg += 0.25f * (readFullScaleMg() - g_fullScaleMg);
    WebUI::setFullScaleMg(g_fullScaleMg);
  }

  // LED follows the signal against the current full scale.
  const float mg = fabsf(Sensor::latestVerticalMg());
  digitalWrite(PIN_LED,
               mg > g_fullScaleMg * LED_TRIGGER_FRACTION ? HIGH : LOW);

  switch (g_button.poll()) {
    case 1:
      Serial.println("[btn] short press -> print window");
      doPrintWindow();
      break;
    case 2:
      Serial.println("[btn] long press -> test page");
      if (BtLink::connected() || BtLink::connect()) {
        Printer::printTestPage();
      } else {
        Serial.println("[btn] printer not connected");
      }
      break;
    default:
      break;
  }

  if (g_printPending) {
    g_printPending = false;
    doPrintWindow();
  }

  static uint32_t lastLog = 0;
  if (millis() - lastLog > 5000) {
    lastLog = millis();
    Serial.printf("[stat] %us buffered, %.1f mg fs, link=%s, clients=%d, "
                  "heap=%u, errs=%lu\n",
                  Sensor::available() / SAMPLE_RATE_HZ, g_fullScaleMg,
                  BtLink::connected() ? "up" : "down", WebUI::clientCount(),
                  (unsigned)ESP.getFreeHeap(),
                  (unsigned long)Sensor::errorCount());
  }

  delay(2);
}
