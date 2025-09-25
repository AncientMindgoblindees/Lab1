#include "hardware.h"
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <OneWire.h>
#include <DallasTemperature.h>

struct Button {
  const uint8_t PIN;
  bool pressed;
};

// Task that runs via FreeRTOS to handle hardware related tasks; buttons, sensors, display updates, etc.
// Can run independently of the main loop and server tasks since it is on its own core (ESP32 dual core)
void hardwareTask(void * parameter) {
  int btn;
  uint32_t lastOledUpdate = millis();
  uint32_t lastServerDataUpdate = millis();
  float sensor1_value;
  float sensor2_value;
  //int sensorActive; // 1 = sensor1, 2 = sensor2 
  
  for(;;) {
    if(xQueueReceive(buttonQueue, &btn, portMAX_DELAY)) {
      Serial.print("Button on pin ");
      Serial.print(btn);
      Serial.println(" pressed!");
    }
    if(millis() - lastOledUpdate > 1000 || xQueueReceive(buttonQueue, &btn, portMAX_DELAY)) { // Update OLED every 1 seconds or also on button press to update OLED immediately
      lastOledUpdate = millis();
      testdrawchar(); //draws on screen every second for demo purposes
      if (sensor1_active && sensor2_active)
      {
        //Average out 

      }
      else if (sensor1_active) {
        //sensor1_value = readSensor(sensorActive); // Simulate sensor 1 data change
        //updateOLED(sensor1_value);
      } else if (sensor2_active) {
        //sensor2_value = readSensor(sensorActive); // Simulate sensor 2 data change
        //updateOLED(sensor2_value);
      }
      else {
        //Do nothing
      }
    }
    // Placeholder for hardware-related tasks
    vTaskDelay(1);
  }
}


void hardware_setup(){

  //Set up buttons with interrupts
  pinMode(button1.PIN, INPUT_PULLUP); //Will read a HIGH to Low transition
  pinMode(button2.PIN, INPUT_PULLUP);
  buttonQueue = xQueueCreate(10, sizeof(int)); // up to 10 button events, shovels events into queue during interrupts to be processed in task
  attachInterrupt(button1.PIN, button18_pressed, FALLING);
  attachInterrupt(button2.PIN, button19_pressed, FALLING);

  // Set up Sensors DallasTemperature and OneWire. Detect what sensors are present at start up
  sensors.begin();
  if (sensors.getAddress(sensor1Addr, 0)) {
    Serial.println("Sensor 1 found!");
  } else {
    Serial.println("Sensor 1 not found.");
  }

  if (sensors.getAddress(sensor2Addr, 1)) {
    Serial.println("Sensor 2 found!");
  } else {
    Serial.println("Sensor 2 not found.");
  }
  // Set up display, OLED initialization

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
  }
  
  display.clearDisplay(); //clear display for first use
  display.display();
}

void IRAM_ATTR button25_pressed() {
  // Puts the function in fast internal ram to ensure quick response
  // then pushes the button pin number to the queue (i.e. which button was pressed)
  // lets the scheduler immediately wake up the tasks thats waiting on the queue (FreeRTOS)
  sensor1_active = !sensor1_active; // Toggle sensor 1 active state
  static const int btn = GPIO_PIN25;
  BaseType_t xHigherPriorityTaskWoken = pdFALSE; 
  xQueueSendFromISR(buttonQueue, &btn, &xHigherPriorityTaskWoken);
  if (xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
}
void IRAM_ATTR button26_pressed() {
    // Handle button press interrupt
    // This is just a placeholder; actual implementation may vary
    sensor2_active = !sensor2_active; // Toggle sensor 1 active state
    static const int btn = GPIO_PIN26;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(buttonQueue, &btn, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
}

void updateOLED(float data) {
  // Placeholder function to update OLED display with new data
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("Updated Data" + String(data));
  display.display();
}

//tesyt function to draw on the OLED
void testdrawchar(void) {
  display.clearDisplay();

  display.setTextSize(3);      // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(0, 0);     // Start at top-left corner
  display.cp437(true);         // Use full 256 char 'Code Page 437' font

  // Not all the characters will fit on the display. This is normal.
  // Library will draw what it can and the rest will be clipped.
  display.print("Hello World!");
  
  display.display();
  delay(2000);
}


//Generalized Sensor state change function since you must be able to select sensors used via client web interface
void changeSensorState(int sensorNum) {
  if (sensorNum == 1) {
    sensor1_active = !sensor1_active;
  } else if (sensorNum == 2) {
    sensor2_active = !sensor2_active;
  }
}


// Rescan bus and update the address for a given index
void rescanAndUpdate(DeviceAddress addr, uint8_t index) {
  sensors.begin(); // reinitialize
  if (sensors.getAddress(addr, index)) {
    Serial.print("Sensor re-found at index ");
    Serial.println(index);
  } else {
    Serial.print("Sensor still not found at index ");
    Serial.println(index);
  }
}
bool isSensorConnected(DeviceAddress addr) {
  // The Dallas library has no direct "connected" check,
  // so you check if the address still exists in the bus.
  DeviceAddress tempAddr;
  for (uint8_t i = 0; i < sensors.getDeviceCount(); i++) {
    if (sensors.getAddress(tempAddr, i)) {
      if (memcmp(tempAddr, addr, sizeof(DeviceAddress)) == 0) {
        return true;
      }
    }
  }
  return false;
}