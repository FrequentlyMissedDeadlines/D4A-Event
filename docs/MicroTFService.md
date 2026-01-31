# MicroTFService Documentation

## Overview
The **MicroTFService** is a new OpenAPI service integrated into the K10-Bot project that enables real-time image classification on the UniHiker K10 camera using MicroTF, a lightweight neural network model optimized for embedded devices.

## Features
- **Real-time Image Classification**: Classifies objects in camera frames using MicroTF
- **OpenAPI Integration**: Fully integrated with the existing OpenAPI/HTTP service architecture
- **Memory Optimized**: Designed for the ESP32-S3's limited memory constraints
- **Modular Design**: Follows the same architectural pattern as other services (WebcamService, ServoService, etc.)

## API Endpoints

### 1. Classify Frame
**Endpoint**: `GET /api/microtf/v1/classify`

**Description**: Captures a frame from the K10 camera and performs image classification using MicroTF.

**Response** (JSON):
```json
{
  "label": "dog",
  "confidence": 0.95,
  "inference_time_ms": 250,
  "top_k_results": [
    {"label": "dog", "confidence": 0.95},
    {"label": "wolf", "confidence": 0.03},
    {"label": "coyote", "confidence": 0.02}
  ]
}
```

### 2. Service Status
**Endpoint**: `GET /api/microtf/v1/status`

**Description**: Returns the current status and initialization state of the MicroTFService.

**Response** (JSON):
```json
{
  "status": "ready",
  "initialized": true
}
```

## File Structure

### Header File
- **Location**: `src/services/MicroTFService.h`
- **Contains**: Class definition implementing `IsServiceInterface` and `IsOpenAPIInterface`
- **Key Methods**:
  - `initializeService()`: Initialize the MicroTF model
  - `startService()`: Start the service
  - `stopService()`: Stop the service
  - `registerRoutes()`: Register HTTP routes with the webserver
  - `classifyFrame()`: Perform inference on camera frame

### Implementation File
- **Location**: `src/services/MicroTFService.cpp`
- **Contains**: Implementation of all service methods
- **Key Features**:
  - Frame preprocessing for MicroTF input
  - Inference execution
  - Output parsing and JSON formatting

## Integration with Main Application

The MicroTFService is integrated in `src/main.cpp`:

1. **Instantiation**:
   ```cpp
   MicroTFService microTFService = MicroTFService();
   ```

2. **Service Startup** (in setup function):
   ```cpp
   start_service(microTFService);
   ```

## Implementation Status

### Completed
✅ Service class structure and interfaces  
✅ HTTP route registration  
✅ OpenAPI endpoint definitions  
✅ Integration with main application  
✅ Build compilation and linking  

### TODO - Future Implementation
- [ ] Load TensorFlow Lite model file (microtf_quant.tflite)
- [ ] Implement `preprocessFrame()` for image resizing and normalization
- [ ] Implement `parseOutput()` to extract classification results
- [ ] Integrate with TensorFlow Lite Micro for actual inference
- [ ] Create ImageNet label mapping (class indices to human-readable names)
- [ ] Optimize for real-time performance on ESP32-S3
- [ ] Add support for top-K predictions
- [ ] Implement confidence threshold filtering

## Architecture Details

### Service Hierarchy
```
IsServiceInterface
├── getServiceName()
├── initializeService()
├── startService()
└── stopService()

IsOpenAPIInterface
├── registerRoutes()
├── getServiceSubPath()
├── getPath()
└── getOpenAPIRoutes()

MicroTFService (implements both)
```

### Constants (PROGMEM)
All string constants are stored in PROGMEM to minimize RAM usage:
- Service names and paths
- Error messages
- JSON field names
- OpenAPI descriptions

## Memory Considerations

The service is designed for the ESP32-S3's memory constraints:
- **SRAM**: ~327 KB available
- **PSRAM**: ~8 MB available (if configured)
- **Model**: MicroTF models typically require 10-20 MB
- **Tensor Arena**: Currently allocated 64 KB (configurable)

## Configuration

### TensorFlow Lite Setup
Once TensorFlow Lite Micro is integrated, the following may need to be configured:
- Model file path
- Tensor arena size
- Input/output tensor dimensions
- Quantization format

### Camera Integration
The service captures frames from the K10 camera:
- Frame size: Configurable (currently 800x600 in WebcamService)
- Format: JPEG (must be decoded before MicroTF)
- Frame rate: Limited by inference time

## Testing

To test the service after implementation:

1. **Start the device**:
   ```bash
   pio run --target upload
   ```

2. **Call the classify endpoint**:
   ```bash
   curl http://<device-ip>/api/microtf/v1/classify
   ```

3. **Check status**:
   ```bash
   curl http://<device-ip>/api/microtf/v1/status
   ```

## Performance Expectations

Once fully implemented, expected performance metrics:
- **Inference Time**: 200-500ms (depending on image size and ESP32-S3 clock speed)
- **Throughput**: ~1-2 frames per second
- **Accuracy**: ~70-80% top-1 accuracy on ImageNet classes

## Related Services

- **WebcamService**: Provides camera frame capture
- **HTTPService**: Handles HTTP requests and OpenAPI aggregation
- **WiFiService**: Provides network connectivity

## Future Enhancements

1. **Model Selection**: Support for multiple MicroTF variants (v1, v2, v3)
2. **Quantization**: INT8 quantization for faster inference
3. **Custom Models**: Support for custom-trained classification models
4. **Caching**: Cache inference results to improve responsiveness
5. **Batch Processing**: Process multiple frames in succession
6. **Event Triggers**: Trigger actions based on detected objects

## References

- [MicroTF Paper](https://arxiv.org/abs/1704.04861)
- [TensorFlow Lite Micro](https://www.tensorflow.org/lite/microcontrollers)
- [ESP32-S3 Documentation](https://docs.espressif.com/projects/esp-idf/en/release-v5.0/esp32s3/index.html)

---

**Last Updated**: January 30, 2026  
**Status**: Framework Complete, Implementation Pending
