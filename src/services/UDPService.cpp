#include "UDPService.h"
#include "FlashStringHelper.h"
/**
 * @file UDPService.cpp
 * @brief Implementation for UDP server integration
 * @details Exposed routes:
 *          - GET /api/udp/v1/ - Get UDP server statistics including total messages, dropped packets, buffer usage, and recent message history
 * 
 */

#include <Arduino.h>
#include <AsyncUDP.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// UDPService constants namespace
namespace UDPConsts
{
    constexpr const char msg_failed_alloc_udp[] PROGMEM = "Failed to allocate AsyncUDP instance";
    constexpr const char msg_missing_udp_handle[] PROGMEM = "Missing UDP handle or invalid port ";
    constexpr const char msg_failed_start_udp[] PROGMEM = "Failed to start UDP on port ";
    constexpr const char msg_buffer_locked[] PROGMEM = "I'm busy (buffer locked), retry later.";
    constexpr const char str_service_name[] PROGMEM = "UDP Service";
    constexpr const char path_service[] PROGMEM = "udp/v1";
    constexpr const char field_total[] PROGMEM = "total";
    constexpr const char field_dropped[] PROGMEM = "dropped";
    constexpr const char field_buffer[] PROGMEM = "buffer";
    constexpr const char field_messages[] PROGMEM = "messages";
    constexpr const char field_port[] PROGMEM = "port";
    constexpr const char resp_ok[] PROGMEM = "UDP server statistics retrieved successfully";
    constexpr const char desc_route[] PROGMEM = "Get UDP server statistics including total messages received, dropped packets, buffer usage, and recent message history with inter-arrival times";
    constexpr const char tag_udp[] PROGMEM = "UDP";
}

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
unsigned long packetsDropped = 0;
static UDPService* udp_service_instance = nullptr;

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

  // Invoke registered handlers first
  if (udp_service_instance && udp_service_instance->handler_mutex)
  {
    if (xSemaphoreTake(udp_service_instance->handler_mutex, 10 / portTICK_PERIOD_MS))
    {
      std::string msg(buffer);
      IPAddress remoteIP = packet.remoteIP();
      uint16_t remotePort = packet.remotePort();
      
      for (const auto& entry : udp_service_instance->message_handlers)
      {
        try {
          entry.handler_callback(msg, remoteIP, remotePort);
        } catch (...) {
          // Silently ignore handler exceptions
        }
      }
      xSemaphoreGive(udp_service_instance->handler_mutex);
    }
  }

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
      logger->error(std::string(UDPConsts::msg_failed_alloc_udp));
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
  if (!handler_mutex)
  {
    handler_mutex = xSemaphoreCreateMutex();
  }
  udp_service_instance = this;
  setServiceStatus(INITIALIZED);
  #ifdef VERBOSE_DEBUG
  logger->debug(getServiceName() + " " + getStatusString());   
  #endif
  return true;
}

