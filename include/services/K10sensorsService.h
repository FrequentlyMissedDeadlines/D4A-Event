/**
 * @file K10sensorsService.h
 * @brief Header for aht20_sensor module integration with the main application.
 */
#pragma once

#include <ESPAsyncWebServer.h>
#include <unihiker_k10.h>
#include "IsOpenAPIInterface.h"
#include "IsServiceInterface.h"
#include "isUDPMessageHandlerInterface.h"

constexpr size_t kSensorResponseBufferSize = 192;

struct SensorSnapshot
{
    uint16_t light;
    float humidity;
    float temperature;
    uint64_t micData;
    int16_t accelerometerX;
    int16_t accelerometerY;
    int16_t accelerometerZ;
};

class K10SensorsService : public IsOpenAPIInterface, public IsUDPMessageHandlerInterface
{
public:

    bool registerRoutes() override;
    std::string getPath(const std::string& finalpathstring) override;
    std::string getServiceSubPath() override;
    std::string getServiceName() override;

    bool messageHandler(const std::string &message,
                        const IPAddress &remoteIP,
                        uint16_t remotePort) override;

    IsUDPMessageHandlerInterface *asUDPMessageHandlerInterface() override { return this; }

private:

    std::string baseServicePath;  // Cached for optimization
    
    std::string getSensorJson();
    bool sensorReady();

};
