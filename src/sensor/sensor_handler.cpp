#include "sensor_handler.h"
#include <WebServer.h>

#ifdef DEBUG
#define DEBUG_TO_SERIAL(x) Serial.println(x)
#define DEBUGF_TO_SERIAL(fmt, ...) Serial.printf(fmt, __VA_ARGS__)
#else
#define DEBUG_TO_SERIAL(x)
#define DEBUGF_TO_SERIAL(fmt, ...)
#endif
#include <unihiker_k10.h>
UNIHIKER_K10 unihiker;
DFRobot_AHT20 sensor;

bool WebServerModule_registerSensors (WebServer* server){
   if (!server) {
        DEBUG_TO_SERIAL("ERROR: Cannot register sensor routes - WebServer pointer is NULL!");
        return false;
    }
     DEBUG_TO_SERIAL("Registering sensor routes with WebServer...");

    if (sensor.begin()!=0){
        DEBUG_TO_SERIAL("ERROR: Failed to initialize AHT20 sensor");
        return false;
    }
    if (!sensor.startMeasurementReady()){
        DEBUG_TO_SERIAL("ERROR: AHT20 sensor measurement not ready");
        return false;
    }

      server->on("/api/sensor", HTTP_PUT, [server]() {
        DEBUG_TO_SERIAL("PUT /api/sensor (sensor data)");

        uint16_t als = unihiker.readALS();
        float humidity = sensor.getHumidity_RH();
        float temperature = sensor.getTemperature_C();
        u_int64_t micData = unihiker.readMICData();
        int16_t accelerometerX = unihiker.getAccelerometerX();
        int16_t accelerometerY = unihiker.getAccelerometerY();
        int16_t accelerometerZ = unihiker.getAccelerometerZ();
        String jsonResponse = "{";
        jsonResponse += "\"light\":" + String(als) + ",";
        jsonResponse += "\"humidity\":" + String(humidity, 2) + ",";
        jsonResponse += "\"temperature\":" + String(temperature, 2) + ",";
        jsonResponse += "\"mic_data\":" + String(micData) + ",";
        jsonResponse += "\"accelerometer\":[" + String(accelerometerX) + ","+ String(accelerometerY) + ","  + String(accelerometerZ);
        jsonResponse += "]}";

        server->send(200, "application/json", jsonResponse);
      });
  

  return (true);}