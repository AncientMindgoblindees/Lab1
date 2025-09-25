#include "server.h"
#include <WiFi.h>



void server_setup() {
    WiFi.softAP(ssid, password);
    WiFi.softAPConfig(local_ip, gateway, subnet);
    delay(100);
    server.on("/", handle_OnConnect);
    server.on("/data", HTTP_GET, handle_request);
	server.on("/temp", HTTP_GET, handle_tempRequest);
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
	server.send(200, "text/html", "<!DOCTYPE html><html><body><h1>ESP32 Web Server</h1><p>Backend Active</p></body></html>");
}

void handle_request(){
    //temp1
    //temp2 
	server.sendHeader("Access-Control-Allow-Origin", "*");
	RequestFlag = true;
    server.send(200, "application/json", "{\"Sending Data\":" + String(RequestFlag) + "}");
	//server.send(200, "text/plain", "Sending Data");
}

void handle_NotFound() {
	server.send(404, "text/plain", "Not found");
}

void handle_BadCommand() {
	server.send(400, "text/plain", "Bad Request");
}

void handle_tempRequest() {
	// Placeholder temperature data; replace with actual sensor readings
	float temperature = 25.0; // Example temperature value
	server.sendHeader("Access-Control-Allow-Origin", "*");
	server.send(200, "application/json", "{\"temp\":" + String(temperature) + "}");
}
