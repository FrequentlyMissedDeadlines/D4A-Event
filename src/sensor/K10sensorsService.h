/**
 * @file K10sensorsService.h
 * @brief Header for aht20_sensor module integration with the main application.
 */
#pragma once

#include <WebServer.h>
#include <unihiker_k10.h>
#include "../services/IsOpenAPIInterface.h"
#include "../services/IsServiceInterface.h"

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

class K10SensorsService : public IsOpenAPIInterface, public IsServiceInterface
{
public:
    IsOpenAPIInterface* asOpenAPIInterface() override { return this; }



    bool registerRoutes() override;
    std::string getPath(const std::string& finalpathstring) override;

    bool initializeService() override;
    bool startService() override;
    bool stopService() override;
    std::string getName() override;

private:
    std::string baseServicePath;  // Cached for optimization
    
    std::string getSensorJson();
    bool sensorReady();

};
