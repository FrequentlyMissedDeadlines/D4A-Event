#pragma once

#include <string>
#include "RollingLogger.h"
#include <pgmspace.h>

/**
 * @file IsServiceInterface.h
 * @brief Base interface for all services
 * @details Defines the contract that all services must implement for initialization,
 * starting, stopping, and optional OpenAPI support.
 */

namespace ServiceInterfaceConsts {
    constexpr const char msg_initialize_done[] PROGMEM = "initialize done";
    constexpr const char msg_initialize_failed[] PROGMEM = "initialize failed";
    constexpr const char msg_start_done[] PROGMEM = "start done";
    constexpr const char msg_start_failed[] PROGMEM = "start failed";
    constexpr const char msg_stop_done[] PROGMEM = "stop done";
    constexpr const char msg_stop_failed[] PROGMEM = "stop failed";
}

enum ServiceStatus { INIT_FAILED, START_FAILED, STARTED, STOPPED, STOP_FAILED };

struct IsServiceInterface
{
public:
    /**
     * @fn getServiceName
     * @brief Get the name of the service
     * @return Service name as a string
     */
    virtual std::string getServiceName() = 0;
    /**
     * @fn initializeService
     * @brief Initialize the service
     * @return true if initialization was successful, false otherwise
     */
    virtual bool initializeService() = 0;
    /**
     * @fn startService
     * @brief Start the service
     * @return true if the service started successfully, false otherwise
     */
    virtual bool startService() = 0;
    /**
     * @fn stopService
     * @brief Stop the service
     * @return true if the service stopped successfully, false otherwise
     */
    virtual bool stopService() = 0;
    /**
     * @fn asOpenAPIInterface
     * @brief Get the OpenAPI interface for this service if it supports it
     * @return Pointer to IsOpenAPIInterface if supported, nullptr otherwise
     */
    virtual class IsOpenAPIInterface* asOpenAPIInterface() { return nullptr; }
    /**
     * @fn saveSettings
     * @brief Save service settings
     * @return true if settings were saved successfully, false otherwise
     */
    virtual bool saveSettings() { return true; }
    /**
     * @fn loadSettings
     * @brief Load service settings
     * @return true if settings were loaded successfully, false otherwise
     */
    virtual bool loadSettings() { return true; }

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