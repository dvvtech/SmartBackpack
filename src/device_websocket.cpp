#include "device_websocket.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsClient.h>

#include "rfid_mappings.h"
#include "rfid_scanner.h"
#include "wifi_provisioning.h"

namespace {
constexpr char API_HOST[] = "api.scanbox.tech";
constexpr uint16_t API_PORT = 443;
constexpr unsigned long RECONNECT_INTERVAL_MS = 5000;

const char ISRG_ROOT_X1_CA[] PROGMEM =
    "-----BEGIN CERTIFICATE-----\n"
    "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\n"
    "-----END CERTIFICATE-----\n";

WebSocketsClient webSocket;
bool webSocketStarted = false;
uint16_t activeTagScanSubjectId = 0;
uint16_t candidateTagSubjectId = 0;
String candidateRfidTag;

String buildTagMappingsMessage() {
  const std::vector<RfidTagMapping> mappings = rfidMappingsList();
  String message = "{\"type\":\"tagMappings\",\"mappings\":[";
  for (size_t i = 0; i < mappings.size(); ++i) {
    if (i != 0) message += ",";
    message += "{\"subjectId\":" + String(mappings[i].subjectId) +
        ",\"rfidTag\":\"" + mappings[i].rfidTag + "\"}";
  }
  message += "]}";
  return message;
}

void sendTagMappings() {
  String message = buildTagMappingsMessage();
  webSocket.sendTXT(message);
}

bool readSubjectId(const String& message, uint16_t& subjectId) {
  const int keyIndex = message.indexOf("\"subjectId\"");
  if (keyIndex < 0) return false;
  const int colonIndex = message.indexOf(':', keyIndex);
  if (colonIndex < 0) return false;

  int valueIndex = colonIndex + 1;
  while (valueIndex < static_cast<int>(message.length()) && message[valueIndex] == ' ') ++valueIndex;
  if (valueIndex >= static_cast<int>(message.length()) || !isDigit(message[valueIndex])) return false;

  unsigned long parsed = 0;
  while (valueIndex < static_cast<int>(message.length()) && isDigit(message[valueIndex])) {
    parsed = parsed * 10 + static_cast<unsigned long>(message[valueIndex] - '0');
    if (parsed > 65535) return false;
    ++valueIndex;
  }
  if (parsed == 0) return false;
  subjectId = static_cast<uint16_t>(parsed);
  return true;
}

bool readJsonString(const String& message, const char* key, String& value) {
  const String keyToken = String("\"") + key + "\"";
  const int keyIndex = message.indexOf(keyToken);
  if (keyIndex < 0) return false;
  const int colonIndex = message.indexOf(':', keyIndex + keyToken.length());
  if (colonIndex < 0) return false;

  int valueIndex = colonIndex + 1;
  while (valueIndex < static_cast<int>(message.length()) && message[valueIndex] == ' ') ++valueIndex;
  if (valueIndex >= static_cast<int>(message.length()) || message[valueIndex] != '\"') return false;
  const int valueStart = ++valueIndex;
  while (valueIndex < static_cast<int>(message.length()) && message[valueIndex] != '\"') ++valueIndex;
  if (valueIndex >= static_cast<int>(message.length())) return false;

  value = message.substring(valueStart, valueIndex);
  return true;
}

void sendTagScanResult(const RfidTimedScanResult& result) {
  const uint16_t subjectId = activeTagScanSubjectId;
  activeTagScanSubjectId = 0;

  String response = "{\"type\":\"tagScanResult\",\"subjectId\":" + String(subjectId);
  if (result.tags.size() == 1) {
    candidateTagSubjectId = subjectId;
    candidateRfidTag = result.tags[0];
    response += ",\"success\":true,\"rfidTag\":\"" + candidateRfidTag + "\"}";
  } else {
    candidateTagSubjectId = 0;
    candidateRfidTag = "";
    response += ",\"success\":false,\"tagCount\":" + String(result.tags.size()) + "}";
  }
  webSocket.sendTXT(response);
  Serial.printf("[WS] Timed RFID scan finished for subject %u; tags=%u\n",
                subjectId, static_cast<unsigned>(result.tags.size()));
}

void onWebSocketEvent(WStype_t eventType, uint8_t* payload, size_t length) {
  switch (eventType) {
    case WStype_CONNECTED:
      Serial.printf("[WS] Connected to %s:%u\n", API_HOST, API_PORT);
      webSocket.sendTXT("{\"type\":\"deviceConnected\"}");
      sendTagMappings();
      Serial.println("[WS] RFID subject mappings sent to API.");
      break;

    case WStype_DISCONNECTED:
      Serial.println("[WS] Disconnected from ScanBox API.");
      break;

    case WStype_TEXT: {
      String message;
      message.reserve(length);
      for (size_t i = 0; i < length; ++i) {
        message += static_cast<char>(payload[i]);
      }

      if (message.indexOf("\"type\":\"scan\"") >= 0) {
        Serial.printf("[WS] Scan request received: %s\n", message.c_str());
        webSocket.sendTXT("{\"type\":\"scanReceived\"}");
      } else if (message.indexOf("\"type\":\"deleteTagMapping\"") >= 0) {
        uint16_t subjectId = 0;
        if (!readSubjectId(message, subjectId)) {
          webSocket.sendTXT("{\"type\":\"tagMappingDeleted\",\"success\":false,\"message\":\"Invalid subjectId\"}");
          break;
        }

        const bool removed = rfidMappingRemove(subjectId);
        String response = "{\"type\":\"tagMappingDeleted\",\"subjectId\":" + String(subjectId) +
            ",\"success\":" + String(removed ? "true" : "false") + "}";
        webSocket.sendTXT(response);
        sendTagMappings();
        Serial.printf("[WS] RFID mapping delete for subject %u: %s\n", subjectId, removed ? "removed" : "not found");
      } else if (message.indexOf("\"type\":\"scanTag\"") >= 0) {
        uint16_t subjectId = 0;
        if (!readSubjectId(message, subjectId)) {
          webSocket.sendTXT("{\"type\":\"tagScanResult\",\"success\":false,\"tagCount\":0,\"message\":\"Предмет не выбран.\"}");
          break;
        }
        if (!rfidScannerStartTimedScan(2000)) {
          String response = "{\"type\":\"tagScanResult\",\"subjectId\":" + String(subjectId) +
              ",\"success\":false,\"tagCount\":0,\"message\":\"Сканирование уже выполняется.\"}";
          webSocket.sendTXT(response);
          break;
        }
        candidateTagSubjectId = 0;
        candidateRfidTag = "";
        activeTagScanSubjectId = subjectId;
        Serial.printf("[WS] Starting a 2-second RFID scan for subject %u.\n", subjectId);
      } else if (message.indexOf("\"type\":\"confirmTagMapping\"") >= 0) {
        uint16_t subjectId = 0;
        String rfidTag;
        const bool parsed = readSubjectId(message, subjectId) && readJsonString(message, "rfidTag", rfidTag);
        const bool isCandidate = parsed && subjectId == candidateTagSubjectId && rfidTag == candidateRfidTag;
        const bool saved = isCandidate && rfidMappingSave(subjectId, rfidTag);
        if (saved) {
          candidateTagSubjectId = 0;
          candidateRfidTag = "";
        }
        String response = "{\"type\":\"tagMappingSaved\",\"subjectId\":" + String(subjectId) +
            ",\"success\":" + String(saved ? "true" : "false") + "}";
        webSocket.sendTXT(response);
        if (saved) sendTagMappings();
        Serial.printf("[WS] RFID mapping save for subject %u: %s\n", subjectId, saved ? "saved" : "rejected");
      } else {
        Serial.printf("[WS] Message received: %s\n", message.c_str());
      }
      break;
    }

    case WStype_ERROR:
      Serial.println("[WS] Connection error.");
      break;

    default:
      break;
  }
}
}  // namespace

void deviceWebSocketLoop() {
  if (WiFi.status() != WL_CONNECTED) {
    if (webSocketStarted) {
      webSocket.disconnect();
      webSocketStarted = false;
    }
    return;
  }

  if (!webSocketStarted) {
    String macAddress = WiFi.macAddress();
    macAddress.replace(":", "%3A");
    const String pairingToken = wifiProvisioningPairingToken();
    if (pairingToken.length() == 0) {
      Serial.println("[WS] Could not load the device pairing code.");
      return;
    }
    const String path = "/ws?role=device&mac=" + macAddress + "&token=" + pairingToken;
    Serial.printf("[WS] Connecting to wss://%s:%u%s\n", API_HOST, API_PORT, path.c_str());

    webSocket.beginSslWithCA(API_HOST, API_PORT, path.c_str(), ISRG_ROOT_X1_CA);
    webSocket.onEvent(onWebSocketEvent);
    webSocket.setReconnectInterval(RECONNECT_INTERVAL_MS);
    webSocketStarted = true;
  }

  webSocket.loop();
  if (activeTagScanSubjectId != 0) {
    const RfidTimedScanResult result = rfidScannerTakeTimedScanResult();
    if (result.ready) sendTagScanResult(result);
  }
}
