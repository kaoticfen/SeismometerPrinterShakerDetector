// Bluetooth discovery firmware. Flash this FIRST.
//
//   pio run -e probe -t upload -t monitor
//
// The Netum NT-1809 advertises "Bluetooth 4.0" and supports iOS, which means it
// is almost certainly dual-mode -- but no published spec says whether it exposes
// Classic SPP, BLE, or both. This sketch scans for both and prints what it finds
// so you can fill in PRINTER_MAC and PRINTER_USE_BLE in src/config.h.
//
// Power-cycle the printer so it is discoverable (not already paired to a phone)
// before running this.

#include <Arduino.h>
#include <BluetoothSerial.h>
#include <NimBLEDevice.h>

static BluetoothSerial SerialBT;

static void printMac(const uint8_t* mac) {
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X", mac[i]);
    if (i < 5) Serial.print(":");
  }
}

static void scanClassic() {
  Serial.println();
  Serial.println("=== Bluetooth Classic (SPP) discovery, 15s ===");

  if (!SerialBT.begin("SEISMO_PROBE", true)) {  // true = master/initiator role
    Serial.println("  BluetoothSerial.begin() failed");
    return;
  }

  BTScanResults* results = SerialBT.discover(15000);
  if (!results || results->getCount() == 0) {
    Serial.println("  no Classic devices found");
    Serial.println("  -> printer may be BLE-only, or is already paired elsewhere");
  } else {
    for (int i = 0; i < results->getCount(); i++) {
      BTAdvertisedDevice* d = results->getDevice(i);
      Serial.printf("  [%d] name=\"%s\" addr=%s rssi=%d\n", i,
                    d->getName().c_str(),
                    d->getAddress().toString().c_str(),
                    d->getRSSI());
      Serial.print("       config.h -> static const uint8_t PRINTER_MAC[6] = {");
      // getNative() returns esp_bd_addr_t*, i.e. a pointer to a uint8_t[6].
      const uint8_t* mac = *d->getAddress().getNative();
      for (int b = 0; b < 6; b++) {
        Serial.printf("0x%02X%s", mac[b], b < 5 ? ", " : "");
      }
      Serial.println("};");
    }
  }

  SerialBT.end();
}

class ScanCallbacks : public NimBLEAdvertisedDeviceCallbacks {
  void onResult(NimBLEAdvertisedDevice* d) override {
    Serial.printf("  name=\"%s\" addr=%s rssi=%d",
                  d->haveName() ? d->getName().c_str() : "(none)",
                  d->getAddress().toString().c_str(),
                  d->getRSSI());
    if (d->haveServiceUUID()) {
      Serial.print(" services=");
      for (int i = 0; i < d->getServiceUUIDCount(); i++) {
        Serial.printf("%s ", d->getServiceUUID(i).toString().c_str());
      }
    }
    Serial.println();
  }
};

static void scanBle() {
  Serial.println();
  Serial.println("=== BLE discovery, 15s ===");
  Serial.println("Look for services FFE0, 18F0, FF00 -- the usual printer ones.");

  NimBLEDevice::init("");
  NimBLEScan* scan = NimBLEDevice::getScan();
  static ScanCallbacks cbs;
  scan->setAdvertisedDeviceCallbacks(&cbs);
  scan->setActiveScan(true);  // request scan response, which carries the name
  scan->start(15, false);
  scan->clearResults();
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println();
  Serial.println("================================================");
  Serial.println(" NT-1809 Bluetooth probe");
  Serial.println(" Power-cycle the printer first so it is pairable.");
  Serial.println("================================================");

  // Classic and BLE both want the controller, so run them one after the other
  // rather than concurrently.
  scanClassic();
  delay(500);
  scanBle();

  Serial.println();
  Serial.println("=== done ===");
  Serial.println("Found it under Classic? -> PRINTER_USE_BLE 0, set PRINTER_MAC");
  Serial.println("BLE only?               -> PRINTER_USE_BLE 1, set PRINTER_MAC");
  Serial.println("Neither? -> printer not in pairing mode, or out of range.");
}

void loop() {
  delay(1000);
}
