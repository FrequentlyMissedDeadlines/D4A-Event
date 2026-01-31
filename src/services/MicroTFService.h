#ifndef MICROTFSERVICE_H
#define MICROTFSERVICE_H

#include "IsOpenAPIInterface.h"
#include "IsServiceInterface.h"
#include <vector>
#include <cstdint>
#include <string>

/**
 * @struct DetectedObject
 * @brief Represents a detected object with bounding box coordinates
 */
struct DetectedObject {
    float x_min;           ///< Normalized X coordinate of top-left corner (0.0-1.0)
    float y_min;           ///< Normalized Y coordinate of top-left corner (0.0-1.0)
    float x_max;           ///< Normalized X coordinate of bottom-right corner (0.0-1.0)
    float y_max;           ///< Normalized Y coordinate of bottom-right corner (0.0-1.0)
    float confidence;      ///< Confidence score (0.0-1.0)
    const char* label;     ///< Object class label
};

/**
 * @class MicroTFService
 * @brief On-demand object detection service using TensorFlow Lite for Microcontrollers
 * 
 * Captures frames from K10 camera and performs inference to detect objects.
 * Provides REST API endpoints for triggering detection and retrieving results.
 */
class MicroTFService : public IsServiceInterface, public IsOpenAPIInterface {
public:
    MicroTFService();
    ~MicroTFService() = default;

    // IsServiceInterface implementation
    /**
     * @brief Initialize the TensorFlow Lite service
     * @return true if initialization successful, false otherwise
     */
    bool initializeService() override;

    /**
     * @brief Start the TensorFlow Lite service
     * @return true if start successful, false otherwise
     */
    bool startService() override;

    /**
     * @brief Stop the TensorFlow Lite service
     * @return true if stop successful, false otherwise
     */
    bool stopService() override;

    /**
     * @brief Get service identifier
     * @return Service name string
     */
    std::string getServiceName() override;

    /**
     * @brief Get OpenAPI interface for this service
     * @return Pointer to this service as IsOpenAPIInterface
     */
    IsOpenAPIInterface* asOpenAPIInterface() override { return this; }

    // IsOpenAPIInterface implementation
    /**
     * @brief Register HTTP API routes
     * @return true if registration was successful, false otherwise
     */
    bool registerRoutes() override;

    /**
     * @brief Get the service's subpath component used in API routes
     * @return Service subpath (e.g., "microtf/v1")
     */
    std::string getServiceSubPath() override;

    /**
     * @brief Get path for a specific route
     * @param finalPathString The final path segment
     * @return Full API path
     */
    std::string getPath(const std::string& finalPathString) override;

    /**
     * @brief Capture image from camera and run inference
     * @return true if detection completed successfully
     */
    bool detectObjects();

    /**
     * @brief Get last detection results
     * @return Vector of detected objects
     */
    const std::vector<DetectedObject>& getLastDetections() const;

    /**
     * @brief Get inference time in milliseconds
     * @return Time taken for last inference
     */
    uint32_t getLastInferenceTimeMs() const;

private:
    static constexpr const char* svc_name_microtf = "MicroTFService";
    static constexpr uint32_t svc_model_input_width = 320;
    static constexpr uint32_t svc_model_input_height = 320;
    static constexpr float svc_confidence_threshold = 0.5f;

    std::vector<DetectedObject> last_detections;
    uint32_t last_inference_time_ms;
    bool is_initialized;

    // HTTP route handlers
    void handleDetect();
    void handleGetResults();
};

#endif // MICROTFSERVICE_H

