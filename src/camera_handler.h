// Camera handler module - uses who_camera API only
// ⚠️ ALERT: This module uses ONLY who_camera API - NO esp_camera direct calls
#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// Camera frame buffer info
typedef struct {
    uint8_t* data;
    size_t length;
    uint32_t timestamp;
} CameraFrame;

// Initialize camera with who_camera API
// Returns true if successful
bool CameraHandler_init(void);

// Start camera capture task
// Returns true if task started successfully
bool CameraHandler_startCapture(void);

// Stop camera capture task
void CameraHandler_stopCapture(void);

// Get the latest frame queue handle (thread-safe)
// Returns the FreeRTOS queue handle for frame reception
QueueHandle_t CameraHandler_getFrameQueue(void);

// Get camera status
bool CameraHandler_isRunning(void);

// Get frame count
unsigned long CameraHandler_getFrameCount(void);

// Get last frame timestamp
uint32_t CameraHandler_getLastFrameTime(void);

// Cleanup camera resources
void CameraHandler_cleanup(void);
