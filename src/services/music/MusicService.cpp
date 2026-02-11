/**
 * @file MusicService.cpp
 * @brief Implementation for music playback service
 * @details Exposed routes:
 *          - POST /api/music/v1/play - Play built-in melody with options
 *          - POST /api/music/v1/tone - Play a tone at specified frequency and duration
 *          - POST /api/music/v1/stop - Stop current tone playback
 */

#include "MusicService.h"
#include <WebServer.h>
#include <unihiker_k10.h>
#include <ArduinoJson.h>

extern Music music;

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
    constexpr const char music_action_get_melodies[] PROGMEM = "melodies";
    constexpr const char music_action_play[] PROGMEM = "play";
    constexpr const char music_action_stop[] PROGMEM = "stop";
    constexpr const char music_action_tone[] PROGMEM = "tone";
    constexpr const char music_desc_get_melodies[] PROGMEM = "Get list of available built-in melodies";
    constexpr const char music_desc_play_melody[] PROGMEM = "Play built-in melody with playback options. Query parameters: melody (required, 0-19), option (optional, 1=Once, 2=Forever, 4=OnceInBackground, 8=ForeverInBackground)";
    constexpr const char music_desc_play_tone[] PROGMEM = "Play a tone at specified frequency and duration. Query parameters: freq (required, Hz), beat (optional, default 8000)";
    constexpr const char music_desc_stop_tone[] PROGMEM = "Stop current tone playback";
    constexpr const char music_param_beat[] PROGMEM = "beat";
    constexpr const char music_param_freq[] PROGMEM = "freq";
    constexpr const char music_param_melody[] PROGMEM = "melody";
    constexpr const char music_param_option[] PROGMEM = "option";
    constexpr const char music_service_name[] PROGMEM = "Music";
    constexpr const char music_service_path[] PROGMEM = "music/v1";
    constexpr const char music_tag[] PROGMEM = "Music";
}

bool MusicService::initializeService()
{
    setServiceStatus(INITIALIZED);
    
#ifdef VERBOSE_DEBUG
    logger->debug(getServiceName() + fpstr_to_string(FPSTR(MusicConsts::msg_initialize_done)));
#endif
    return true;
}

bool MusicService::startService()
{
    setServiceStatus(STARTED);
#ifdef VERBOSE_DEBUG
    logger->debug(getServiceName() + fpstr_to_string(FPSTR(MusicConsts::msg_start_done)));
#endif
    return true;
}

bool MusicService::stopService()
{
   setServiceStatus(STOPPED);
#ifdef VERBOSE_DEBUG
    logger->debug(getServiceName() + fpstr_to_string(FPSTR(MusicConsts::msg_stop_done)));
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
    std::string path = getPath(MusicConsts::music_action_play);
    logRouteRegistration(path);
    
    std::vector<OpenAPIResponse> responses;
    OpenAPIResponse successResponse = createSuccessResponse(response_desc);
    responses.push_back(successResponse);
    responses.push_back(createServiceNotStartedResponse());
    
    // Play melody route parameters
    std::vector<OpenAPIParameter> playParams;
    playParams.push_back(OpenAPIParameter("melody", "string", "query", "Melody enum value (0-19)", true));
    playParams.push_back(OpenAPIParameter("option", "string", "query", "Playback option (1=Once, 2=Forever, 4=OnceInBackground, 8=ForeverInBackground)", false));
    
    OpenAPIRoute routePlay(path.c_str(), RoutesConsts::method_post, MusicConsts::music_desc_play_melody, MusicConsts::music_tag, false, playParams, responses);
    registerOpenAPIRoute(routePlay);
    
    webserver.on(path.c_str(), HTTP_POST, [this]() {
        if (!checkServiceStarted()) return;
        
        String melodyStr = webserver.arg(MusicConsts::music_param_melody);
        String optionStr = webserver.arg(MusicConsts::music_param_option);
        
        if (melodyStr.isEmpty()) {
            webserver.send(400, RoutesConsts::mime_json, FPSTR(MusicConsts::json_error_melody_required));
            return;
        }
        
        int melody = melodyStr.toInt();
        int option = optionStr.isEmpty() ? OnceInBackground : optionStr.toInt();
        
        music.playMusic(static_cast<Melodies>(melody), static_cast<MelodyOptions>(option));
        
        webserver.send(200, RoutesConsts::mime_json, FPSTR(MusicConsts::json_status_ok));
    });
    
    // Register play tone route
    path = getPath(MusicConsts::music_action_tone);
    logRouteRegistration(path);
    
    // Play tone route parameters
    std::vector<OpenAPIParameter> toneParams;
    toneParams.push_back(OpenAPIParameter("freq", "string", "query", "Frequency in Hz", true));
    toneParams.push_back(OpenAPIParameter("beat", "string", "query", "Beat duration (default 8000)", false));
    
    OpenAPIRoute routeTone(path.c_str(), RoutesConsts::method_post, MusicConsts::music_desc_play_tone, MusicConsts::music_tag, false, toneParams, responses);
    registerOpenAPIRoute(routeTone);
    
    webserver.on(path.c_str(), HTTP_POST, [this]() {
        if (!checkServiceStarted()) return;
        
        String freqStr = webserver.arg(MusicConsts::music_param_freq);
        String beatStr = webserver.arg(MusicConsts::music_param_beat);
        
        if (freqStr.isEmpty()) {
            webserver.send(400, RoutesConsts::mime_json, FPSTR(MusicConsts::json_error_freq_required));
            return;
        }
        
        int freq = freqStr.isEmpty()? 440 : freqStr.toInt();
        int beat = beatStr.isEmpty() ? 8000 : beatStr.toInt();
        
        music.playTone(freq, beat);
        
        webserver.send(200, RoutesConsts::mime_json, FPSTR(MusicConsts::json_status_ok));
    });
    
    // Register stop tone route
     path = getPath(MusicConsts::music_action_stop);
    logRouteRegistration(path);
    
    OpenAPIRoute routeStop(path.c_str(), RoutesConsts::method_post, MusicConsts::music_desc_stop_tone, MusicConsts::music_tag, false, {}, responses);
    registerOpenAPIRoute(routeStop);
    
    webserver.on(path.c_str(), HTTP_POST, [this]() {
        if (!checkServiceStarted()) return;
        
        music.stopPlayTone();
        webserver.send(200, RoutesConsts::mime_json, FPSTR(MusicConsts::json_status_ok));
    });

    // Register status route
    registerServiceStatusRoute(MusicConsts::music_tag, this);

 path = getPath(MusicConsts::music_action_get_melodies);
#ifdef VERBOSE_DEBUG
    logger->debug(fpstr_to_string(FPSTR(RoutesConsts::msg_registering)) + path);
    logRouteRegistration(path);enAPIRoute routeMelodies(path.c_str(), RoutesConsts::method_post, MusicConsts::music_desc_get_melodies, MusicConsts::music_tag, false, {}, responses);
    registerOpenAPIRoute(routeMelodies  );
    
    webserver.on(path.c_str(), HTTP_POST, [this]() {
        if (!checkServiceStarted()) return;
        
        webserver.send(200, RoutesConsts::mime_json, FPSTR(MusicConsts::json_melodies_list));
    });

    


    return true;
}


std::string MusicService::getServiceSubPath()
{
    return MusicConsts::music_service_path;
}

std::string MusicService::getServiceName()
{
    return MusicConsts::music_service_name;
}
