#include "sensor_handler.h"
#include <WebServer.h>
#include <inttypes.h>
#include <cstdio>
#include <cstdint>

#ifdef DEBUG
#define DEBUG_TO_SERIAL(x) Serial.println(x)
#define DEBUGF_TO_SERIAL(fmt, ...) Serial.printf(fmt, __VA_ARGS__)
#else
#define DEBUG_TO_SERIAL(x)
#define DEBUGF_TO_SERIAL(fmt, ...)
#endif
#include <unihiker_k10.h>

DFRobot_AHT20 sensor;

bool SensorModule::sensorReady()
{
  if (sensor.startMeasurementReady())
  {
    return true;
  }
  DEBUG_TO_SERIAL("WARN: AHT20 sensor not ready for measurement");
  return false;
}

SensorSnapshot SensorModule::collectSensorSnapshot(UNIHIKER_K10 &k10)
{
  SensorSnapshot snapshot{};
  snapshot.light = k10.readALS();
  snapshot.humidity = sensor.getHumidity_RH();
  snapshot.temperature = sensor.getTemperature_C();
  snapshot.micData = k10.readMICData();
  snapshot.accelerometerX = k10.getAccelerometerX();
  snapshot.accelerometerY = k10.getAccelerometerY();
  snapshot.accelerometerZ = k10.getAccelerometerZ();
  return snapshot;
}

bool SensorModule::buildSensorJson(const SensorSnapshot &snapshot, char *buffer, size_t bufferLen)
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
    DEBUG_TO_SERIAL("ERROR: Sensor JSON buffer too small");
    return false;
  }
  return true;
}

bool SensorModule::registerRoutes(WebServer *server, UNIHIKER_K10 &k10)
{
  if (!server)
  {
    DEBUG_TO_SERIAL("ERROR: Cannot register sensor routes - WebServer pointer is NULL!");
    return false;
  }
  DEBUG_TO_SERIAL("Registering sensor routes with WebServer...");

  if (sensor.begin() != 0)
  {
    DEBUG_TO_SERIAL("ERROR: Failed to initialize AHT20 sensor");
    server->on("/api/sensors", HTTP_GET, [server]() {
    server->send(503, "application/json", "{\"error\":\"ERROR: Failed to initialize AHT20 sensor\"}"); 
  });
    return true;
  }

  if (!sensorReady())
  {
    DEBUG_TO_SERIAL("ERROR: AHT20 sensor measurement not reAuto-detected: /dev/ttyACM1ady during initialization");
    server->on("/api/sensors", HTTP_GET, [server]() {
    server->send(503, "application/json", "{\"error\":\"AHT20 sensor measurement not ready during initialization\"}"); 
  });
    return true;
  }

  server->on("/api/sensors", HTTP_GET, [server, &k10, this]()
             {
         DEBUG_TO_SERIAL("GET /api/sensors (sensor data)");



    const SensorSnapshot snapshot = this->collectSensorSnapshot(k10);
    char response[kSensorResponseBufferSize];
    if (!this->buildSensorJson(snapshot, response, sizeof(response))) {
      server->send(503, "application/json", "{\"error\":\"sensors error.\"}");
      return;
    }

    server->send(200, "application/json", response); });

  return (true);
}
