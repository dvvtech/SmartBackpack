#include <Arduino.h>
//#include <HardwareSerial.h>

// Пин встроенного светодиода
// Для большинства плат ESP8266 (NodeMCU, Wemos D1 Mini) это GPIO2
#define LED_PIN 2

//HardwareSerial Serial2(2);//2 - номер аппаратного UART


//Serial    UART0   Обычно USB-UART для отладки
//Serial1   UART1    Обычно только передача TX
//Serial2   UART2    Полноценный UART с RX и TX

void setup() {

  Serial.begin(115200);

  Serial2.begin(
        9600,       // Скорость UART. Скорость, настроенная на RFID-модуле
        SERIAL_8N1, // 8 бит, без чётности, 1 стоп-бит
        16,         // RX — GPIO16
        17          // TX — GPIO17
    );

  // Настраиваем пин светодиода как выход
  pinMode(LED_PIN, OUTPUT);
}

void loop() {
  // Включаем светодиод
  digitalWrite(LED_PIN, LOW);  // LOW = ВКЛ для встроенного светодиода ESP8266
  delay(1000);                   // Ждём 500 мс (0.5 сек)

  // Выключаем светодиод
  digitalWrite(LED_PIN, HIGH); // HIGH = ВЫКЛ для встроенного светодиода ESP8266
  delay(1000);                   // Ждём 500 мс

  while (Serial2.available())
  {
    uint8_t data = Serial2.read();
    Serial.printf("%02X ", data);
  }
}