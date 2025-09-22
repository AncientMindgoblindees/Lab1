// ...existing code...
#include "server.h"
#include "hardware.h"
/*================================================================
+
+=  Simple WiFi Access Point + Web Server
+
+ Codes: 200 (OK), 404 (Not Found), 204 (No Content), 400 (Bad Request/Command Error) 
+
+===============================================================*/

// ...existing code...
void serverTask(void * parameter) {
  for(;;) {
    server_loop();
    vTaskDelay(1);
  }
}

void setup() {
  Serial.begin(9600);
  interrupts(); //enable interrupts
  hardware_setup();
  server_setup();
  xTaskCreatePinnedToCore(
                serverTask,   /* Task function. */
                "Server Task", /* name of task. */
                8192,         /* Stack size of task */
                NULL,          /* parameter of the task */
                1,             /* priority of the task */
                NULL,          /* Task handle to keep track of created task */
                0);            /* pin task to core 0 */
  xTaskCreatePinnedToCore(
                hardwareTask,   /* Task function. */
                "Hardware Task", /* name of task. */
                4096,         /* Stack size of task */
                NULL,          /* parameter of the task */
                1,             /* priority of the task */
                NULL,          /* Task handle to keep track of created task */
                1);            /* pin task to core 1 */
  Serial.println("HTTP server started");
}
void loop() {
  // Server logic moved to server.cpp
  
}