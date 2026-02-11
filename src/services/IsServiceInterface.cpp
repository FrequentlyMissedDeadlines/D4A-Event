/**
 * @file IsServiceInterface.cpp
 * @brief Implementation of IsServiceInterface methods
 */

#include "IsServiceInterface.h"
#include "settings/SettingsService.h"
#include "FlashStringHelper.h"

/**
 * @fn saveSettings
 * @brief Save service settings to persistent storage
 * @details Default implementation saves all key-value pairs from settings_map_
 *          using the SettingsService. Override for custom save logic.
 * @return true if settings were saved successfully, false otherwise
 */
bool IsServiceInterface::saveSettings()
{
    if (!settings_service_)
    {
        if (logger) {
            logger->warning(getServiceName() + ": " + fpstr_to_string(FPSTR(ServiceInterfaceConsts::msg_no_settings_service)));
        }
        return false;
    }
    
    if (settings_map_.empty())
    {
        // No settings to save - this is not an error
        #ifdef VERBOSE_DEBUG
        if (logger) {
            logger->debug(getServiceName() + ": No settings to save");
        }
        #endif
        return true;
    }
    
    std::string domain = getSettingsDomain();
    bool success = true;
    
    for (const auto& pair : settings_map_)
    {
        if (!settings_service_->setSetting(domain, pair.first, pair.second))
        {
            success = false;
            if (logger) {
                logger->error(getServiceName() + ": Failed to save setting '" + pair.first + "'");
            }
        }
    }
    
    if (success)
    {
        #ifdef VERBOSE_DEBUG
        if (logger) {
            logger->debug(getServiceName() + ": " + fpstr_to_string(FPSTR(ServiceInterfaceConsts::msg_settings_save_success)));
        }
        #endif
    }
    else
    {
        if (logger) {
            logger->error(getServiceName() + ": " + fpstr_to_string(FPSTR(ServiceInterfaceConsts::msg_settings_save_failed)));
        }
    }
    
    return success;
}

/**
 * @fn loadSettings
 * @brief Load service settings from persistent storage
 * @details Default implementation loads all keys defined in settings_map_
 *          using the SettingsService. Override for custom load logic.
 * @return true if settings were loaded successfully, false otherwise
 */
bool IsServiceInterface::loadSettings()
{
    if (!settings_service_)
    {
        if (logger) {
            logger->warning(getServiceName() + ": " + fpstr_to_string(FPSTR(ServiceInterfaceConsts::msg_no_settings_service)));
        }
        return false;
    }
    
    if (settings_map_.empty())
    {
        // No settings to load - this is not an error
        #ifdef VERBOSE_DEBUG
        if (logger) {
            logger->debug(getServiceName() + ": No settings to load");
        }
        #endif
        return true;
    }
    
    std::string domain = getSettingsDomain();
    bool success = true;
    
    for (auto& pair : settings_map_)
    {
        // Load each setting, keeping the current value as default if not found
        std::string loaded_value = settings_service_->getSetting(domain, pair.first, pair.second);
        pair.second = loaded_value;
        
        #ifdef VERBOSE_DEBUG
        if (logger) {
            logger->debug(getServiceName() + ": Loaded '" + pair.first + "' = '" + loaded_value + "'");
        }
        #endif
    }
    
    if (success)
    {
        #ifdef VERBOSE_DEBUG
        if (logger) {
            logger->debug(getServiceName() + ": " + fpstr_to_string(FPSTR(ServiceInterfaceConsts::msg_settings_load_success)));
        }
        #endif
    }
    
    return success;
}

void IsServiceInterface::initializeDefaultSettings() { }