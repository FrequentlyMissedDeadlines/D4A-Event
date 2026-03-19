/**
 * @file FlashStringHelper.h
 * @brief Utility functions for working with Flash String Helpers (__FlashStringHelper).
 * Provides conversion and operator overloads for convenient string concatenation.
 */
#pragma once

#include <Arduino.h>
#include <string>

/**
 * @brief Convert a PROGMEM string pointer to std::string.
 * @param flashStr The flash string pointer from FPSTR()
 * @return std::string copy of the PROGMEM string (allocated on heap)
 */
inline std::string fpstr_to_string(const __FlashStringHelper *flashStr)
{
    if (!flashStr) return std::string();
    return std::string(reinterpret_cast<const char *>(flashStr));
}

/**
 * @brief Convert a PROGMEM constant directly to std::string.
 *
 * Usage:
 * @code
 *   progmem_to_string(MyConsts::my_string)
 *   // instead of: fpstr_to_string(FPSTR(MyConsts::my_string))
 * @endcode
 *
 * @tparam T Type of the PROGMEM constant (typically constexpr const char[])
 * @param progmemConst The PROGMEM constant (declared with PROGMEM attribute)
 * @return std::string copy of the PROGMEM string
 */
template <typename T>
inline std::string progmem_to_string(const T &progmemConst)
{
    return fpstr_to_string(FPSTR(progmemConst));
}

/**
 * @brief Concatenate std::string + __FlashStringHelper.
 */
inline std::string operator+(const std::string &lhs, const __FlashStringHelper *rhs)
{
    return lhs + fpstr_to_string(rhs);
}

/**
 * @brief Concatenate __FlashStringHelper + std::string.
 */
inline std::string operator+(const __FlashStringHelper *lhs, const std::string &rhs)
{
    return fpstr_to_string(lhs) + rhs;
}
