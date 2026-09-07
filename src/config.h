#pragma once
#include <stdint.h>

// ---------------------------------------------------------------------------
// Pins
//
// Avoid GPIO6-11 (SPI flash) and GPIO0/2/12/15 (strapping pins).
// The pot MUST be on ADC1 (GPIO32-39): ADC2 stops working the moment the WiFi
// driver starts, and it fails silently by returning garbage rather than error.
// ---------------------------------------------------------------------------
static const int PIN_I2C_SDA = 21;
static const int PIN_I2C_SCL = 22;
static const int PIN_BUTTON  = 27;  // to GND, INPUT_PULLUP
static const int PIN_LED     = 26;  // -> 330R -> LED -> GND
static const int PIN_POT     = 34;  // ADC1_CH6, input-only pin

// ---------------------------------------------------------------------------
// Sampling
// ---------------------------------------------------------------------------
static const int   SAMPLE_RATE_HZ = 200;
static const int   WINDOW_SECONDS = 30;
static const int   BUFFER_SAMPLES = SAMPLE_RATE_HZ * WINDOW_SECONDS;  // 6000

// MPU6050 at +/-2g full scale.
static const float MPU_MG_PER_LSB = 2000.0f / 32768.0f;  // 0.061 mg/LSB

// 1-pole high-pass corner, strips gravity and thermal drift.
static const float HPF_CORNER_HZ = 0.5f;

// ---------------------------------------------------------------------------
// Print layout
//
// 58mm paper at 203dpi = 384 dots = 48 bytes per raster row.
// ---------------------------------------------------------------------------
static const int PRINT_WIDTH_DOTS  = 384;
static const int PRINT_WIDTH_BYTES = PRINT_WIDTH_DOTS / 8;  // 48

// Rows of dots printed per second of signal. At 8 dots/mm this is the paper
// speed: 20 rows/s / 8 = 2.5 mm/s, so a 30s window is ~75mm of graph.
static const int PRINT_ROWS_PER_SEC = 20;
static const int SAMPLES_PER_ROW    = SAMPLE_RATE_HZ / PRINT_ROWS_PER_SEC;  // 10

// Left margin reserved for second ticks and their labels.
static const int GRAPH_LEFT   = 28;
static const int GRAPH_RIGHT  = 380;
static const int GRAPH_CENTER = (GRAPH_LEFT + GRAPH_RIGHT) / 2;  // 204

// Raster is sent in chunks with a pause between them. SPP has no hardware flow
// control and the printer's input buffer is only a few KB, so blasting a whole
// 600-row image in one write is the classic cause of garbled output.
static const int RASTER_CHUNK_ROWS   = 24;
static const int RASTER_CHUNK_PAD_MS = 40;

// ---------------------------------------------------------------------------
// Controls
// ---------------------------------------------------------------------------
static const uint32_t BUTTON_DEBOUNCE_MS   = 40;
static const uint32_t BUTTON_LONGPRESS_MS  = 1000;

// Pot maps log-wise onto the graph's full-scale deflection.
static const float FULLSCALE_MIN_MG = 5.0f;
static const float FULLSCALE_MAX_MG = 500.0f;

// LED lights while the signal exceeds this fraction of full scale.
static const float LED_TRIGGER_FRACTION = 0.5f;

// ---------------------------------------------------------------------------
// Bluetooth printer
//
// Fill this in from the `probe` firmware's output, then rebuild.
// Format: {0x00, 0x11, 0x22, 0x33, 0x44, 0x55}
// ---------------------------------------------------------------------------
// 0 = Bluetooth Classic SPP, 1 = BLE fallback.
// Overridable from platformio.ini so both transports can be build-tested.
#ifndef PRINTER_USE_BLE
#define PRINTER_USE_BLE 0
#endif

static const uint8_t PRINTER_MAC[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// `const` after the star matters: a non-const pointer is a mutable variable, so
// every translation unit that includes this and doesn't use it warns.
static const char* const PRINTER_NAME  = "NT-1809";
static const char* const BT_LOCAL_NAME = "SEISMO";

// ---------------------------------------------------------------------------
// WiFi access point for the live web view
// ---------------------------------------------------------------------------
static const char* const AP_SSID = "SEISMO";
static const char* const AP_PASS = "shakeitup";  // >=8 chars or the AP won't start
