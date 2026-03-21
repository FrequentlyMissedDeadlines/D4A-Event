/**
 * @file dsps_fft2r_platform.h
 * @brief Shim for the ESP-DSP platform capability header required by PNGdec's
 *        s3_simd_rgb565.S on ESP32-S3.
 *
 * The real header lives in the IDF esp-dsp component, which is not on the
 * include path used for external PlatformIO library assembly files.  This shim
 * replicates the ESP32-S3 values so PNGdec's S3 SIMD path is compiled and
 * linked correctly.
 *
 * ESP32-S3 has AES3 vector instructions → dsps_fft2r_sc16_aes3_enabled = 1
 */
#pragma once

#define dsps_fft2r_fc32_aes3_enabled 1
#define dsps_fft2r_sc16_aes3_enabled 1
