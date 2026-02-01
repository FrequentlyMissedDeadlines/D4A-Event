/**
 * @file FlashStringHelper.h
 * @brief Utility functions for working with Flash String Helpers (__FlashStringHelper).
 * Provides conversion and operator overloads for convenient string concatenation.
 */
#pragma once

#include <string>

/**
 * @brief Utility function to convert FPSTR (Flash String Helper) to std::string
 * @param flashStr The flash string pointer from FPSTR()
 * @return std::string copy of the flash stringinline std::string operator+(const __FlashStringHelper* lhs, const std::string& rhs) {

 */
inline std::string fpstr_to_string(const __FlashStringHelper* flashStr) {
    if (!flashStr) return std::string();
    const char* str = reinterpret_cast<const char*>(flashStr);
    return std::string(str);
}

/**
 * @brief Operator overload for concatenating std::string and __FlashStringHelper
 * @param lhs The left-hand side std::string
 * @param rhs The right-hand side __FlashStringHelper pointer
 * @return Concatenated std::string
 */
inline std::string operator+(const std::string& lhs, const __FlashStringHelper* rhs) {
    return lhs + fpstr_to_string(rhs);
}

/**
 * @brief Operator overload for concatenating __FlashStringHelper and std::string
 * @param lhs The left-hand side __FlashStringHelper pointer
 * @param rhs The right-hand side std::string
 * @return Concatenated std::string
 */
inline std::string operator+(const __FlashStringHelper* lhs, const std::string& rhs) {
    return fpstr_to_string(lhs) + rhs;
}
