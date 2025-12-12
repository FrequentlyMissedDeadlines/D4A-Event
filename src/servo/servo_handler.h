// Servo controller handler for DFR0548 board
/**
 * @file servo_handler.h
 * @brief Header for servo controller integration with the main application
 */

#ifndef SERVO_HANDLER_H
#define SERVO_HANDLER_H

#include "DFR0548.h"
#include <WebServer.h>

// Global servo controller instance
extern DFR0548_Controller servoController;

// Initialize servo controller
bool ServoHandler_init();

// Set servo angle (wrapper for easier integration)
bool ServoHandler_setAngle(uint8_t channel, uint16_t angle);

// Set servo speed for continuous rotation
bool ServoHandler_setSpeed(uint8_t channel, int8_t speed);

// Stop servo
bool ServoHandler_stopServo(uint8_t channel);

// Get servo status
String ServoHandler_getStatus(uint8_t channel);

bool ServoModule_registerRoutes(WebServer* server);

#endif // SERVO_HANDLER_H