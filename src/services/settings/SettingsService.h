/**
 * @file SettingsService.h
 * @brief Settings service using ESP32 Preferences library
 * @details Provides persistent key-value storage organized by domains
 */
#pragma once

#include "../IsOpenAPIInterface.h"
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
    
    // IsServiceInterface implementation
    std::string getServiceName() override;
    
    // IsOpenAPIInterface implementation
    bool registerRoutes() override;
    std::string getServiceSubPath() override;
    

    
    /**
     * @brief Get a single setting value from persistent storage
     * @details Retrieves the value from ESP32 Preferences. If the key doesn't exist,
     *          returns the defaultValue without error.
     * @param domain Settings domain/namespace (acts as Preferences namespace)
     * @param key Setting key within the domain
     * @param defaultValue Default value if key doesn't exist (default: empty string)
     * @return Setting value from storage or defaultValue
     */
    std::string getSetting(const std::string& domain, const std::string& key, 
                          const std::string& defaultValue = "");
    
    /**
     * @brief Set a single setting value in persistent storage
     * @details Stores the value in ESP32 Preferences for persistence across reboots
     * @param domain Settings domain/namespace (acts as Preferences namespace)
     * @param key Setting key within the domain
     * @param value Setting value to store
     * @return true if successful, false otherwise
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
