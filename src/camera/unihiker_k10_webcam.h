/*!
 * @file unihiker_k10_webcam.h
 * @brief This is a driver library for a network camera
 * @copyright   Copyright (c) 2025 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @license     The MIT License (MIT)
 * @author [TangJie](jie.tang@dfrobot.com)
 * @version  V1.0
 * @date  2025-03-21 
 * @url https://github.com/DFRobot/unihiker_k10_webcam
 */
#ifndef _UNIHIKER_K10_WEBCAM_H_
#define _UNIHIKER_K10_WEBCAM_H_
#include "Arduino.h"
#include "unihiker_k10.h"
#include <WebServer.h>

#ifdef DEBUG
#define DEBUG_TO_SERIAL(x) Serial.println(x)
#define DEBUGF_TO_SERIAL(fmt, ...) Serial.printf(fmt, __VA_ARGS__)
#else
#define DEBUG_TO_SERIAL(x)
#define DEBUGF_TO_SERIAL(fmt, ...)
#endif

class unihiker_k10_webcam
{
public:
    /**
     * @fn unihiker_k10_webcam
     * @brief This is the class for the network camera
     */
    unihiker_k10_webcam(void);
    
    /**
     * @fn enableWebcam
     * @brief Enable the network camera and register routes with the provided WebServer
     * @return Returns true on success, false on failure
     */
    bool enableWebcam();



private:
    WebServer* _server;
    bool _enabled;
};
#endif
