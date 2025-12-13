// Web server module - wraps WebServer and exposes begin/handleClient
#pragma once

#include <WebServer.h>

class WebServerModule
{
public:
    void begin(WebServer *server);
    void handleClient(WebServer *server);
    // Master registration conflict handling - called by display task
    void handleMasterConflict(void);
    // Button handlers for master conflict resolution
    void acceptMasterConflict(void); // Call when button A pressed
    void denyMasterConflict(void);   // Call when button B pressed
};