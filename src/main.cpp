#include <Arduino.h>

#include "device_websocket.h"
#include "rfid_scanner.h"
#include "wifi_provisioning.h"

void setup() {
  Serial.begin(115200);
  Serial.println("--- UHF RFID YRM1002: Multiple Inventory ---");

  rfidScannerBegin();
  // Временный режим отладки RFID. Отключите, установив false, после проверки меток.
  constexpr bool RFID_CONTINUOUS_DEBUG_MODE = false;
  if (RFID_CONTINUOUS_DEBUG_MODE) {
    rfidScannerStartContinuousInventory();
  } else {
    Serial.println("[RFID] Debug inventory disabled; stopping any previous inventory.");
    rfidScannerStopInventory();
  }
  wifiProvisioningBegin();

}

void loop() {
  wifiProvisioningLoop();
  rfidScannerLoop();
  deviceWebSocketLoop();
}
