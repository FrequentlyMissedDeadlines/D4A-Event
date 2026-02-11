# ESP32 Camera Sensor Function Pointers Documentation

## Overview
The `sensor_t` structure defines a set of function pointers that provide a unified interface for controlling various camera sensors on ESP32 devices. These pointers abstract hardware-specific implementations, allowing the ESP32-camera driver to support multiple sensor models (OV2640, OV5640, GC2145, etc.) through a common API.

## Function Pointer Reference

### Initialization & Reset

#### `int (*init_status)(sensor_t *sensor)`
- **Purpose**: Initialize sensor hardware and read default register values
- **Returns**: 0 on success, error code otherwise
- **Usage**: Called during camera initialization to prepare sensor for operation

#### `int (*reset)(sensor_t *sensor)`
- **Purpose**: Software reset of the sensor to default state
- **Returns**: 0 on success, error code otherwise
- **Usage**: Resets all sensor registers to factory defaults

---

### Image Format Control

#### `int (*set_pixformat)(sensor_t *sensor, pixformat_t pixformat)`
- **Purpose**: Set pixel output format
- **Parameters**:
  - `pixformat`: Target format (RGB565, YUV422, JPEG, GRAYSCALE, etc.)
- **Returns**: 0 on success, error code otherwise
- **Usage**: Configure sensor to output specific pixel format for capture

#### `int (*set_framesize)(sensor_t *sensor, framesize_t framesize)`
- **Purpose**: Set capture resolution
- **Parameters**:
  - `framesize`: Target resolution (QQVGA, QVGA, VGA, SVGA, etc.)
- **Returns**: 0 on success, error code otherwise
- **Usage**: Configure output image dimensions (96x96 to 2560x1920)

---

### Image Quality Adjustments

#### `int (*set_contrast)(sensor_t *sensor, int level)`
- **Purpose**: Adjust image contrast
- **Parameters**: `level` (-2 to +2, where 0 is default)
- **Returns**: 0 on success, error code otherwise

#### `int (*set_brightness)(sensor_t *sensor, int level)`
- **Purpose**: Adjust image brightness
- **Parameters**: `level` (-2 to +2, where 0 is default)
- **Returns**: 0 on success, error code otherwise

#### `int (*set_saturation)(sensor_t *sensor, int level)`
- **Purpose**: Adjust color saturation
- **Parameters**: `level` (-2 to +2, where 0 is default)
- **Returns**: 0 on success, error code otherwise

#### `int (*set_sharpness)(sensor_t *sensor, int level)`
- **Purpose**: Adjust image sharpness
- **Parameters**: `level` (-2 to +2, where 0 is default)
- **Returns**: 0 on success, error code otherwise

#### `int (*set_denoise)(sensor_t *sensor, int level)`
- **Purpose**: Enable/adjust noise reduction
- **Parameters**: `level` (implementation-specific)
- **Returns**: 0 on success, error code otherwise

#### `int (*set_quality)(sensor_t *sensor, int quality)`
- **Purpose**: Set JPEG compression quality
- **Parameters**: `quality` (0-63, lower = higher quality)
- **Returns**: 0 on success, error code otherwise
- **Note**: Only applicable when pixformat is JPEG

---

### Automatic Control Features

#### `int (*set_whitebal)(sensor_t *sensor, int enable)`
- **Purpose**: Enable/disable automatic white balance (AWB)
- **Parameters**: `enable` (0 = off, 1 = on)
- **Returns**: 0 on success, error code otherwise

#### `int (*set_awb_gain)(sensor_t *sensor, int enable)`
- **Purpose**: Enable/disable automatic white balance gain
- **Parameters**: `enable` (0 = off, 1 = on)
- **Returns**: 0 on success, error code otherwise

#### `int (*set_gain_ctrl)(sensor_t *sensor, int enable)`
- **Purpose**: Enable/disable automatic gain control (AGC)
- **Parameters**: `enable` (0 = off, 1 = on)
- **Returns**: 0 on success, error code otherwise

#### `int (*set_agc_gain)(sensor_t *sensor, int gain)`
- **Purpose**: Set manual AGC gain value (when AGC is disabled)
- **Parameters**: `gain` (0-30, sensor-specific)
- **Returns**: 0 on success, error code otherwise

#### `int (*set_exposure_ctrl)(sensor_t *sensor, int enable)`
- **Purpose**: Enable/disable automatic exposure control (AEC)
- **Parameters**: `enable` (0 = off, 1 = on)
- **Returns**: 0 on success, error code otherwise

