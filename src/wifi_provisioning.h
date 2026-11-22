#pragma once

#include <Arduino.h>

void wifiProvisioningBegin();
void wifiProvisioningLoop();
String wifiProvisioningPairingToken();
bool wifiProvisioningClearSavedCredentials();
void wifiProvisioningRequestSetup();
