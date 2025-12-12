// This class handles communication with the Micro_bit_Driver_Expansion_Board_SKU_DFR0548
/**
 * @file DFR0548.cpp
 * @brief Implementation for DFRobot Micro:bit Driver Expansion Board (DFR0548) servo controller
 * @brief Controls servo motors with angular positioning and continuous rotation
 * @brief read more at https://wiki.dfrobot.com/Micro_bit_Driver_Expansion_Board_SKU_DFR0548
 * @brief read more at https://www.dfrobot.com/product-1738.html
 */

#include "DFR0548.h"

// Constructor
DFR0548_Controller::DFR0548_Controller() : _i2cAddr(DFR0548_DEFAULT_I2C_ADDR), _initialized(false) {
    // Initialize all servos with default settings
    for (uint8_t i = 0; i < DFR0548_MAX_CHANNELS; i++) {
        _servos[i].type = SERVO_TYPE_ANGULAR;
        _servos[i].minPulse = DFR0548_SERVO_MIN_PULSE;
        _servos[i].maxPulse = DFR0548_SERVO_MAX_PULSE;
        _servos[i].neutralPulse = DFR0548_SERVO_NEUTRAL_PULSE;
        _servos[i].maxAngle = 180;  // Default to 180 degrees
        _servos[i].currentAngle = 90;  // Center position
        _servos[i].currentSpeed = 0;   // Stopped
    }
}

// Initialize I2C communication and PCA9685
bool DFR0548_Controller::init() {
    Wire.begin();
    delay(10);  // Allow time for I2C to initialize

    if (!isConnected()) {
        return false;
    }

    reset();
    setPWMFreq(DFR0548_PWM_FREQUENCY);
    _initialized = true;
    return true;
}

// Start communication with specific I2C address
bool DFR0548_Controller::begin(uint8_t i2c_addr) {
    _i2cAddr = i2c_addr;
    return init();
}

// Reset all servo channels to neutral position
void DFR0548_Controller::resetAll() {
    for (uint8_t i = 0; i < DFR0548_MAX_CHANNELS; i++) {
        if (_servos[i].type == SERVO_TYPE_ANGULAR) {
            centerServo(i);
        } else {
            stopServo(i);
        }
    }
}

// Set PWM frequency (typically 50Hz for servos)
void DFR0548_Controller::setPWMFreq(float freq) {
    // Calculate prescale value
    float prescaleval = ((DFR0548_OSCILLATOR_FREQ / (freq * 4096.0)) + 0.5) - 1;
    if (prescaleval < 3) prescaleval = 3;  // Min value
    if (prescaleval > 255) prescaleval = 255;  // Max value

    uint8_t prescale = (uint8_t)prescaleval;

    // Set sleep mode
    uint8_t oldmode = readRegister(PCA9685_MODE1);
    uint8_t newmode = (oldmode & 0x7F) | 0x10;  // Set sleep bit
    writeRegister(PCA9685_MODE1, newmode);

    // Set prescale
    writeRegister(PCA9685_PRESCALE, prescale);

    // Wake up
    writeRegister(PCA9685_MODE1, oldmode);
    delay(5);

    // Set restart bit
    writeRegister(PCA9685_MODE1, oldmode | 0x80);
}

// ===== SERVO CONFIGURATION FUNCTIONS =====

// Set servo type for channel
void DFR0548_Controller::setServoType(uint8_t channel, bool isContinuous) {
    if (channel >= DFR0548_MAX_CHANNELS) return;

    _servos[channel].type = isContinuous ? SERVO_TYPE_CONTINUOUS : SERVO_TYPE_ANGULAR;

    // Reset to neutral/safe position
    if (isContinuous) {
        stopServo(channel);
    } else {
        centerServo(channel);
    }
}

// Set pulse limits for calibration
void DFR0548_Controller::setServoLimits(uint8_t channel, uint16_t minPulse, uint16_t maxPulse) {
    if (channel >= DFR0548_MAX_CHANNELS) return;

    _servos[channel].minPulse = minPulse;
    _servos[channel].maxPulse = maxPulse;

    // Recalculate neutral pulse as midpoint
    _servos[channel].neutralPulse = (minPulse + maxPulse) / 2;
}

