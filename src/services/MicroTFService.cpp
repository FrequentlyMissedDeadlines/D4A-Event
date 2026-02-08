#include "MicroTFService.h"
/**
 * @file MicroTFService.cpp
 * @brief NOT IMPLEMENTED for MicroTensorFlow object detection service
 * @details Exposed routes:
 *          - POST /api/microtf/v1/detect - Trigger object detection asynchronously
 *          - GET /api/microtf/v1/results - Retrieve results from last detection inference
 * 
 */

#include "WebcamService.h"
#include <ArduinoJson.h>
#include <WebServer.h>

extern WebServer webserver;
extern WebcamService webcam_service;

// Service constants namespace
namespace MicroTFConsts {
    constexpr const char svc_name[] PROGMEM = "MicroTF Service";
    constexpr const char svc_path[] PROGMEM = "microtf/v1";
    constexpr const char msg_initialized[] PROGMEM = "MicroTFService initialized";
    constexpr const char msg_not_initialized[] PROGMEM = "MicroTFService not initialized";
    constexpr const char msg_detection_complete[] PROGMEM = "Detection complete: %u ms";
}

MicroTFService::MicroTFService()
    : last_inference_time_ms(0), is_initialized(false) {
}


std::string MicroTFService::getServiceName() {
    return MicroTFConsts::svc_name;
}

bool MicroTFService::registerRoutes() {
    webserver.on(getPath("detect").c_str(), HTTP_POST, [this]() { handleDetect(); });
    webserver.on(getPath("results").c_str(), HTTP_GET, [this]() { handleGetResults(); });
    return true;
}

std::string MicroTFService::getServiceSubPath() {
    return MicroTFConsts::svc_path;
}

bool MicroTFService::detectObjects() {
    if (!is_initialized) {
        if (logger) {
            logger->error(PSTR("MicroTFService not initialized"));
        }
        return false;
    }

    uint32_t start_time = millis();

    // TODO: Implement detection pipeline:
    // 1. Capture frame from webcam_service
    // 2. Resize/preprocess to model input dimensions
    // 3. Run inference
    // 4. Post-process outputs (NMS, filtering)
    // 5. Populate last_detections vector

    last_inference_time_ms = millis() - start_time;
    if (logger) {
        logger->info(PSTR("Detection complete"));
    }
    return true;
}

const std::vector<DetectedObject>& MicroTFService::getLastDetections() const {
    return last_detections;
}

uint32_t MicroTFService::getLastInferenceTimeMs() const {
    return last_inference_time_ms;
}

void MicroTFService::handleDetect() {
    if (!checkServiceStarted()) return;
    // TODO: Trigger detection asynchronously
    webserver.send(501, "application/json", "{\"status\":\"detection_triggered\"}");
}

void MicroTFService::handleGetResults() {
    if (!checkServiceStarted()) return;
    // TODO: Serialize detection results to JSON
    // Include inference time and all detected objects with boundaries
    webserver.send(501, "application/json", "{\"objects\":[]}");
}