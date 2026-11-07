#include <Arduino.h>

#define RXD2 16
#define TXD2 17

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
  while (!Serial);
  
  Serial2.begin(38400, SERIAL_8N1, RXD2, TXD2);
  delay(500); // Даем UART стабилизироваться
  
  Serial.println("--- UHF RFID YRM1002: Multiple Inventory ---");
  
  // Запускаем многократный опрос
  Serial.println("Отправка команды многократного сканирования...");
  Serial2.write(CMD_START_INVENTORY, sizeof(CMD_START_INVENTORY));
}

void loop() {
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
