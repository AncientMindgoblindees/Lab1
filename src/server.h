

#ifndef SERVER_H
#define SERVER_H

#include <WebServer.h>



//extern bool RequestFlag;
//extern const char* ssid;
//extern const char* password;
//extern IPAddress local_ip;
//extern IPAddress gateway;
//extern IPAddress subnet;
//extern WebServer server;


void server_setup();
void server_loop();
void handle_request();
void handle_NotFound();
void handle_BadCommand();
void handle_OnConnect();
void handle_tempRequest();

bool RequestFlag = false;
const char* ssid = "ESP32TEST";
const char* password = "12345678";
const int GPIO_PIN19 = 19;
const int GPIO_PIN18 = 18;
IPAddress local_ip(192,168,1,1);
IPAddress gateway(192,168,1,1);
IPAddress subnet(255,255,255,0);
WebServer server(80);
#endif // SERVER_H
