

#ifndef SERVER_H
#define SERVER_H

#include <WebServer.h>



extern bool RequestFlag;
extern const char* ssid;
extern const char* password;
extern IPAddress local_ip;
extern IPAddress gateway;
extern IPAddress subnet;
extern WebServer server;


void server_setup();
void server_loop();
void handle_request();
void handle_NotFound();
void handle_BadCommand();
void handle_OnConnect();
void handle_tempRequest();


#endif // SERVER_H
