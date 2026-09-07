#pragma once
#include <Arduino.h>

// Transport to the printer. Two implementations behind one interface, selected
// by PRINTER_USE_BLE in config.h -- run the `probe` firmware first to find out
// which one this printer needs.
//
// Classic SPP is preferred: it is a plain byte stream with sane throughput.
// BLE is the fallback and is slower, since every write is capped near the MTU
// and needs pacing.
namespace BtLink {

bool begin();

// Attempts a connection. Non-blocking failures are normal -- the printer sleeps
// and drops the link when idle, so callers retry.
bool connect();
bool connected();
void disconnect();

// Writes all `len` bytes, blocking until they are handed to the stack.
// Returns false if the link dropped partway.
bool write(const uint8_t* data, size_t len);

// Call from loop(); handles reconnect backoff.
void poll();

const char* statusText();

}  // namespace BtLink
