/**
 * @file MusicService.cpp
 * @brief Implementation for music playback service
 * @details Exposed routes:
 *          - POST /api/music/v1/play - Play built-in melody with options
 *          - POST /api/music/v1/tone - Play a tone at specified frequency and duration
 *          - POST /api/music/v1/stop - Stop current tone playback
 */

#include "services/MusicService.h"
#include <ESPAsyncWebServer.h>
#include <unihiker_k10.h>
#include <ArduinoJson.h>
#include "services/UDPService.h"

extern Music music;
extern UDPService udp_service;

// MusicService constants namespace
namespace MusicConsts
{
    constexpr const char desc_beat_param[] PROGMEM = "Beat duration (1 beat = 8000)";
    constexpr const char desc_freq_param[] PROGMEM = "Frequency in Hz";
    constexpr const char desc_melody_param[] PROGMEM = "Melody enum value (0-19)";
    constexpr const char desc_option_param[] PROGMEM = "Playback option (1=Once, 2=Forever, 4=OnceInBackground, 8=ForeverInBackground)";
    constexpr const char json_error_freq_required[] PROGMEM = "{\"error\":\"freq parameter required\"}";
    constexpr const char json_error_melody_required[] PROGMEM = "{\"error\":\"melody parameter required\"}";
    constexpr const char json_melodies_list[] PROGMEM = "[\"DADADADUM\",\"ENTERTAINER\",\"PRELUDE\",\"ODE\",\"NYAN\",\"RINGTONE\",\"FUNK\",\"BLUES\",\"BIRTHDAY\",\"WEDDING\",\"FUNERAL\",\"PUNCHLINE\",\"BADDY\",\"CHASE\",\"BA_DING\",\"WAWAWAWAA\",\"JUMP_UP\",\"JUMP_DOWN\",\"POWER_UP\",\"POWER_DOWN\"]";
    constexpr const char json_status_ok[] PROGMEM = "{\"status\":\"ok\"}";
    constexpr const char msg_initialize_done[] PROGMEM = " initialize done";
    constexpr const char msg_start_done[] PROGMEM = " start done";
    constexpr const char msg_stop_done[] PROGMEM = " stop done";
    constexpr const char action_get_melodies[] PROGMEM = "melodies";
    constexpr const char action_play[] PROGMEM = "play";
    constexpr const char action_stop[] PROGMEM = "stop";
    constexpr const char action_tone[] PROGMEM = "tone";
    constexpr const char desc_get_melodies[] PROGMEM = "Get list of available built-in melodies";
    constexpr const char desc_play_melody[] PROGMEM = "Play built-in melody with playback options. Query parameters: melody (required, 0-19), option (optional, 1=Once, 2=Forever, 4=OnceInBackground, 8=ForeverInBackground)";
    constexpr const char desc_play_tone[] PROGMEM = "Play a tone at specified frequency and duration. Query parameters: freq (required, Hz), beat (optional, default 8000)";
    constexpr const char desc_stop_tone[] PROGMEM = "Stop current tone playback";
    constexpr const char param_beat[] PROGMEM = "beat";
    constexpr const char param_freq[] PROGMEM = "freq";
    constexpr const char param_melody[] PROGMEM = "melody";
    constexpr const char param_option[] PROGMEM = "option";
    constexpr const char service_name[] PROGMEM = "Music";
    constexpr const char service_path[] PROGMEM = "music/v1";
    constexpr const char tag[] PROGMEM = "Music";

