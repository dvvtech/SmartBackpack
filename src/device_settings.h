#pragma once

#include <Arduino.h>

String deviceSettingsClass();
bool deviceSettingsSetClass(const String& className);
bool deviceSettingsIsValidClass(const String& className);
