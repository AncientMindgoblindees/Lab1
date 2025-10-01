#ifndef HARDWARE_H
#define HARDWARE_H

#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include <DallasTemperature.h>
#include <OneWire.h>

// Button configuration structure
struct Button {
  const uint8_t PIN;
  bool pressed;
  unsigned long lastDebounceTime;
  unsigned long debounceDelay;
};

class HardwareManager {
public:
  // Constructor
  HardwareManager();
  
  // Initialization
  void begin();
  
  // Main task loop (call this from FreeRTOS task)
  void update();
  
  // Button handlers (ISR functions)
  void handleButton1Press();
  void handleButton2Press();
  
  // Sensor management
  float readSensor(int sensorNum);
  bool isSensor1Active() const { return sensor1_active; }
  bool isSensor2Active() const { return sensor2_active; }
  void setSensor1Active(bool active) { sensor1_active = active; }
  void setSensor2Active(bool active) { sensor2_active = active; }
  
  // Queue access for ISR
  QueueHandle_t getButtonQueue() { return buttonQueue; }
  float getCachedSensor1Temp() const { return cachedTemp1; }
  float getCachedSensor2Temp() const { return cachedTemp2; }
  int getTempType() const { return tempType; }
  void toggleTempType() { tempType = (tempType == 0) ? 1 : 0; } // Toggle between 0 and 1
private:
  // Display management
  void updateDisplay();
  void initializeDisplay();
  void scanI2CBus();
  
  // Sensor management
  void initializeSensors();
  void readSensorValues();
  
  // Button management
  void initializeButtons();
  //void processButtonPress(int buttonPin);
  
  // Hardware objects
  Adafruit_SSD1306* display;
  OneWire* oneWire;
  DallasTemperature* sensors;
  
  // Sensor state
  DeviceAddress sensor1Addr;
  DeviceAddress sensor2Addr;
  bool sensor1_active;
  bool sensor2_active;
  volatile float cachedTemp1;
  volatile float cachedTemp2;
  volatile int tempType;
  // Button state
  Button button1;
  Button button2;
  QueueHandle_t buttonQueue;
  
  // Timing
  uint32_t lastOledUpdate;
  uint32_t lastServerDataUpdate;
  
  // Configuration constants
  static const uint8_t ONE_WIRE_BUS = 4;
  static const uint8_t SCREEN_WIDTH = 128;
  static const uint8_t SCREEN_HEIGHT = 32;
  static const uint8_t SCREEN_ADDRESS = 0x3C;
  static const int8_t OLED_RESET = -1;
  static const uint8_t GPIO_PIN25 = 25;
  static const uint8_t GPIO_PIN26 = 26;
  static const uint16_t OLED_UPDATE_INTERVAL = 20;
  static const uint16_t SENSOR_UPDATE_INTERVAL = 20;
};

// Global instance pointer for ISR access
extern HardwareManager* g_hardwareManager;

// ISR wrapper functions
void IRAM_ATTR button25_ISR();
void IRAM_ATTR button26_ISR();

#endif