// Set angle limits (0-maxAngle)
void DFR0548_Controller::setServoAngleLimits(uint8_t channel, uint8_t minAngle, uint16_t maxAngle) {
    if (channel >= DFR0548_MAX_CHANNELS) return;

    _servos[channel].maxAngle = maxAngle;
    // minAngle is typically 0, so we don't store it separately
}

// Calibrate servo neutral position
void DFR0548_Controller::calibrateServo(uint8_t channel) {
    if (channel >= DFR0548_MAX_CHANNELS) return;

    // Set to neutral pulse
    setPWM(channel, 0, _servos[channel].neutralPulse);
}

// Configure for specific servo model
void DFR0548_Controller::setServoModel(uint8_t channel, uint16_t maxAngle) {
    if (channel >= DFR0548_MAX_CHANNELS) return;

    _servos[channel].maxAngle = maxAngle;
    setServoType(channel, false);  // Assume angular servo
}

// ===== POSITIONAL SERVO CONTROL FUNCTIONS =====

// Set servo angle (0-180° or 0-270° depending on servo)
void DFR0548_Controller::setAngle(uint8_t channel, uint16_t angle) {
    if (channel >= DFR0548_MAX_CHANNELS || _servos[channel].type != SERVO_TYPE_ANGULAR) return;

    // Constrain angle to servo range
    if (angle > _servos[channel].maxAngle) {
        angle = _servos[channel].maxAngle;
    }

    uint16_t pulse = angleToPulse(channel, angle);
    setPWM(channel, 0, pulse);
    _servos[channel].currentAngle = angle;
}

// Set pulse width in microseconds
void DFR0548_Controller::setAngleMicroseconds(uint8_t channel, uint16_t us) {
    if (channel >= DFR0548_MAX_CHANNELS) return;

    // Convert microseconds to PCA9685 steps (assuming 50Hz, 20ms cycle)
    // 1ms = 1000μs, at 50Hz (20ms cycle), 4096 steps
    // So 1μs = 4096/20000 = 0.2048 steps
    uint16_t steps = (us * 4096UL) / 20000UL;
    setPWM(channel, 0, steps);
}

// Get current servo angle
uint16_t DFR0548_Controller::getCurrentAngle(uint8_t channel) {
    if (channel >= DFR0548_MAX_CHANNELS) return 0;
    return _servos[channel].currentAngle;
}

// Move servo to center position
void DFR0548_Controller::centerServo(uint8_t channel) {
    if (channel >= DFR0548_MAX_CHANNELS) return;

    uint16_t centerAngle = _servos[channel].maxAngle / 2;
    setAngle(channel, centerAngle);
}

// Set maximum angle for servo
void DFR0548_Controller::setServoRange(uint8_t channel, uint16_t maxAngle) {
    if (channel >= DFR0548_MAX_CHANNELS) return;
    _servos[channel].maxAngle = maxAngle;
}

// ===== CONTINUOUS ROTATION SERVO FUNCTIONS =====

// Set speed (-100 to +100)
void DFR0548_Controller::setSpeed(uint8_t channel, int8_t speed) {
    if (channel >= DFR0548_MAX_CHANNELS || _servos[channel].type != SERVO_TYPE_CONTINUOUS) return;

    // Constrain speed to -100 to +100
    if (speed < -100) speed = -100;
    if (speed > 100) speed = 100;

    uint16_t pulse = speedToPulse(speed);
    setPWM(channel, 0, pulse);
    _servos[channel].currentSpeed = speed;
}

// Set speed as percentage
void DFR0548_Controller::setSpeedPercent(uint8_t channel, float percent) {
    if (channel >= DFR0548_MAX_CHANNELS) return;

    // Convert percentage to speed (-100 to +100)
    int8_t speed = (int8_t)(percent * 100.0f);
    setSpeed(channel, speed);
}