#### `int (*set_aec_value)(sensor_t *sensor, int value)`
- **Purpose**: Set manual exposure value (when AEC is disabled)
- **Parameters**: `value` (0-1200, sensor-specific)
- **Returns**: 0 on success, error code otherwise

#### `int (*set_aec2)(sensor_t *sensor, int enable)`
- **Purpose**: Enable/disable advanced exposure control
- **Parameters**: `enable` (0 = off, 1 = on)
- **Returns**: 0 on success, error code otherwise

#### `int (*set_ae_level)(sensor_t *sensor, int level)`
- **Purpose**: Set auto-exposure level/bias
- **Parameters**: `level` (-2 to +2, where 0 is default)
- **Returns**: 0 on success, error code otherwise

#### `int (*set_gainceiling)(sensor_t *sensor, gainceiling_t gainceiling)`
- **Purpose**: Set maximum AGC gain limit
- **Parameters**: `gainceiling` (2X, 4X, 8X, 16X, 32X, 64X, 128X)
- **Returns**: 0 on success, error code otherwise
- **Usage**: Prevents AGC from introducing excessive noise in low light

---

### Geometric Transformations

#### `int (*set_hmirror)(sensor_t *sensor, int enable)`
- **Purpose**: Enable/disable horizontal mirror (flip left-right)
- **Parameters**: `enable` (0 = off, 1 = on)
- **Returns**: 0 on success, error code otherwise

#### `int (*set_vflip)(sensor_t *sensor, int enable)`
- **Purpose**: Enable/disable vertical flip (flip upside-down)
- **Parameters**: `enable` (0 = off, 1 = on)
- **Returns**: 0 on success, error code otherwise

#### `int (*set_dcw)(sensor_t *sensor, int enable)`
- **Purpose**: Enable/disable downsize cropping window
- **Parameters**: `enable` (0 = off, 1 = on)
- **Returns**: 0 on success, error code otherwise

---

### Advanced Image Processing

#### `int (*set_bpc)(sensor_t *sensor, int enable)`
- **Purpose**: Enable/disable black pixel correction
- **Parameters**: `enable` (0 = off, 1 = on)
- **Returns**: 0 on success, error code otherwise

#### `int (*set_wpc)(sensor_t *sensor, int enable)`
- **Purpose**: Enable/disable white pixel correction
- **Parameters**: `enable` (0 = off, 1 = on)
- **Returns**: 0 on success, error code otherwise

#### `int (*set_raw_gma)(sensor_t *sensor, int enable)`
- **Purpose**: Enable/disable gamma correction
- **Parameters**: `enable` (0 = off, 1 = on)
- **Returns**: 0 on success, error code otherwise

#### `int (*set_lenc)(sensor_t *sensor, int enable)`
- **Purpose**: Enable/disable lens correction
- **Parameters**: `enable` (0 = off, 1 = on)
- **Returns**: 0 on success, error code otherwise
- **Usage**: Compensates for lens distortion/vignetting

---

### Special Effects & Modes

#### `int (*set_special_effect)(sensor_t *sensor, int effect)`
- **Purpose**: Apply special visual effects
- **Parameters**: `effect` (0-6: Normal, Negative, Grayscale, Red/Green/Blue tint, Sepia)
- **Returns**: 0 on success, error code otherwise

#### `int (*set_wb_mode)(sensor_t *sensor, int mode)`
- **Purpose**: Set white balance preset mode
- **Parameters**: `mode` (0-4: Auto, Sunny, Cloudy, Office, Home)
- **Returns**: 0 on success, error code otherwise

#### `int (*set_colorbar)(sensor_t *sensor, int enable)`
- **Purpose**: Enable/disable test pattern (color bars)
- **Parameters**: `enable` (0 = off, 1 = on)
- **Returns**: 0 on success, error code otherwise
- **Usage**: Hardware test pattern for debugging

---

### Low-Level Register Access

#### `int (*get_reg)(sensor_t *sensor, int reg, int mask)`
- **Purpose**: Read sensor register value
- **Parameters**:
  - `reg`: Register address
  - `mask`: Bit mask to apply
- **Returns**: Register value (masked) or error code
- **Usage**: Direct hardware register access for debugging

#### `int (*set_reg)(sensor_t *sensor, int reg, int mask, int value)`
- **Purpose**: Write sensor register value
- **Parameters**:
  - `reg`: Register address
  - `mask`: Bit mask for modification
  - `value`: Value to write
