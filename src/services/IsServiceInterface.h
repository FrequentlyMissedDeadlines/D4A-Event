#include <string>
#include "RollingLogger.h"

#pragma once

struct IsServiceInterface
{
public:
    /**
     * @fn: getName
     * @brief: Get the name of the service.
     * @return: Service name as a string.
     */
    virtual std::string getName() = 0;
    /**
   * @fn: initializeService
   * @brief: Initialize the service.
   * @return: true if initialization was successful, false otherwise.
   */
    virtual bool initializeService() = 0;
    /**
     * @fn: startService
     * @brief: Start the service.
     * @return: true if the service started successfully, false otherwise.
     */
    virtual bool startService() = 0;
    /**
     * @fn: stopService
     * @brief: Stop the service.
     * @return: true if the service stopped successfully, false otherwise.
     */
    virtual bool stopService() = 0;
    /**
     * @fn: asOpenAPIInterface
     * @brief: Get the OpenAPI interface for this service if it supports it.
     * @return: Pointer to IsOpenAPIInterface if supported, nullptr otherwise.
     */
    virtual class IsOpenAPIInterface* asOpenAPIInterface() { return nullptr; }

    bool setLogger(RollingLogger *rollingLogger)
    {
        if (!rollingLogger)
            return false;
        logger = rollingLogger;
        return true;
    };


    virtual ~IsServiceInterface() = default;
    
protected:
    RollingLogger *logger = nullptr;    
};