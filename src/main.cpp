#include <Arduino.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>

#define RXD2 16
#define TXD2 17

namespace {
constexpr char WIFI_CONFIG_NAMESPACE[] = "wifi-config";
constexpr char WIFI_SSID_KEY[] = "ssid";
constexpr char WIFI_PASSWORD_KEY[] = "password";
constexpr char SETUP_AP_SSID[] = "SmartBackpack-Setup";
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

// Multiple Inventory: максимум 65535 циклов; опрос можно остановить командой 0x28.
const byte CMD_START_INVENTORY[] = {0xBB, 0x00, 0x27, 0x00, 0x03, 0x22, 0xFF, 0xFF, 0x4A, 0x7E};

// Команда остановки опроса (может пригодиться для управления)
const byte CMD_STOP_INVENTORY[] = {0xBB, 0x00, 0x28, 0x00, 0x00, 0x28, 0x7E};

// Буфер для сборки одного пакета данных
#define MAX_PKT_LEN 64
byte pktBuffer[MAX_PKT_LEN];
int pktIndex = 0;
bool isReadingPkt = false;

void parse_rfid_stream();
void process_packet(const byte* pkt, int len);

//Serial    UART0   Обычно USB-UART для отладки
//Serial1   UART1    Обычно только передача TX
//Serial2   UART2    Полноценный UART с RX и TX

void setup() {
  Serial.begin(115200);
  
  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);
  delay(500); // Даем UART стабилизироваться
  
  Serial.println("--- UHF RFID YRM1002: Multiple Inventory ---");

  if (!connectToSavedWiFi()) {
    startWiFiProvisioning();
  }

  // Запуск инвентаризации пока отключен на время первичной настройки.
  // Serial2.write(CMD_START_INVENTORY, sizeof(CMD_START_INVENTORY));
}

void loop() {
  if (provisioningMode) {
    provisioningServer.handleClient();
  }

  // Постоянно проверяем буфер UART и разбираем поток байт
  parse_rfid_stream();
}

// Функция потокового парсинга (выделяет пакеты из UART)
void parse_rfid_stream() {
  while (Serial2.available() > 0) {
    byte b = Serial2.read();
    
    // Если нашли стартовый байт и мы еще не в процессе чтения пакета
    if (b == 0xBB && !isReadingPkt) {
      isReadingPkt = true;
      pktIndex = 0;
    }
    
    if (isReadingPkt) {
      if (pktIndex < MAX_PKT_LEN) {
        pktBuffer[pktIndex++] = b;
      } else {
        // Защита от переполнения буфера при ошибке связи
        isReadingPkt = false;
        continue;
      }
      // Если поймали стоповый байт
      if (b == 0x7E) {
        isReadingPkt = false;
        process_packet(pktBuffer, pktIndex);
      }
    }
  }
}

// Функция обработки и фильтрации одного готового пакета
void process_packet(const byte* pkt, int len) {
  // Минимальный кадр: Header(1), Type(1), Command(1), PL(2), Checksum(1), End(1).
  if (len < 7) return;
  
  const byte frameType = pkt[1];
  const byte command = pkt[2];
  const int dataLen = (static_cast<int>(pkt[3]) << 8) | pkt[4];
  
  // Полный кадр: Header + Type + Command + PL(2) + Data + Checksum + End.
  if (len != 7 + dataLen) return;

  // Уведомление о прочитанной метке: Type=0x02, Command=0x22.
  if (frameType == 0x02 && command == 0x22) {
    // Данные: RSSI (1) + PC (2) + EPC (переменная длина) + CRC (2).
    if (dataLen < 5) return;

    const int epcLength = ((pkt[6] >> 3) & 0x1F) * 2; // длина EPC из PC, в байтах
    const int epcStartIndex = 8;
    if (epcLength == 0 || dataLen != 5 + epcLength) return;

    Serial.print("Обнаружена метка [EPC]: ");
    for (int i = 0; i < epcLength; i++) {
      if (pkt[epcStartIndex + i] < 0x10) Serial.print("0");
      Serial.print(pkt[epcStartIndex + i], HEX);
      Serial.print(" ");
    }

    // RSSI передается как знаковое 8-битное значение в дополнительном коде.
    const int rssi = static_cast<int8_t>(pkt[5]);
    Serial.print(" | RSSI: ");
    Serial.print(rssi);
    Serial.println(" dBm");
  } else if (frameType == 0x01 && command == 0xFF && dataLen >= 1) {
    // Документированный ответ об ошибке инвентаризации, например 0x15.
    Serial.print("Ошибка RFID-команды, код: 0x");
    if (pkt[5] < 0x10) Serial.print("0");
    Serial.println(pkt[5], HEX);
  }
}
