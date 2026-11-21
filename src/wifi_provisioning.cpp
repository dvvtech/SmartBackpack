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
    .password-field{position:relative}.password-field input{padding-right:48px}
    .password-toggle{position:absolute;right:4px;top:50%;transform:translateY(-50%);margin:0;padding:8px;border:0;background:transparent;color:#52616b;line-height:0}
    .password-toggle svg{width:20px;height:20px;fill:none;stroke:currentColor;stroke-width:2;stroke-linecap:round;stroke-linejoin:round}
    .password-toggle .slash{display:none}.password-toggle[aria-pressed="true"] .slash{display:block}
    .submit-button{display:inline-flex;align-items:center;gap:9px}.submit-button:disabled{opacity:.75;cursor:wait}
    .spinner{display:none;width:16px;height:16px;border:2px solid rgba(255,255,255,.45);border-top-color:#fff;border-radius:50%;animation:spin .7s linear infinite}
    .submit-button[aria-busy="true"] .spinner{display:inline-block}@keyframes spin{to{transform:rotate(360deg)}}
  </style>
</head>
<body>
  <h1>Настройка Wi-Fi</h1>
  <p>Укажите сеть, к которой должно подключаться устройство.</p>
  <form id="wifi-form" method="post" action="/save">
    <label for="ssid">Имя сети (SSID)</label>
    <input id="ssid" name="ssid" maxlength="32" required autocomplete="off">
    <label for="password">Пароль</label>
    <div class="password-field">
      <input id="password" name="password" type="password" maxlength="63" autocomplete="new-password">
      <button class="password-toggle" type="button" aria-label="Показать пароль" aria-pressed="false" onclick="togglePassword()">
        <svg viewBox="0 0 24 24" aria-hidden="true"><path d="M2 12s3.6-7 10-7 10 7 10 7-3.6 7-10 7S2 12 2 12z"></path><circle cx="12" cy="12" r="3"></circle><path class="slash" d="M3 3l18 18"></path></svg>
      </button>
    </div>
    <button id="submit-button" class="submit-button" type="submit"><span class="spinner" aria-hidden="true"></span><span class="submit-label">Сохранить и подключиться</span></button>
  </form>
  <script>
    function togglePassword(){
      const field=document.getElementById('password');
      const toggle=document.querySelector('.password-toggle');
      const visible=field.type==='password';
      field.type=visible?'text':'password';
      toggle.setAttribute('aria-pressed',visible?'true':'false');
      toggle.setAttribute('aria-label',visible?'Скрыть пароль':'Показать пароль');
    }
    document.getElementById('wifi-form').addEventListener('submit',function(){
      const button=document.getElementById('submit-button');
      button.disabled=true;
      button.setAttribute('aria-busy','true');
      button.querySelector('.submit-label').textContent='Подключаемся...';
    });
  </script>
</body>
</html>
)rawliteral";

void handleProvisioningRoot() {
  provisioningServer.send(200, "text/html; charset=utf-8", PROVISIONING_PAGE);
}

bool connectToWiFi(const String& ssid, const String& password) {
  WiFi.mode(provisioningMode ? WIFI_AP_STA : WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());

  const unsigned long startedAt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startedAt < WIFI_CONNECT_TIMEOUT_MS) {
    delay(250);
  }

  return WiFi.status() == WL_CONNECTED;
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

  String redirectUrl = "https://scanbox.tech/?mac=" + WiFi.macAddress();
  redirectUrl.replace(":", "%3A");
  provisioningServer.sendHeader("Location", redirectUrl, true);
  provisioningServer.send(302, "text/plain; charset=utf-8", "Перенаправление на ScanBox...");
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
