// Servo controller handler for DFR0548 board
/**
 * @file servo_handler.cpp
 * @brief Implementation for servo controller integration with the main application
 */


#include "servo_handler.h"
#include <WebServer.h>
#include <pgmspace.h>

#ifdef DEBUG
#define DEBUG_TO_SERIAL(x) Serial.println(x)
#define DEBUGF_TO_SERIAL(fmt, ...) Serial.printf(fmt, __VA_ARGS__)
#else
#define DEBUG_TO_SERIAL(x)
#define DEBUGF_TO_SERIAL(fmt, ...)
#endif

static const char SERVO_CONTROL_PANEL_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
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
    </style>
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
        <p><a href="/servo/status">View Servo Status</a></p>
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
bool ServoHandler_init() {
    DEBUG_TO_SERIAL("Initializing DFR0548 servo controller...");

    if (servoController.init()) {
        DEBUG_TO_SERIAL("Servo controller initialized successfully");

        // Configure default servo types (all angular by default)
        for (uint8_t i = 0; i < 8; i++) {
            servoController.setServoType(i, false);  // Angular servo
            servoController.centerServo(i);          // Center position
        }
        initialized=true;
        return true;
    } else {
        DEBUG_TO_SERIAL("ERROR: Failed to initialize servo controller");
        return false;
    }
}

// Set servo angle (wrapper for easier integration)
bool ServoHandler_setAngle(uint8_t channel, uint16_t angle) {
    if (!initialized) {
        DEBUG_TO_SERIAL("ERROR: Servo controller not initialized");
        return false;
    }
    if (channel >= 8) {
        DEBUG_TO_SERIAL("ERROR: Invalid servo channel");
        return false;
    }

    servoController.setAngle(channel, angle);
    DEBUGF_TO_SERIAL("Servo %d set to angle %d°\n", channel, angle);
    return true;
}

// Set servo speed for continuous rotation
bool ServoHandler_setSpeed(uint8_t channel, int8_t speed) {
        if (!initialized) {
        DEBUG_TO_SERIAL("ERROR: Servo controller not initialized");
        return false;
    }
    if (channel >= 8) {
        DEBUG_TO_SERIAL("ERROR: Invalid servo channel");
        return false;
    }

    // First set this channel to continuous rotation mode
    servoController.setServoType(channel, true);

    servoController.setSpeed(channel, speed);
    DEBUGF_TO_SERIAL("Servo %d set to speed %d%%\n", channel, speed);
    return true;
}

// Stop servo
bool ServoHandler_stopServo(uint8_t channel) {
        if (!initialized) {
        DEBUG_TO_SERIAL("ERROR: Servo controller not initialized");
        return false;
    }
    if (channel >= 8) {
        DEBUG_TO_SERIAL("ERROR: Invalid servo channel");
        return false;
    }

    servoController.stopServo(channel);
    DEBUGF_TO_SERIAL("Servo %d stopped\n", channel);
    return true;
}

// Get servo status
String ServoHandler_getStatus(uint8_t channel) {
        if (!initialized) {
        DEBUG_TO_SERIAL("ERROR: Servo controller not initialized");
        return "Not initialized";
    }
    if (channel >= 8) {
        return "Invalid channel";
    }

    return servoController.getChannelStatus(channel);
}

