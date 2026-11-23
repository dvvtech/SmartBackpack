#include "rfid_mappings.h"

#include <Preferences.h>

namespace {
constexpr char PREFERENCES_NAMESPACE[] = "scanbox_tags";
constexpr uint8_t MAX_MAPPINGS = 30;

String idKey(uint8_t index) { return "id" + String(index); }
String tagKey(uint8_t index) { return "tag" + String(index); }

String normalizeRfidTag(const String& value) {
  String normalized = value;
  normalized.replace(" ", "");
  normalized.replace(":", "");
  normalized.toUpperCase();
  if (normalized.length() == 0 || normalized.length() > 62 || normalized.length() % 2 != 0) {
    return "";
  }
  for (size_t i = 0; i < normalized.length(); ++i) {
    const char ch = normalized[i];
    if (!((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F'))) return "";
  }
  return normalized;
}
}  // namespace

std::vector<RfidTagMapping> rfidMappingsList() {
  std::vector<RfidTagMapping> mappings;
  Preferences preferences;
  if (!preferences.begin(PREFERENCES_NAMESPACE, true)) return mappings;

  const uint8_t count = preferences.getUChar("count", 0);
  mappings.reserve(count);
  for (uint8_t index = 0; index < count; ++index) {
    const uint16_t subjectId = preferences.getUShort(idKey(index).c_str(), 0);
    const String rfidTag = preferences.getString(tagKey(index).c_str(), "");
    if (subjectId != 0 && rfidTag.length() != 0) mappings.push_back({subjectId, rfidTag});
  }

  preferences.end();
  return mappings;
}

uint16_t rfidMappingSubjectIdForTag(const String& rfidTag) {
  const String normalizedTag = normalizeRfidTag(rfidTag);
  if (normalizedTag.length() == 0) return 0;

  for (const RfidTagMapping& mapping : rfidMappingsList()) {
    if (normalizeRfidTag(mapping.rfidTag) == normalizedTag) return mapping.subjectId;
  }
  return 0;
}

bool rfidMappingSave(uint16_t subjectId, const String& rfidTag) {
  const String normalizedTag = normalizeRfidTag(rfidTag);
  if (subjectId == 0 || normalizedTag.length() == 0) return false;

  Preferences preferences;
  if (!preferences.begin(PREFERENCES_NAMESPACE, false)) return false;

  uint8_t count = preferences.getUChar("count", 0);
  int targetIndex = -1;
  for (uint8_t index = 0; index < count; ++index) {
    const uint16_t storedSubjectId = preferences.getUShort(idKey(index).c_str(), 0);
    const String storedTag = preferences.getString(tagKey(index).c_str(), "");
    if (storedSubjectId == subjectId) {
      targetIndex = index;
    } else if (storedTag == normalizedTag) {
      preferences.end();
      return false;
    }
  }

  if (targetIndex < 0) {
    if (count >= MAX_MAPPINGS) {
      preferences.end();
      return false;
    }
    targetIndex = count++;
  }

  const bool saved = preferences.putUShort(idKey(targetIndex).c_str(), subjectId) != 0
      && preferences.putString(tagKey(targetIndex).c_str(), normalizedTag) != 0
      && preferences.putUChar("count", count) != 0;
  preferences.end();
  return saved;
}

bool rfidMappingRemove(uint16_t subjectId) {
  if (subjectId == 0) return false;

  Preferences preferences;
  if (!preferences.begin(PREFERENCES_NAMESPACE, false)) return false;

  const uint8_t count = preferences.getUChar("count", 0);
  int targetIndex = -1;
  for (uint8_t index = 0; index < count; ++index) {
    if (preferences.getUShort(idKey(index).c_str(), 0) == subjectId) {
      targetIndex = index;
      break;
    }
  }

  if (targetIndex < 0) {
    preferences.end();
    return false;
  }

  for (uint8_t index = static_cast<uint8_t>(targetIndex); index + 1 < count; ++index) {
    preferences.putUShort(idKey(index).c_str(), preferences.getUShort(idKey(index + 1).c_str(), 0));
    preferences.putString(tagKey(index).c_str(), preferences.getString(tagKey(index + 1).c_str(), ""));
  }

  const uint8_t newCount = count - 1;
  preferences.remove(idKey(newCount).c_str());
  preferences.remove(tagKey(newCount).c_str());
  const bool saved = preferences.putUChar("count", newCount) != 0 || newCount == 0;
  preferences.end();
  return saved;
}
