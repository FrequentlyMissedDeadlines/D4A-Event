/**
 * @file FlashStringHelper.h
 * @brief Utility functions for working with Flash String Helpers (__FlashStringHelper).
 * Provides conversion and operator overloads for convenient string concatenation.
 */
#pragma once

#include <Arduino.h>
#include <string>

/**
 * @brief Utility function to convert PROGMEM string to std::string
 * @details Converts a PROGMEM (flash memory) string to a std::string in RAM.
 *          The PROGMEM string data is copied to the heap for the std::string.
 * @param flashStr The flash string pointer from FPSTR()
 * @return std::string copy of the PROGMEM string (allocated on heap)
 */
inline std::string fpstr_to_string(const __FlashStringHelper* flashStr) {
    if (!flashStr) return std::string();
    const char* str = reinterpret_cast<const char*>(flashStr);
    return std::string(str);
}

/**
 * @brief Simplified utility to convert PROGMEM constant directly to std::string
 * @details Convenience wrapper that combines FPSTR() and fpstr_to_string() into one call.
 *          Usage: progmem_to_string(MyConsts::my_string) instead of fpstr_to_string(FPSTR(MyConsts::my_string))
 * @tparam T Type of the PROGMEM constant (typically constexpr const char[])
 * @param progmemConst The PROGMEM constant (declared with PROGMEM attribute)
 * @return std::string copy of the PROGMEM string (allocated on heap)
 */
template<typename T>
inline std::string progmem_to_string(const T& progmemConst) {
    return fpstr_to_string(FPSTR(progmemConst));
}

/**
 * @brief Operator overload for concatenating std::string and __FlashStringHelper
 * @details Safely concatenates a std::string with a PROGMEM string.
 *          The PROGMEM string is converted to RAM and concatenated.
 * @param lhs The left-hand side std::string
 * @param rhs The right-hand side __FlashStringHelper pointer (from FPSTR())
 * @return Concatenated std::string (allocated on heap)
 */
inline std::string operator+(const std::string& lhs, const __FlashStringHelper* rhs) {
    return lhs + fpstr_to_string(rhs);
}

/**
 * @brief Operator overload for concatenating __FlashStringHelper and std::string
 * @details Safely concatenates a PROGMEM string with a std::string.
 *          The PROGMEM string is converted to RAM and concatenated.
 * @param lhs The left-hand side __FlashStringHelper pointer (from FPSTR())
 * @param rhs The right-hand side std::string
 * @return Concatenated std::string (allocated on heap)
 */
inline std::string operator+(const __FlashStringHelper* lhs, const std::string& rhs) {
    return fpstr_to_string(lhs) + rhs;
}
