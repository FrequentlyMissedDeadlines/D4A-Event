// This class handles communication with the Micro_bit_Driver_Expansion_Board_SKU_DFR0548
/**
 * @file DFR0548.h
 * @brief Header for DFRobot Micro:bit Driver Expansion Board (DFR0548) servo controller
 * @brief Controls servo motors with angular positioning and continuous rotation
 * @brief read more at https://wiki.dfrobot.com/Micro_bit_Driver_Expansion_Board_SKU_DFR0548
 * @brief read more at https://www.dfrobot.com/product-1738.html
 */
#pragma once

#include <Arduino.h>
#include <Wire.h>

// PCA9685 Registers
#define PCA9685_MODE1 0x00
#define PCA9685_MODE2 0x01
#define PCA9685_PRESCALE 0xFE
#define PCA9685_LED0_ON_L 0x06
#define PCA9685_LED0_ON_H 0x07
#define PCA9685_LED0_OFF_L 0x08
#define PCA9685_LED0_OFF_H 0x09
#define PCA9685_RESTART 0x80

// DFR0548 Constants
#define DFR0548_DEFAULT_I2C_ADDR 0x40
#define DFR0548_MAX_CHANNELS 8
#define DFR0548_OSCILLATOR_FREQ 25000000.0f // 25MHz internal oscillator

// Servo Pulse Width Constants (PCA9685 steps at 50Hz)
// 50Hz = 20ms cycle, 4096 steps, each step ≈ 4.88μs
// Standard servo: 1.0ms (205 steps), 1.5ms (307 steps), 2.0ms (410 steps)
#define DFR0548_SERVO_MIN_PULSE 205     // ~1.0ms
#define DFR0548_SERVO_MAX_PULSE 410     // ~2.0ms
#define DFR0548_SERVO_NEUTRAL_PULSE 307 // ~1.5ms
#define DFR0548_PWM_FREQUENCY 50.0f     // 50Hz for servos

// Servo Types
enum ServoType
{
    SERVO_TYPE_ANGULAR = 0,   // Standard angular servo (0-180° or 0-270°)
    SERVO_TYPE_CONTINUOUS = 1 // Continuous rotation servo (360° with speed control)
};

// Servo Configuration Structure
struct ServoConfig
{
    ServoType type;        // Angular or continuous rotation
    uint16_t minPulse;     // Minimum pulse width (calibration)
    uint16_t maxPulse;     // Maximum pulse width (calibration)
    uint16_t neutralPulse; // Neutral pulse width (calibration)
    uint16_t maxAngle;     // Maximum angle for angular servos (180° or 270°)
    uint16_t currentAngle; // Current angle (for angular servos)
    int8_t currentSpeed;   // Current speed (-100 to +100 for continuous servos)
};

class DFR0548_Controller
{
private:
    uint8_t _i2cAddr;                          // I2C address of PCA9685
    bool _initialized;                         // Initialization status
    ServoConfig _servos[DFR0548_MAX_CHANNELS]; // Configuration for each servo channel

    // Private helper functions
    void writeRegister(uint8_t reg, uint8_t value);
    uint8_t readRegister(uint8_t reg);
    void reset();

public:
    // Constructor
    DFR0548_Controller();

    // ===== INITIALIZATION FUNCTIONS =====
    bool init();                  // Initialize I2C communication and PCA9685
    bool begin(uint8_t i2c_addr); // Start communication with specific I2C address
    void resetAll();              // Reset all servo channels to neutral position
    void setPWMFreq(float freq);  // Set PWM frequency (typically 50Hz for servos)

    // ===== SERVO CONFIGURATION FUNCTIONS =====
    void setServoType(uint8_t channel, bool isContinuous);                          // Set servo type: false=angular, true=continuous
    void setServoLimits(uint8_t channel, uint16_t minPulse, uint16_t maxPulse);     // Set pulse limits for calibration
    void setServoAngleLimits(uint8_t channel, uint8_t minAngle, uint16_t maxAngle); // Set angle limits
    void calibrateServo(uint8_t channel);                                           // Calibrate servo neutral position
    void setServoModel(uint8_t channel, uint16_t maxAngle);                         // Configure for specific servo model

    // ===== POSITIONAL SERVO CONTROL FUNCTIONS =====
    void setAngle(uint8_t channel, uint16_t angle);          // Set servo angle (0-180° or 0-270°)
    void setAngleMicroseconds(uint8_t channel, uint16_t us); // Set pulse width in microseconds
    uint16_t getCurrentAngle(uint8_t channel);               // Get current servo angle
    void centerServo(uint8_t channel);                       // Move servo to center position
    void setServoRange(uint8_t channel, uint16_t maxAngle);  // Set maximum angle for servo

    // ===== CONTINUOUS ROTATION SERVO FUNCTIONS =====
    void setSpeed(uint8_t channel, int8_t speed);         // Set speed (-100 to +100)
    void setSpeedPercent(uint8_t channel, float percent); // Set speed as percentage
    void stopServo(uint8_t channel);                      // Stop continuous rotation servo
    int8_t getCurrentSpeed(uint8_t channel);              // Get current speed setting

    // ===== MULTI-CHANNEL CONTROL FUNCTIONS =====
    void setAllAngles(uint16_t angles[8]); // Set angles for all positional servos
    void setAllSpeeds(int8_t speeds[8]);   // Set speeds for all continuous servos
    void stopAllServos();                  // Stop all servos
    void centerAllServos();                // Center all positional servos

    // ===== PWM CONTROL FUNCTIONS (LOW-LEVEL) =====
    void setPWM(uint8_t channel, uint16_t on, uint16_t off);         // Set PWM on/off times directly
    void setPin(uint8_t channel, uint16_t val, bool invert = false); // Set pin PWM value
    uint16_t getPWM(uint8_t channel);                                // Get current PWM setting

    // ===== DIAGNOSTIC AND STATUS FUNCTIONS =====
    bool isConnected();                            // Check if PCA9685 is responding
    std::string getChannelStatus(uint8_t channel); // Get servo status for channel

    // ===== UTILITY FUNCTIONS =====
    uint16_t angleToPulse(uint8_t channel, uint16_t angle); // Convert angle to pulse width
    uint16_t pulseToAngle(uint8_t channel, uint16_t pulse); // Convert pulse width to angle
    uint16_t speedToPulse(int8_t speed);                    // Convert speed to pulse width
    int8_t pulseToSpeed(uint16_t pulse);                    // Convert pulse width to speed
    uint16_t getServoRange(uint8_t channel);                // Get configured angle range for channel
};