// Stop continuous rotation servo
void DFR0548_Controller::stopServo(uint8_t channel) {
    if (channel >= DFR0548_MAX_CHANNELS) return;

    setPWM(channel, 0, _servos[channel].neutralPulse);
    _servos[channel].currentSpeed = 0;
}

// Get current speed setting
int8_t DFR0548_Controller::getCurrentSpeed(uint8_t channel) {
    if (channel >= DFR0548_MAX_CHANNELS) return 0;
    return _servos[channel].currentSpeed;
}

// ===== MULTI-CHANNEL CONTROL FUNCTIONS =====

// Set angles for all positional servos
void DFR0548_Controller::setAllAngles(uint16_t angles[8]) {
    for (uint8_t i = 0; i < DFR0548_MAX_CHANNELS; i++) {
        if (_servos[i].type == SERVO_TYPE_ANGULAR) {
            setAngle(i, angles[i]);
        }
    }
}

// Set speeds for all continuous servos
void DFR0548_Controller::setAllSpeeds(int8_t speeds[8]) {
    for (uint8_t i = 0; i < DFR0548_MAX_CHANNELS; i++) {
        if (_servos[i].type == SERVO_TYPE_CONTINUOUS) {
            setSpeed(i, speeds[i]);
        }
    }
}

// Stop all servos
void DFR0548_Controller::stopAllServos() {
    for (uint8_t i = 0; i < DFR0548_MAX_CHANNELS; i++) {
        stopServo(i);
    }
}

// Center all positional servos
void DFR0548_Controller::centerAllServos() {
    for (uint8_t i = 0; i < DFR0548_MAX_CHANNELS; i++) {
        if (_servos[i].type == SERVO_TYPE_ANGULAR) {
            centerServo(i);
        }
    }
}

// ===== PWM CONTROL FUNCTIONS (LOW-LEVEL) =====

// Set PWM on/off times directly
void DFR0548_Controller::setPWM(uint8_t channel, uint16_t on, uint16_t off) {
    if (channel >= DFR0548_MAX_CHANNELS) return;

    writeRegister(PCA9685_LED0_ON_L + 4 * channel, on & 0xFF);
    writeRegister(PCA9685_LED0_ON_H + 4 * channel, on >> 8);
    writeRegister(PCA9685_LED0_OFF_L + 4 * channel, off & 0xFF);
    writeRegister(PCA9685_LED0_OFF_H + 4 * channel, off >> 8);
}

// Set pin PWM value
void DFR0548_Controller::setPin(uint8_t channel, uint16_t val, bool invert) {
    if (channel >= DFR0548_MAX_CHANNELS) return;

    // If invert, set on time to val, off time to 0
    // If not invert, set on time to 0, off time to val
    if (invert) {
        setPWM(channel, val, 0);
    } else {
        setPWM(channel, 0, val);
    }
}

// Get current PWM setting
uint16_t DFR0548_Controller::getPWM(uint8_t channel) {
    if (channel >= DFR0548_MAX_CHANNELS) return 0;

    uint8_t off_l = readRegister(PCA9685_LED0_OFF_L + 4 * channel);
    uint8_t off_h = readRegister(PCA9685_LED0_OFF_H + 4 * channel);
    return (uint16_t)off_h << 8 | off_l;
}

// ===== DIAGNOSTIC AND STATUS FUNCTIONS =====

// Check if PCA9685 is responding
bool DFR0548_Controller::isConnected() {
    Wire.beginTransmission(_i2cAddr);
    return (Wire.endTransmission() == 0);
}

// Get servo status for channel
String DFR0548_Controller::getChannelStatus(uint8_t channel) {
    if (channel >= DFR0548_MAX_CHANNELS) return "Invalid channel";

    String status = "Channel " + String(channel) + ": ";
    if (_servos[channel].type == SERVO_TYPE_ANGULAR) {
        status += "Angular, Angle: " + String(_servos[channel].currentAngle) + "°";
    } else {
        status += "Continuous, Speed: " + String(_servos[channel].currentSpeed) + "%";
    }
    status += ", Range: " + String(_servos[channel].maxAngle) + "°";
    return status;
}

