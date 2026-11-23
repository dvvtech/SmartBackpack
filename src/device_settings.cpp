#include "device_settings.h"

#include <Preferences.h>

namespace {
constexpr char DEVICE_SETTINGS_NAMESPACE[] = "device-settings";
constexpr char DEVICE_CLASS_KEY[] = "school-class";
}

bool deviceSettingsIsValidClass(const String& className) {
  return className.length() == 0
      || className == "8А"
      || className == "8Б"
      || className == "8В";
}

String deviceSettingsClass() {
  Preferences preferences;
  if (!preferences.begin(DEVICE_SETTINGS_NAMESPACE, true)) return "";
  const String className = preferences.getString(DEVICE_CLASS_KEY, "");
  preferences.end();
  return deviceSettingsIsValidClass(className) ? className : "";
}

bool deviceSettingsSetClass(const String& className) {
  if (!deviceSettingsIsValidClass(className)) return false;

  Preferences preferences;
  if (!preferences.begin(DEVICE_SETTINGS_NAMESPACE, false)) return false;
  const bool saved = className.length() == 0
      ? (preferences.remove(DEVICE_CLASS_KEY) || !preferences.isKey(DEVICE_CLASS_KEY))
      : preferences.putString(DEVICE_CLASS_KEY, className) != 0;
  preferences.end();
  return saved;
}