    // UDP message handling
    constexpr const char msg_udp_bad_format[]  PROGMEM = "UDP bad message format";
    constexpr const char msg_udp_unknown_cmd[] PROGMEM = "UDP unknown command";
    constexpr const char msg_udp_json_error[]  PROGMEM = "UDP JSON parse error";
    constexpr const char msg_failed_action[]   PROGMEM = "Music action failed";
    constexpr const char action_play_notes[]   PROGMEM = "playnotes";
    constexpr const char desc_play_notes[]     PROGMEM = "Play a sequence of MIDI notes (hex-encoded binary). Format: first byte = tempo in BPM, then one byte per note: bit7=silence, bits6-0=MIDI note 0-127. All notes play for exactly 1 beat at the given tempo.";
    constexpr const char msg_udp_invalid_notes[] PROGMEM = "UDP invalid notes payload";
    constexpr const char udp_field_result[]    PROGMEM = "result";
    constexpr const char udp_field_message[]   PROGMEM = "message";
    constexpr const char udp_val_ok[]          PROGMEM = "ok";
    constexpr const char udp_val_error[]       PROGMEM = "error";
    constexpr uint16_t midi_frequencies[128] PROGMEM = {
        //octave -1
        16,17,18,19,20,21,23,24,25,27,29,30,
        //octave -0
        32,34,36,38,41,43,46,48,51,55,58,61,
        //octave 1
        65,69,73,77,82,87,92,97,103,110,116,123,
        //octave 2
        130,138,146,155,164,174,184,196,207,220,233,246,
        //octave 3
        261,277,293,311,329,349,369,392,415,440,466,496,
        //octave 4
        523,554,587,622,659,698,739,783,830,880,932,987,
        // octave 5
        1046,1108,1174,1244,1318,1396,1479,1568,1661,1760,1864,1975,
        // octave 6
        2093,2217,2349,2489,2637,2793,2959,3136,3322,3520,3729,3951,
        // octave 7
        4186,4434,4698,4978,5374,5587,5919,6272,6644,7040,7458,7902,
        //octave 8
        8372,8869,9397,9956,10748,11175,11839,12544,13289,14080,14917,15804
        };
}

bool MusicService::initializeService()
{
    setServiceStatus(INITIALIZED);
    
#ifdef VERBOSE_DEBUG
    logger->debug(getServiceName() + progmem_to_string(MusicConsts::msg_initialize_done));
#endif
    return true;
}

bool MusicService::startService()
{
    setServiceStatus(STARTED);
#ifdef VERBOSE_DEBUG
    logger->debug(getServiceName() + progmem_to_string(MusicConsts::msg_start_done));
#endif
    return true;
}

bool MusicService::stopService()
{
   setServiceStatus(STOPPED);
#ifdef VERBOSE_DEBUG
    logger->debug(getServiceName() + progmem_to_string(MusicConsts::msg_stop_done));
#endif
    return true;
}

