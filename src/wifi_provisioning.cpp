#include "wifi_provisioning.h"

#include <Arduino.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>

namespace {
constexpr char WIFI_CONFIG_NAMESPACE[] = "wifi-config";
constexpr char WIFI_SSID_KEY[] = "ssid";
constexpr char WIFI_PASSWORD_KEY[] = "password";
constexpr char SETUP_AP_SSID[] = "ScanBox-Setup";
constexpr unsigned long WIFI_CONNECT_TIMEOUT_MS = 20000;

WebServer provisioningServer(80);
bool provisioningMode = false;

const char PROVISIONING_PAGE[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="ru">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>Настройка SmartBackpack</title>
  <style>
    body{font:16px system-ui,sans-serif;max-width:480px;margin:10vh auto;padding:0 20px;color:#17202a}
    label{display:block;margin:18px 0 6px}input{box-sizing:border-box;width:100%;padding:12px;border:1px solid #aab7b8;border-radius:6px;font:inherit}
    button{margin-top:22px;padding:12px 18px;border:0;border-radius:6px;background:#1769aa;color:white;font:inherit;cursor:pointer}
  </style>
</head>
<body>
  <h1>Настройка Wi-Fi</h1>
  <p>Укажите сеть, к которой должно подключаться устройство.</p>
  <form method="post" action="/save">
    <label for="ssid">Имя сети (SSID)</label>
    <input id="ssid" name="ssid" maxlength="32" required autocomplete="off">
    <label for="password">Пароль</label>
    <input id="password" name="password" type="password" maxlength="63" autocomplete="new-password">
    <button type="submit">Сохранить и перезагрузить</button>
  </form>
</body>
</html>
)rawliteral";

void handleProvisioningRoot() {
  provisioningServer.send(200, "text/html; charset=utf-8", PROVISIONING_PAGE);
}

void handleSaveWiFi() {
  const String ssid = provisioningServer.arg("ssid");
  const String password = provisioningServer.arg("password");

  if (ssid.length() == 0 || ssid.length() > 32 || password.length() > 63) {
    provisioningServer.send(400, "text/plain; charset=utf-8",
                            "Проверьте имя сети и пароль. Максимум: 32 и 63 символа.");
    return;
  }

  Serial.printf("Wi-Fi SSID: %s\n", ssid.c_str());
  Serial.printf("Wi-Fi password: %s\n", password.c_str());

  Preferences preferences;
  if (!preferences.begin(WIFI_CONFIG_NAMESPACE, false)) {
    provisioningServer.send(500, "text/plain; charset=utf-8",
                            "Не удалось открыть хранилище настроек.");
    return;
  }
  preferences.putString(WIFI_SSID_KEY, ssid);
  preferences.putString(WIFI_PASSWORD_KEY, password);
  preferences.end();

  provisioningServer.send(200, "text/html; charset=utf-8",
      "<!doctype html><html lang=\"ru\"><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>Настройки сохранены</title><body><h1>Настройки сохранены</h1><p>Устройство перезагрузится и подключится к указанной сети.</p></body></html>");
  delay(1200);
  ESP.restart();
}

bool connectToSavedWiFi() {
  Preferences preferences;
  if (!preferences.begin(WIFI_CONFIG_NAMESPACE, true)) {
    return false;
  }

  if (!preferences.isKey(WIFI_SSID_KEY) || !preferences.isKey(WIFI_PASSWORD_KEY)) {
    preferences.end();
    return false;
  }

  const String ssid = preferences.getString(WIFI_SSID_KEY, "");
  const String password = preferences.getString(WIFI_PASSWORD_KEY, "");
  preferences.end();

  if (ssid.length() == 0) {
    return false;
  }

  Serial.print("Подключение к Wi-Fi: ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());

  const unsigned long startedAt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startedAt < WIFI_CONNECT_TIMEOUT_MS) {
    delay(250);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Wi-Fi подключен, IP: ");
    Serial.println(WiFi.localIP());
    return true;
  }

  Serial.println("Не удалось подключиться к сохраненной сети.");
  WiFi.disconnect(true);
  return false;
}

void startWiFiProvisioning() {
  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(SETUP_AP_SSID)) {
    Serial.println("Не удалось запустить точку доступа настройки.");
    return;
  }

  provisioningMode = true;
  provisioningServer.on("/", HTTP_GET, handleProvisioningRoot);
  provisioningServer.on("/save", HTTP_POST, handleSaveWiFi);
  provisioningServer.onNotFound(handleProvisioningRoot);
  provisioningServer.begin();

  Serial.print("Режим настройки Wi-Fi. Подключитесь к сети ");
  Serial.println(SETUP_AP_SSID);
  Serial.print("Откройте в браузере: http://");
  Serial.println(WiFi.softAPIP());
}
}  // namespace

void wifiProvisioningBegin() {
  if (!connectToSavedWiFi()) {
    startWiFiProvisioning();
  }
}

void wifiProvisioningLoop() {
  if (provisioningMode) {
    provisioningServer.handleClient();
  }
}
