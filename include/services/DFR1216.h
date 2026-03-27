/*!
 * @file DFR1216.h
 * @brief Define the basic structure of the DFR1216 class, the implementation of the basic methods
 * @copyright	Copyright (c) 2025 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @license The MIT License (MIT)
 * @author [ZhixinLiu](zhixin.liu@dfrobot.com)
 * @version V1.0
 * @date 2025-04-03
 * @url https://github.com/DFRobot/DFRobot_UnihikerExpansion
 */
#ifndef __DFROBOT__UNIHIKEREXPANSION__H__
#define __DFROBOT__UNIHIKEREXPANSION__H__
#include <Arduino.h>
#include <Wire.h>
#include <freertos/semphr.h>
#include "IsServiceInterface.h"
#include "BotCommunication/BotMessageHandler.h"
// #define ENABLE_DBG  1 ///> Open this macro and you can see the details of the program
#ifdef ENABLE_DBG
#define DBG(...)                 \
  {                              \
    Serial.print("[");           \
    Serial.print(__FUNCTION__);  \
    Serial.print("(): ");        \
    Serial.print(__LINE__);      \
    Serial.print(" ] ");         \
    Serial.println(__VA_ARGS__); \
  }
#else
#define DBG(...)
#endif

typedef enum
{
  eMotor1_2,
  eMotor3_4,
  eServo0_1,
  eServo2_5,
} ePeriod_t;

typedef enum
{
  eServo0,
  eServo1,
  eServo2,
  eServo3,
  eServo4,
  eServo5,
} eServoNumber_t;

typedef enum
{
  eMotor1_A,
  eMotor1_B,
  eMotor2_A,
  eMotor2_B,
  eMotor3_A,
  eMotor3_B,
  eMotor4_A,
  eMotor4_B,
} eMotorNumber_t;

typedef enum
{
  eADC,
  eDHT11,
  eDHT22,
  eDS18B20,
  eWriteGpio,
  eReadGpio,
} eIOType_t;

typedef enum
{
  eC0,
  eC1,
  eC2,
  eC3,
  eC4,
  eC5,
} eIONumber_t;

typedef enum
{
  eForward,
  eBackward,
  eStop,
} eServo360Direction_t;

typedef enum
{
  eLOW,
  eHIGH,
} eGpioState_t;

typedef enum
{
  eTypeDHT11,
  eTypeDHT22,
} eDhtType_t;

typedef struct
{
  float temperature;
  float humidity;
  uint8_t state;
} sDhtData_t;

#define I2C_MOTOR12_PERIOD_H 0X00
#define I2C_MOTOR34_PERIOD_H 0x02
#define I2C_MOTOR1_Z_DUTY_H 0X04
#define I2C_MOTOR1_F_DUTY_H 0X06
#define I2C_MOTOR2_Z_DUTY_H 0X08
#define I2C_MOTOR2_F_DUTY_H 0X0a
#define I2C_MOTOR3_Z_DUTY_H 0x0c
#define I2C_MOTOR3_F_DUTY_H 0x0E
#define I2C_MOTOR4_Z_DUTY_H 0x10
#define I2C_MOTOR4_F_DUTY_H 0x12
#define I2C_SERVO01_PERIOD_H 0x14
#define I2C_SERVO25_PERIOD_H 0x16
#define I2C_SERVO0_DUTY_H 0x18
#define I2C_SERVO1_DUTY_H 0x1a
#define I2C_SERVO2_DUTY_H 0x1c
#define I2C_SERVO3_DUTY_H 0x1e
#define I2C_SERVO4_DUTY_H 0x20
#define I2C_SERVO5_DUTY_H 0x22
#define I2C_IR_S_STATE 0x24
#define I2C_SR04_STATE 0x29
#define SR04_COLLECT 0x01
#define SR04_COMPLETE 0x02
#define I2C_IO_MODE_C0 0x2c
#define I2C_W_C0 0x39
#define I2C_R_C0 0x3f
#define I2C_ADC_C0_S 0x45
#define I2C_DHT_C0_S 0x57
#define I2C_18B20_C0_S 0x75
#define I2C_BATTERY 0x87
#define I2C_IR_R_STATE 0x88
#define I2C_WS2812_STATE 0x90
#define I2C_RESET_SENSOR 0xa0
#define DATA_ENABLE 0x01  ///> enable
#define DATA_DISABLE 0x00 ///> disable
#define MODE_ERROR 0x02
#define RETRY_COUNT 3
#define I2C_RETRY_DELAY_MS 5  ///> Delay between I2C retries (ms)
#define TEMP_LEN 32

