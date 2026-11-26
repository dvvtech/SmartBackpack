#pragma once

#include <Arduino.h>
#include <vector>

struct RfidTimedScanResult {
  bool ready;
  std::vector<String> tags;
};

void rfidScannerBegin();
void rfidScannerLoop();
void rfidScannerStartInventory();
void rfidScannerStartContinuousInventory();
void rfidScannerStopInventory();
bool rfidScannerStartTimedScan(uint32_t durationMs);
RfidTimedScanResult rfidScannerTakeTimedScanResult();
std::vector<String> rfidScannerTakeNewTags();
