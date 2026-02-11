/**
 * @file K10sensorsService.h
 * @brief Header for aht20_sensor module integration with the main application.
 */
#pragma once

#include <WebServer.h>
#include <unihiker_k10.h>
#include "../IsOpenAPIInterface.h"
#include "../IsServiceInterface.h"

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

class K10SensorsService : public IsOpenAPIInterface
{
public:

    bool registerRoutes() override;
    std::string getPath(const std::string& finalpathstring) override;
    std::string getServiceSubPath() override;
    std::string getServiceName() override;

private:

    std::string baseServicePath;  // Cached for optimization
    
    std::string getSensorJson();
    bool sensorReady();

};