/// Servo 360 PWM pulse-width constants (microseconds)
/// Tune these to match your specific servo's dead-band and full-speed points.
constexpr uint16_t SERVO360_STOP_US         = 1500; ///< Neutral / stop pulse width
constexpr uint16_t SERVO360_FORWARD_MIN_US  = 500; ///< Full-speed forward pulse width
constexpr uint16_t SERVO360_BACKWARD_MAX_US = 2500; ///< Full-speed backward pulse width

/// Standard 180° Servo calibration (microseconds)
constexpr uint16_t SERVO180_MIN_US    = 500;
constexpr uint16_t SERVO180_MIDDLE_US = 1500;
constexpr uint16_t SERVO180_MAX_US    = 2500;

/// Wide 270° Servo calibration (microseconds)
constexpr uint16_t SERVO270_MIN_US    = 500;
constexpr uint16_t SERVO270_MIDDLE_US = 2250;
constexpr uint16_t SERVO270_MAX_US    = 3900;

/// Per-LED colour/brightness state cache (3 on-board WS2812 LEDs).
struct LedState
{
    uint8_t red   = 0;
    uint8_t green = 0;
    uint8_t blue  = 0;
};

class DFR1216Board : public IsServiceInterface,
                     public IsBotActionHandlerInterface
{
public:
  DFR1216Board();
  ~DFR1216Board();

  /**
   * @brief Initialize the hardware bus/device. Implemented by DFR1216_I2C.
   * Called automatically by initializeService().
   */
  virtual bool begin() = 0;

  // ---- IsServiceInterface overrides ------------------------------------
  /** @return "DFR1216 Service" */
  std::string getServiceName() override;
  bool initializeService() override;
  bool startService()       override;
  bool stopService()        override;
  bool saveSettings()       override;
  bool loadSettings()       override;

  // ---- Service-layer wrappers (validated, logged) ----------------------
  /**
   * @brief Set a servo to an angle (-90–90°, clamped).
   * @param channel Servo channel 0–5
   * @param angle   Angle in degrees -90–90
   */
  bool setServoAngle(uint8_t channel, uint16_t angle);

  /**
   * @brief Set a DC motor speed.
   * @param motor Motor number 1–4
   * @param speed Speed percentage −100..+100 (negative = reverse)
   */
  bool setMotorSpeed(uint8_t motor, int8_t speed);

  /**
   * @brief Set one on-board WS2812 LED colour.
   * @param led_index  LED index 0–2
   * @param brightness Overall brightness 0–255
   */
  bool setLEDColor(uint8_t led_index, uint8_t red, uint8_t green,
                   uint8_t blue, uint8_t brightness);

  /** @brief Turn one LED off (sets colour and brightness to 0). */
  bool turnOffLED(uint8_t led_index);

  /** @brief Turn all three LEDs off. */
  bool turnOffAllLEDs();

  // ---- IsBotActionHandlerInterface -------------------------------------
  /** @return DFR1216Consts::udp_service_id (0x03) */
  uint8_t getBotServiceId() const override;

  /**
   * @brief Dispatch an incoming binary bot frame to the appropriate DFR1216 command.
   *        Returns the binary response; the caller (UDP/WebSocket/Web) is
   *        responsible for sending it.
   *
   * | Action | Cmd  | Payload                               | Reply                          |
   * |--------|------|---------------------------------------|--------------------------------|
   * | 0x31   | 0x01 | [led:u8][r:u8][g:u8][b:u8][bright:u8]| [0x31][resp_ok or error]       |
   * | 0x32   | 0x02 | [led:u8]                              | [0x32][resp_ok or error]       |
   * | 0x33   | 0x03 | (none)                                | [0x33][resp_ok or error]       |
   * | 0x34   | 0x04 | (none)                                | [0x34][resp_ok][JSON]          |
   * | 0x35   | 0x05 | (none)                                | [0x35][resp_ok][level:u8 0-100]|
   *
   * @param data Pointer to the raw binary frame (byte[0] is the action byte)
   * @param len  Frame length in bytes
   * @return Binary response string; empty means no reply
   */
  std::string handleBotMessage(const uint8_t *data, size_t len) override;

  // ---- Hardware API ----------------------------------------------------
  /**
   * @fn: getBattery
   * @brief: Get the k10's power
   * @return: uint8_t 0-100 percentage
   */
  uint8_t getBattery(void);

  /**
   * @fn: getIRData
   * @brief: Get the IR data
   * @return uint32_t Infrared coding
   */
  uint32_t getIRData(void);

  /**
   * @fn: sendIR
   * @brief: Send the IR data
   * @param data: The infrared code to be sent
   * @return: uint8_t result
   * @retval: 0x00 is successSERVO360_STOP_US
   * @retval: 0xff is failure
   */
  uint8_t sendIR(uint32_t data);

  /**
   * @fn: setWS2812
   * @brief: Set the WS2812 LED
   * @param data: The color of the LED
   * @param bright: Brightness 0-255
   * @return: uint8_t result
   * @retval: 0x00 is success
   * @retval: 0xff is failure
   */
  uint8_t setWS2812(uint32_t *data, uint8_t bright);

  /**
   * @fn: setMode
   * @brief: set io port mode
   * @return: uint8_t result
   * @retval: 0x00 is success
   * @retval: 0xff is failure
   */
  uint8_t setMode(eIONumber_t number, eIOType_t mode);

  /**
   * @fn: setGpioState
   * @brief: Set the GPIO state
   * @param number: GPIO number
   * @param state: GPIO state
   * @return: uint8_t result
   * @retval: 0x00 is success
   * @retval: 0xff is failure
   */
  uint8_t setGpioState(eIONumber_t number, eGpioState_t state);

  /**
   * @fn: getGpioState
   * @brief: Get the io state
   * @param number: io number
   * @return: uint8_t Pin level
   */
  uint8_t getGpioState(eIONumber_t number);

  /**
   * @fn: getADCValue
   * @brief: Get the ADC value
   * @param number: io number
   * @return: uint16_t adc value
   */
  uint16_t getADCValue(eIONumber_t number);

  /**
   * @fn: getDHTValuecan you
   * @brief: Get the DHT value
   * @param number: io number
   * @return: sDhtData_t dhtData
   * @retval: temperature is dhtData.temperature
   * @retval: humidity is dhtData.humidity
   */
  sDhtData_t getDHTValue(eIONumber_t number);

  /**
   * @fn: get18b20Value
   * @brief: Get the ds18b20 value
   * @param number: io number
   * @return: temperature
   */
  float get18b20Value(eIONumber_t number);

  /**
   * @fn: setMotorPeriod
   * @brief: Set the motor period
   * @param number: motor number
   * @param motorPeriod: motor period
   * @return: NULL
   */
  void setMotorPeriod(ePeriod_t number, uint16_t motorPeriod);

  /**
   * @fn: setMotorDuty
   * @brief: Set the motor duty
   * @param number: motor number
   * @param duty: motor duty
   * @return: NULL
   */
  void setMotorDuty(eMotorNumber_t number, uint16_t duty);

  /**
   * @fn: setServoAngle
   * @brief: Set the servo angle
   * @param number: servo number
   * @param angle: servo angle
   * @param maxAngle: servo max angle (180 or 270)
   * @return: NULL
   */
  void setServoAngle(eServoNumber_t number, uint16_t angle);
  
  void setServoAngle(eServoNumber_t number, uint16_t angle, uint16_t maxAngle);

  /**
   * @fn: setServo360
   * @brief Set the Servo360
   * @param number: servo number
   * @param direction
   * @param speed (0-100)
   * @return: NULL
   */
  void setServo360(eServoNumber_t number, eServo360Direction_t direction, uint8_t speed);

  /**
   * @fn: getSr04Distance
   * @brief: get sr04 distance
   * @return: distace
   * @retval: 0x00 is success
   * @retval: -1 is timerout error
   * @retval: -2 is timerout error
   * @retval: -3 is no to scale
   */
  int16_t getSr04Distance(void);

protected:
private:
  virtual uint8_t writeReg(uint8_t reg, uint8_t *data, uint8_t len) = 0;
  virtual int16_t readReg(uint8_t reg, uint8_t *data, uint8_t len)  = 0;

  LedState led_states_[3];  ///< Cached colour values for WS2812 LEDs 0–2
};

class DFR1216_I2C : public DFR1216Board
{
public:
  DFR1216_I2C(TwoWire *pWire = &Wire, uint8_t addr = 0x33);
  bool begin(void);

protected:
  virtual uint8_t writeReg(uint8_t reg, uint8_t *data, uint8_t len);
  virtual int16_t readReg(uint8_t reg, uint8_t *data, uint8_t len);

private:
  TwoWire         *__pWire;
  uint8_t          __I2C_addr;
  static SemaphoreHandle_t __i2c_mutex;  ///< Shared recursive mutex for I2C bus access
};

#endif