bool MusicService::registerRoutes()
{
    static constexpr char play_schema[] PROGMEM = "{\"type\":\"object\",\"properties\":{\"melody\":{\"type\":\"integer\",\"description\":\"Melody enum value (0-19)\"},\"option\":{\"type\":\"integer\",\"description\":\"Playback option (1=Once, 2=Forever, 4=OnceInBackground, 8=ForeverInBackground)\"}},\"required\":[\"melody\"]}";
    static constexpr char tone_schema[] PROGMEM = "{\"type\":\"object\",\"properties\":{\"freq\":{\"type\":\"integer\",\"description\":\"Frequency in Hz\"},\"beat\":{\"type\":\"integer\",\"description\":\"Beat duration (1 beat = 8000)\"}},\"required\":[\"freq\"]}";
    static constexpr char play_example[] PROGMEM = "{\"melody\":0,\"option\":4}";
    static constexpr char tone_example[] PROGMEM = "{\"freq\":440,\"beat\":8000}";
    static constexpr char response_desc[] PROGMEM = "Operation completed successfully";
    
    // Register play melody route
    std::string path = getPath(MusicConsts::action_play);
    logRouteRegistration(path);
    
    std::vector<OpenAPIResponse> responses;
    OpenAPIResponse successResponse = createSuccessResponse(response_desc);
    responses.push_back(successResponse);
    responses.push_back(createServiceNotStartedResponse());
    
    // Play melody route parameters
    std::vector<OpenAPIParameter> playParams;
    playParams.push_back(OpenAPIParameter("melody", "string", "query", "Melody enum value (0-19)", true));
    playParams.push_back(OpenAPIParameter("option", "string", "query", "Playback option (1=Once, 2=Forever, 4=OnceInBackground, 8=ForeverInBackground)", false));
    
    OpenAPIRoute routePlay(path.c_str(), RoutesConsts::method_post, MusicConsts::desc_play_melody, MusicConsts::tag, false, playParams, responses);
    registerOpenAPIRoute(routePlay);
    
    webserver.on(path.c_str(), HTTP_POST, [this](AsyncWebServerRequest *request) {
        if (!checkServiceStarted(request)) return;
        
        String melodyStr = request->arg(MusicConsts::param_melody);
        String optionStr = request->arg(MusicConsts::param_option);
        
        if (melodyStr.isEmpty()) {
            request->send(400, RoutesConsts::mime_json, FPSTR(MusicConsts::json_error_melody_required));
            return;
        }
        
        int melody = melodyStr.toInt();
        int option = optionStr.isEmpty() ? OnceInBackground : optionStr.toInt();
        
        music.playMusic(static_cast<Melodies>(melody), static_cast<MelodyOptions>(option));
        
        request->send(200, RoutesConsts::mime_json, FPSTR(MusicConsts::json_status_ok));
    });
    
    // Register play tone route
    path = getPath(MusicConsts::action_tone);
    logRouteRegistration(path);
    
    // Play tone route parameters
    std::vector<OpenAPIParameter> toneParams;
    toneParams.push_back(OpenAPIParameter("freq", "string", "query", "Frequency in Hz", true));
    toneParams.push_back(OpenAPIParameter("beat", "string", "query", "Beat duration (default 8000)", false));
    
    OpenAPIRoute routeTone(path.c_str(), RoutesConsts::method_post, MusicConsts::desc_play_tone, MusicConsts::tag, false, toneParams, responses);
    registerOpenAPIRoute(routeTone);
    
    webserver.on(path.c_str(), HTTP_POST, [this](AsyncWebServerRequest *request) {
        if (!checkServiceStarted(request)) return;
        
        String freqStr = request->arg(MusicConsts::param_freq);
        String beatStr = request->arg(MusicConsts::param_beat);
        
        if (freqStr.isEmpty()) {
            request->send(400, RoutesConsts::mime_json, FPSTR(MusicConsts::json_error_freq_required));
            return;
        }
        
        int freq = freqStr.isEmpty()? 440 : freqStr.toInt();
        int beat = beatStr.isEmpty() ? 8000 : beatStr.toInt();
        
        music.playTone(freq, beat);
        
        request->send(200, RoutesConsts::mime_json, FPSTR(MusicConsts::json_status_ok));
    });
    
    // Register stop tone route
     path = getPath(MusicConsts::action_stop);
    logRouteRegistration(path);
    
    OpenAPIRoute routeStop(path.c_str(), RoutesConsts::method_post, MusicConsts::desc_stop_tone, MusicConsts::tag, false, {}, responses);
    registerOpenAPIRoute(routeStop);
    
    webserver.on(path.c_str(), HTTP_POST, [this](AsyncWebServerRequest *request) {
        if (!checkServiceStarted(request)) return;
        
        music.stopPlayTone();
        request->send(200, RoutesConsts::mime_json, FPSTR(MusicConsts::json_status_ok));
    });

    // Register status route
registerServiceStatusRoute( this);
  registerSettingsRoutes( this);

 path = getPath(MusicConsts::action_get_melodies);
#ifdef VERBOSE_DEBUG
    logger->debug(progmem_to_string(RoutesConsts::msg_registering) + path);
    #endif
    logRouteRegistration(path);
    OpenAPIRoute routeMelodies(path.c_str(), RoutesConsts::method_get, MusicConsts::desc_get_melodies, MusicConsts::tag, false, {}, responses);
    registerOpenAPIRoute(routeMelodies  );
    
    webserver.on(path.c_str(), HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!checkServiceStarted(request)) return;
        
        request->send(200, RoutesConsts::mime_json, FPSTR(MusicConsts::json_melodies_list));
    });

    


    return true;
}


std::string MusicService::getServiceSubPath()
{
    return MusicConsts::service_path;
}

std::string MusicService::getServiceName()
{
    return MusicConsts::service_name;
}

// ─── UDP helpers ──────────────────────────────────────────────────────────────

// Module-level static JSON documents — zero heap allocation on hot UDP path
static JsonDocument s_music_udp_req;   ///< Reused for incoming UDP JSON payload
static JsonDocument s_music_udp_resp;  ///< Reused for outgoing UDP JSON response

/**
 * @brief Serialize a success response into @p out.
 * @param action  Command name echoed back
 * @param out     Output string populated in-place
 */
static void music_udp_buildSuccess(const char *action, std::string &out)
{
    s_music_udp_resp.clear();
    s_music_udp_resp[MusicConsts::udp_field_result]  = MusicConsts::udp_val_ok;
    s_music_udp_resp[MusicConsts::udp_field_message] = action;
    String buf;
    serializeJson(s_music_udp_resp, buf);
    out = buf.c_str();
}

