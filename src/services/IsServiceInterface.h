#pragma once

#include <string>
#include <map>
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
class SettingsService;

namespace ServiceInterfaceConsts
{

    constexpr const char service_status_uninitialized[] PROGMEM = "uninitialized";
    constexpr const char service_status_not_initialized[] PROGMEM = "not initialized";
    constexpr const char service_status_initialized[] PROGMEM = "initialized";
    constexpr const char service_status_initialize_failed[] PROGMEM = "initialize failed";
    constexpr const char service_status_started[] PROGMEM = "started";
    constexpr const char service_status_start_failed[] PROGMEM = "start failed";
    constexpr const char service_status_stopped[] PROGMEM = "stopped";
    constexpr const char service_status_stop_failed[] PROGMEM = "stop failed";
    constexpr const char service_status_unknown[] PROGMEM = "unknown";

    constexpr const char msg_settings_load_success[] PROGMEM = "Settings loaded successfully";
    constexpr const char msg_settings_load_failed[] PROGMEM = "Settings load failed";
    constexpr const char msg_settings_save_success[] PROGMEM = "Settings saved successfully";
    constexpr const char msg_settings_save_failed[] PROGMEM = "Settings save failed";
    constexpr const char msg_no_settings_service[] PROGMEM = "No settings service available";

}
/**
 * @brief Enumeration for service status
 * Defines the various states a service can be in, such as uninitialized, initialized, started, stopped, etc.
 * possible path : 
 * UNINITIALIZED->INITIALIZED | INITIALIZED_FAILED
 * INITIALIZED-> STARTED | START_FAILED
 * STARTED -> STOPPED | STOP_FAILED
 * STOPPED -> STARTED | STOP_FAILED
 */
