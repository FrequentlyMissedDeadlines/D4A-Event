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
    constexpr const char music_service_name[] PROGMEM = "Music";
    constexpr const char music_service_path[] PROGMEM = "music/v1";

    constexpr const char music_action_play[] PROGMEM = "play";
    constexpr const char music_action_tone[] PROGMEM = "tone";
    constexpr const char music_action_stop[] PROGMEM = "stop";
    constexpr const char music_action_get_melodies[] PROGMEM = "melodies";
    constexpr const char music_param_melody[] PROGMEM = "melody";
    constexpr const char music_param_option[] PROGMEM = "option";
    constexpr const char music_param_freq[] PROGMEM = "freq";
    constexpr const char music_param_beat[] PROGMEM = "beat";

    constexpr const char music_tag[] PROGMEM = "Music";
    constexpr const char music_desc_play_melody[] PROGMEM = "Play built-in melody with playback options. Query parameters: melody (required, 0-19), option (optional, 1=Once, 2=Forever, 4=OnceInBackground, 8=ForeverInBackground)";
    constexpr const char music_desc_play_tone[] PROGMEM = "Play a tone at specified frequency and duration. Query parameters: freq (required, Hz), beat (optional, default 8000)";
    constexpr const char music_desc_stop_tone[] PROGMEM = "Stop current tone playback";
    constexpr const char music_desc_get_melodies[] PROGMEM = "Get list of available built-in melodies";
}

bool MusicService::initializeService()
{
    service_status_ = STARTED;
    status_timestamp_ = millis();
    
#ifdef VERBOSE_DEBUG
    logger->debug(getServiceName() + " initialize done");
#endif
    return true;
}

bool MusicService::startService()
{
    service_status_ = STARTED;
    status_timestamp_ = millis();
#ifdef VERBOSE_DEBUG
    logger->debug(getServiceName() + " start done");
#endif
    return true;
}

bool MusicService::stopService()
{
    service_status_ = STOPPED;
    status_timestamp_ = millis();
#ifdef VERBOSE_DEBUG
    logger->debug(getServiceName() + " stop done");
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
#ifdef VERBOSE_DEBUG
    logger->debug("Registering " + pathPlay);
#endif
    
    std::vector<OpenAPIResponse> responses;
    OpenAPIResponse successResponse(200, response_desc);
    responses.push_back(successResponse);
    
    // Play melody route parameters
    std::vector<OpenAPIParameter> playParams;
    playParams.push_back(OpenAPIParameter("melody", "string", "query", "Melody enum value (0-19)", true));
    playParams.push_back(OpenAPIParameter("option", "string", "query", "Playback option (1=Once, 2=Forever, 4=OnceInBackground, 8=ForeverInBackground)", false));
    
    OpenAPIRoute routePlay(path.c_str(), RoutesConsts::method_post, MusicConsts::music_desc_play_melody, MusicConsts::music_tag, false, playParams, responses);
    registerOpenAPIRoute(routePlay);
    
    webserver.on(path.c_str(), HTTP_POST, []() {
        String melodyStr = webserver.arg(MusicConsts::music_param_melody);
        String optionStr = webserver.arg(MusicConsts::music_param_option);
        
        if (melodyStr.isEmpty()) {
            webserver.send(400, "application/json", "{\"error\":\"melody parameter required\"}");
            return;
        }
        
        int melody = melodyStr.toInt();
        int option = optionStr.isEmpty() ? OnceInBackground : optionStr.toInt();
        
        music.playMusic(static_cast<Melodies>(melody), static_cast<MelodyOptions>(option));
        
        webserver.send(200, "application/json", "{\"status\":\"ok\"}");
    });
    
    // Register play tone route
    path = getPath(MusicConsts::music_action_tone);
#ifdef VERBOSE_DEBUG
    logger->debug("Registering " + path);
#endif
    
    // Play tone route parameters
    std::vector<OpenAPIParameter> toneParams;
    toneParams.push_back(OpenAPIParameter("freq", "string", "query", "Frequency in Hz", true));
    toneParams.push_back(OpenAPIParameter("beat", "string", "query", "Beat duration (default 8000)", false));
    
    OpenAPIRoute routeTone(path.c_str(), RoutesConsts::method_post, MusicConsts::music_desc_play_tone, MusicConsts::music_tag, false, toneParams, responses);
    registerOpenAPIRoute(routeTone);
    
    webserver.on(path.c_str(), HTTP_POST, []() {
        String freqStr = webserver.arg(MusicConsts::music_param_freq);
        String beatStr = webserver.arg(MusicConsts::music_param_beat);
        
        if (freqStr.isEmpty()) {
            webserver.send(400, "application/json", "{\"error\":\"freq parameter required\"}");
            return;
        }
        
        int freq = freqStr.isEmpty()? 440 : freqStr.toInt();
        int beat = beatStr.isEmpty() ? 8000 : beatStr.toInt();
        
        music.playTone(freq, beat);
        
        webserver.send(200, "application/json", "{\"status\":\"ok\"}");
    });
    
    // Register stop tone route
     path = getPath(MusicConsts::music_action_stop);
#ifdef VERBOSE_DEBUG
    logger->debug("Registering " + path);
#endif
    
    OpenAPIRoute routeStop(path.c_str(), RoutesConsts::method_post, MusicConsts::music_desc_stop_tone, MusicConsts::music_tag, false, {}, responses);
    registerOpenAPIRoute(routeStop);
    
    webserver.on(path.c_str(), HTTP_POST, []() {
        music.stopPlayTone();
        webserver.send(200, "application/json", "{\"status\":\"ok\"}");
    });
 path = getPath(MusicConsts::music_action_get_melodies);
#ifdef VERBOSE_DEBUG
    logger->debug("Registering " + path);
#endif
    
    OpenAPIRoute routeMelodies(path.c_str(), RoutesConsts::method_post, MusicConsts::music_desc_get_melodies, MusicConsts::music_tag, false, {}, responses);
    registerOpenAPIRoute(routeMelodies  );
    
    webserver.on(path.c_str(), HTTP_POST, []() {
        
        webserver.send(200, "application/json", "[\"DADADADUM\",\"ENTERTAINER\",\"PRELUDE\",\"ODE\",\"NYAN\",\"RINGTONE\",\"FUNK\",\"BLUES\",\"BIRTHDAY\",\"WEDDING\",\"FUNERAL\",\"PUNCHLINE\",\"BADDY\",\"CHASE\",\"BA_DING\",\"WAWAWAWAA\",\"JUMP_UP\",\"JUMP_DOWN\",\"POWER_UP\",\"POWER_DOWN\"]");
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

bool MusicService::saveSettings()
{
    return true;
}

bool MusicService::loadSettings()
{
    return true;
}