bool ServoModule_registerRoutes(WebServer* server) {
    if (!server) {
        DEBUG_TO_SERIAL("ERROR: Cannot register servo routes - WebServer pointer is NULL!");
        return false;
    }
    if (!initialized) {
        DEBUG_TO_SERIAL("ERROR: Servo controller not initialized");
        return false;
    }
    
    DEBUG_TO_SERIAL("Registering servo routes with WebServer...");

    // Servo control panel HTML page at /servo
    server->on("/servo", HTTP_GET, [server]() {
        server->send_P(200, "text/html", SERVO_CONTROL_PANEL_HTML);
        DEBUG_TO_SERIAL("  - GET /servo (servo control panel)");
    
    });

    // Servo status page at /servo/status
    server->on("/servo/status", HTTP_GET, [server]() {
        String html = "<html><head><title>Servo Status</title><style>";
        html += "body { font-family: Arial; background-color: #1e1e1e; color: #e0e0e0; margin: 20px; }";
        html += "h1 { color: #00d4ff; }";
        html += "table { border-collapse: collapse; width: 100%; max-width: 800px; }";
        html += "th, td { border: 1px solid #444; padding: 10px; text-align: left; }";
        html += "th { background-color: #2d2d2d; color: #00d4ff; }";
        html += "tr:nth-child(even) { background-color: #252525; }";
        html += "a { color: #00d4ff; text-decoration: none; margin-top: 20px; display: inline-block; }";
        html += "</style></head><body>";
        html += "<h1>📊 Servo Status</h1>";
        html += "<table><tr><th>Channel</th><th>Status</th></tr>";
        
        for (uint8_t i = 0; i < 8; i++) {
            html += "<tr><td>Channel " + String(i) + "</td><td>" + ServoHandler_getStatus(i) + "</td></tr>";
        }
        
        html += "</table>";
        html += "<p><a href=\"/servo\">Back to Control Panel</a></p>";
        html += "</body></html>";
        
        server->send(200, "text/html", html);
        DEBUG_TO_SERIAL("  - GET /servo/status (servo status)");

    });

    // API: Set servo angle at /api/servo/set
    server->on("/api/servo/set", HTTP_PUT, [server]() {
        if (!server->hasArg("ch") || !server->hasArg("angle")) {
            server->send(400, "application/json", "{\"result\":\"error\",\"message\":\"Missing ch or angle parameter\"}");
            return;
        }
        
        uint8_t ch = (uint8_t)server->arg("ch").toInt();
        uint16_t angle = (uint16_t)server->arg("angle").toInt();
        
        if (ch > 7 || angle > 180) {
            server->send(400, "application/json", "{\"result\":\"error\",\"message\":\"Invalid parameters\"}");
            return;
        }
        
        if (ServoHandler_setAngle(ch, angle)) {
            String response = "{\"result\":\"ok\",\"ch\":" + String(ch) + ",\"angle\":" + String(angle) + "}";
            server->send(200, "application/json", response);
        } else {
            server->send(500, "application/json", "{\"result\":\"error\",\"message\":\"Failed to set servo\"}");
        }
            DEBUG_TO_SERIAL("  - PUT /api/servo/set (set angle)");

    });

    // API: Set servo speed at /api/servo/speed
    server->on("/api/servo/speed", HTTP_PUT, [server]() {
        if (!server->hasArg("ch") || !server->hasArg("speed")) {
            server->send(400, "application/json", "{\"result\":\"error\",\"message\":\"Missing ch or speed parameter\"}");
            return;
        }
        
        uint8_t ch = (uint8_t)server->arg("ch").toInt();
        int8_t speed = (int8_t)server->arg("speed").toInt();
        
        if (ch > 7 || speed < -100 || speed > 100) {
            server->send(400, "application/json", "{\"result\":\"error\",\"message\":\"Invalid parameters\"}");
            return;
        }
        
        if (ServoHandler_setSpeed(ch, speed)) {
            String response = "{\"result\":\"ok\",\"ch\":" + String(ch) + ",\"speed\":" + String(speed) + "}";
            server->send(200, "application/json", response);
        } else {
            server->send(500, "application/json", "{\"result\":\"error\",\"message\":\"Failed to set servo speed\"}");
        }
            DEBUG_TO_SERIAL("  - PUT /api/servo/speed (set speed)");
    });

    // API: Center all servos at /api/servo/center
    server->on("/api/servo/center", HTTP_PUT, [server]() {
        bool success = true;
        for (uint8_t i = 0; i < 8; i++) {
            if (!ServoHandler_setAngle(i, 90)) {  // Center at 90 degrees
                success = false;
            }
        }
        
        if (success) {
            server->send(200, "application/json", "{\"result\":\"ok\",\"message\":\"All servos centered\"}");
        } else {
            server->send(500, "application/json", "{\"result\":\"error\",\"message\":\"Failed to center servos\"}");
        }DEBUG_TO_SERIAL("  - PUT /api/servo/center (center all)");
    });
    
    // API: Stop all servos at /api/servo/stop_all
    server->on("/api/servo/stop_all", HTTP_PUT, [server]() {
        bool success = true;
        for (uint8_t i = 0; i < 8; i++) {
            if (!ServoHandler_stopServo(i)) {
                success = false;
            }
        }
        
        if (success) {
            server->send(200, "application/json", "{\"result\":\"ok\",\"message\":\"All servos stopped\"}");
        } else {
            server->send(500, "application/json", "{\"result\":\"error\",\"message\":\"Failed to stop servos\"}");
        }
        DEBUG_TO_SERIAL("  - PUT /api/servo/stop_all (stop all)");
    });

    
    

    
    
    return true;
}