// ===== UTILITY FUNCTIONS =====

// Convert angle to pulse width
uint16_t DFR0548_Controller::angleToPulse(uint8_t channel, uint16_t angle) {
    if (channel >= DFR0548_MAX_CHANNELS) return _servos[0].neutralPulse;

    // Linear interpolation between min and max pulse
    uint16_t pulseRange = _servos[channel].maxPulse - _servos[channel].minPulse;
    uint16_t angleRange = _servos[channel].maxAngle;

    uint16_t pulse = _servos[channel].minPulse + (angle * pulseRange) / angleRange;
    return pulse;
}

// Convert pulse width to angle
uint16_t DFR0548_Controller::pulseToAngle(uint8_t channel, uint16_t pulse) {
    if (channel >= DFR0548_MAX_CHANNELS) return 0;

    // Reverse of angleToPulse
    uint16_t pulseRange = _servos[channel].maxPulse - _servos[channel].minPulse;
    uint16_t angleRange = _servos[channel].maxAngle;

    if (pulseRange == 0) return 0;

    uint16_t angle = ((pulse - _servos[channel].minPulse) * angleRange) / pulseRange;
    return angle;
}

// Convert speed to pulse width
uint16_t DFR0548_Controller::speedToPulse(int8_t speed) {
    // For continuous rotation servos:
    // Speed -100 = min pulse (full reverse)
    // Speed 0 = neutral pulse (stop)
    // Speed +100 = max pulse (full forward)

    if (speed == 0) {
        return DFR0548_SERVO_NEUTRAL_PULSE;
    } else if (speed > 0) {
        // Forward: neutral to max pulse
        uint16_t range = DFR0548_SERVO_MAX_PULSE - DFR0548_SERVO_NEUTRAL_PULSE;
        return DFR0548_SERVO_NEUTRAL_PULSE + (speed * range) / 100;
    } else {
        // Reverse: neutral to min pulse
        uint16_t range = DFR0548_SERVO_NEUTRAL_PULSE - DFR0548_SERVO_MIN_PULSE;
        return DFR0548_SERVO_NEUTRAL_PULSE - ((-speed) * range) / 100;
    }
}

// Convert pulse width to speed
int8_t DFR0548_Controller::pulseToSpeed(uint16_t pulse) {
    if (pulse == DFR0548_SERVO_NEUTRAL_PULSE) {
        return 0;
    } else if (pulse > DFR0548_SERVO_NEUTRAL_PULSE) {
        // Forward
        uint16_t range = DFR0548_SERVO_MAX_PULSE - DFR0548_SERVO_NEUTRAL_PULSE;
        if (range == 0) return 0;
        return (pulse - DFR0548_SERVO_NEUTRAL_PULSE) * 100 / range;
    } else {
        // Reverse
        uint16_t range = DFR0548_SERVO_NEUTRAL_PULSE - DFR0548_SERVO_MIN_PULSE;
        if (range == 0) return 0;
        return -((DFR0548_SERVO_NEUTRAL_PULSE - pulse) * 100 / range);
    }
}

// Get configured angle range for channel
uint16_t DFR0548_Controller::getServoRange(uint8_t channel) {
    if (channel >= DFR0548_MAX_CHANNELS) return 180;
    return _servos[channel].maxAngle;
}

// ===== PRIVATE HELPER FUNCTIONS =====

// Write to PCA9685 register
void DFR0548_Controller::writeRegister(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(_i2cAddr);
    Wire.write(reg);
    Wire.write(value);
    Wire.endTransmission();
}

// Read from PCA9685 register
uint8_t DFR0548_Controller::readRegister(uint8_t reg) {
    Wire.beginTransmission(_i2cAddr);
    Wire.write(reg);
    Wire.endTransmission();

    Wire.requestFrom(_i2cAddr, (uint8_t)1);
    return Wire.read();
}

// Reset PCA9685
void DFR0548_Controller::reset() {
    writeRegister(PCA9685_MODE1, PCA9685_RESTART);
    delay(10);
}
