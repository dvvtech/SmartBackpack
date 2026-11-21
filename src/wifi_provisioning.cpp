#include "wifi_provisioning.h"

#include <Arduino.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include "provisioning_page.h"

namespace {
constexpr char WIFI_CONFIG_NAMESPACE[] = "wifi-config";
constexpr char WIFI_SSID_KEY[] = "ssid";
constexpr char WIFI_PASSWORD_KEY[] = "password";
constexpr char SETUP_AP_SSID[] = "ScanBox-Setup";
constexpr unsigned long WIFI_CONNECT_TIMEOUT_MS = 20000;

WebServer provisioningServer(80);
bool provisioningMode = false;

const char* httpMethodName(HTTPMethod method) {
  switch (method) {
    case HTTP_GET: return "GET";
    case HTTP_POST: return "POST";
    case HTTP_PUT: return "PUT";
    case HTTP_DELETE: return "DELETE";
    case HTTP_OPTIONS: return "OPTIONS";
    case HTTP_HEAD: return "HEAD";
    default: return "OTHER";
  }
}

void handleProvisioningRoot() {
  Serial.printf("[HTTP] %s %s -> provisioning page\n",
                httpMethodName(provisioningServer.method()), provisioningServer.uri().c_str());
  provisioningServer.send(200, "text/html; charset=utf-8", PROVISIONING_PAGE);
}

void handleNotFound() {
  Serial.printf("[HTTP] Unmatched request: %s %s\n",
                httpMethodName(provisioningServer.method()), provisioningServer.uri().c_str());
  handleProvisioningRoot();
}

bool connectToWiFi(const String& ssid, const String& password) {
  Serial.printf("[WiFi] Connecting to SSID: %s\n", ssid.c_str());
  WiFi.mode(provisioningMode ? WIFI_AP_STA : WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());

  const unsigned long startedAt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startedAt < WIFI_CONNECT_TIMEOUT_MS) {
    delay(250);
  }

  const bool connected = WiFi.status() == WL_CONNECTED;
  Serial.printf("[WiFi] %s; status=%d; IP=%s\n",
                connected ? "Connected" : "Connection failed",
                static_cast<int>(WiFi.status()), WiFi.localIP().toString().c_str());
  return connected;
}

void handleSaveWiFi() {
  Serial.printf("[HTTP] %s %s -> save Wi-Fi settings\n",
                httpMethodName(provisioningServer.method()), provisioningServer.uri().c_str());
  const String ssid = provisioningServer.arg("ssid");
  const String password = provisioningServer.arg("password");

  if (ssid.length() == 0 || ssid.length() > 32 || password.length() > 63) {
    provisioningServer.send(400, "text/plain; charset=utf-8",
                            "Проверьте имя сети и пароль. Максимум: 32 и 63 символа.");
    return;
  }

  Serial.printf("Wi-Fi SSID: %s\n", ssid.c_str());

  if (!connectToWiFi(ssid, password)) {
    WiFi.disconnect(false);
    provisioningServer.send(200, "text/html; charset=utf-8",
        "<!doctype html><html lang=\"ru\"><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>Ошибка подключения</title><body><h1>Не удалось подключиться к Wi-Fi</h1><p>Проверьте имя сети и пароль и попробуйте снова.</p><p><a href=\"/\">Вернуться к настройке</a></p></body></html>");
    return;
  }

  Preferences preferences;
  if (!preferences.begin(WIFI_CONFIG_NAMESPACE, false)) {
    provisioningServer.send(500, "text/plain; charset=utf-8",
                            "Не удалось открыть хранилище настроек.");
    return;
  }
  preferences.putString(WIFI_SSID_KEY, ssid);
  preferences.putString(WIFI_PASSWORD_KEY, password);
  preferences.end();

  String macAddress = WiFi.macAddress();
  macAddress.replace(":", "%3A");
  const String redirectUrl = "https://scanbox.tech/?mac=" + macAddress;
  Serial.printf("[HTTP] Showing manual ScanBox link: %s\n", redirectUrl.c_str());

  String successPage =
      "<!doctype html><html lang=\"ru\"><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>Wi-Fi подключен</title>"
      "<style>body{font:16px system-ui,sans-serif;max-width:480px;margin:10vh auto;padding:0 20px;color:#17202a}a{display:inline-block;margin:12px 0;padding:12px 18px;border-radius:6px;background:#1769aa;color:white;text-decoration:none;word-break:break-all}code{word-break:break-all}</style>"
      "<body><h1>Wi-Fi подключен</h1><p>Устройство подключилось к сети. Теперь вручную переключите телефон с ScanBox-Setup на Wi-Fi сеть с доступом в интернет, затем вернитесь на эту страницу и нажмите ссылку:</p><p><a href=\"";
  successPage += redirectUrl;
  successPage += "\">Открыть ScanBox</a></p><p>Если страница закроется, откройте ссылку вручную:</p><code>";
  successPage += redirectUrl;
  successPage += "</code></body></html>";
  provisioningServer.send(200, "text/html; charset=utf-8", successPage);
  Serial.println("[WiFi] Setup AP remains active for the phone to switch networks.");
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

  if (connectToWiFi(ssid, password)) {
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
  provisioningServer.on("/favicon.ico", HTTP_GET, []() { provisioningServer.send(204); });
  provisioningServer.onNotFound(handleNotFound);
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
