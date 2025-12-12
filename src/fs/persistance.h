// persistance.h
// Header for persistance.cpp

#ifndef PERSISTANCE_H
#define PERSISTANCE_H

#include <Arduino.h>


// Load settings from persistent storage
String loadSettings();

// Save settings to persistent storage
bool saveSettings(const String &data);

// Save to a specified file
String load(const String &filename);

// Save to a specified file
bool save(const String &filename, const String &data);

// Get a specific setting by key
String getSetting(const String &key) ;

#endif // PERSISTANCE_H
