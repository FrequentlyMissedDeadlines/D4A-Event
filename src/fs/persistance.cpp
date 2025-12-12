// persistance.cpp
// Functions for saving and loading persistent settings

#include "persistance.h"
#include <Arduino.h>
#include <FS.h>
#ifdef ESP32
#include <LittleFS.h>
#else
#include <SPIFFS.h>
#endif

#define SETTINGS_FILE "/settings.txt"

bool saveSettings(const String &data) {
#ifdef ESP32
    if (!LittleFS.begin()) return false;
#else
    if (!SPIFFS.begin()) return false;
#endif
    File file =
#ifdef ESP32
        LittleFS.open(SETTINGS_FILE, "w");
#else
        SPIFFS.open(SETTINGS_FILE, "w");
#endif
    if (!file) return false;
    file.print(data);
    file.close();
    return true;
}

bool save(const String &filename, const String &data) {
#ifdef ESP32
    if (!LittleFS.begin()) return false;
#else
    if (!SPIFFS.begin()) return false;
#endif
    File file =
#ifdef ESP32
        LittleFS.open(filename, "w");
#else
        SPIFFS.open(filename, "w");
#endif
    if (!file) return false;
    file.print(data);
    file.close();
    return true;
}

String load(const String &filename) {
#ifdef ESP32
    if (!LittleFS.begin()) return "";
#else
    if (!SPIFFS.begin()) return "";
#endif
    File file =
#ifdef ESP32
        LittleFS.open(filename, "r");
#else
        SPIFFS.open(filename, "r");
#endif
    if (!file) return "";
    String data = file.readString();
    file.close();
    return data;
}

String loadSettings() {
    return(load(SETTINGS_FILE)); 
}
bool saveSetting(const String &data) {
    return(save(SETTINGS_FILE, data)); 
}


String getSetting(const String &key) {
    String settings = loadSettings();
    if (settings == "") return "";
    int startIndex = settings.indexOf(key + "=");
    if (startIndex == -1) return "";
    startIndex += key.length() + 1;
    int endIndex = settings.indexOf("\n", startIndex);
    if (endIndex == -1) endIndex = settings.length();
    String data = settings.substring(startIndex, endIndex);
    data.trim();
    return data;
}