- **Returns**: 0 on success, error code otherwise
- **Usage**: Direct hardware register access for custom configurations

---

### Advanced Configuration

#### `int (*set_res_raw)(sensor_t *sensor, int startX, int startY, int endX, int endY, int offsetX, int offsetY, int totalX, int totalY, int outputX, int outputY, bool scale, bool binning)`
- **Purpose**: Configure raw sensor windowing/scaling parameters
- **Parameters**: 
  - `startX`, `startY`: Start coordinates of capture window
  - `endX`, `endY`: End coordinates of capture window
  - `offsetX`, `offsetY`: Offset values for windowing
  - `totalX`, `totalY`: Total sensor dimensions
  - `outputX`, `outputY`: Output image dimensions
  - `scale`: Enable/disable scaling
  - `binning`: Enable/disable pixel binning
- **Returns**: 0 on success, error code otherwise
- **Usage**: Low-level control of sensor capture window and downsampling

#### `int (*set_pll)(sensor_t *sensor, int bypass, int mul, int sys, int root, int pre, int seld5, int pclken, int pclk)`
- **Purpose**: Configure sensor PLL (Phase-Locked Loop) clock parameters
- **Parameters**: 
  - `bypass`: PLL bypass mode
  - `mul`: Multiplier value
  - `sys`, `root`, `pre`, `seld5`: Clock divider parameters
  - `pclken`: Pixel clock enable
  - `pclk`: Pixel clock divider
- **Returns**: 0 on success, error code otherwise
- **Usage**: Advanced clock configuration for specific timing requirements

#### `int (*set_xclk)(sensor_t *sensor, int timer, int xclk)`
- **Purpose**: Set external clock (XCLK) frequency
- **Parameters**:
  - `timer`: Timer number
  - `xclk`: Clock frequency in Hz
- **Returns**: 0 on success, error code otherwise
- **Usage**: Configure master clock provided to sensor

---

## Implementation Pattern

Each supported camera sensor (OV2640, OV5640, etc.) implements these function pointers in its driver file (e.g., `ov2640.c`, `gc2145.c`). Not all sensors support all features—unsupported functions may return error codes or be set to `NULL`.

Example usage pattern:
```cpp
sensor_t *sensor = esp_camera_sensor_get();

// Set resolution
sensor->set_framesize(sensor, FRAMESIZE_QVGA);

// Enable auto exposure
sensor->set_exposure_ctrl(sensor, 1);

// Flip image vertically
sensor->set_vflip(sensor, 1);

// Set JPEG quality
sensor->set_quality(sensor, 12);
```

## Memory Constraints Note

When implementing services that use these functions on the K10 board:
- Cache frequently used settings instead of polling registers
- Avoid calling multiple functions per frame capture (high overhead)
- Use PROGMEM constants for sensor configuration tables
- Consider implementing configuration presets to reduce function call overhead

## Supported Camera Sensors

_Unihiker K10 has a **GC2145** cam, 2MP，80°FOV _


The following sensors are supported by the ESP32-camera driver:

| Model | PID | I2C Address | Max Resolution | JPEG Support |
|-------|-----|-------------|----------------|--------------|
| **GC2145** | 0x2145 | 0x3C | **UXGA** | **Yes** |
| OV7725 | 0x77 | 0x21 | VGA | No |
| OV2640 | 0x26 | 0x30 | UXGA | Yes |
| OV3660 | 0x3660 | 0x3C | QXGA | Yes |
| OV5640 | 0x5640 | 0x3C | QSXGA | Yes |
| OV7670 | 0x76 | 0x21 | VGA | No |
| NT99141 | 0x1410 | 0x2A | HD | Yes |
| GC032A | 0x232a | 0x21 | VGA | No |
| GC0308 | 0x9b | 0x21 | VGA | No |
| BF3005 | 0x30 | 0x6E | VGA | No |
| BF20A6 | 0x20a6 | 0x6E | UXGA | Yes |

## Related Documentation

- [K10CamService Implementation](../src/services/camera/K10CamService.h) - K10 bot's camera service
- [IsServiceInterface](IsServiceInterface.md) - Service architecture pattern
- [IsOpenAPIInterface](IsOpenAPIInterface.md) - HTTP API integration
| **GC2145** | 0x2145 | 0x3C | UXGA | Yes |
