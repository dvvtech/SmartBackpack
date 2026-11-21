#include "wifi_provisioning.h"

#include <Arduino.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_system.h>
#include "provisioning_page.h"

namespace {
constexpr char WIFI_CONFIG_NAMESPACE[] = "wifi-config";
constexpr char WIFI_SSID_KEY[] = "ssid";
constexpr char WIFI_PASSWORD_KEY[] = "password";
constexpr char WIFI_PAIRING_TOKEN_KEY[] = "pairing-token";
constexpr char SETUP_AP_SSID[] = "ScanBox-Setup";
constexpr unsigned long WIFI_CONNECT_TIMEOUT_MS = 20000;
constexpr int BOOT_BUTTON_PIN = 0;
constexpr unsigned long PAIRING_RECOVERY_HOLD_MS = 3000;
constexpr unsigned long PAIRING_RECOVERY_TIMEOUT_MS = 10UL * 60UL * 1000UL;

WebServer provisioningServer(80);
bool provisioningMode = false;
bool provisioningServerStarted = false;
bool pairingRecoveryMode = false;
unsigned long pairingRecoveryStartedAt = 0;

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

void handlePairingRecoveryRoot() {
  Serial.printf("[HTTP] %s %s -> pairing recovery page\n",
                httpMethodName(provisioningServer.method()), provisioningServer.uri().c_str());

  String macAddress = WiFi.macAddress();
  macAddress.replace(":", "%3A");
  const String pairingToken = wifiProvisioningPairingToken();
  const String scanboxLink = "https://scanbox.tech/?mac=" + macAddress + "#token=" + pairingToken;

  String page =
      "<!doctype html><html lang=\"ru\"><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>Восстановление ScanBox</title>"
      "<style>body{font:16px system-ui,sans-serif;max-width:480px;margin:10vh auto;padding:0 20px;color:#17202a}a{display:inline-block;margin:12px 0;padding:12px 18px;border-radius:6px;background:#1769aa;color:white;text-decoration:none;word-break:break-all}code{word-break:break-all}</style>"
      "<body><h1>Данные ScanBox</h1><p>MAC-адрес: <code>";
  page += WiFi.macAddress();
  page += "</code></p><p>Код сопряжения: <strong>";
  page += pairingToken;
  page += "</strong></p><p>Переключите телефон с ScanBox-Setup на Wi-Fi сеть с доступом в интернет, затем откройте ссылку:</p><p><a href=\"";
  page += scanboxLink;
  page += "\">Открыть ScanBox и восстановить профиль</a></p><p>Эта точка настройки автоматически отключится через 10 минут.</p></body></html>";
  provisioningServer.send(200, "text/html; charset=utf-8", page);
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

  const String pairingToken = wifiProvisioningPairingToken();

  String macAddress = WiFi.macAddress();
  macAddress.replace(":", "%3A");
  const String redirectUrl = "https://scanbox.tech/?mac=" + macAddress;
  const String setupLink = redirectUrl + "#token=" + pairingToken;
  Serial.printf("[HTTP] Showing manual ScanBox link: %s\n", redirectUrl.c_str());

  String successPage =
      "<!doctype html><html lang=\"ru\"><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>Wi-Fi подключен</title>"
      "<style>body{font:16px system-ui,sans-serif;max-width:480px;margin:10vh auto;padding:0 20px;color:#17202a}a{display:inline-block;margin:12px 0;padding:12px 18px;border-radius:6px;background:#1769aa;color:white;text-decoration:none;word-break:break-all}code{word-break:break-all}</style>"
      "<body><h1>Wi-Fi подключен</h1><p>Устройство подключилось к сети. Теперь вручную переключите телефон с ScanBox-Setup на Wi-Fi сеть с доступом в интернет, затем вернитесь на эту страницу и нажмите ссылку:</p><p>Код сопряжения: <strong>";
  successPage += pairingToken;
  successPage += "</strong></p><p><a href=\"";
  successPage += setupLink;
  successPage += "\">Открыть ScanBox</a></p><p>Если страница закроется, откройте ссылку вручную:</p><code>";
  successPage += setupLink;
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
  provisioningServerStarted = true;

  Serial.print("Режим настройки Wi-Fi. Подключитесь к сети ");
  Serial.println(SETUP_AP_SSID);
  Serial.print("Откройте в браузере: http://");
  Serial.println(WiFi.softAPIP());
}

void startPairingRecovery() {
  Serial.println("[WiFi] Starting temporary pairing recovery access point.");
  WiFi.mode(WIFI_AP_STA);
  if (!WiFi.softAP(SETUP_AP_SSID)) {
    Serial.println("[WiFi] Could not start pairing recovery access point.");
    WiFi.mode(WIFI_STA);
    return;
  }

  if (!provisioningServerStarted) {
    provisioningServer.on("/", HTTP_GET, handlePairingRecoveryRoot);
    provisioningServer.on("/favicon.ico", HTTP_GET, []() { provisioningServer.send(204); });
    provisioningServer.onNotFound(handlePairingRecoveryRoot);
    provisioningServer.begin();
    provisioningServerStarted = true;
  }

  pairingRecoveryMode = true;
  pairingRecoveryStartedAt = millis();
  Serial.print("[WiFi] Recovery AP: ");
  Serial.println(SETUP_AP_SSID);
  Serial.print("[WiFi] Open http://");
  Serial.println(WiFi.softAPIP());
}

void stopPairingRecovery() {
  provisioningServer.stop();
  provisioningServerStarted = false;
  WiFi.softAPdisconnect(false);
  WiFi.mode(WIFI_STA);
  pairingRecoveryMode = false;
  Serial.println("[WiFi] Pairing recovery access point disabled.");
}
}  // namespace

