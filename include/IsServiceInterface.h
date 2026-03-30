/**
 * @file IsServiceInterface.h
 * @brief Base interface for all services.
 *
 * @details Every service inherits from this struct and implements the
 * three lifecycle methods (initializeService / startService / stopService)
 * plus optional persistence (saveSettings / loadSettings).
 *
 * Typical usage in main.cpp:
 * @code
 *   bool start_service(IsServiceInterface &svc) {
 *       svc.setDebugLogger(&debug_logger);
 *       svc.setServiceLogger(&svc_logger);
 *       if (!svc.initializeService()) return false;
 *       if (!svc.startService())      return false;
 *       return true;
 *   }
 * @endcode
 */
#pragma once

#include <string>
#include <pgmspace.h>
#include "RollingLogger.h"
#include "FlashStringHelper.h"

// Forward declarations (avoid circular includes)
struct IsOpenAPIInterface;
struct IsBotMessageHandlerInterface;

// ---------------------------------------------------------------------------
// Status strings — stored in flash to save RAM
// ---------------------------------------------------------------------------
namespace ServiceInterfaceConsts
{
    constexpr const char service_status_uninitialized[]     PROGMEM = "uninitialized";
    constexpr const char service_status_initialized[]       PROGMEM = "initialized";
    constexpr const char service_status_initialize_failed[] PROGMEM = "initialize failed";
    constexpr const char service_status_started[]           PROGMEM = "started";
    constexpr const char service_status_start_failed[]      PROGMEM = "start failed";
    constexpr const char service_status_stopped[]           PROGMEM = "stopped";
    constexpr const char service_status_stop_failed[]       PROGMEM = "stop failed";
    constexpr const char service_status_unknown[]           PROGMEM = "unknown";
}

