#include "K10sensorsService.h"
#include <WebServer.h>
#include <inttypes.h>
#include <cstdio>
#include <cstdint>


DFRobot_AHT20 sensor;
extern UNIHIKER_K10 unihiker;


bool K10SensorsService::init()
{
  return true;
}
bool K10SensorsService::start()
{
  return true;
}
bool K10SensorsService::stop()
{
  return true;
}
bool K10SensorsService::sensorReady()
{
  if (sensor.startMeasurementReady())
  {
    return true;
  }
  
  return false;
}
SensorSnapshot K10SensorsService::collectSensorSnapshot()
{
  SensorSnapshot snapshot{};
  snapshot.light = unihiker.readALS();
  snapshot.humidity = sensor.getHumidity_RH();
  snapshot.temperature = sensor.getTemperature_C();
  snapshot.micData = unihiker.readMICData();
  snapshot.accelerometerX = unihiker.getAccelerometerX();
  snapshot.accelerometerY = unihiker.getAccelerometerY();
  snapshot.accelerometerZ = unihiker.getAccelerometerZ();
  return snapshot;
}

bool K10SensorsService::buildSensorJson(const SensorSnapshot &snapshot, char *buffer, size_t bufferLen)
{
  const int written = snprintf(
      buffer,
      bufferLen,
      "{\"light\":%u,\"humidity\":%.2f,\"temperature\":%.2f,\"mic_data\":%" PRIu64 ",\"accelerometer\":[%d,%d,%d]}",
      snapshot.light,
      snapshot.humidity,
      snapshot.temperature,
      snapshot.micData,
      snapshot.accelerometerX,
      snapshot.accelerometerY,
      snapshot.accelerometerZ);

  if (written < 0 || static_cast<size_t>(written) >= bufferLen)
  {
    return false;
  }
  return true;
}

std::set<std::string> K10SensorsService::getRoutes()
{
  return routes;
}

bool K10SensorsService::registerRoutes(WebServer *server, std::string basePath)
{
  if (!server)
  {
    return false;
  }

  if (sensor.begin() != 0)
  {
    server->on(sensorsRoutePath.c_str(), HTTP_GET, [server]() {
    server->send(503, "application/json", "{\"error\":\"ERROR: Failed to initialize AHT20 sensor\"}"); 
  });
    return true;
  }

  if (!sensorReady())
  {
    server->on(sensorsRoutePath.c_str(), HTTP_GET, [server]() {
    server->send(503, "application/json", "{\"error\":\"AHT20 sensor measurement not ready during initialization\"}"); 
  });
    return true;
  }

  server->on(sensorsRoutePath.c_str(), HTTP_GET, [server, this]()
             {

    const SensorSnapshot snapshot = this->collectSensorSnapshot();
    char response[kSensorResponseBufferSize];
    if (!this->buildSensorJson(snapshot, response, sizeof(response))) {
      server->send(503, "application/json", "{\"error\":\"sensors error.\"}");
      return;
    }

    server->send(200, "application/json", response); });

  return (true);
}
