#include "UDPService.h"
#include <Arduino.h>
#include <AsyncUDP.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <WebServer.h>
#include <ArduinoJson.h>

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
unsigned long packetsDropped = 0; // Add this

bool UDPService::begin(AsyncUDP *u, int p)
{
  if (u == nullptr)
  {
    udp = nullptr;
    udpHandle = nullptr;
    udpOwned = false;
  }
  else
  {
    udp = u;
    udpHandle = u;
    udpOwned = false;
  }

  if (p > 0)
    port = p;

  return true;
}

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



bool UDPService::initializeService()
{
  if (!udp)
  {
    // Create our own AsyncUDP instance when none provided via begin()
    udpHandle = new AsyncUDP();
    if (!udpHandle)
    {
      logger->error("Failed to allocate AsyncUDP instance");
      return false;
    }
    udpOwned = true;
  }
  else
  {
    // Use the provided AsyncUDP instance
    udpHandle = udp;
    udpOwned = false;
  }
  if (!messageMutex)
  {
    messageMutex = xSemaphoreCreateMutex();
  }
  return true;
}

bool UDPService::startService()
{
  if (!udpHandle || port <= 0)
  {
    logger->error("Missing UDP handle or invalid port " + std::to_string(port));
    return false;
  }
  
  if (udpHandle->listen(port))
  {
    udpHandle->onPacket(handleUDPPacket);
    return true;
  }
  logger->error("Failed to start UDP on port " + std::to_string(port));
  return false;
}

bool UDPService::stopService()
{
  if (udpHandle)
  {
    udpHandle->close();
    if (udpOwned)
    {
      delete udpHandle;
    }
    udpHandle = nullptr;
    udp = nullptr;
    udpOwned = false;
  }
  if (messageMutex)
  {
    vSemaphoreDelete(messageMutex);
    messageMutex = nullptr;
  }
  return true;
}
std::string UDPService::buildJson()
{
  JsonDocument doc;

  if (!messageMutex)
  { doc["port"]=port;
    doc["total"] = 0;
    doc["dropped"] = 0;
    doc["buffer"] = "0/0";
    doc["messages"] = serialized("[]");
    String output;
    serializeJson(doc, output);
    return std::string(output.c_str());
  }

  if (xSemaphoreTake(messageMutex, 100 / portTICK_PERIOD_MS))
  {
    doc["total"] = totalMessages;
    doc["dropped"] = packetsDropped;

    char bufferStr[16];
    snprintf(bufferStr, sizeof(bufferStr), "%d/%d", messageCount, MAX_MESSAGES);
    doc["buffer"] = bufferStr;

    JsonArray messagesArray = doc["messages"].to<JsonArray>();
    int count = (messageCount < MAX_MESSAGES) ? messageCount : MAX_MESSAGES;
    for (int i = 0; i < count; i++)
    {
      int idx = (messageIndex - 1 - i + MAX_MESSAGES) % MAX_MESSAGES;
      if (messages[idx][0] != '\0')
        messagesArray.add(messages[idx]);
      else
        messagesArray.add("");
    }

    xSemaphoreGive(messageMutex);
  }
  else
  {
    doc["error"] = "I'm busy (buffer locked), retry later.";
  }

  String output;
  serializeJson(doc, output);
  return std::string(output.c_str());
}

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


bool UDPService::registerRoutes()
{
  std::string path = std::string(RoutesConsts::kPathAPI) + getName();
#ifdef DEBUG
  logger->debug("Registering " + path);
#endif
  
  // Define response schemas
  std::vector<OpenAPIResponse> responses;
  OpenAPIResponse successResponse(200, "UDP server statistics retrieved successfully");
  successResponse.schema = "{\"type\":\"object\",\"properties\":{\"port\":{\"type\":\"integer\",\"description\":\"UDP listening port\"},\"total\":{\"type\":\"integer\",\"description\":\"Total messages received since start\"},\"dropped\":{\"type\":\"integer\",\"description\":\"Number of packets dropped due to buffer lock\"},\"buffer\":{\"type\":\"string\",\"description\":\"Current buffer usage (used/max)\"},\"messages\":{\"type\":\"array\",\"description\":\"Recent messages with timestamps\",\"items\":{\"type\":\"string\"}},\"error\":{\"type\":\"string\",\"description\":\"Error message if buffer is locked\"}}}";
  successResponse.example = "{\"port\":12345,\"total\":1523,\"dropped\":5,\"buffer\":\"15/20\",\"messages\":[\"[125 ms] Hello\",\"[230 ms] World\"]}";
  responses.push_back(successResponse);
  
  registerOpenAPIRoute(OpenAPIRoute(path.c_str(), "GET", 
                                     "Get UDP server statistics including total messages received, dropped packets, buffer usage, and recent message history with inter-arrival times",
                                     "UDP", false, {}, responses));
  
  webserver.on(path.c_str(), HTTP_GET, [this]()
             {
    std::string json = this->buildJson();
    webserver.send(200, RoutesConsts::kMimeJSON, json.c_str()); });

  return true;
}

std::string UDPService::getName()
{
  return "udp/v1";
}

std::string UDPService::getPath(const std::string& finalpathstring)
{
  if (baseServicePath.empty()) {
    // Cache base path on first call
    std::string serviceName = getName();
    size_t slashPos = serviceName.find('/');
    if (slashPos != std::string::npos) {
      serviceName = serviceName.substr(0, slashPos);
    }
    baseServicePath = std::string(RoutesConsts::kPathAPI) + serviceName + "/";
  }
  return baseServicePath + finalpathstring;
}