/**
 * @brief Serialize an error response into @p out.
 * @param reason  Human-readable error string
 * @param out     Output string populated in-place
 */
static void music_udp_buildError(const char *reason, std::string &out)
{
    s_music_udp_resp.clear();
    s_music_udp_resp[MusicConsts::udp_field_result]  = MusicConsts::udp_val_error;
    s_music_udp_resp[MusicConsts::udp_field_message] = reason;
    String buf;
    serializeJson(s_music_udp_resp, buf);
    out = buf.c_str();
}

// ─── Per-command UDP handlers ─────────────────────────────────────────────────

bool MusicService::handleUDP_play(const JsonDocument &doc, std::string &response)
{
    if (!doc[MusicConsts::param_melody].is<int>())
    {
        music_udp_buildError(reinterpret_cast<const char *>(FPSTR(RoutesConsts::msg_invalid_params)), response);
        return false;
    }
    int melody = doc[MusicConsts::param_melody].as<int>();
    if (melody < 0 || melody > 19)
    {
        music_udp_buildError(reinterpret_cast<const char *>(FPSTR(RoutesConsts::msg_invalid_values)), response);
        return false;
    }
    int option = doc[MusicConsts::param_option].is<int>()
                     ? doc[MusicConsts::param_option].as<int>()
                     : static_cast<int>(OnceInBackground);
    music.playMusic(static_cast<Melodies>(melody), static_cast<MelodyOptions>(option));
    music_udp_buildSuccess(MusicConsts::action_play, response);
    return true;
}

bool MusicService::handleUDP_tone(const JsonDocument &doc, std::string &response)
{
    if (!doc[MusicConsts::param_freq].is<int>())
    {
        music_udp_buildError(reinterpret_cast<const char *>(FPSTR(RoutesConsts::msg_invalid_params)), response);
        return false;
    }
    int freq = doc[MusicConsts::param_freq].as<int>();
    int beat = doc[MusicConsts::param_beat].is<int>() ? doc[MusicConsts::param_beat].as<int>() : 8000;
    if (freq <= 0)
    {
        music_udp_buildError(reinterpret_cast<const char *>(FPSTR(RoutesConsts::msg_invalid_values)), response);
        return false;
    }
    music.playTone(freq, beat);
    music_udp_buildSuccess(MusicConsts::action_tone, response);
    return true;
}

bool MusicService::handleUDP_stop(const JsonDocument &, std::string &response)
{
    music.stopPlayTone();
    music_udp_buildSuccess(MusicConsts::action_stop, response);
    return true;
}

bool MusicService::handleUDP_getMelodies(const JsonDocument &, std::string &response)
{
    response = progmem_to_string(MusicConsts::json_melodies_list);
    return true;
}

