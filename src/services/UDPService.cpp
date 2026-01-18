#include "UDPService.h"
#include <Arduino.h>
#include <AsyncUDP.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <WebServer.h>

// Mirror of previous globals, but scoped to this module
static const int MAX_MESSAGES = 20;
static const int MAX_MESSAGE_LEN = 256;
 char messages[MAX_MESSAGES][MAX_MESSAGE_LEN];
 int messageCount = 0;
 int messageIndex = 0;
 unsigned long totalMessages = 0;
 unsigned long lastMessageTime = 0;
 char lastMessage[MAX_MESSAGE_LEN] = "";
 SemaphoreHandle_t messageMutex = NULL;
 AsyncUDP *udpInstance = nullptr;
 unsigned long packetsDropped = 0; // Add this

 std::set <std::string> routes = {};

void handleUDPPacket(AsyncUDPPacket packet)
{
  if (packet.length() == 0)
    return;

  char buffer[MAX_MESSAGE_LEN];
  int len = packet.length();
  if (len >= MAX_MESSAGE_LEN)
    len = MAX_MESSAGE_LEN - 1;
  memcpy(buffer, packet.data(), len);
  buffer[len] = '\0';

  // Try to take mutex without blocking
  if (xSemaphoreTake(messageMutex, 0))
  {
    unsigned long currentTime = millis();
    unsigned long deltaMs = currentTime - lastMessageTime;
    lastMessageTime = currentTime;

    char fullMessage[MAX_MESSAGE_LEN];
    snprintf(fullMessage, MAX_MESSAGE_LEN, "[%lu ms] %s", deltaMs, buffer);

    strncpy(messages[messageIndex], fullMessage, MAX_MESSAGE_LEN - 1);
    messages[messageIndex][MAX_MESSAGE_LEN - 1] = '\0';
    messageIndex = (messageIndex + 1) % MAX_MESSAGES;

    totalMessages++;
    if (messageCount < MAX_MESSAGES)
      messageCount++;

    strncpy(lastMessage, buffer, MAX_MESSAGE_LEN - 1);
    lastMessage[MAX_MESSAGE_LEN - 1] = '\0';

    xSemaphoreGive(messageMutex);
  }
  else
  {
    // Could not take mutex, increment dropped packets
    packetsDropped++;
  }
}

bool UDPService::begin(AsyncUDP *udpPtr, int portNum)
{
  this->udp = udpPtr;
  this->port = portNum;
  return true;
}

bool UDPService::init()
{
  if (!messageMutex)
  {
    messageMutex = xSemaphoreCreateMutex();
  }
  return true;
}

bool UDPService::stop()
{
  if (udpInstance)
  {
    udpInstance->close();
    udpInstance = nullptr;
  }
  if (messageMutex)
  {
    vSemaphoreDelete(messageMutex);
    messageMutex = nullptr;
  }
  return true;    
}
bool UDPService::start()
{
  if (!udp || port == 0)
    return false;

  udpInstance = udp;
  if (udpInstance->listen(port))
  {
    udpInstance->onPacket(handleUDPPacket);
    return true;
  }
  return false;
}

std::string UDPService::buildJson()
{

  if (!messageMutex)
    return "{\"total\": 0, \"dropped\": 0, \"buffer\": 0, \"messages\": []}";

  std::string json;

  if (xSemaphoreTake(messageMutex, 100 / portTICK_PERIOD_MS))
  {

    json += "{\"total\":" + std::to_string(totalMessages) + ", ";
    json += "\"dropped\": " + std::to_string(packetsDropped) + ",";
    json += "\"buffer\": \"" + std::to_string(messageCount) + "/" + std::to_string(MAX_MESSAGES) + "\", ";
    json += "\"messages\": [";
    int count = (messageCount < MAX_MESSAGES) ? messageCount : MAX_MESSAGES;
    for (int i = 0; i < count; i++)
    {
      int idx = (messageIndex - 1 - i + MAX_MESSAGES) % MAX_MESSAGES;
      if (messages[idx][0] != '\0')
      {
        // Escape the message string for JSON
        std::string escaped = std::string(messages[idx]);
        size_t pos = 0;
        while ((pos = escaped.find('\\', pos)) != std::string::npos) {
          escaped.replace(pos, 1, "\\\\");
          pos += 2;
        }
        pos = 0;
        while ((pos = escaped.find('"', pos)) != std::string::npos) {
          escaped.replace(pos, 1, "\\\"");
          pos += 2;
        }
        pos = 0;
        while ((pos = escaped.find('\n', pos)) != std::string::npos) {
          escaped.replace(pos, 1, "\\n");
          pos += 2;
        }
        pos = 0;
        while ((pos = escaped.find('\r', pos)) != std::string::npos) {
          escaped.replace(pos, 1, "\\r");
          pos += 2;
        }
        json += "\"" + escaped + "\"";
      }
      else
      {
        json += "\"\"";
      }
      if (i < count - 1)
      {
        json += ",";
      }
    }

    json += "]}";
    xSemaphoreGive(messageMutex);
  }
  else
  {
    json = "{\"error\": \"I'm busy (buffer locked), retry later.\"}";
  }

  return json;
}

// bool UDPService::tryCopyDisplay(char *outBuf, int bufLen, int &outTotalMessages)
// {
//   if (!messageMutex)
//     return false;
//   bool ok = false;
//   if (xSemaphoreTake(messageMutex, 10 / portTICK_PERIOD_MS))
//   {
//     strncpy(outBuf, lastMessage, bufLen - 1);
//     outBuf[bufLen - 1] = '\0';
//     outTotalMessages = (int)totalMessages;
//     xSemaphoreGive(messageMutex);
//     ok = true;
//   }
//   return ok;
// }

unsigned long UDPService::getDroppedPackets()
{
  unsigned long dropped = 0;
  if (messageMutex && xSemaphoreTake(messageMutex, 10 / portTICK_PERIOD_MS))
  {
    dropped = packetsDropped;
    xSemaphoreGive(messageMutex);    
  }
  return dropped;
}


unsigned long UDPService::getHandledPackets()
{
  unsigned long handled = 0;
  if (messageMutex && xSemaphoreTake(messageMutex, 10 / portTICK_PERIOD_MS))
  {
    handled = totalMessages;
    xSemaphoreGive(messageMutex);
  }
  return handled;
}


bool UDPService::registerRoutes(WebServer *server, std::string basePath)
{
  if (!server)
    return false;
  std::string path = "/api/" + basePath; 
  server->on(path.c_str(), HTTP_GET, [server, this]()
             {
    std::string json = this->buildJson();
    server->send(200, "application/json", json.c_str()); });

  routes.insert(path);
  return true;
}