String wifiProvisioningPairingToken() {
  Preferences preferences;
  if (!preferences.begin(WIFI_CONFIG_NAMESPACE, false)) {
    return "";
  }

  String token = preferences.getString(WIFI_PAIRING_TOKEN_KEY, "");
  bool tokenIsValid = token.length() == 5;
  for (size_t i = 0; tokenIsValid && i < token.length(); ++i) {
    tokenIsValid = isDigit(token[i]);
  }

  if (!tokenIsValid) {
    char tokenBuffer[6];
    snprintf(tokenBuffer, sizeof(tokenBuffer), "%05u",
             static_cast<unsigned int>(esp_random() % 100000));
    token = tokenBuffer;
    preferences.putString(WIFI_PAIRING_TOKEN_KEY, token);
    Serial.printf("[WS] New pairing code (enter in ScanBox profile): %s\n", token.c_str());
  }

  preferences.end();
  return token;
}

void wifiProvisioningBegin() {
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
  Serial.println("Удерживайте BOOT 3 секунды, чтобы восстановить MAC и код сопряжения.");
  if (!connectToSavedWiFi()) {
    startWiFiProvisioning();
  }
}

void wifiProvisioningLoop() {
  if (!provisioningMode && !pairingRecoveryMode && WiFi.status() == WL_CONNECTED) {
    static bool bootButtonPressed = false;
    static bool recoveryTriggered = false;
    static unsigned long bootButtonPressedAt = 0;

    if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
      if (!bootButtonPressed) {
        bootButtonPressed = true;
        recoveryTriggered = false;
        bootButtonPressedAt = millis();
      } else if (!recoveryTriggered && millis() - bootButtonPressedAt >= PAIRING_RECOVERY_HOLD_MS) {
        recoveryTriggered = true;
        startPairingRecovery();
      }
    } else {
      bootButtonPressed = false;
      recoveryTriggered = false;
    }
  }

  if (provisioningMode || pairingRecoveryMode) {
    provisioningServer.handleClient();
  }

  if (pairingRecoveryMode && millis() - pairingRecoveryStartedAt >= PAIRING_RECOVERY_TIMEOUT_MS) {
    stopPairingRecovery();
  }
}