bool MusicService::handleUDP_playnotes(const char *hex_data, size_t hex_len, std::string &response)
{
    // Payload layout (hex-encoded):
    //   Byte 0        : tempo in BPM (0 → 120 BPM)
    //   Bytes 1..N    : pairs of 2 bytes per note
    //     note byte   : bit7=silence, bits6-0=MIDI note (0-127)
    //     duration byte: number of 16th notes (1 = one 16th, 4 = quarter note, …)
    //                    0 treated as 1
    // Total hex length must be even, and after the tempo byte the remaining
    // bytes must come in pairs → (hex_len/2 - 1) must be even.
    const size_t total_bytes = hex_len / 2;
    if (hex_len < 6 || (hex_len % 2) != 0 || ((total_bytes - 1) % 2) != 0)
    {
        music_udp_buildError(MusicConsts::msg_udp_invalid_notes, response);
        return false;
    }

    // Decode a single hex nibble; returns -1 on invalid character
    auto hexNibble = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };

    // Decode one byte from hex_data at hex offset hex_pos (byte index * 2)
    auto hexByte = [&](size_t hex_pos) -> int {
        if (hex_pos + 1 >= hex_len) return -1;
        int hi = hexNibble(hex_data[hex_pos]);
        int lo = hexNibble(hex_data[hex_pos + 1]);
        if (hi < 0 || lo < 0) return -1;
        return (hi << 4) | lo;
    };

    // ── Byte 0: tempo in BPM (1-240; 0 → default 120 BPM) ─────────────────
    int tempo_raw = hexByte(0);
    if (tempo_raw < 0)
    {
        music_udp_buildError(MusicConsts::msg_udp_invalid_notes, response);
        return false;
    }
    // Clamp: 0 → 120 (default), 1-240 as-is, >240 → 240
    const uint16_t tempo_bpm = (tempo_raw == 0)  ? 120u
                             : (tempo_raw > 240) ? 240u
                             : static_cast<uint16_t>(tempo_raw);

    // Duration of one 16th note in milliseconds:
    //   1 quarter note at BPM = 60000 / BPM ms
    //   1 sixteenth note = quarter / 4 = 15000 / BPM ms
    const uint32_t sixteenth_ms = 15000UL / tempo_bpm;

    // ── Bytes 1..N (pairs): [note_byte][duration_byte] ────────────────────
    const size_t num_notes = (total_bytes - 1) / 2;
    for (size_t i = 0; i < num_notes; ++i)
    {
        // Each note pair starts at byte index (1 + i*2), i.e. hex offset (2 + i*4)
        const size_t note_hex_pos = 2 + i * 4;
        int note_raw     = hexByte(note_hex_pos);
        int duration_raw = hexByte(note_hex_pos + 2);
        if (note_raw < 0 || duration_raw < 0)
            break;   // stop on invalid hex

        const bool    is_silence  = (note_raw & 0x80) != 0;
        const uint8_t midi_note   = note_raw & 0x7F;               // 0-127
        const uint8_t sixteenths  = (duration_raw == 0) ? 1u : static_cast<uint8_t>(duration_raw);

        if (is_silence)
        {
            delay(sixteenth_ms * sixteenths);
        }
        else
        {
            // Read frequency from PROGMEM table
            const uint16_t freq       = pgm_read_word(&MusicConsts::midi_frequencies[midi_note]);
            const uint32_t note_ms    = sixteenth_ms * sixteenths;
            // playTone beat parameter: 8000 samples/s → 8 samples per ms
            const uint32_t beat_units = note_ms * 8;
            const uint32_t t0         = millis();
            music.playTone(freq, static_cast<int>(beat_units));
            // playTone is blocking (generates samples at 8 kHz); only
            // delay for the remainder so the total per-note time ≈ note_ms.
            const uint32_t elapsed = millis() - t0;
            if (elapsed < note_ms)
                delay(note_ms - elapsed);
        }
    }

    music_udp_buildSuccess(MusicConsts::action_play_notes, response);
    return true;
}

// ─── Main UDP dispatcher ──────────────────────────────────────────────────────

/**
 * @brief Handle an incoming UDP message for MusicService.
 *        Format: "Music:<command>[:<JSON>]"
 *        Examples:
 *          "Music:play:{\"melody\":0,\"option\":4}"
 *          "Music:tone:{\"freq\":440,\"beat\":8000}"
 *          "Music:stop"
 *          "Music:melodies"
 * @param message    Raw UDP payload
 * @param remoteIP   Sender IP (used to send reply)
 * @param remotePort Sender port (used to send reply)
 * @return true if the message was claimed by this service
 */