enum ServiceStatus
{
    UNINITIALIZED,
    INITIALIZED,
    INITIALIZED_FAILED,
    STARTED,
    START_FAILED,
    STOPPED,
    STOP_FAILED
};

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
     * @fn setDefaultSettings
     * @brief Load default settings for the service
     * @details Default implementation does nothing. Override to provide default settings for the service.
     */
    virtual void setDefaultSettings() {}

    /**
     * @fn initializeService
     * @brief Initialize the service
     * @details Default implementation sets status to INITIALIZED and logs completion.
     *          Override if your service needs custom initialization logic.
     * @return true if initialization was successful, false otherwise
     */
    virtual bool initializeService()
    {
        setDefaultSettings();
        setServiceStatus(INITIALIZED);
#ifdef VERBOSE_DEBUG
        if (logger)
        {
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
        if (logger)
        {
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
        if (logger)
        {
            logger->debug(getServiceName() + " " + FPSTR(ServiceInterfaceConsts::msg_stop_done));
        }
#endif
        return true;
    }
    /**
     * @fn saveSettings
     * @brief Save service settings to persistent storage
     * @details Default implementation saves all key-value pairs from settings_map_
     *          using the SettingsService. Override for custom save logic.
     * @return true if settings were saved successfully, false otherwise
     */
    virtual bool saveSettings();

    /**
     * @fn loadSettings
     * @brief Load service settings from persistent storage
     * @details Default implementation loads all keys defined in settings_map_
     *          using the SettingsService. Override for custom load logic.
     * @return true if settings were loaded successfully, false otherwise
     */
    virtual bool loadSettings();

    /**
     * @fn initializeDefaultSettings
     * @brief Initialize the settings map with default values for this service
     * @details Override this method to populate the settings_map_ with default key-value pairs for your service. 
     * This should be called during service initialization before loading any saved settings, allowing defaults to be
     * applied if no saved settings are found.
     */ 
    virtual void initializeDefaultSettings() ;
    /**
     * @fn getSettingsDomain
     * @brief Get the settings domain name for this service
     * @details Used by default load/save implementations. Override to customize.
     * @return Domain name (defaults to service name)
     */
    virtual std::string getSettingsDomain() { return getServiceName(); }

    /**
     * @fn setSettingsService
     * @brief Set the settings service for persistent storage
     * @param settingsService Pointer to the SettingsService instance
     * @return true if successful, false otherwise
     */
    bool setSettingsService(SettingsService *settingsService)
    {
        if (!settingsService)
            return false;
        settings_service_ = settingsService;
        return true;
    }

    /**
     * @fn getSettingsMap
     * @brief Get reference to the settings map
     * @return Reference to the settings map
     */
    std::map<std::string, std::string> &getSettingsMap() { return settings_map_; }

    /**
     * @fn getSettingsMap (const version)
     * @brief Get const reference to the settings map
     * @return Const reference to the settings map
     */
    const std::map<std::string, std::string> &getSettingsMap() const { return settings_map_; }

    
    /**
     * @fn setLogger
     * @brief Set the logger for this service
     * @param rollingLogger Pointer to a RollingLogger instance
     * @return true if the logger was set successfully, false otherwise
     */
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
    virtual IsOpenAPIInterface *asOpenAPIInterface() { return nullptr; }
    /**
     * @fn ~IsServiceInterface
     * @brief Virtual destructor for proper cleanup of derived classes
     */
    virtual ~IsServiceInterface() = default;
    /**
     * @fn getStatus
     * @brief Get the current status of the service
     * @return Current service status as a ServiceStatus enum value
     */
    ServiceStatus getStatus()
    {
        return service_status_;
    }
    /**
     * @fn isServiceInitialized
     * @brief Check if the service is currently initialized
     * @return true if the service is initialized, false otherwise
     */
    bool isServiceInitialized() const
    {
        return service_status_ == INITIALIZED;
    }
    /**
     * @fn isServiceStarted
     * @brief Check if the service is currently started
     * @return true if the service is started, false otherwise
     */
    bool isServiceStarted() const
    {
        return service_status_ == STARTED;
    }
    /**
     * @fn isServiceStopped
     * @brief Check if the service is currently stopped
     * @return true if the service is stopped, false otherwise
     */
    bool isServiceStopped() const
    {
        return service_status_ == STOPPED;
    }
    /**
     * @fn getStatusString
     * @brief Get a human-readable string representation of the current service status
     * @return Status string corresponding to the current service status
     */
    std::string getStatusString() const
    {
        switch (service_status_)
        {
        case UNINITIALIZED:
            return progmem_to_string(ServiceInterfaceConsts::service_status_uninitialized);
        case INITIALIZED:
            return progmem_to_string(ServiceInterfaceConsts::service_status_initialized);
        case INITIALIZED_FAILED:
            return progmem_to_string(ServiceInterfaceConsts::service_status_initialize_failed);
        case STARTED:
            return progmem_to_string(ServiceInterfaceConsts::service_status_started);
        case START_FAILED:
            return progmem_to_string(ServiceInterfaceConsts::service_status_start_failed);
        case STOPPED:
            return progmem_to_string(ServiceInterfaceConsts::service_status_stopped);
        case STOP_FAILED:
            return progmem_to_string(ServiceInterfaceConsts::service_status_stop_failed);
        default:
            return progmem_to_string(ServiceInterfaceConsts::service_status_unknown);
        }
    }

protected:
    SettingsService *settings_service_ = nullptr;
    /**
     * @brief Set the current status of the service
     * @param status The new status to set
     * @details Updates the service status and records the timestamp of the change.
     */
    void setServiceStatus(ServiceStatus status)
    {
        service_status_ = status;
        status_timestamp_ = millis();
    }
    /**
     * @brief Pointer to the logger for this service
     * @details Set via setLogger. Used for logging messages from the service.
     */
    RollingLogger *logger = nullptr;
    /**
     * @brief Pointer to the settings service for this service to use in load/save operations
     * @details Set via setSettingsService. Used in default implementations of loadSettings and saveSettings.
     *          Services can also use this pointer directly for custom load/save logic if needed.
     */

    /**
     * @brief Current status of the service, used for monitoring and control flow. Initialized to UNINITIALIZED.
     * @details Updated by setServiceStatus() whenever the service changes state. Can be used
     */
    ServiceStatus service_status_ = UNINITIALIZED;
    /**
     * @brief Timestamp of the last status change, used for monitoring and debugging purposes
     * @details Updated whenever setServiceStatus is called. Can be used to track how long the service has been in its current state.
     */
    unsigned long status_timestamp_ = 0;

    /**
     * @brief Map of settings keys and values for this service
     * @details Services can populate this map with their settings.
     *          Default load/save implementations will use this map.
     */
    std::map<std::string, std::string> settings_map_;
};