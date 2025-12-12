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

namespace {

constexpr size_t kSensorResponseBufferSize = 192;

struct SensorSnapshot {
  uint16_t light;
  float humidity;
  float temperature;
  uint64_t micData;
  int16_t accelerometerX;
  int16_t accelerometerY;
  int16_t accelerometerZ;
};

bool ensureSensorReady() {
  if (sensor.startMeasurementReady()) {
    return true;
  }
  DEBUG_TO_SERIAL("WARN: AHT20 sensor not ready for measurement");
  return false;
}

SensorSnapshot collectSensorSnapshot(UNIHIKER_K10 &k10) {
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

bool buildSensorJson(const SensorSnapshot &snapshot, char *buffer, size_t bufferLen) {
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

  if (written < 0 || static_cast<size_t>(written) >= bufferLen) {
    DEBUG_TO_SERIAL("ERROR: Sensor JSON buffer too small");
    return false;
  }
  return true;
}

} // namespace

bool SensorModule_registerRoutes (WebServer* server,UNIHIKER_K10 &k10){
   if (!server) {
        DEBUG_TO_SERIAL("ERROR: Cannot register sensor routes - WebServer pointer is NULL!");
        return false;
    }
     DEBUG_TO_SERIAL("Registering sensor routes with WebServer...");

    if (sensor.begin()!=0){
        DEBUG_TO_SERIAL("ERROR: Failed to initialize AHT20 sensor");
        return false;
    }
  if (!ensureSensorReady()){
    DEBUG_TO_SERIAL("ERROR: AHT20 sensor measurement not ready during initialization");
    return false;
  }

      server->on("/api/sensor", HTTP_PUT, [server, &k10]() {
        DEBUG_TO_SERIAL("PUT /api/sensor (sensor data)");

    if (!ensureSensorReady()) {
      server->send(503, "application/json", "{\"error\":\"sensor_busy\"}");
      return;
    }

    const SensorSnapshot snapshot = collectSensorSnapshot(k10);
    char response[kSensorResponseBufferSize];
    if (!buildSensorJson(snapshot, response, sizeof(response))) {
      server->send(500, "application/json", "{\"error\":\"sensor_response_overflow\"}");
      return;
    }

    server->send(200, "application/json", response);
      });
  

  return (true);}