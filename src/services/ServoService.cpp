// Servo controller handler for DFR0548 board
/**
 * @file servo_handler.cpp
 * @brief Implementation for servo controller integration with the main application
 */

#include "ServoService.h"
#include <WebServer.h>
#include <pgmspace.h>
#include "../DFR0558/DFR0548.h"

#define MAX_SERVOS 5
// TODO: move html out of code
static const char SERVO_CONTROL_PANEL_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>om src/sensor/sensor_handler.cpp:1:
    <title>Servo Control Panel</title>
    <style>
        body { font-family: Arial, sans-serif; background-color: #1e1e1e; color: #e0e0e0; margin: 20px; }
        h1 { color: #00d4ff; }
        .channel { background-color: #2d2d2d; padding: 15px; margin: 10px 0; border-radius: 5px; }
        input { padding: 8px; margin: 5px; }
        button { background-color: #00d4ff; color: #1e1e1e; padding: 10px 20px; border: none; border-radius: 4px; cursor: pointer; font-weight: bold; }
        button:hover { background-color: #00a8cc; }
        .status-link { margin-top: 20px; }
        a { color: #00d4ff; text-decoration: none; }
        a:hover { text-decoration: underline; }
    </style>*sensorsRoutePath=
</head>
<body>
    <h1>🤖 Servo Control Panel</h1>
    <p>Control up to 8 servos (channels 0-7)</p>
    
    <div class="channel">
        <h3>Set Servo Angle</h3>
        <label>Channel (0-7): <input type="number" id="angle_ch" min="0" max="7" value="0"></label>
        <label>Angle (0-180): <input type="number" id="angle_val" min="0" max="180" value="90"></label>
        <button onclick="setAngle()">Set Angle</button>
        <div id="angle_result"></div>
    </div>
    
    <div class="channel">
        <h3>Set Servo Speed (Continuous Rotation)</h3>
        <label>Channel (0-7): <input type="number" id="speed_ch" min="0" max="7" value="0"></label>
        <label>Speed (-100 to 100): <input type="number" id="speed_val" min="-100" max="100" value="50"></label>
        <button onclick="setSpeed()">Set Speed</button>
        <div id="speed_result"></div>
    </div>
    
    <div class="channel">
        <h3>Quick Actions</h3>
        <button onclick="centerAll()">Center All Servos</button>
        <button onclick="stopAll()">Stop All Servos</button>
        <div id="action_result"></div>
    </div>
    
    <div class="status-link">
        <p><a href="/servo/statuom src/sensor/sensor_handler.cpp:1:s">View Servo Status</a></p>
    </div>
    
    <script>
        async function setAngle() {
            const ch = document.getElementById('angle_ch').value;
            const angle = document.getElementById('angle_val').value;
            try {
                const response = await fetch(`/api/servo/set?ch=${ch}&angle=${angle}`, { method: 'PUT' });
                const data = await response.json();
                document.getElementById('angle_result').innerHTML = 
                    response.ok ? `✓ Channel ${ch} set to ${angle}°` : `✗ Error: ${data.message}`;
            } catch(e) {
                document.getElementById('angle_result').innerHTML = `✗ Error: ${e.message}`;
            }
        }
        
        async function setSpeed() {
            const ch = document.getElementById('speed_ch').value;
            const speed = document.getElementById('speed_val').value;
            try {
                const response = await fetch(`/api/servo/speed?ch=${ch}&speed=${speed}`, { method: 'PUT' });
                const data = await response.json();
                document.getElementById('speed_result').innerHTML = 
                    response.ok ? `✓ Channel ${ch} speed set to ${speed}%` : `✗ Error: ${data.message}`;
            } catch(e) {
                document.getElementById('speed_result').innerHTML = `✗ Error: ${e.message}`;
            }
        }
        
        async function centerAll() {
            try {
                const response = await fetch('/api/servo/center', { method: 'PUT' });
                const data = await response.json();
                document.getElementById('action_result').innerHTML = 
                    response.ok ? '✓ All servos centered' : `✗ Error: ${data.message}`;
            } catch(e) {
                document.getElementById('action_result').innerHTML = `✗ Error: ${e.message}`;
            }
        }
        
        async function stopAll() {
            try {
                const response = await fetch('/api/servo/stop_all', { method: 'PUT' });
                const data = await response.json();
                document.getElementById('action_result').innerHTML = 
                    response.ok ? '✓ All servos stopped' : `✗ Error: ${data.message}`;
            } catch(e) {
                document.getElementById('action_result').innerHTML = `✗ Error: ${e.message}`;
            }
        }
    </script>
</body>
</html>
)rawliteral";

// Global servo controller instance
DFR0548_Controller servoController;

// Initialize servo controller
bool initialized = false;

bool ServoService::init()
{

    if (servoController.init())
    {
        // Configure default servo types (all angular by default)
        for (uint8_t i = 0; i < MAX_SERVOS; i++)
        {
            servoController.setServoType(i, false); // Angular servo
            servoController.centerServo(i);         // Center position
        }
        initialized = true;
        return true;
    }
    else
    {
        return false;
    }
}

// Set servo angle (wrapper for easier integration)
bool ServoService::setAngle(uint8_t channel, uint16_t angle)
{
    if (!initialized)
    {
        return false;
    }
    if (channel >= MAX_SERVOS)
    {
        return false;
    }

    servoController.setAngle(channel, angle);
    return true;
}

// Set servo speed for continuous rotation
bool ServoService::setSpeed(uint8_t channel, int8_t speed)
{
    if (!initialized)
    {
        return false;
    }
    if (channel >= MAX_SERVOS || channel < 0)
    {
        return false;
    }

    // First set this channel to continuous rotation mode
    servoController.setServoType(channel, true);

    servoController.setSpeed(channel, speed);
    return true;
}

// Stop servo
bool ServoService::stopServo(uint8_t channel)
{
    if (!initialized)
    {
        return false;
    }
    if (channel >= MAX_SERVOS || channel < 0)
    {
        return false;
    }

    servoController.stopServo(channel);
    return true;
}

// Get servo status
std::string ServoService::getStatus(uint8_t channel)
{
    if (!initialized)
    {
        return "Not initialized";
    }
    if (channel >= MAX_SERVOS || channel < 0)
    {
        return "Invalid channel";
    }

    return servoController.getChannelStatus(channel);
}
/**
 * @brief Register HTTP routes for servo control.
 * @param server Pointer to the WebServer instance.
 * @param basePath Base path to prefix to all routes.
 * @return true if registration was successful, false otherwise.
 */
bool ServoService::registerRoutes(WebServer *server, std::string basePath)
{
    if (!server)
    {
        return false;
    }
    if (!initialized)
    {
        return false;
    }

    std::string path = std::string("/www/") + basePath ;
    // Servo control panel HTML page at /servo
    server->on(basePath.c_str(), HTTP_GET, [server]()
               {
                   server->send_P(200, "text/html", SERVO_CONTROL_PANEL_HTML);
               });
    routes.insert(path);

    // API: Set servo angle at /api/servo/set
    path = std::string("/api/") + basePath + "/setAngle";
    server->on(path.c_str(), HTTP_PUT, [=]()
               {
                   if (!server->hasArg("servo") || !server->hasArg("angle"))
                   {
                       server->send(400, "application/json", "{\"result\":\"error\",\"message\":\"Missing ch or angle parameter\"}");
                          return;   
                   }

                   uint8_t ch = (uint8_t)server->arg("ch").toInt();
                   uint16_t angle = (uint16_t)server->arg("angle").toInt();

                   if (ch > 7 || angle > 360)
                   {
                       server->send(400, "application/json", "{\"result\":\"error\",\"message\":\"Invalid parameters\"}");
                       return;
                   }

                   if (setAngle(ch, angle))
                   {
                       std::string response = "{\"result\":\"ok\",\"ch\":" + std::to_string(ch) + ",\"angle\":" + std::to_string(angle) + "}";
                       server->send(200, "application/json", response.c_str());
                   }
                   else
                   {
                       server->send(500, "application/json", "{\"result\":\"error\",\"message\":\"Failed to set servo\"}");
                   } });

    routes.insert(path);

    path = std::string("/api/") + basePath + "/setSpeed";
    // API: Set servo speed at /api/servo/speed
    server->on(path.c_str(), HTTP_PUT, [=]()
               {
        if (!server->hasArg("servo") || !server->hasArg("speed")) {
            server->send(400, "application/json", "{\"result\":\"error\",\"message\":\"Missing ch or speed parameter\"}");
            return;
        }
        
        uint8_t ch = (uint8_t)server->arg("servo").toInt();
        int8_t speed = (int8_t)server->arg("speed").toInt();
        
        if (ch > 7 || speed < -100 || speed > 100) {
            server->send(400, "application/json", "{\"result\":\"error\",\"message\":\"Invalid parameters\"}");
            return;
        }
        
        if (setSpeed(ch, speed)) {
            std::string response = "{\"result\":\"ok\",\"ch\":" + std::to_string(ch) + ",\"speed\":" + std::to_string(speed) + "}";
            server->send(200, "application/json", response.c_str());
        } else {
            server->send(500, "application/json", "{\"result\":\"error\",\"message\":\"Failed to set servo speed\"}");
        } });

    routes.insert(path);


    // API: Stop all servos at /api/servo/stop_all
    path = std::string("/api/") + basePath + "/stop_all";
    server->on(path.c_str(), HTTP_PUT, [=]()
               {
        bool success = true;
        for (uint8_t i = 0; i < MAX_SERVOS; i++) {
            success = success && stopServo(i);
            
        }
        
        if (success) {
            server->send(200, "application/json", "{\"result\":\"ok\",\"message\":\"All servos stopped\"}");
        } else {
            server->send(500, "application/json", "{\"result\":\"error\",\"message\":\"Failed to stop all servos\"}");
        } });
    routes.insert(path);
    return true;
}
