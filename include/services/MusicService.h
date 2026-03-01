#pragma once
/**
 * @file MusicService.h
 * @brief OpenAPI service for music playback on K10
 * @details Exposed routes:
 *          - POST /api/music/v1/play - Play built-in melody with options
 *          - POST /api/music/v1/tone - Play a tone at specified frequency and duration
 *          - POST /api/music/v1/stop - Stop current tone playback
 */
#include "IsServiceInterface.h"
#include "IsOpenAPIInterface.h"
#include "isUDPMessageHandlerInterface.h"

class MusicService : public IsOpenAPIInterface, public IsUDPMessageHandlerInterface
{
public:
    bool registerRoutes() override;
    std::string getServiceSubPath() override;
    bool initializeService() override;
    bool startService() override;
    bool stopService() override;
    std::string getServiceName() override;

    bool messageHandler(const std::string &message,
                        const IPAddress &remoteIP,
                        uint16_t remotePort) override;

    IsUDPMessageHandlerInterface *asUDPMessageHandlerInterface() override { return this; }

private:
    bool handleUDP_play(const JsonDocument &doc, std::string &response);
    bool handleUDP_tone(const JsonDocument &doc, std::string &response);
    bool handleUDP_stop(const JsonDocument &doc, std::string &response);
    bool handleUDP_getMelodies(const JsonDocument &doc, std::string &response);

    /**
     * @brief Handle UDP playnotes command with raw hex payload.
     *        Format: tempo_byte + N note_bytes, hex-encoded.
     *        Note byte: bit7=silence, bits6-0=MIDI note (0-127).
     * @param hex_data  Pointer to hex-encoded byte string
     * @param hex_len   Length of hex string (must be even, >= 4)
     * @param response  Output JSON response
     * @return true on success
     */
    bool handleUDP_playnotes(const char *hex_data, size_t hex_len, std::string &response);
};
