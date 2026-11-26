#include "rfid_scanner.h"

#include <Arduino.h>
#include <algorithm>

namespace {
constexpr int RXD2 = 16;
constexpr int TXD2 = 17;
constexpr int MAX_PKT_LEN = 64;
constexpr size_t MAX_CAPTURED_RFID_TAGS = 64;

// Multiple Inventory: максимум 65535 циклов; опрос можно остановить командой 0x28.
const byte CMD_START_INVENTORY[] = {0xBB, 0x00, 0x27, 0x00, 0x03, 0x22, 0xFF, 0xFF, 0x4A, 0x7E};
const byte CMD_STOP_INVENTORY[] = {0xBB, 0x00, 0x28, 0x00, 0x00, 0x28, 0x7E};

byte pktBuffer[MAX_PKT_LEN];
int pktIndex = 0;
bool isReadingPkt = false;
bool timedScanActive = false;
bool timedScanComplete = false;
bool continuousInventoryActive = false;
unsigned long timedScanStartedAt = 0;
uint32_t timedScanDurationMs = 0;
std::vector<String> timedScanTags;
std::vector<String> newTags;
unsigned long lastNoTagLogAt = 0;

String bytesToHex(const byte* bytes, int length) {
  String tag;
  tag.reserve(length * 2);
  for (int i = 0; i < length; ++i) {
    if (bytes[i] < 0x10) tag += '0';
    tag += String(bytes[i], HEX);
  }
  tag.toUpperCase();
  return tag;
}

void processPacket(const byte* pkt, int len) {
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

    if (timedScanActive) {
      const String tag = bytesToHex(pkt + epcStartIndex, epcLength);
      if (timedScanTags.size() < MAX_CAPTURED_RFID_TAGS
          && std::find(timedScanTags.begin(), timedScanTags.end(), tag) == timedScanTags.end()) {
        timedScanTags.push_back(tag);
        // Новую метку сразу отдаем наружу, чтобы отправить ее на фронт без ожидания конца скана.
        newTags.push_back(tag);
      }
    }
  } else if (frameType == 0x01 && command == 0xFF && dataLen >= 1) {
    // Документированный ответ об ошибке инвентаризации, например 0x15.
    if (pkt[5] == 0x15 && millis() - lastNoTagLogAt < 1000) return;
    if (pkt[5] == 0x15) lastNoTagLogAt = millis();
    Serial.print("Ошибка RFID-команды, код: 0x");
    if (pkt[5] < 0x10) Serial.print("0");
    Serial.println(pkt[5], HEX);
  }
}
}  // namespace

void rfidScannerBegin() {
  // Serial2 — UART2 с RX и TX.
  Serial2.setRxBufferSize(4096);
  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);
  delay(500); // Даем UART стабилизироваться
}

void rfidScannerLoop() {
  while (Serial2.available() > 0) {
    const byte b = Serial2.read();

    // Ищем стартовый байт пакета.
    if (b == 0xBB && !isReadingPkt) {
      isReadingPkt = true;
      pktIndex = 0;
    }

    if (!isReadingPkt) continue;

    if (pktIndex < MAX_PKT_LEN) {
      pktBuffer[pktIndex++] = b;
    } else {
      // Защита от переполнения буфера при ошибке связи.
      isReadingPkt = false;
      continue;
    }

    if (b == 0x7E) {
      isReadingPkt = false;
      processPacket(pktBuffer, pktIndex);
    }
  }

  if (timedScanActive && millis() - timedScanStartedAt >= timedScanDurationMs) {
    if (!continuousInventoryActive) rfidScannerStopInventory();
    timedScanActive = false;
    timedScanComplete = true;
  }
}

void rfidScannerStartInventory() {
  Serial.println("Отправка команды многократного сканирования...");
  Serial2.write(CMD_START_INVENTORY, sizeof(CMD_START_INVENTORY));
}

void rfidScannerStartContinuousInventory() {
  continuousInventoryActive = true;
  rfidScannerStartInventory();
}

void rfidScannerStopInventory() {
  Serial2.write(CMD_STOP_INVENTORY, sizeof(CMD_STOP_INVENTORY));
}

bool rfidScannerStartTimedScan(uint32_t durationMs) {
  if (timedScanActive || durationMs == 0) return false;
  timedScanTags.clear();
  newTags.clear();
  timedScanComplete = false;
  timedScanDurationMs = durationMs;
  timedScanStartedAt = millis();
  timedScanActive = true;
  if (!continuousInventoryActive) rfidScannerStartInventory();
  return true;
}

RfidTimedScanResult rfidScannerTakeTimedScanResult() {
  if (!timedScanComplete) return {false, {}};
  timedScanComplete = false;
  return {true, timedScanTags};
}

std::vector<String> rfidScannerTakeNewTags() {
  std::vector<String> tags;
  tags.swap(newTags);
  return tags;
}
