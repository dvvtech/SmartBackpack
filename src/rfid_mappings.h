#pragma once

#include <Arduino.h>
#include <vector>

struct RfidTagMapping {
  uint16_t subjectId;
  String rfidTag;
};

std::vector<RfidTagMapping> rfidMappingsList();
bool rfidMappingSave(uint16_t subjectId, const String& rfidTag);
bool rfidMappingRemove(uint16_t subjectId);
