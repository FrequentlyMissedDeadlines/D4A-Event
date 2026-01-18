/*!
 * @file DFRobot_UnihikerExpansion.h
 * @brief Define the basic structure of the DFRobot_UnihikerExpansion class, the implementation of the basic methods
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

//#define ENABLE_DBG  1 ///> Open this macro and you can see the details of the program
#ifdef ENABLE_DBG
  #define DBG(...) {Serial.print("[");Serial.print(__FUNCTION__); Serial.print("(): "); Serial.print(__LINE__); Serial.print(" ] "); Serial.println(__VA_ARGS__);}
#else
  #define DBG(...)
#endif

typedef enum {
  eMotor1_2,
  eMotor3_4,
  eServo0_1,
  eServo2_5,
}ePeriod_t;

typedef enum {
  eServo0,
  eServo1,
  eServo2,
  eServo3,
  eServo4,
  eServo5,
}eServoNumber_t;

typedef enum {
  eMotor1_A,
  eMotor1_B,
  eMotor2_A,
  eMotor2_B,
  eMotor3_A,
  eMotor3_B,
  eMotor4_A,
  eMotor4_B,
}eMotorNumber_t;

typedef enum {
  eADC,
  eDHT11,
  eDHT22,
  eDS18B20,
  eWriteGpio,
  eReadGpio,
}eIOType_t;

typedef enum {
  eC0,
  eC1,
  eC2,
  eC3,
  eC4,
  eC5,
}eIONumber_t;

typedef enum {
  eForward,
  eBackward,
  eStop,
}eServo360Direction_t;

typedef enum {
  eLOW,
  eHIGH,
}eGpioState_t;

typedef enum {
  eTypeDHT11,
  eTypeDHT22,
}eDhtType_t;

typedef struct{
  float temperature;
  float humidity;
  uint8_t state;
}sDhtData_t;

#define I2C_MOTOR12_PERIOD_H  0X00 
#define I2C_MOTOR34_PERIOD_H  0x02
#define I2C_MOTOR1_Z_DUTY_H   0X04
#define I2C_MOTOR1_F_DUTY_H   0X06
#define I2C_MOTOR2_Z_DUTY_H   0X08
#define I2C_MOTOR2_F_DUTY_H   0X0a
#define I2C_MOTOR3_Z_DUTY_H   0x0c
#define I2C_MOTOR3_F_DUTY_H   0x0E
#define I2C_MOTOR4_Z_DUTY_H   0x10
#define I2C_MOTOR4_F_DUTY_H   0x12
#define I2C_SERVO01_PERIOD_H  0x14
#define I2C_SERVO25_PERIOD_H  0x16
#define I2C_SERVO0_DUTY_H     0x18
#define I2C_SERVO1_DUTY_H     0x1a
#define I2C_SERVO2_DUTY_H     0x1c
#define I2C_SERVO3_DUTY_H     0x1e
#define I2C_SERVO4_DUTY_H     0x20
#define I2C_SERVO5_DUTY_H     0x22
#define I2C_IR_S_STATE 				0x24
#define I2C_SR04_STATE 				0x29
	#define SR04_COLLECT  	    0x01
	#define SR04_COMPLETE 	    0x02
#define I2C_IO_MODE_C0		    0x2c
#define I2C_W_C0		          0x39
#define I2C_R_C0 			        0x3f
#define I2C_ADC_C0_S				  0x45
#define I2C_DHT_C0_S				  0x57
#define I2C_18B20_C0_S			  0x75
#define I2C_BATTERY					  0x87
#define I2C_IR_R_STATE 			  0x88
#define I2C_WS2812_STATE			0x90
#define I2C_RESET_SENSOR 			0xa0
  #define DATA_ENABLE         0x01     ///> enable
  #define DATA_DISABLE        0x00     ///> disable
  #define MODE_ERROR          0x02
  #define RETRY_COUNT         10
  #define TEMP_LEN            32

class DFRobot_UnihikerExpansion{
public:
  DFRobot_UnihikerExpansion();
  ~DFRobot_UnihikerExpansion();
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
   * @retval: 0x00 is success
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
   * @fn: getDHTValue
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
  void setMotorPeriod(ePeriod_t number ,uint16_t motorPeriod);

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
   * @param angle: servo angle (0-180)
   * @return: NULL
   */
  void setServoAngle(eServoNumber_t number, uint8_t angle);

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
private:
  virtual uint8_t writeReg(uint8_t reg, uint8_t *data, uint8_t len) = 0;
  virtual int16_t readReg(uint8_t reg, uint8_t *data, uint8_t len) = 0;
};

class DFRobot_UnihikerExpansion_I2C:public DFRobot_UnihikerExpansion{
public:
  DFRobot_UnihikerExpansion_I2C(TwoWire *pWire=&Wire, uint8_t addr = 0x33);
  bool begin(void);
protected:
  virtual uint8_t writeReg(uint8_t reg, uint8_t *data, uint8_t len);
  virtual int16_t readReg(uint8_t reg, uint8_t *data, uint8_t len);
private:
  TwoWire *__pWire;
  uint8_t __I2C_addr;
};
#endif