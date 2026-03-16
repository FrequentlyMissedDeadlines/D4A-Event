/**
 * @file RollingLoggerMiddleware.h
 * @brief AsyncWebServer middleware that logs HTTP requests to a RollingLogger
 * @details Lightweight alternative to AsyncLoggingMiddleware that avoids the
 *          Print-stream overhead. Logs method + URI in a single call to
 *          RollingLogger, skipping noisy paths (static assets, /ping, etc.).
 */
#pragma once

#include <ESPAsyncWebServer.h>
#include <functional>
#include "RollingLogger.h"

namespace RollingLoggerMiddlewareConsts
{
  constexpr const char str_name[] PROGMEM = "RollingLoggerMiddleware";
  constexpr const char str_get[] PROGMEM = "GET";
  constexpr const char str_post[] PROGMEM = "POST";
  constexpr const char str_put[] PROGMEM = "PUT";
  constexpr const char str_delete[] PROGMEM = "DELETE";
  constexpr const char str_patch[] PROGMEM = "PATCH";
  constexpr const char str_head[] PROGMEM = "HEAD";
  constexpr const char str_options[] PROGMEM = "OPTIONS";
  constexpr const char str_unknown[] PROGMEM = "???";
}

/**
 * @class RollingLoggerMiddleware
 * @brief Logs every incoming HTTP request (method + path) to a RollingLogger.
 *
 * Designed for low-overhead use on ESP32:
 *  - One virtual dispatch per request (run())
 *  - One snprintf into a stack buffer
 *  - One logger->info() call
 *  - No heap allocation, no Print byte-pumping
 *
 * Noisy paths (static assets, /ping, /favicon.ico) are silently skipped.
 */
class RollingLoggerMiddleware : public AsyncMiddleware
{
public:
  /**
   * @brief Construct the middleware
   * @param log Pointer to the RollingLogger instance (must outlive this object)
   */
  explicit RollingLoggerMiddleware(RollingLogger *log) : logger_(log) {}

  /**
   * @brief Enable or disable logging at runtime
   * @param enabled true to log requests, false to pass through silently
   */
  void setEnabled(bool enabled) { enabled_ = enabled; }

  /**
   * @brief Register an on-request callback (e.g. to update a watchdog timer)
   * @param cb Callback invoked for every request before logging
   */
  void onRequest(std::function<void()> cb) { on_request_cb_ = cb; }

  /**
   * @brief Check whether logging is active
   * @return true if enabled and a logger is attached
   */
  bool isEnabled() const { return enabled_ && logger_ != nullptr; }

  /**
   * @brief Middleware entry point called by the AsyncWebServer chain
   * @param request The incoming HTTP request
   * @param next    Callback to continue to the next middleware / handler
   */
  void run(AsyncWebServerRequest *request, ArMiddlewareNext next) override
  {
    if (request && on_request_cb_)
      on_request_cb_();

    if (enabled_ && logger_ && request)
    {
      const String &path = request->url();
      if (shouldLog(path))
      {
        const char *method = methodToString(request->method());
        // Stack buffer — no heap allocation
        char buf[128];
        snprintf(buf, sizeof(buf), "%s %s", method, path.c_str());
        logger_->info(std::string(buf));
      }
    }
    next();
  }

private:
  RollingLogger *logger_ = nullptr;
  bool enabled_ = true;
  std::function<void()> on_request_cb_;

  /**
   * @brief Decide whether a path is worth logging
   * @param path The request URI
   * @return false for static assets, /ping, /favicon.ico; true otherwise
   */
  static bool shouldLog(const String &path)
  {
    if (path.length() == 0)
      return false;
    if (path == "/ping" || path == "/favicon.ico")
      return false;

    // Skip common static-file extensions
    static constexpr const char *skip_ext[] = {
        ".html", ".css", ".js", ".map", ".json", ".png", ".jpg",
        ".jpeg", ".svg", ".ico", ".woff", ".woff2", ".ttf", ".otf", ".wasm"};
    for (const char *ext : skip_ext)
    {
      if (path.endsWith(ext))
        return false;
    }
    return true;
  }

  /**
   * @brief Convert HTTP method enum to a PROGMEM C-string
   * @param method The WebRequestMethodComposite value
   * @return Pointer to a PROGMEM string literal
   */
  static const char *methodToString(WebRequestMethodComposite method)
  {
    switch (method)
    {
    case HTTP_GET:     return RollingLoggerMiddlewareConsts::str_get;
    case HTTP_POST:    return RollingLoggerMiddlewareConsts::str_post;
    case HTTP_PUT:     return RollingLoggerMiddlewareConsts::str_put;
    case HTTP_DELETE:  return RollingLoggerMiddlewareConsts::str_delete;
    case HTTP_PATCH:   return RollingLoggerMiddlewareConsts::str_patch;
    case HTTP_HEAD:    return RollingLoggerMiddlewareConsts::str_head;
    case HTTP_OPTIONS: return RollingLoggerMiddlewareConsts::str_options;
    default:           return RollingLoggerMiddlewareConsts::str_unknown;
    }
  }
};
