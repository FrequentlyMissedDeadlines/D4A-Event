/**
 * @file K10sensorsService.h
 * @brief Header for sensor module integration with the main application.
 */
#pragma once

#include <WebServer.h>
#include <unihiker_k10.h>
#include "../services/HasRoutesInterface.h"
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

class K10SensorsService : public HasRoutesInterface, public IsServiceInterface
{
public:

    SensorSnapshot collectSensorSnapshot();

    std::set<std::string> getRoutes() override;
    bool registerRoutes(WebServer *server, std::string basePath) override;
    bool init() override;
    bool start() override;
    bool stop() override; 

private:
    bool buildSensorJson(const SensorSnapshot &snapshot, char *buffer, size_t bufferLen);
    bool sensorReady();
    std::string sensorsRoutePath = "/api/sensors";
};