bool MusicService::messageHandler(const std::string &message,
                                  const IPAddress &remoteIP,
                                  uint16_t remotePort)
{
    // ── 1. Ownership check ────────────────────────────────────────────────
    const std::string service_name = getServiceName();   // "Music"
    const size_t      name_len     = service_name.size();

    if (message.size() <= name_len + 1 ||
        message.compare(0, name_len, service_name) != 0 ||
        message[name_len] != ':')
        return false;

    // ── 2. Extract command token ──────────────────────────────────────────
    const size_t cmd_start = name_len + 1;
    const size_t sep_pos   = message.find(':', cmd_start);
    const std::string cmd  = (sep_pos == std::string::npos)
                                 ? message.substr(cmd_start)
                                 : message.substr(cmd_start, sep_pos - cmd_start);

    if (cmd.empty())
    {
        logger->warning(progmem_to_string(MusicConsts::msg_udp_bad_format));
        return true;   // claimed but malformed
    }

    // ── 3a. Non-JSON commands: playnotes uses a raw hex payload ───────────
    //        Handle before the JSON-parse step so an invalid-JSON warning
    //        is not emitted for perfectly valid hex strings.
    static std::string response;   // static buffer — no realloc after first call
    response.clear();

    if (cmd == MusicConsts::action_play_notes)
    {
        if (isServiceStarted())
        {
            if (sep_pos != std::string::npos && sep_pos + 1 < message.size())
                handleUDP_playnotes(message.c_str() + sep_pos + 1,
                                    message.size() - sep_pos - 1,
                                    response);
            else
                music_udp_buildError(MusicConsts::msg_udp_invalid_notes, response);
        }
#ifdef VERBOSE_DEBUG
        logger->debug("UDP playnotes -> " + response);
#endif
        if (!response.empty())
            udp_service.sendReply(response, remoteIP, remotePort);
        return true;
    }

    // ── 3b. Parse optional JSON payload (reuse static doc) ─────────────────
    s_music_udp_req.clear();
    if (sep_pos != std::string::npos && sep_pos + 1 < message.size())
    {
        DeserializationError err = deserializeJson(s_music_udp_req, message.c_str() + sep_pos + 1);
        if (err)
        {
            logger->warning(progmem_to_string(MusicConsts::msg_udp_json_error));
            return true;   // claimed but JSON invalid
        }
    }

    if (!isServiceStarted())
        return true;   // claimed, silently drop

    // ── 4. Dispatch table ─────────────────────────────────────────────────
    response.clear();

    if      (cmd == MusicConsts::action_play)
        handleUDP_play(s_music_udp_req, response);
    else if (cmd == MusicConsts::action_tone)
        handleUDP_tone(s_music_udp_req, response);
    else if (cmd == MusicConsts::action_stop)
        handleUDP_stop(s_music_udp_req, response);
    else if (cmd == MusicConsts::action_get_melodies)
        handleUDP_getMelodies(s_music_udp_req, response);
    else
    {
        logger->warning(progmem_to_string(MusicConsts::msg_udp_unknown_cmd) + ": " + cmd);
        music_udp_buildError(MusicConsts::msg_udp_unknown_cmd, response);
    }

#ifdef VERBOSE_DEBUG
    logger->debug("UDP " + cmd + " -> " + response);
#endif

    // ── 5. Send reply back to sender ──────────────────────────────────────
    if (!response.empty())
        udp_service.sendReply(response, remoteIP, remotePort);

    return true;
}

uint8_t mission_impossible_theme[][2] = { 
    {64,8}, {255,8}, {64,8}, {255,8}, {64,8}, {255,8}, {64,8}, {255,8}, {67,8}, {255,8}, {67,8}, {255,8}, {67,8}, {255,8}, {67,8}, {255,8}, {64,8}, {255,8}, {64,8}, {255,8}, {64,8}, {255,8}, {64,8}, {255,8}, {67,8}, {255,8}, {67,8}, {255,8}, {67,8}, {255,8}, {67,8}, {255,8}, 
// Descending spy line 
{72,16}, {71,16}, {69,16}, {67,16}, {66,16}, {65,16}, {64,32} };

uint8_t imperial_march_theme[][2] = {
  // --- Phrase 1 ---
  {67,8}, {67,8}, {67,16},
  {63,8}, {70,8}, {67,16},
  {63,8}, {70,8}, {67,32},
  // --- Phrase 2 ---
  {79,8}, {79,8}, {79,16},
  {75,8}, {70,8}, {63,16},
  {75,8}, {70,8}, {63,32},
  // --- Phrase 3 (repeat of phrase 1) ---
  {67,8}, {67,8}, {67,16},
  {63,8}, {70,8}, {67,16},
  {63,8}, {70,8}, {67,32},
  // --- Phrase 4 (variation) ---
  {79,8}, {79,8}, {79,16},
  {75,8}, {70,8}, {63,16},
  {70,8}, {63,8}, {55,32},
  // --- Bridge 1 ---
  {82,16}, {82,16}, {82,16}, {84,16},
  {82,16}, {84,16}, {82,16}, {79,16},
  {75,16}, {72,16}, {70,16}, {67,32},
  // --- Bridge 2 ---
  {82,16}, {82,16}, {82,16}, {84,16},
  {82,16}, {84,16}, {82,16}, {79,16},
  {75,16}, {77,16}, {75,16}, {72,32},
  // --- Return to main theme (shortened) ---
  {67,8}, {67,8}, {67,16},
  {63,8}, {70,8}, {67,16},
  {63,8}, {70,8}, {67,32},
  // --- Final cadence ---
  {79,8}, {79,8}, {79,16},
  {75,8}, {70,8}, {63,16},
  {70,8}, {63,8}, {55,32}
};

