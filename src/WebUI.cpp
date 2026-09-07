#include "WebUI.h"

#include <WebServer.h>
#include <WiFi.h>

#include "Sensor.h"
#include "Waveform.h"
#include "config.h"
#include "web_index.h"

namespace WebUI {
namespace {

WebServer g_server(80);
bool (*g_requestPrint)() = nullptr;
float g_fullScaleMg = 100.0f;

// Live view bins. 240 is what the page asks for; sized once, statically.
constexpr int MAX_LIVE_BINS = 256;
Bin g_bins[MAX_LIVE_BINS];

// FFT window for the live readout.
int16_t g_fftBuf[256];

void handleRoot() {
  g_server.sendHeader("Cache-Control", "max-age=3600");
  g_server.send_P(200, "text/html", INDEX_HTML);
}

void handleWave() {
  int n = g_server.hasArg("n") ? g_server.arg("n").toInt() : 240;
  if (n < 16) n = 16;
  if (n > MAX_LIVE_BINS) n = MAX_LIVE_BINS;

  // Show the most recent few seconds rather than the whole 30s window -- at
  // 240 bins across a phone screen, a full window would bin 25 samples each
  // and turn footsteps into mush.
  constexpr int LIVE_SECONDS = 6;
  const int wantSamples = LIVE_SECONDS * SAMPLE_RATE_HZ;
  int perBin = wantSamples / n;
  if (perBin < 1) perBin = 1;

  Stats st;
  const int bins = Sensor::snapshotBins(wantSamples, perBin, g_bins, n, &st);

  const int got = Sensor::snapshotVertical(256, g_fftBuf);
  const float domHz =
      Waveform::dominantFrequency(g_fftBuf, got, (float)SAMPLE_RATE_HZ);

  // Hand-rolled JSON: ArduinoJson would need a buffer this path can't spare.
  String out;
  out.reserve(bins * 14 + 200);
  out += "{\"fullScaleMg\":";
  out += String(g_fullScaleMg, 1);
  out += ",\"peakMg\":";
  out += String(st.peakMg, 2);
  out += ",\"rmsMg\":";
  out += String(st.rmsMg, 3);
  out += ",\"domHz\":";
  out += String(domHz, 2);
  out += ",\"bins\":[";
  for (int i = 0; i < bins; i++) {
    if (i) out += ',';
    out += '[';
    out += String(Sensor::countsToMg(g_bins[i].min), 1);
    out += ',';
    out += String(Sensor::countsToMg(g_bins[i].max), 1);
    out += ']';
  }
  out += "]}";

  g_server.send(200, "application/json", out);
}

void handlePrint() {
  if (!g_requestPrint) {
    g_server.send(500, "application/json",
                  "{\"ok\":false,\"error\":\"no handler\"}");
    return;
  }
  const bool ok = g_requestPrint();
  String out = "{\"ok\":";
  out += ok ? "true" : "false";
  out += ",\"seconds\":";
  out += String((float)Sensor::available() / SAMPLE_RATE_HZ, 1);
  if (!ok) out += ",\"error\":\"printer not connected\"";
  out += "}";
  g_server.send(ok ? 200 : 503, "application/json", out);
}

}  // namespace

void begin(bool (*requestPrint)()) {
  g_requestPrint = requestPrint;

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.printf("[web] AP \"%s\" at %s\n", AP_SSID,
                WiFi.softAPIP().toString().c_str());

  g_server.on("/", handleRoot);
  g_server.on("/api/wave", handleWave);
  g_server.on("/api/print", HTTP_POST, handlePrint);
  g_server.begin();
}

void poll() { g_server.handleClient(); }

void setFullScaleMg(float mg) { g_fullScaleMg = mg; }

int clientCount() { return WiFi.softAPgetStationNum(); }

}  // namespace WebUI
