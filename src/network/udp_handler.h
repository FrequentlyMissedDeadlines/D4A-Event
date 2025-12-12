// UDP handler module - non-blocking packet callback and message buffer
#pragma once

#include <AsyncUDP.h>

bool UDPHandler_begin(AsyncUDP* udp, int port);
// Build a JSON message containing all infos.
String UDPHandler_buildJson();
// Try to copy the latest message for display; returns true if copied
bool UDPHandler_tryCopyDisplay(char* outBuf, int bufLen, int& outTotalMessages);
// Get the number of dropped packets (thread-safe)
unsigned long UDPHandler_getDroppedPackets();
// Get the number of handled packets (thread-safe)
unsigned long UDPHandler_getHandledPackets();
