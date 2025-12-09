#include "udp_handler.h"
#include <Arduino.h>
#include <AsyncUDP.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Mirror of previous globals, but scoped to this module
static const int MAX_MESSAGES = 20;
static const int MAX_MESSAGE_LEN = 256;
static char messages[MAX_MESSAGES][MAX_MESSAGE_LEN];
static int messageCount = 0;
static int messageIndex = 0;
static unsigned long totalMessages = 0;
static unsigned long lastMessageTime = 0;
static char lastMessage[MAX_MESSAGE_LEN] = "";
static SemaphoreHandle_t messageMutex = NULL;
static AsyncUDP* udpInstance = nullptr;
static unsigned long packetsDropped = 0;  // Add this

// Packet callback (must be non-blocking)
static void handleUDPPacket(AsyncUDPPacket packet) {
  if (packet.length() == 0) return;

  char buffer[MAX_MESSAGE_LEN];
  int len = packet.length();
  if (len >= MAX_MESSAGE_LEN) len = MAX_MESSAGE_LEN - 1;
  memcpy(buffer, packet.data(), len);
  buffer[len] = '\0';

  // Try to take mutex without blocking
  if (xSemaphoreTake(messageMutex, 0)) {
  unsigned long currentTime = millis();
  unsigned long deltaMs = currentTime - lastMessageTime;
  lastMessageTime = currentTime;

    char fullMessage[MAX_MESSAGE_LEN];
    snprintf(fullMessage, MAX_MESSAGE_LEN, "[%lu ms] %s", deltaMs, buffer);

    strncpy(messages[messageIndex], fullMessage, MAX_MESSAGE_LEN - 1);
    messages[messageIndex][MAX_MESSAGE_LEN - 1] = '\0';
    messageIndex = (messageIndex + 1) % MAX_MESSAGES;

    totalMessages++;
    if (messageCount < MAX_MESSAGES) messageCount++;

    strncpy(lastMessage, buffer, MAX_MESSAGE_LEN - 1);
    lastMessage[MAX_MESSAGE_LEN - 1] = '\0';

    xSemaphoreGive(messageMutex);
  }
  else {
    // Could not take mutex, increment dropped packets
    packetsDropped++;
  }
}

bool UDPHandler_begin(AsyncUDP* udp, int port) {
  if (!udp) return false;
  if (!messageMutex) {
    messageMutex = xSemaphoreCreateMutex();
  }

  udpInstance = udp;
  if (udpInstance->listen(port)) {
    udpInstance->onPacket(handleUDPPacket);
    return true;
  }
  return false;
}

String UDPHandler_buildJson() {
  
  if (!messageMutex) return  "{\"total\": 0, \"dropped\": 0, \"buffer\": 0, \"messages\": []}";

  String json;   

  if (xSemaphoreTake(messageMutex, 100 / portTICK_PERIOD_MS)) {

    json +="{\"total\":"+String(totalMessages)+", ";
    json  += "\"dropped\": " + String(packetsDropped) + ",";
    json +=  "\"buffer\": \""  + String(messageCount) + "/" + String(MAX_MESSAGES) + "\", ";
    json += "\"messages\": [";
    int count = (messageCount < MAX_MESSAGES) ? messageCount : MAX_MESSAGES;
    for (int i = 0; i < count; i++) {
      int idx = (messageIndex - 1 - i + MAX_MESSAGES) % MAX_MESSAGES;
      if (messages[idx][0] != '\0') {
        // Escape the message string for JSON
        String escaped = String(messages[idx]);
        escaped.replace("\\", "\\\\");  // Escape backslashes first
        escaped.replace("\"", "\\\"");  // Escape quotes
        escaped.replace("\n", "\\n");   // Escape newlines
        escaped.replace("\r", "\\r");   // Escape carriage returns
        json += "\"" + escaped + "\"";
      } else {
        json += "\"\"";
      }
      if (i < count - 1) {
        json += ",";
      }
    }

    json += "]}";
    xSemaphoreGive(messageMutex);
  } else {
    json = "{\"error\": \"I'm busy (buffer locked), retry later.\"}";
  }

  return json;
}

bool UDPHandler_tryCopyDisplay(char* outBuf, int bufLen, int& outTotalMessages) {
  if (!messageMutex) return false;
  bool ok = false;
  if (xSemaphoreTake(messageMutex, 10 / portTICK_PERIOD_MS)) {
    strncpy(outBuf, lastMessage, bufLen - 1);
    outBuf[bufLen - 1] = '\0';
    outTotalMessages = (int)totalMessages;
    xSemaphoreGive(messageMutex);
    ok = true;
  }
  return ok;
}

unsigned long UDPHandler_getDroppedPackets() {
  unsigned long dropped = 0;
  if (messageMutex && xSemaphoreTake(messageMutex, 10 / portTICK_PERIOD_MS)) {
    dropped = packetsDropped;
    xSemaphoreGive(messageMutex);
  }
  return dropped;
}

unsigned long UDPHandler_getHandledPackets() {
  unsigned long handled = 0;
  if (messageMutex && xSemaphoreTake(messageMutex, 10 / portTICK_PERIOD_MS)) {
    handled = totalMessages;
    xSemaphoreGive(messageMutex);
  }
  return handled;
}

