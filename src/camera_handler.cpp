// Camera handler implementation - uses ONLY who_camera API
// ⚠️ ALERT: This module uses ONLY who_camera API - NO esp_camera direct calls
// Note: who_camera internally uses esp_camera, but we access it only through who_camera

#include "camera_handler.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

// who_camera header - the ONLY camera API we use
#include "who_camera.h"
// who_camera.h includes esp_camera.h, giving us access to camera_fb_t
#include "esp_camera.h"

// ============================================================================
// CONFIGURATION CONSTANTS
// ============================================================================

// Use pixformat from who_camera API
// K10 native camera supports RGB565 (not JPEG compression at hardware level)
// We'll send as RGB565 and handle JPEG compression in web server if needed
#define CAMERA_PIXFORMAT PIXFORMAT_RGB565
#define CAMERA_FRAMESIZE FRAMESIZE_VGA
#define CAMERA_FB_COUNT 2  // Double buffer for continuous capture

// Frame queue configuration - queue stores camera_fb_t* pointers
#define FRAME_QUEUE_SIZE 2
#define FRAME_QUEUE_TIMEOUT_MS 100

// ============================================================================
// STATIC MODULE STATE
// ============================================================================

// Queue stores camera_fb_t* pointers (from who_camera)
static QueueHandle_t cameraFrameQueue = NULL;
static SemaphoreHandle_t cameraMutex = NULL;
static TaskHandle_t captureTaskHandle = NULL;

static bool isRunning = false;
static unsigned long frameCount = 0;
static uint32_t lastFrameTime = 0;

// ============================================================================
// CAMERA CAPTURE TASK
// ============================================================================

static void cameraTask(void* pvParameters) {
    Serial.println("Camera task started - receiving frames from who_camera");
    
    for (;;) {
        // Wait for frame from who_camera
        // The queue will receive camera_fb_t* pointers from who_camera
        camera_fb_t* fb = NULL;
        
        if (xQueueReceive(cameraFrameQueue, &fb, FRAME_QUEUE_TIMEOUT_MS / portTICK_PERIOD_MS)) {
            if (fb != NULL) {
                // Frame received successfully from who_camera
                xSemaphoreTake(cameraMutex, portMAX_DELAY);
                
                frameCount++;
                lastFrameTime = millis();
                
                Serial.printf("Frame #%lu from who_camera: %d bytes, %dx%d\n", 
                             frameCount, fb->len, fb->width, fb->height);
                
                // Return the frame buffer to who_camera
                // This is REQUIRED - we must return the buffer so who_camera can reuse it
                esp_camera_fb_return(fb);
                
                xSemaphoreGive(cameraMutex);
            }
        }
        
        // Allow other tasks to run
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

// ============================================================================
// PUBLIC API IMPLEMENTATION
// ============================================================================

bool CameraHandler_init(void) {
    Serial.println("Initializing camera handler...");
    
    // Create synchronization primitives
    if (!cameraMutex) {
        cameraMutex = xSemaphoreCreateMutex();
        if (!cameraMutex) {
            Serial.println("ERROR: Failed to create camera mutex");
            return false;
        }
    }
    
    // Create frame queue to receive camera_fb_t* pointers from who_camera
    // Queue size: 2 items, each item is a pointer (camera_fb_t*)
    if (!cameraFrameQueue) {
        cameraFrameQueue = xQueueCreate(FRAME_QUEUE_SIZE, sizeof(camera_fb_t*));
        if (!cameraFrameQueue) {
            Serial.println("ERROR: Failed to create frame queue");
            return false;
        }
    }
    
    Serial.println("Camera handler initialized successfully");
    return true;
}

bool CameraHandler_startCapture(void) {
    Serial.println("Starting camera capture using who_camera...");
    
    if (!cameraFrameQueue) {
        Serial.println("ERROR: Camera not initialized");
        return false;
    }
    
    if (isRunning) {
        Serial.println("Camera capture already running");
        return true;
    }
    
    // Register camera using who_camera API
    // who_camera will push camera_fb_t* pointers to our queue
    Serial.println("Registering camera with who_camera (RGB565 format)...");
    register_camera(
        CAMERA_PIXFORMAT,       // RGB565 format (native K10 format)
        CAMERA_FRAMESIZE,       // VGA resolution (640x480)
        CAMERA_FB_COUNT,        // 2 frame buffers for double buffering
        cameraFrameQueue        // Our queue receives camera_fb_t* pointers
    );
    
    // Create and start capture task
    BaseType_t taskResult = xTaskCreatePinnedToCore(
        cameraTask,             // Task function
        "Camera_Task",          // Task name
        4096,                   // Stack size
        NULL,                   // Parameters
        2,                      // Priority
        &captureTaskHandle,     // Task handle
        0                       // Core 0 (parallel with UDP)
    );
    
    if (taskResult != pdPASS) {
        Serial.println("ERROR: Failed to create camera task");
        return false;
    }
    
    isRunning = true;
    Serial.println("Camera capture started successfully - who_camera is active");
    return true;
}

void CameraHandler_stopCapture(void) {
    if (!isRunning) {
        return;
    }
    
    Serial.println("Stopping camera capture...");
    
    if (captureTaskHandle != NULL) {
        vTaskDelete(captureTaskHandle);
        captureTaskHandle = NULL;
    }
    
    isRunning = false;
    Serial.println("Camera capture stopped");
}

QueueHandle_t CameraHandler_getFrameQueue(void) {
    return cameraFrameQueue;
}

bool CameraHandler_isRunning(void) {
    return isRunning;
}

unsigned long CameraHandler_getFrameCount(void) {
    unsigned long count = 0;
    if (cameraMutex && xSemaphoreTake(cameraMutex, 10 / portTICK_PERIOD_MS)) {
        count = frameCount;
        xSemaphoreGive(cameraMutex);
    }
    return count;
}

uint32_t CameraHandler_getLastFrameTime(void) {
    uint32_t time = 0;
    if (cameraMutex && xSemaphoreTake(cameraMutex, 10 / portTICK_PERIOD_MS)) {
        time = lastFrameTime;
        xSemaphoreGive(cameraMutex);
    }
    return time;
}

void CameraHandler_cleanup(void) {
    Serial.println("Cleaning up camera resources...");
    
    CameraHandler_stopCapture();
    
    if (cameraFrameQueue != NULL) {
        vQueueDelete(cameraFrameQueue);
        cameraFrameQueue = NULL;
    }
    
    if (cameraMutex != NULL) {
        vSemaphoreDelete(cameraMutex);
        cameraMutex = NULL;
    }
    
    Serial.println("Camera cleanup complete");
}
