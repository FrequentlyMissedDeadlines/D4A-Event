/**
 * @file SettingsService.h
 * @brief Settings service using ESP32 Preferences library
 * @details Provides persistent key-value storage organized by domains
 */
#pragma once

#include "../services/IsServiceInterface.h"
#include "../services/IsOpenAPIInterface.h"
#include <Preferences.h>
#include <string>
#include <vector>

/**
 * @struct Setting
 * @brief Represents a single setting with domain, key, and value
 */
struct Setting
{
    std::string domain;
    std::string key;
    std::string value;
    
    Setting() : domain(""), key(""), value("") {}
    Setting(const std::string& d, const std::string& k, const std::string& v) 
        : domain(d), key(k), value(v) {}
};

/**
 * @class SettingsService
 * @brief Service for managing persistent settings using ESP32 Preferences
 */
class SettingsService : public IsOpenAPIInterface
{
public:
    SettingsService();
    ~SettingsService();
    
    /**
     * @brief Initialize the settings service
     * @return true if successful, false otherwise
     */
    bool initializeService() override;
    
    /**
     * @brief Start the settings service
     * @return true if successful, false otherwise
     */
    bool startService() override;
    
    /**
     * @brief Stop the settings service
     * @return true if successful, false otherwise
     */
    bool stopService() override;
    
    /**
     * @brief Get service name
     * @return Service name as string
     */
    std::string getServiceName() override;
    
    /**
     * @brief Register HTTP routes for settings operations
     * @return true if registration was successful
     */
    bool registerRoutes() override;
    
    /**
     * @brief Get service subpath component
     * @return Service subpath
     */
    std::string getServiceSubPath() override;
    

    
    /**
     * @brief Get a single setting value
     * @param domain Settings domain/namespace
     * @param key Setting key
     * @param defaultValue Default value if key doesn't exist
     * @return Setting value or default
     */
    std::string getSetting(const std::string& domain, const std::string& key, 
                          const std::string& defaultValue = "");
    
    /**
     * @brief Set a single setting value
     * @param domain Settings domain/namespace
     * @param key Setting key
     * @param value Setting value
     * @return true if successful
     */
    bool setSetting(const std::string& domain, const std::string& key, 
                   const std::string& value);
    
    /**
     * @brief Get all settings in a domain
     * @param domain Settings domain/namespace
     * @return Vector of Setting structs
     */
    std::vector<Setting> getAllSettings(const std::string& domain);
    
    /**
     * @brief Set multiple settings in a domain
     * @param domain Settings domain/namespace
     * @param settings Vector of Setting structs (only key and value are used)
     * @return true if all settings were saved successfully
     */
    bool setMultipleSettings(const std::string& domain, 
                            const std::vector<Setting>& settings);
    
    /**
     * @brief Delete a setting
     * @param domain Settings domain/namespace
     * @param key Setting key
     * @return true if successful
     */
    bool deleteSetting(const std::string& domain, const std::string& key);
    
    /**
     * @brief Clear all settings in a domain
     * @param domain Settings domain/namespace
     * @return true if successful
     */
    bool clearDomain(const std::string& domain);

private:    
    bool initialized_;
    
    /**
     * @brief Handle GET /settings request
     */
    static void handleGetSettings();
    
    /**
     * @brief Handle POST /settings request
     */
    static void handlePostSettings();
    
    /**
     * @brief Build JSON response for multiple settings
     * @param settings Vector of Setting structs
     * @return JSON string
     */
    static std::string buildSettingsJson(const std::vector<Setting>& settings);
    
    /**
     * @brief Validate domain name (length and character restrictions)
     * @param domain Domain name to validate
     * @return true if valid
     */
    static bool isValidDomain(const std::string& domain);
    
    /**
     * @brief Validate key name
     * @param key Key name to validate
     * @return true if valid
     */
    static bool isValidKey(const std::string& key);
};
