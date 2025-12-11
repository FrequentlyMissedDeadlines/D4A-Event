// Web server module - wraps WebServer and exposes begin/handleClient
#pragma once

#include <WebServer.h>

void WebServerModule_begin(WebServer* server);
void WebServerModule_handleClient(WebServer* server);

// Camera webcam registration - call after WebServerModule_begin
void WebServerModule_registerWebcam(WebServer* server);

// Master registration conflict handling - called by display task
void WebServerModule_handleMasterConflict(void);

// Button handlers for master conflict resolution
void WebServerModule_acceptMasterConflict(void);   // Call when button A pressed
void WebServerModule_denyMasterConflict(void);     // Call when button B pressed

