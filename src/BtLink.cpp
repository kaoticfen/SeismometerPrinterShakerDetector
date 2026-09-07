#include "BtLink.h"

#include "config.h"

// These must be included at file scope, not inside namespace BtLink -- the
// library headers refer to std:: types, which would resolve as BtLink::std::.
#if PRINTER_USE_BLE
#include <NimBLEDevice.h>
#else
#include <BluetoothSerial.h>
#endif

namespace BtLink {
namespace {
uint32_t g_lastAttempt = 0;
uint32_t g_backoffMs = 2000;
constexpr uint32_t BACKOFF_MAX_MS = 30000;
char g_status[64] = "init";

bool macIsUnset() {
  for (int i = 0; i < 6; i++) {
    if (PRINTER_MAC[i] != 0) return false;
  }
  return true;
}
}  // namespace

#if PRINTER_USE_BLE
// ---------------------------------------------------------------------------
// BLE transport
// ---------------------------------------------------------------------------
namespace {
NimBLEClient* g_client = nullptr;
NimBLERemoteCharacteristic* g_writeChar = nullptr;

// The service/characteristic pairs these printers actually use. Checked in
// order; the first one that exists and is writable wins.
struct CandidateUuid {
  const char* service;
  const char* characteristic;
};
const CandidateUuid CANDIDATES[] = {
    {"18F0", "2AF1"},  // most common on ESC/POS BLE printers
    {"FFE0", "FFE1"},
    {"FF00", "FF02"},
    {"E7810A71-73AE-499D-8C15-FAA9AEF0C3F2",
     "BEF8D6C9-9C21-4C9E-B632-BD58C1009F9F"},
};
constexpr int CANDIDATE_COUNT = sizeof(CANDIDATES) / sizeof(CANDIDATES[0]);

// BLE writes are capped near the negotiated MTU. 20 bytes is the safe floor
// that works before any MTU exchange.
constexpr size_t BLE_CHUNK = 20;
}  // namespace

bool begin() {
  NimBLEDevice::init(BT_LOCAL_NAME);
  NimBLEDevice::setMTU(247);  // request more; we still chunk conservatively
  snprintf(g_status, sizeof(g_status), "BLE ready");
  return true;
}

bool connect() {
  if (macIsUnset()) {
    snprintf(g_status, sizeof(g_status), "PRINTER_MAC unset");
    return false;
  }
  if (connected()) return true;

  if (!g_client) g_client = NimBLEDevice::createClient();

  NimBLEAddress addr(const_cast<uint8_t*>(PRINTER_MAC), BLE_ADDR_PUBLIC);
  if (!g_client->connect(addr)) {
    snprintf(g_status, sizeof(g_status), "BLE connect failed");
    return false;
  }

  g_writeChar = nullptr;
  for (int i = 0; i < CANDIDATE_COUNT; i++) {
    NimBLERemoteService* svc =
        g_client->getService(NimBLEUUID(CANDIDATES[i].service));
    if (!svc) continue;
    NimBLERemoteCharacteristic* ch =
        svc->getCharacteristic(NimBLEUUID(CANDIDATES[i].characteristic));
    if (ch && (ch->canWrite() || ch->canWriteNoResponse())) {
      g_writeChar = ch;
      snprintf(g_status, sizeof(g_status), "BLE %s/%s",
               CANDIDATES[i].service, CANDIDATES[i].characteristic);
      break;
    }
  }

  if (!g_writeChar) {
    // Nothing matched the known pairs. Fall back to the first writable
    // characteristic anywhere on the device.
    for (auto* svc : *g_client->getServices(true)) {
      for (auto* ch : *svc->getCharacteristics(true)) {
        if (ch->canWrite() || ch->canWriteNoResponse()) {
          g_writeChar = ch;
          snprintf(g_status, sizeof(g_status), "BLE fallback %s",
                   ch->getUUID().toString().c_str());
          break;
        }
      }
      if (g_writeChar) break;
    }
  }

  if (!g_writeChar) {
    g_client->disconnect();
    snprintf(g_status, sizeof(g_status), "no writable characteristic");
    return false;
  }
  return true;
}

bool connected() { return g_client && g_client->isConnected() && g_writeChar; }

void disconnect() {
  if (g_client) g_client->disconnect();
  g_writeChar = nullptr;
}

bool write(const uint8_t* data, size_t len) {
  if (!connected()) return false;
  const bool needsResponse = !g_writeChar->canWriteNoResponse();

  size_t sent = 0;
  while (sent < len) {
    const size_t n = min(BLE_CHUNK, len - sent);
    if (!g_writeChar->writeValue(data + sent, n, needsResponse)) return false;
    sent += n;
    // Write-without-response gets no flow control from the stack, so pace it
    // by hand or the printer's buffer overruns and rows go missing.
    if (!needsResponse) delay(4);
  }
  return true;
}

#else
// ---------------------------------------------------------------------------
// Bluetooth Classic SPP transport
// ---------------------------------------------------------------------------
namespace {
BluetoothSerial g_serial;

// SPP has no hardware flow control. Writes go into the stack's own buffer,
// which is small, so hand it work in modest pieces.
constexpr size_t SPP_CHUNK = 256;
}  // namespace

bool begin() {
  // true = master role, so we initiate to the printer rather than waiting.
  if (!g_serial.begin(BT_LOCAL_NAME, true)) {
    snprintf(g_status, sizeof(g_status), "SPP begin failed");
    return false;
  }
  snprintf(g_status, sizeof(g_status), "SPP ready");
  return true;
}

bool connect() {
  if (connected()) return true;

  bool ok;
  if (macIsUnset()) {
    // No MAC configured yet -- try by name. Slower and less reliable than an
    // address, but it lets the firmware work before you have run the probe.
    snprintf(g_status, sizeof(g_status), "SPP connecting by name");
    ok = g_serial.connect(String(PRINTER_NAME));
  } else {
    snprintf(g_status, sizeof(g_status), "SPP connecting by MAC");
    ok = g_serial.connect(const_cast<uint8_t*>(PRINTER_MAC));
  }

  snprintf(g_status, sizeof(g_status), ok ? "SPP connected" : "SPP connect failed");
  return ok;
}

bool connected() { return g_serial.connected(); }

void disconnect() { g_serial.disconnect(); }

bool write(const uint8_t* data, size_t len) {
  if (!connected()) return false;

  size_t sent = 0;
  while (sent < len) {
    const size_t n = min(SPP_CHUNK, len - sent);
    const size_t w = g_serial.write(data + sent, n);
    if (w == 0) {
      if (!connected()) return false;
      delay(5);  // stack buffer full, let it drain
      continue;
    }
    sent += w;
  }
  g_serial.flush();
  return true;
}

#endif

// ---------------------------------------------------------------------------
// Shared reconnect logic
// ---------------------------------------------------------------------------
void poll() {
  if (connected()) {
    g_backoffMs = 2000;  // reset once a connection sticks
    return;
  }
  const uint32_t now = millis();
  if (now - g_lastAttempt < g_backoffMs) return;
  g_lastAttempt = now;

  if (!connect()) {
    // The printer powers its radio down when idle, so failed attempts are
    // routine rather than an error. Back off so retries don't monopolise the
    // radio and starve WiFi.
    g_backoffMs = min(g_backoffMs * 2, BACKOFF_MAX_MS);
  }
}

const char* statusText() { return g_status; }

}  // namespace BtLink
