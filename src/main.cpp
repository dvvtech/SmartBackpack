#include <Arduino.h>

#include "device_websocket.h"
#include "rfid_scanner.h"
#include "wifi_provisioning.h"

void setup() {
  Serial.begin(115200);
  Serial.println("--- UHF RFID YRM1002: Multiple Inventory ---");

  rfidScannerBegin();
  wifiProvisioningBegin();

  // Запуск инвентаризации пока отключен.
  // rfidScannerStartInventory();
}

void loop() {
  wifiProvisioningLoop();
  deviceWebSocketLoop();
  //rfidScannerLoop();
}