uint8_t nyan_cat_theme[][2] = {

  // --- Phrase 1 ---
  {76,8}, {74,8}, {72,8}, {74,8}, {76,8}, {76,8}, {76,16},
  {74,8}, {74,8}, {74,16}, {76,8}, {80,8}, {80,16},

  // --- Phrase 2 ---
  {76,8}, {74,8}, {72,8}, {74,8}, {76,8}, {76,8}, {76,16},
  {74,8}, {74,8}, {76,8}, {74,8}, {72,32},

  // --- Phrase 3 (jumping part) ---
  {80,8}, {78,8}, {76,8}, {74,8}, {76,8}, {78,8}, {80,8}, {76,8},
  {78,8}, {76,8}, {74,8}, {72,8}, {74,8}, {76,8}, {74,8}, {72,8},

  // --- Phrase 4 (lower echo) ---
  {69,8}, {71,8}, {72,8}, {74,8}, {72,8}, {71,8}, {69,8}, {68,8},
  {69,8}, {71,8}, {72,8}, {74,8}, {72,8}, {71,8}, {69,8}, {64,32},

  // --- Phrase 5 (repeat of phrase 1) ---
  {76,8}, {74,8}, {72,8}, {74,8}, {76,8}, {76,8}, {76,16},
  {74,8}, {74,8}, {74,16}, {76,8}, {80,8}, {80,16},

  // --- Phrase 6 (repeat of phrase 2) ---
  {76,8}, {74,8}, {72,8}, {74,8}, {76,8}, {76,8}, {76,16},
  {74,8}, {74,8}, {76,8}, {74,8}, {72,32}
};

uint8_t ateam_theme[][2] = {

  // Opening fanfare
  {72,8}, {72,8}, {72,16},   // C5 C5 C5—
  {67,8}, {67,8}, {67,16},   // G4 G4 G4—
  {72,8}, {72,8}, {72,16},   // C5 C5 C5—
  {67,8}, {67,8}, {67,16},   // G4 G4 G4—

  // Rising heroic line
  {60,8}, {62,8}, {64,8}, {65,8},   // C4 D4 E4 F4
  {67,16},                          // G4—
  {72,16},                          // C5—
  {74,32},                          // D5——

  // Repeat variation
  {72,8}, {72,8}, {72,16},
  {67,8}, {67,8}, {67,16},
  {72,8}, {72,8}, {72,16},
  {67,8}, {67,8}, {67,16},

  // Final cadence
  {60,8}, {62,8}, {64,8}, {65,8},
  {67,16},
  {72,16},
  {74,32}
};


uint8_t twilight_zone_theme[][2] = {

  // First iconic pattern (C5 ↔ Eb5)
  {72,8}, {75,8}, {72,8}, {75,8},
  {72,8}, {75,8}, {72,8}, {75,16},

  // Second pattern one octave higher (C6 ↔ Eb6)
  {84,8}, {87,8}, {84,8}, {87,8},
  {84,8}, {87,8}, {84,8}, {87,16},

  // Repeat lower octave
  {72,8}, {75,8}, {72,8}, {75,8},
  {72,8}, {75,8}, {72,8}, {75,16},

  // Final high echo
  {84,8}, {87,8}, {84,8}, {87,8},
  {84,8}, {87,8}, {84,8}, {87,32}
};
uint8_t super_mario_theme[][2] = {

  // Intro "da-da-da, da-da"
  {76,4}, {76,4}, {255,4}, {76,4},
  {255,4}, {72,4}, {76,4}, {255,4},
  {79,4}, {255,12},

  // Phrase 1
  {67,4}, {255,8}, {67,4}, {255,8},
  {79,4}, {255,8}, {79,4}, {255,8},

  // Phrase 2
  {76,4}, {255,4}, {72,4}, {76,4},
  {255,4}, {79,4}, {255,4}, {67,4},

  // "ba‑da‑ba‑da‑ba‑da‑ba"
  {72,4}, {255,4}, {67,4}, {255,4},
  {64,4}, {255,4}, {69,4}, {255,4},
  {71,4}, {255,4}, {70,4}, {69,4},

  // Jumping upward run
  {67,4}, {76,4}, {79,4}, {81,4},
  {79,4}, {76,4}, {74,4}, {72,4},

  // Ending cadence
  {74,4}, {255,4}, {67,4}, {255,4},
  {72,4}, {255,4}, {67,4}, {255,12}
};