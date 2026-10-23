#include <Arduino.h>

#define RXD2 16
#define TXD2 17

// Команда запуска непрерывного опроса (Continuous Poll)
// Параметры: 0x00 0x00 (опрашивать бесконечно, пока не остановим)
const byte CMD_START_CONTINUOUS[] = {0xBB, 0x00, 0x27, 0x00, 0x03, 0x22, 0x00, 0x00, 0x4C, 0x7E};

// Команда остановки опроса (может пригодиться для управления)
const byte CMD_STOP_CONTINUOUS[]  = {0xBB, 0x00, 0x28, 0x00, 0x00, 0x28, 0x7E};

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
  
  Serial.println("--- UHF RFID YRM1002: Continuous Mode ---");
  
  // Запускаем непрерывный опрос
  Serial.println("Отправка команды запуска сканирования...");
  Serial2.write(CMD_START_CONTINUOUS, sizeof(CMD_START_CONTINUOUS));
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
  // Минимальный пакет должен содержать Header(1), Type(1), Len(2), Checksum(1), End(1) = 6 байт
  if (len < 6) return; 
  
  byte type = pkt[2]; // Тип команды/ответа
  
  // Если это пакет с данными метки (тип 0x22)
  if (type == 0x22) {
    // В структуре YRM1002 длина данных находится в байтах pkt[3] и pkt[4]
    int dataLen = (pkt[3] << 8) | pkt[4];
    
    // Проверяем, что пакет содержит EPC (стандартный EPC = 12 байт + RSSI и PC)
    // Обычно для 12-байтного EPC длина dataLen равна 15-19 байт в зависимости от настроек
    if (dataLen >= 15 && len >= 21) {
      
      // EPC в пакете YRM1002 обычно начинается с 8-го байта (индекс 7)
      // Структура данных внутри: [RSSI (1б)] + [PC (2б)] + [EPC (12б)]
      int epcStartIndex = 8; 
      int epcLength = 12;
      
      Serial.print("Обнаружена метка [EPC]: ");
      for (int i = 0; i < epcLength; i++) {
        if (pkt[epcStartIndex + i] < 0x10) Serial.print("0");
        Serial.print(pkt[epcStartIndex + i], HEX);
        Serial.print(" ");
      }
      
      // Дополнительно выводим уровень сигнала RSSI (находится перед PC)
      int rssi = pkt[5];
      Serial.print(" | RSSI: -");
      Serial.print(rssi);
      Serial.println(" dBm");
    }
  } 
  // Если это ответ на команду старта (0x27)
  else if (type == 0x27) {
    Serial.println(">> Модуль успешно переведен в режим Continuous Poll.");
  }
}