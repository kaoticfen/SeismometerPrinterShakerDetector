#pragma once
#include <Arduino.h>

// SoftAP plus a small HTTP API, so you can watch the live trace on a phone and
// set the pot before committing paper.
//
// Deliberately HTTP polling rather than WebSockets: this build already hosts
// both radios, and skipping the WebSocket library keeps its RAM out of the
// picture. A 5Hz poll of a 240-bin envelope is plenty to tune a gain by eye.
namespace WebUI {

// `requestPrint` is invoked from the HTTP task when the page's print button is
// pressed. It must be safe to call off the main loop.
void begin(bool (*requestPrint)());

void poll();

// Current full-scale in mg, read from the pot; shown on the page.
void setFullScaleMg(float mg);

int clientCount();

}  // namespace WebUI
