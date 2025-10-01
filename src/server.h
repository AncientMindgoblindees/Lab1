#ifndef SERVER_H
#define SERVER_H

#include <WebServer.h>
#include <WiFi.h>

// Forward declaration
class HardwareManager;

// Main server functions
void server_setup(HardwareManager* hwManager);
void server_loop();

// HTTP request handlers
void handle_OnConnect();
void handle_request();
void handle_tempRequest();
void handle_sensorStatus();
void handle_toggleSensor();
void handle_NotFound();
void handle_BadCommand();

#endif