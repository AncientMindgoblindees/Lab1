#include <Arduino.h>
#include "server.h"
#include "hardware.h"

/*================================================================
+
+=  Simple WiFi Access Point + Web Server
+
+ Codes: 200 (OK), 404 (Not Found), 204 (No Content), 400 (Bad Request/Command Error) 
+
+===============================================================*/

// Global hardware manager instance
HardwareManager hardwareManager;

// FreeRTOS task for server
void serverTask(void * parameter) {
  for(;;) {
    server_loop();
    vTaskDelay(1);
  }
}

// FreeRTOS task for hardware
void hardwareTask(void * parameter) {
  for(;;) {
    hardwareManager.update();
    taskYIELD(); // Yield to other tasks but run as fast as possible
  }
}

void setup() {
  Serial.begin(9600);
  interrupts(); // Enable interrupts
  
  // Initialize hardware manager
  hardwareManager.begin();
  
  // Initialize server and pass hardware manager pointer
  server_setup(&hardwareManager);
  // Create server task on core 0
  xTaskCreatePinnedToCore(
    serverTask,       /* Task function */
    "Server Task",    /* Name of task */
    8192,            /* Stack size */
    NULL,            /* Parameter */
    1,               /* Priority */
    NULL,            /* Task handle */
    0);              /* Core 0 */
  
  // Create hardware task on core 1
  xTaskCreatePinnedToCore(
    hardwareTask,     /* Task function */
    "Hardware Task",  /* Name of task */
    4096,            /* Stack size */
    NULL,            /* Parameter */
    1,               /* Priority */
    NULL,            /* Task handle */
    1);              /* Core 1 */
}

void loop() {
  // Empty - all logic in FreeRTOS tasks
}