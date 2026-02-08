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

// Forward declaration to avoid circular dependency
struct IsOpenAPIInterface;

namespace ServiceInterfaceConsts {

    constexpr const char service_status_uninitialized[] PROGMEM = "uninitialized";
    constexpr const char service_status_initialized[] PROGMEM = "initialized";
    constexpr const char service_status_initialize_failed[] PROGMEM = "initialize failed";
    constexpr const char service_status_started[] PROGMEM = "started";
    constexpr const char service_status_start_failed[] PROGMEM = "start failed";
    constexpr const char service_status_stopped[] PROGMEM = "stopped";
    constexpr const char service_status_stop_failed[] PROGMEM = "stop failed";
    constexpr const char service_status_unknown[] PROGMEM = "unknown";
    

}

enum ServiceStatus { UNINITIALIZED, INITIALIZED, INITIALIZED_FAILED, STARTED, START_FAILED,  STOPPED, STOP_FAILED };

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
     * @details Default implementation sets status to INITIALIZED and logs completion.
     *          Override if your service needs custom initialization logic.
     * @return true if initialization was successful, false otherwise
     */
    virtual bool initializeService() 
    { 
        setServiceStatus(INITIALIZED);
        #ifdef VERBOSE_DEBUG
        if (logger) {
            logger->debug(getServiceName() + " " + FPSTR(ServiceInterfaceConsts::msg_initialize_done));
        }
        #endif
        return true; 
    }
    
    /**
     * @fn startService
     * @brief Start the service
     * @details Default implementation sets status to STARTED and logs completion.
     *          Override if your service needs custom startup logic.
     * @return true if the service started successfully, false otherwise
     */
    virtual bool startService() 
    { 
        setServiceStatus(STARTED);
        #ifdef VERBOSE_DEBUG
        if (logger) {
            logger->debug(getServiceName() + " " + FPSTR(ServiceInterfaceConsts::msg_start_done));
        }
        #endif
        return true; 
    }
    
    /**
     * @fn stopService
     * @brief Stop the service
     * @details Default implementation sets status to STOPPED and logs completion.
     *          Override if your service needs custom shutdown logic.
     * @return true if the service stopped successfully, false otherwise
     */
    virtual bool stopService() 
    { 
        setServiceStatus(STOPPED);
        #ifdef VERBOSE_DEBUG
        if (logger) {
            logger->debug(getServiceName() + " " + FPSTR(ServiceInterfaceConsts::msg_stop_done));
        }
        #endif
        return true; 
    }
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

    /**
     * @fn asOpenAPIInterface
     * @brief Check if this service implements IsOpenAPIInterface
     * @return Pointer to IsOpenAPIInterface if implemented, nullptr otherwise
     */
    virtual IsOpenAPIInterface* asOpenAPIInterface() { return nullptr; }

    virtual ~IsServiceInterface() = default;

    ServiceStatus getStatus(){
        return service_status_;
    }
    
    bool isStarted() const
    {
        return service_status_ == STARTED;  

    }
    std::string getStatusString() const
    {
        switch (service_status_)
        {
            case UNINITIALIZED: return fpstr_to_string(FPSTR(ServiceInterfaceConsts::service_status_uninitialized));
            case INITIALIZED: return fpstr_to_string(FPSTR(ServiceInterfaceConsts::service_status_initialized));
            case INITIALIZED_FAILED: return fpstr_to_string(FPSTR(ServiceInterfaceConsts::service_status_initialize_failed));
            case STARTED: return fpstr_to_string(FPSTR(ServiceInterfaceConsts::service_status_started));
            case START_FAILED: return fpstr_to_string(FPSTR(ServiceInterfaceConsts::service_status_start_failed));
            case STOPPED: return fpstr_to_string(FPSTR(ServiceInterfaceConsts::service_status_stopped));
            case STOP_FAILED: return fpstr_to_string(FPSTR(ServiceInterfaceConsts::service_status_stop_failed));
            default: return fpstr_to_string(FPSTR(ServiceInterfaceConsts::service_status_unknown));
        }
    }   
protected:
    void setServiceStatus(ServiceStatus status){
        service_status_ = status;
        status_timestamp_ = millis();
    }   
    RollingLogger *logger = nullptr;
    ServiceStatus service_status_ = UNINITIALIZED;
    unsigned long status_timestamp_ = 0;
};