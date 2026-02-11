# Code Factorization Analysis - Webserver Response Patterns

**Date**: February 11, 2026  
**Scope**: HTTP response handling and JSON serialization patterns across all services

---

## Executive Summary

Analysis of webserver response patterns reveals **significant code duplication** across multiple services, particularly in:
1. **JSON serialization and response sending** (90+ occurrences)
2. **Error response patterns** (repeated error JSON creation)
3. **Input validation and error handling** (repeated parameter checking)
4. **Response status code handling** (200, 422, 456, 503 patterns)

**Estimated code reduction**: 15-25% through factorization  
**Maintenance improvement**: High - centralized error handling  
**Priority**: HIGH - widely used across 8+ services

---

## Critical Factorization Opportunities

### 1. **JSON Response Serialization Pattern** 🔴 CRITICAL
**Severity**: HIGH  
**Occurrence**: 40+ instances across codebase

**Current Pattern** (repeated everywhere):
```cpp
// SettingsService.cpp
JsonDocument doc;
doc[FPSTR(SettingsConsts::json_success)] = true;
doc[RoutesConsts::message] = FPSTR(SettingsConsts::msg_success);

String output;
serializeJson(doc, output);
webserver.send(200, RoutesConsts::mime_json, output.c_str());
```

**Found in**:
- [src/services/settings/SettingsService.cpp](src/services/settings/SettingsService.cpp#L343)
- [src/services/log/RollingLoggerService.cpp](src/services/log/RollingLoggerService.cpp#L44-L56)
- [src/services/servo/ServoService.cpp](src/services/servo/ServoService.cpp) (multiple locations)
- [src/services/board/DFR1216Service.cpp](src/services/board/DFR1216Service.cpp#L308-L318)

**Factorization Opportunity**:
```cpp
// Proposed helper in HTTPService or new ResponseHelper utility
class ResponseHelper {
    /**
     * @brief Send JSON success response
     * @param statusCode HTTP status code (200, 201, etc.)
     * @param message Optional message field
     * @return void
     */
    static void sendJsonSuccess(int statusCode = 200, const char* message = nullptr) {
        JsonDocument doc;
        doc[RoutesConsts::result] = RoutesConsts::result_ok;
        if (message) {
            doc[RoutesConsts::message] = message;
        }
        
        String output;
        serializeJson(doc, output);
        webserver.send(statusCode, RoutesConsts::mime_json, output.c_str());
    }
    
    /**
     * @brief Send JSON response with custom data
     */
    static void sendJsonResponse(int statusCode, const JsonDocument& doc) {
        String output;
        serializeJson(doc, output);
        webserver.send(statusCode, RoutesConsts::mime_json, output.c_str());
    }
};
```

**Impact**: Reduces 40+ lines to simple function calls  
**Improvement**: Easier error handling, consistent formatting

---

### 2. **Error Response Pattern** 🔴 CRITICAL
**Severity**: HIGH  
**Occurrence**: 35+ instances

**Current Pattern** (repeated):
```cpp
// Multiple services
webserver.send(422, RoutesConsts::mime_json, 
              createJsonError(FPSTR(SettingsConsts::msg_missing_domain)));

webserver.send(503, RoutesConsts::mime_json, 
              createJsonError(FPSTR(SettingsConsts::msg_not_started)));

webserver.send(456, RoutesConsts::mime_json,
              create_error_json(FPSTR(RoutesConsts::resp_operation_failed)));
```

**Found in**:
- [src/services/settings/SettingsService.cpp](src/services/settings/SettingsService.cpp) (8+ calls)
- [src/services/board/DFR1216Service.cpp](src/services/board/DFR1216Service.cpp#L295-L325)
- [src/services/servo/ServoService.cpp](src/services/servo/ServoService.cpp) (10+ calls)
- [src/services/log/RollingLoggerService.cpp](src/services/log/RollingLoggerService.cpp)

**Issue**: 
- `createJsonError()` and `create_error_json()` are inconsistent (two implementations)
- Repeated error response pattern
- Status codes hardcoded (422, 456, 503)

**Factorization Opportunity**:
```cpp
// Centralized error response helper
class ResponseHelper {
    enum ErrorType {
        INVALID_PARAMS = 422,      // Client error: invalid input
        OPERATION_FAILED = 456,    // Server error: operation failed
        SERVICE_UNAVAILABLE = 503, // Service not ready
        NOT_FOUND = 404,
        UNAUTHORIZED = 401,
        BAD_REQUEST = 400
    };
    
    /**
     * @brief Send error response with standard format
     */
    static void sendError(ErrorType type, const char* message) {
        String output;
        createJsonError(message, output);
        webserver.send((int)type, RoutesConsts::mime_json, output.c_str());
    }
};

// Usage:
ResponseHelper::sendError(ResponseHelper::INVALID_PARAMS, 
                          FPSTR(SettingsConsts::msg_missing_domain));
```

**Impact**: Consolidates error handling, unifies error codes  
**Improvement**: Single point of error response control

---

### 3. **Parameter Validation Pattern** 🟠 MAJOR
**Severity**: HIGH  
**Occurrence**: 30+ instances

**Current Pattern** (repeated):
```cpp
// SettingsService
if (!webserver.hasArg(RoutesConsts::param_domain)) {
    webserver.send(422, RoutesConsts::mime_json, 
                  createJsonError(FPSTR(SettingsConsts::msg_missing_domain)));
    return;
}

std::string domain = webserver.arg(RoutesConsts::param_domain).c_str();

if (!isValidDomain(domain)) {
    webserver.send(422, RoutesConsts::mime_json, 
                  createJsonError(FPSTR(SettingsConsts::msg_invalid_domain)));
    return;
}
```

**Found in**:
- [src/services/settings/SettingsService.cpp](src/services/settings/SettingsService.cpp#L254-L276) (5+ occurrences)
- [src/services/servo/ServoService.cpp](src/services/servo/ServoService.cpp) (multiple checks)
- [src/services/board/DFR1216Service.cpp](src/services/board/DFR1216Service.cpp)

**Factorization Opportunity**:
```cpp
// Parameter validation helper
class ParamValidator {
    /**
     * @brief Check if parameter exists and is valid
     * @param param Parameter name
     * @param validator Optional custom validation function
     * @return Parameter value if valid, empty string and sends error if invalid
     */
    static std::string getValidatedParam(
        const char* paramName,
        std::function<bool(const std::string&)> validator = nullptr)
    {
        if (!webserver.hasArg(paramName)) {
            ResponseHelper::sendError(ResponseHelper::INVALID_PARAMS, 
                                     std::string("Missing parameter: ") + paramName);
            return "";
        }
        
        std::string value = webserver.arg(paramName).c_str();
        
        if (validator && !validator(value)) {
            ResponseHelper::sendError(ResponseHelper::INVALID_PARAMS, 
                                     std::string("Invalid ") + paramName);
            return "";
        }
        
        return value;
    }
};

// Usage:
std::string domain = ParamValidator::getValidatedParam(
    RoutesConsts::param_domain,
    [](const std::string& d) { return isValidDomain(d); }
);
if (domain.empty()) return;  // Error already sent
```

**Impact**: Reduces ~15 lines per route to ~2 lines  
**Improvement**: Consistent validation, centralized error messages

---

### 4. **Body Parsing and Validation Pattern** 🟠 MAJOR
**Severity**: MEDIUM  
**Occurrence**: 15+ instances

**Current Pattern** (repeated in POST handlers):
```cpp
// ServoService
String body = webserver.arg(FPSTR(ServoConsts::str_plain));

if (body.isEmpty()) {
    webserver.send(422, RoutesConsts::mime_json, 
                  getResultJsonString(RoutesConsts::result_err, 
                                     RoutesConsts::msg_invalid_params).c_str());
    return;
}

JsonDocument doc;
DeserializationError error = deserializeJson(doc, body.c_str());
if (error) {
    webserver.send(422, RoutesConsts::mime_json, 
                  getResultJsonString(RoutesConsts::result_err, 
                                     RoutesConsts::msg_invalid_params).c_str());
    return;
}
```

**Found in**:
- [src/services/servo/ServoService.cpp](src/services/servo/ServoService.cpp#L629-L650) (5+ routes)
- [src/services/settings/SettingsService.cpp](src/services/settings/SettingsService.cpp#L357-L374)

**Factorization Opportunity**:
```cpp
// JSON body parser helper
class JsonBodyParser {
    /**
     * @brief Parse and validate JSON request body
     * @param doc Reference to JsonDocument to populate
     * @param validator Optional validation function
     * @return true if body valid, false and sends error response if invalid
     */
    static bool parseBody(JsonDocument& doc, 
                         std::function<bool(const JsonDocument&)> validator = nullptr)
    {
        String body = webserver.arg("plain");
        
        if (body.isEmpty()) {
            ResponseHelper::sendError(ResponseHelper::INVALID_PARAMS, "Empty request body");
            return false;
        }
        
        DeserializationError error = deserializeJson(doc, body.c_str());
        if (error) {
            ResponseHelper::sendError(ResponseHelper::INVALID_PARAMS, 
                                     std::string("Invalid JSON: ") + error.c_str());
            return false;
        }
        
        if (validator && !validator(doc)) {
            ResponseHelper::sendError(ResponseHelper::INVALID_PARAMS, "Invalid payload schema");
            return false;
        }
        
        return true;
    }
};

// Usage:
JsonDocument doc;
if (!JsonBodyParser::parseBody(doc, [](const JsonDocument& d) { 
    return d[ServoConsts::servos].is<JsonArray>();
})) return;
```

**Impact**: Reduces 10-15 lines per POST handler to 1-2 lines  
**Improvement**: Consistent error messages, centralized validation

---

### 5. **Service Status Check Pattern** 🟠 MAJOR
**Severity**: MEDIUM  
**Occurrence**: 25+ instances

**Current Pattern** (repeated):
```cpp
// SettingsService
if (!g_settingsServiceInstance || !g_settingsServiceInstance->isServiceStarted()) {
    webserver.send(503, RoutesConsts::mime_json, 
                  createJsonError(FPSTR(SettingsConsts::msg_not_started)));
    return;
}

// ServoService
if (!checkServiceStarted()) return;
```

**Found in**:
- [src/services/settings/SettingsService.cpp](src/services/settings/SettingsService.cpp#L249-L252)
- [src/services/servo/ServoService.cpp](src/services/servo/ServoService.cpp) (many locations)
- [src/services/board/DFR1216Service.cpp](src/services/board/DFR1216Service.cpp)
- [src/services/log/RollingLoggerService.cpp](src/services/log/RollingLoggerService.cpp)

**Issue**: Two implementations - `checkServiceStarted()` and inline checks

**Factorization Opportunity**:
```cpp
// Service status helper
class ServiceStatus {
    /**
     * @brief Check if service is running, send error if not
     * @param service Pointer to service instance
     * @param serviceName Service name for error message
     * @return true if running, false and sends error if not
     */
    static bool ensureServiceRunning(IsServiceInterface* service, const char* serviceName) {
        if (!service || !service->isServiceStarted()) {
            std::string msg = std::string(serviceName) + " not initialized";
            ResponseHelper::sendError(ResponseHelper::SERVICE_UNAVAILABLE, msg.c_str());
            return false;
        }
        return true;
    }
};

// Usage:
if (!ServiceStatus::ensureServiceRunning(g_settingsServiceInstance, "Settings")) return;
```

**Impact**: Unifies service status checking across codebase  
**Improvement**: Consistent error messages, single implementation

---

### 6. **Success Response Pattern** 🟡 MINOR
**Severity**: MEDIUM  
**Occurrence**: 20+ instances

**Current Pattern** (repeated):
```cpp
// ServoService
webserver.send(200, RoutesConsts::mime_json, 
              getResultJsonString(RoutesConsts::result_ok, ServoConsts::action_stop_all).c_str());

// Different format in other services
JsonDocument doc;
doc[RoutesConsts::result] = RoutesConsts::result_ok;
String output;
serializeJson(doc, output);
webserver.send(200, RoutesConsts::mime_json, output.c_str());
```

**Issue**: Multiple success response formats across codebase

**Factorization Opportunity**:
```cpp
// Success response helper
class ResponseHelper {
    static void sendSuccess(const char* action = nullptr) {
        JsonDocument doc;
        doc[RoutesConsts::result] = RoutesConsts::result_ok;
        if (action) {
            doc[RoutesConsts::action] = action;
        }
        sendJsonResponse(200, doc);
    }
    
    static void sendSuccess(int statusCode, const JsonDocument& dataDoc) {
        sendJsonResponse(statusCode, dataDoc);
    }
};
```

**Impact**: Consistent success response format  
**Improvement**: Single point of success response control

---

## Additional Factorization Opportunities

### 7. **MIME Type Constants** 🟡 MINOR
**Severity**: LOW  
**Occurrence**: Scattered across codebase

**Current Pattern**:
```cpp
// RoutesConsts::mime_json
// RoutesConsts::mime_plain_text
// "application/json" (hardcoded in some places)
// "text/html; charset=utf-8" (hardcoded)
```

**Recommendation**: Consolidate all MIME types in `RoutesConsts` namespace

---

### 8. **OpenAPI Response Metadata** 🟡 MINOR
**Severity**: LOW  
**Occurrence**: Repeated in multiple services

**Pattern**:
```cpp
std::vector<OpenAPIResponse> responses;
OpenAPIResponse successResponse(200, "Operation successful");
responses.push_back(successResponse);

OpenAPIResponse invalidResponse(422, "Invalid parameters");
responses.push_back(invalidResponse);
```

**Recommendation**: Create helper factory for common response sets

---

## Current Helper Functions Analysis

### Existing Fragmentation

| Function | Location | Purpose | Issues |
|----------|----------|---------|--------|
| `createJsonError()` | SettingsService | Create error JSON | Duplicated, inconsistent |
| `create_error_json()` | DFR1216Service | Create error JSON | Duplicated, different signature |
| `getResultJsonString()` | ServoService | Create result JSON | Different format than others |
| `checkServiceStarted()` | ServoService | Check service status | Not in SettingsService |
| `sendJsonSuccess()` | Various | Send success | Inconsistently named/implemented |

**Problem**: 5+ different error handling implementations across codebase

---

## Memory & Performance Impact

### Current State
- Repeated JsonDocument allocations: 40+ instances across handlers
- Repeated String serializations: 40+ instances
- No helper function call overhead
- Stack usage per handler: ~500-800 bytes (includes JsonDocument)

### After Factorization
- Centralized helpers reduce duplicate code
- Consistent error handling reduces memory overhead
- Potential for reusing JsonDocument (minor)
- Helper function call adds minimal overhead (~10-20 bytes)

**Net Benefit**: -3-5% code size, +0.2% runtime overhead (negligible)

---

## Proposed Implementation Strategy

### Phase 1: Create Response Helpers (HIGH PRIORITY)
1. Create `src/services/ResponseHelper.h/cpp`
2. Implement core helper classes:
   - `ResponseHelper` (JSON serialization)
   - `ErrorHandler` (error responses)
   - `JsonBodyParser` (body parsing)
3. Update all services to use helpers

### Phase 2: Consolidate Error Handling (HIGH PRIORITY)
1. Remove duplicate error functions
2. Update all services to use centralized error responses
3. Unify error codes and messages

### Phase 3: Consolidate Parameter Validation (MEDIUM PRIORITY)
1. Implement `ParamValidator` helper
2. Update services to use centralized validation
3. Add request body validation helper

### Phase 4: Service Status Checking (LOW PRIORITY)
1. Consolidate service status checks
2. Unify status error messages

---

## Files Most Affected

| File | Issue Count | Priority |
|------|-------------|----------|
| [src/services/servo/ServoService.cpp](src/services/servo/ServoService.cpp) | 15+ | HIGH |
| [src/services/settings/SettingsService.cpp](src/services/settings/SettingsService.cpp) | 12+ | HIGH |
| [src/services/log/RollingLoggerService.cpp](src/services/log/RollingLoggerService.cpp) | 8+ | MEDIUM |
| [src/services/board/DFR1216Service.cpp](src/services/board/DFR1216Service.cpp) | 6+ | MEDIUM |
| [src/services/sensor/K10sensorsService.cpp](src/services/sensor/K10sensorsService.cpp) | 4+ | MEDIUM |
| [src/services/http/HTTPService.cpp](src/services/http/HTTPService.cpp) | 3+ | LOW |

---

## Code Reuse Metrics

### Current Duplication
```
Total webserver.send() calls: 90+
- JSON responses: 40+
- Error responses: 35+
- Success responses: 15+

JsonDocument allocations: 45+
serializeJson() calls: 45+
DeserializationError checks: 15+
```

### After Refactoring
```
Helper function calls: 90+
- ResponseHelper::sendJsonResponse: ~40
- ResponseHelper::sendError: ~35
- ResponseHelper::sendSuccess: ~15

JsonDocument allocations: 45+ (same)
serializeJson() calls: ~10 (consolidated in helpers)
```

**Reduction**: ~35 instances of boilerplate code eliminated

---

## Recommendations Priority

### 🔴 HIGH (Implement First)
1. ✅ Create ResponseHelper class with `sendJsonResponse()`, `sendError()`, `sendSuccess()`
2. ✅ Create JsonBodyParser class with `parseBody()`
3. ✅ Consolidate error handling (remove duplicate error functions)

### 🟠 MEDIUM (Implement Next)
4. Create ParamValidator with `getValidatedParam()`
5. Create ServiceStatus with `ensureServiceRunning()`

### 🟡 LOW (Nice to Have)
6. Consolidate MIME type constants
7. Create OpenAPI response factory

---

## Risk Assessment

### Low Risk Items ✅
- ResponseHelper implementations (new utility, no breaking changes)
- JsonBodyParser (new utility, opt-in usage)

### Medium Risk Items ⚠️
- Error consolidation (requires updates across 6+ files)
- Parameter validation helpers (requires changes to validation flow)

### Implementation Precautions
1. Create helpers first, add deprecation wrappers for old functions
2. Update one service at a time to verify compatibility
3. Test all error paths after consolidation
4. Monitor for any behavioral changes in error handling

---

## Estimated Effort

| Task | Effort | Impact |
|------|--------|--------|
| ResponseHelper implementation | 2-3 hours | 40+ line reduction |
| JsonBodyParser implementation | 1-2 hours | 20+ line reduction |
| Error consolidation | 3-4 hours | 15+ function removals |
| ParamValidator implementation | 2-3 hours | 30+ line reduction |
| Update all services | 4-6 hours | Full adoption |
| **Total** | **12-18 hours** | **~25% code reduction** |

---

## Summary

**Webserver response handling has significant factorization opportunities**, particularly in:

1. **JSON serialization** - 40+ duplications → Could be 1-2 helper functions
2. **Error responses** - 35+ duplications → Could be 1-2 centralized functions
3. **Parameter validation** - 30+ duplications → Could be 1 validation helper
4. **Service status checks** - 25+ duplications → Could be 1 status helper
5. **Body parsing** - 15+ duplications → Could be 1 parser helper

**Total estimated code reduction**: 15-25% in response handling code  
**Maintenance improvement**: Centralized error control, consistent API responses  
**Implementation effort**: 12-18 hours for full refactoring

**Recommendation**: Implement ResponseHelper and JsonBodyParser as priority, then consolidate error handling across all services.