bool UDPService::startService()
{
  if (!udpHandle || port <= 0)
  {
    logger->error(std::string(UDPConsts::msg_missing_udp_handle) + std::to_string(port));
    return false;
  }
  
  if (udpHandle->listen(port))
  {
    udpHandle->onPacket(handleUDPPacket);
    setServiceStatus(STARTED);
  #ifdef VERBOSE_DEBUG
    logger->debug(getServiceName() + " " + getStatusString());   
  #endif
    return true;
  }
  setServiceStatus(START_FAILED);
  logger->error(std::string(UDPConsts::msg_failed_start_udp) + std::to_string(port));
  logger->error(getServiceName() + " " + getStatusString());   
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
  if (handler_mutex)
  {
    vSemaphoreDelete(handler_mutex);
    handler_mutex = nullptr;
  }
  udp_service_instance = nullptr;
  setServiceStatus(STOPPED);
  #ifdef VERBOSE_DEBUG
    logger->debug(getServiceName() + " "  + getStatusString());   
  #endif
  return true;
}
std::string UDPService::buildJson()
{
  JsonDocument doc;

  if (!messageMutex)
  { doc[FPSTR(UDPConsts::field_port)]=port;
    doc[FPSTR(UDPConsts::field_total)] = 0;
    doc[FPSTR(UDPConsts::field_dropped)] = 0;
    doc[FPSTR(UDPConsts::field_buffer)] = "0/0";
    doc[FPSTR(UDPConsts::field_messages)] = serialized("[]");
    String output;
    serializeJson(doc, output);
    return std::string(output.c_str());
  }

  if (xSemaphoreTake(messageMutex, 100 / portTICK_PERIOD_MS))
  {
    doc[FPSTR(UDPConsts::field_total)] = totalMessages;
    doc[FPSTR(UDPConsts::field_dropped)] = packetsDropped;

    char bufferStr[16];
    snprintf(bufferStr, sizeof(bufferStr), "%d/%d", messageCount, MAX_MESSAGES);
    doc[FPSTR(UDPConsts::field_buffer)] = bufferStr;

    JsonArray messagesArray = doc[FPSTR(UDPConsts::field_messages)].to<JsonArray>();
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
    doc[RoutesConsts::field_error] = FPSTR(UDPConsts::msg_buffer_locked);
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
  std::string path = getPath("");
#ifdef VERBOSE_DEBUG
  logger->debug("Registering " + path);
#endif
  
  // Define response schemas
  std::vector<OpenAPIResponse> responses;
  OpenAPIResponse successResponse(200, UDPConsts::resp_ok);
  successResponse.schema = "{\"type\":\"object\",\"properties\":{\"port\":{\"type\":\"integer\",\"description\":\"UDP listening port\"},\"total\":{\"type\":\"integer\",\"description\":\"Total messages received since start\"},\"dropped\":{\"type\":\"integer\",\"description\":\"Number of packets dropped due to buffer lock\"},\"buffer\":{\"type\":\"string\",\"description\":\"Current buffer usage (used/max)\"},\"messages\":{\"type\":\"array\",\"description\":\"Recent messages with timestamps\",\"items\":{\"type\":\"string\"}},\"error\":{\"type\":\"string\",\"description\":\"Error message if buffer is locked\"}}}";
  successResponse.example = "{\"port\":12345,\"total\":1523,\"dropped\":5,\"buffer\":\"15/20\",\"messages\":[\"[125 ms] Hello\",\"[230 ms] World\"]}";
  responses.push_back(successResponse);
  responses.push_back(createServiceNotStartedResponse());
  
  registerOpenAPIRoute(OpenAPIRoute(path.c_str(), RoutesConsts::method_get,
                                     UDPConsts::desc_route,
                                     UDPConsts::tag_udp, false, {}, responses));
  
  webserver.on(path.c_str(), HTTP_GET, [this]()
             {
    if (!checkServiceStarted()) return;
    std::string json = this->buildJson();
    webserver.send(200, RoutesConsts::mime_json, json.c_str()); });

  registerSettingsRoutes("UDP", this);

  return true;
}

std::string UDPService::getServiceName()
{
  return fpstr_to_string(FPSTR(UDPConsts::str_service_name));
}
std::string UDPService::getServiceSubPath()
{
    return fpstr_to_string(FPSTR(UDPConsts::path_service));
}

bool UDPService::saveSettings()
{
    // To be implemented if needed
    return true;
}

bool UDPService::loadSettings()
{
    // To be implemented if needed
    return true;
}

/**
 * @brief Register a message handler callback
 * @param handler The callback function to register
 * @return A unique handler ID that can be used to unregister the handler
 */
int UDPService::registerMessageHandler(UDPMessageHandler handler)
{
    if (!handler_mutex)
    {
        return -1;
    }

    int handler_id = -1;
    if (xSemaphoreTake(handler_mutex, 100 / portTICK_PERIOD_MS))
    {
        handler_id = next_handler_id++;
        MessageHandlerEntry entry;
        entry.handler_id = handler_id;
        entry.handler_callback = handler;
        message_handlers.push_back(entry);
        xSemaphoreGive(handler_mutex);
    }
    return handler_id;
}

/**
 * @brief Unregister a message handler by ID
 * @param handlerId The ID returned by registerMessageHandler
 * @return true if the handler was found and removed, false otherwise
 */
bool UDPService::unregisterMessageHandler(int handlerId)
{
    if (!handler_mutex)
    {
        return false;
    }

    bool found = false;
    if (xSemaphoreTake(handler_mutex, 100 / portTICK_PERIOD_MS))
    {
        for (auto it = message_handlers.begin(); it != message_handlers.end(); ++it)
        {
            if (it->handler_id == handlerId)
            {
                message_handlers.erase(it);
                found = true;
                break;
            }
        }
        xSemaphoreGive(handler_mutex);
    }
    return found;
}
