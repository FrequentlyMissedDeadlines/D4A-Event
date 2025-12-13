// UDP handler module - non-blocking packet callback and message buffer
#pragma once

#include <AsyncUDP.h>
#include <WebServer.h>
class UDPModule
{
public:
    bool begin(AsyncUDP *udp, int port);
    // Try to copy the latest message for display; returns true if copied
    bool tryCopyDisplay(char *outBuf, int bufLen, int &outTotalMessages);
    bool registerRoutes(WebServer *server);
private:
    // Build a JSON message containing all infos.
    String buildJson();

    // Get the number of dropped packets (thread-safe)
    unsigned long getDroppedPackets();
    // Get the number of handled packets (thread-safe)
    unsigned long getHandledPackets();
};