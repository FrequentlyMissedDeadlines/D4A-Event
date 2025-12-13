#ifndef SENSOR_HANDLER_H
#define SENSOR_HANDLER_H

#include <WebServer.h>
#include <unihiker_k10.h>

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
class SensorModule
{
public:
    bool registerRoutes(WebServer *server, UNIHIKER_K10 &k10);

private:
    bool buildSensorJson(const SensorSnapshot &snapshot, char *buffer, size_t bufferLen);
    bool sensorReady();
    SensorSnapshot collectSensorSnapshot(UNIHIKER_K10 &k10);
};

#endif  // SENSOR_HANDLER_H