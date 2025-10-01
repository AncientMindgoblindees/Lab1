#include "server.h"
#include "hardware.h"
#include <WiFi.h>

bool RequestFlag = false;
const char* ssid = "ESP32TEST";
const char* password = "12345678";
const int GPIO_PIN19 = 19;
const int GPIO_PIN18 = 18;
IPAddress local_ip(192,168,1,1);
IPAddress gateway(192,168,1,1);
IPAddress subnet(255,255,255,0);
WebServer server(80);

// Global pointer to hardware manager
static HardwareManager* g_hwManager = nullptr;

void server_setup(HardwareManager* hwManager) {
    // Store the hardware manager pointer
    g_hwManager = hwManager;
    
    WiFi.softAP(ssid, password);
    WiFi.softAPConfig(local_ip, gateway, subnet);
    delay(100);
    
    server.on("/", handle_OnConnect);
    server.on("/data", HTTP_GET, handle_request);
    server.on("/temp", HTTP_GET, handle_tempRequest);
    server.on("/status", HTTP_GET, handle_sensorStatus);
    server.on("/toggle", HTTP_POST, handle_toggleSensor);
    server.on("/%", HTTP_POST, handle_BadCommand);
    server.onNotFound(handle_NotFound);
    server.begin();
    Serial.println("HTTP server started");
}

void server_loop() {
    server.handleClient();
    if (RequestFlag) {
        RequestFlag = false;
    }
}

void handle_OnConnect() {
    server.send(200, "text/html", 
        "<!DOCTYPE html><html><body>"
        "<h1>ESP32 Web Server</h1>"
        "<p>Backend Active</p>"
        "</body></html>");
}

void handle_request() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    RequestFlag = true;
    server.send(200, "application/json", 
        "{\"Sending Data\":" + String(RequestFlag) + "}");
}

void handle_tempRequest() {
    if (!g_hwManager) {
        server.send(500, "application/json", 
            "{\"error\":\"Hardware not initialized\"}");
        return;
    }
    
    server.sendHeader("Access-Control-Allow-Origin", "*");
    
    bool s1_active = g_hwManager->isSensor1Active();
    bool s2_active = g_hwManager->isSensor2Active();
    
    String json = "{";
    
    if (s1_active) {
        float temp1 = g_hwManager->readSensor(1);
        json += "\"sensor1\":" + String(temp1, 2);
        if (s2_active) json += ",";
    }
    
    if (s2_active) {
        float temp2 = g_hwManager->readSensor(2);
        json += "\"sensor2\":" + String(temp2, 2);
    }
    
    if (s1_active && s2_active) {
        float temp1 = g_hwManager->readSensor(1);
        float temp2 = g_hwManager->readSensor(2);
        float avg = (temp1 + temp2) / 2.0;
        json += ",\"average\":" + String(avg, 2);
    }
    
    if (!s1_active && !s2_active) {
        json += "\"message\":\"No sensors active\"";
    }
    
    json += "}";
    
    server.send(200, "application/json", json);
}

void handle_sensorStatus() {
    if (!g_hwManager) {
        server.send(500, "application/json", 
            "{\"error\":\"Hardware not initialized\"}");
        return;
    }
    
    server.sendHeader("Access-Control-Allow-Origin", "*");
    
    String json = "{";
    json += "\"sensor1_active\":" + String(g_hwManager->isSensor1Active() ? "true" : "false");
    json += ",\"sensor2_active\":" + String(g_hwManager->isSensor2Active() ? "true" : "false");
    json += "}";
    
    server.send(200, "application/json", json);
}

void handle_toggleSensor() {
    if (!g_hwManager) {
        server.send(500, "application/json", 
            "{\"error\":\"Hardware not initialized\"}");
        return;
    }
    
    server.sendHeader("Access-Control-Allow-Origin", "*");
    
    // Check for query parameters
    if (server.hasArg("sensor")) {
        String sensorNum = server.arg("sensor");
        
        if (sensorNum == "1") {
            bool newState = !g_hwManager->isSensor1Active();
            g_hwManager->setSensor1Active(newState);
            server.send(200, "application/json", 
                "{\"sensor1_active\":" + String(newState ? "true" : "false") + "}");
        }
        else if (sensorNum == "2") {
            bool newState = !g_hwManager->isSensor2Active();
            g_hwManager->setSensor2Active(newState);
            server.send(200, "application/json", 
                "{\"sensor2_active\":" + String(newState ? "true" : "false") + "}");
        }
        else {
            server.send(400, "application/json", 
                "{\"error\":\"Invalid sensor number. Use 1 or 2\"}");
        }
    }
    else {
        server.send(400, "application/json", 
            "{\"error\":\"Missing sensor parameter\"}");
    }
}

void handle_NotFound() {
    server.send(404, "text/plain", "Not found");
}

void handle_BadCommand() {
    server.send(400, "text/plain", "Bad Request");
}