// ---------------------------------------------------------------------------
// ServiceStatus — state machine for service lifecycle
// ---------------------------------------------------------------------------
/**
 * @brief Lifecycle states of a service.
 *
 * Valid transitions:
 *   UNINITIALIZED → INITIALIZED | INITIALIZED_FAILED
 *   INITIALIZED   → STARTED     | START_FAILED
 *   STARTED       → STOPPED     | STOP_FAILED
 *   STOPPED       → STARTED     | STOP_FAILED
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

namespace ServiceConst {
    constexpr const char PROGMEM msg_init_ok[] PROGMEM = "Initialized.";
    constexpr const char PROGMEM msg_init_failed[] PROGMEM = "Initialize failed.";
    constexpr const char PROGMEM msg_start_ok[] PROGMEM = "Started.";
    constexpr const char PROGMEM msg_start_failed[] PROGMEM = "Start failed.";
    constexpr const char PROGMEM msg_stop_ok[]  PROGMEM = "Stopped.";
    constexpr const char PROGMEM msg_stop_failed[]  PROGMEM = "Stop failed.";
}
// ---------------------------------------------------------------------------
// IsServiceInterface
// ---------------------------------------------------------------------------
struct IsServiceInterface
{
   
public:
    // ---- Pure virtual: every service must provide a name ----

    /**
     * @brief Return the human-readable service name (e.g. "WiFi Service").
     */
    virtual std::string getServiceName() = 0;

    // ---- Lifecycle (default implementations set status & return true) ----

    /**
     * @brief Initialize hardware / allocate resources.
     * @return true on success, false on failure.
     */
    virtual bool initializeService()
    {
        setServiceStatus(INITIALIZED);
        return true;
    }

    /**
     * @brief Start the service (connect, listen, …).
     * @return true on success, false on failure.
     */
    virtual bool startService()
    {
        setServiceStatus(STARTED);
        return true;
    }

    /**
     * @brief Stop the service and release resources.
     * @return true on success, false on failure.
     */
    virtual bool stopService()
    {
        setServiceStatus(STOPPED);
        return true;
    }

    // ---- Optional persistence ----

    /**
     * @brief Persist current settings to NVS / flash.
     * @return true on success. Default: no-op, returns false.
     */
    virtual bool saveSettings() { return false; }

    /**
     * @brief Load settings from NVS / flash.
     * @return true on success. Default: no-op, returns false.
     */
    virtual bool loadSettings() { return false; }

    /**
     * @brief Reset settings to compile-time defaults and clear NVS.
     * @return true on success. Default: no-op, returns false.
     */
    virtual bool resetSettings() { return false; }

    // ---- Logger ----

    /**
     * @brief Attach a debugLogger (must be called before initializeService).
     * @param rollingLogger Pointer to a RollingLogger; nullptr disables logging.
     * @return true if debugLogger was set, false if nullptr was passed.
     */
    bool setDebugLogger(RollingLogger *rollingLogger)
    {
        if (!rollingLogger)
            return false;
        debugLogger = rollingLogger;
        return true;
    }
    /**
     * @brief Attach a service-level logger (must be called before initializeService).
     *
     * Use this for info/error messages that should appear in the service log
     * (svc_logger), as opposed to the verbose debugLogger.
     * @param rollingLogger Pointer to a RollingLogger; nullptr disables logging.
     * @return true if logger was set, false if nullptr was passed.
     */
    bool setServiceLogger(RollingLogger *rollingLogger)
    {
        if (!rollingLogger)
            return false;
        serviceLogger = rollingLogger;
        return true;
    }
    // ---- Downcast helpers (avoids dynamic_cast) ----

    /** @return this as IsOpenAPIInterface* if the service implements it, else nullptr. */
    virtual IsOpenAPIInterface *asOpenAPIInterface() { return nullptr; }

    /** @return this as IsBotMessageHandlerInterface* if the service implements it, else nullptr. */
    virtual IsBotMessageHandlerInterface *asUDPMessageHandlerInterface() { return nullptr; }

    // ---- Status queries ----

    /** @return Current lifecycle state. */
    ServiceStatus getStatus() const { return service_status_; }

    /** @return true if the service is currently in STARTED state. */
    bool isServiceStarted()     const { return service_status_ == STARTED;      }

    /** @return true if the service is currently in INITIALIZED state. */
    bool isServiceInitialized() const { return service_status_ == INITIALIZED;  }

    /** @return true if the service is currently in STOPPED state. */
    bool isServiceStopped()     const { return service_status_ == STOPPED;      }

    /**
     * @brief Return a human-readable representation of the current status.
     */
    std::string getStatusString() const
    {
        switch (service_status_)
        {
        case UNINITIALIZED:      return progmem_to_string(ServiceInterfaceConsts::service_status_uninitialized);
        case INITIALIZED:        return progmem_to_string(ServiceInterfaceConsts::service_status_initialized);
        case INITIALIZED_FAILED: return progmem_to_string(ServiceInterfaceConsts::service_status_initialize_failed);
        case STARTED:            return progmem_to_string(ServiceInterfaceConsts::service_status_started);
        case START_FAILED:       return progmem_to_string(ServiceInterfaceConsts::service_status_start_failed);
        case STOPPED:            return progmem_to_string(ServiceInterfaceConsts::service_status_stopped);
        case STOP_FAILED:        return progmem_to_string(ServiceInterfaceConsts::service_status_stop_failed);
        default:                 return progmem_to_string(ServiceInterfaceConsts::service_status_unknown);
        }
    }

    // ---- Virtual destructor ----
    virtual ~IsServiceInterface() = default;

protected:
    /**
     * @brief Update the lifecycle state and record the timestamp.
     * @param status New state.
     */
    void setServiceStatus(ServiceStatus status)
    {
        service_status_    = status;
        status_timestamp_  = millis();
    }

    /// Verbose debug logger — set via setDebugLogger(); may be nullptr.
    RollingLogger *debugLogger = nullptr;

    /// Service-level logger — set via setServiceLogger(); may be nullptr.
    RollingLogger *serviceLogger = nullptr;

    /// Current lifecycle state.
    ServiceStatus  service_status_   = UNINITIALIZED;

    /// millis() timestamp of the last state change.
    unsigned long  status_timestamp_ = 0;
};
