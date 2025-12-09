// Web server module - wraps WebServer and exposes begin/handleClient
#pragma once

#include <WebServer.h>

void WebServerModule_begin(WebServer* server);
void WebServerModule_handleClient(WebServer* server);
