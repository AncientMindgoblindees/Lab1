#include "hardware.h"
#include <Wire.h>
#define DEVICE_DISCONNECTED_F -196.6
// Global pointer for ISR access
HardwareManager* g_hardwareManager = nullptr;
// ISR wrapper functions
void IRAM_ATTR button25_ISR() {
  if (g_hardwareManager) {
    g_hardwareManager->handleButton1Press();
  }
}

void IRAM_ATTR button26_ISR() {
  if (g_hardwareManager) {
    g_hardwareManager->handleButton2Press();
  }
}

HardwareManager::HardwareManager() 
  : display(nullptr),
    oneWire(nullptr),
    sensors(nullptr),
    sensor1_active(false),
    sensor2_active(false),
    cachedTemp1(-127.0),
    cachedTemp2(-127.0),
    tempType(0),
    button1{GPIO_PIN25, false},
    button2{GPIO_PIN26, false},
    buttonQueue(nullptr),
    lastOledUpdate(0),
    lastServerDataUpdate(0),
    lastSensorScan(0),
    tempChange(false)
{
  // Set global pointer for ISR access
  g_hardwareManager = this;
}

void HardwareManager::begin() {
  Serial.println("Starting hardware setup...");
  
  // Initialize I2C
  Wire.begin(21, 22); // SDA=21, SCL=22 for ESP32
  Wire.setClock(400000);
  Serial.println("I2C initialized on pins 21(SDA), 22(SCL)");
  
  // Scan I2C bus
  scanI2CBus();
  
  // Initialize components
  initializeButtons();
  initializeSensors();
  initializeDisplay();
  
  Serial.println("Hardware setup complete!");
}

void HardwareManager::scanI2CBus() {
  Serial.println("Scanning I2C bus...");
  byte error, address;
  int nDevices = 0;
  
  for(address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    
    if (error == 0) {
      Serial.print("I2C device found at address 0x");
      if (address < 16) Serial.print("0");
      Serial.print(address, HEX);
      Serial.println();
      nDevices++;
    }
  }
  
  if (nDevices == 0) {
    Serial.println("No I2C devices found!");
    Serial.println("Check wiring:");
    Serial.println("- SDA to GPIO 21");
    Serial.println("- SCL to GPIO 22"); 
    Serial.println("- VCC to 3.3V");
    Serial.println("- GND to GND");
  } else {
    Serial.print(nDevices);
    Serial.println(" I2C device(s) found");
  }
}

void HardwareManager::initializeButtons() {
  pinMode(button1.PIN, INPUT_PULLUP);
  pinMode(button2.PIN, INPUT_PULLUP);
  buttonQueue = xQueueCreate(20, sizeof(int));
  attachInterrupt(digitalPinToInterrupt(button1.PIN), button25_ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(button2.PIN), button26_ISR, FALLING);
  Serial.println("Buttons configured");
}

void HardwareManager::initializeSensors() {
  oneWire = new OneWire(ONE_WIRE_BUS);
  sensors = new DallasTemperature(oneWire);
  
  sensors->begin();
  Serial.print("Found ");
  Serial.print(sensors->getDeviceCount());
  Serial.println(" temperature sensors");
  
  if (sensors->getAddress(sensor1Addr, 0)) {
    Serial.println("Sensor 1 found!");
    sensor1_active = false; // Start inactive, user must enable
  } else {
    Serial.println("Sensor 1 not found.");
    sensor1_active = false;
  }

  if (sensors->getAddress(sensor2Addr, 1)) {
    Serial.println("Sensor 2 found!");
    sensor2_active = false; // Start inactive, user must enable
  } else {
    Serial.println("Sensor 2 not found.");
    sensor2_active = false;
  }
}

void HardwareManager::initializeDisplay() {
  Serial.println("Initializing OLED display...");
  
  display = new Adafruit_SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
  
  Serial.print("Attempting address 0x3C...");
  
  if(!display->begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(" FAILED");
    Serial.print("Attempting address 0x3D...");
    
    if(!display->begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
      Serial.println(" FAILED");
      Serial.println("Display initialization failed completely!");
      Serial.println("Possible issues:");
      Serial.println("1. Wrong I2C address (try I2C scanner)");
      Serial.println("2. Bad wiring");
      Serial.println("3. Insufficient power");
      Serial.println("4. Faulty display");
    } else {
      Serial.println(" SUCCESS on 0x3D");
    }
  } else {
    Serial.println(" SUCCESS on 0x3C");
  }
  
  // Initial display update
  updateDisplay();
}

void HardwareManager::update() {
  int btn;
  bool buttonPressed = false;
  
  // Drain all button presses from queue quickly
  // This processes all pending button presses in one go
  while(xQueueReceive(buttonQueue, &btn, 0)) {
    buttonPressed = true;
    // Don't need to do anything with btn value
    // The ISR already toggled the sensor_active flags
  }
  
  // Get current time once to avoid multiple millis() calls
  uint32_t current = millis();
  // Update sensor readings periodically
  if(tempChange || current - lastServerDataUpdate > SENSOR_UPDATE_INTERVAL) {
    lastServerDataUpdate = current;
    tempChange = false;
    readSensorValues();
  }
  // Update display immediately if button was pressed OR periodically
  if(buttonPressed || (current - lastOledUpdate > OLED_UPDATE_INTERVAL)) {
    lastOledUpdate = current;
    updateDisplay();
  }
  
  

  // Auto-rescan for new sensors only when both are inactive
  bool needsRescan = (!sensor1_active && !sensor2_active) ||
                   (!sensor1_active && !hasSensorAddress(sensor1Addr)) ||
                   (!sensor2_active && !hasSensorAddress(sensor2Addr));

  if(needsRescan) {
    if(current - lastSensorScan > SENSOR_RESCAN_INTERVAL) {
      lastSensorScan = current;
      rescanSensors();
    }
  }
}
bool HardwareManager::isTempValid(float temp) const {
  // Check if temperature is valid (not disconnected)
  if (tempType == 1) { // Fahrenheit
    return (temp > DEVICE_DISCONNECTED_F + 1.0); // Add small margin
  } else { // Celsius
    return (temp > DEVICE_DISCONNECTED_C + 1.0); // Add small margin
  }
}


void HardwareManager::updateDisplay() {
  if (!display) return;

  display->clearDisplay();
  display->setTextSize(1);
  display->setTextColor(SSD1306_WHITE);
  display->setCursor(0, 0);

  // Sensor 1
  if (sensor1_active) {
    float t1 = readSensor(1);
    if (!isTempValid(t1)) {
      display->println("Sensor 1 Error");
      display->println("Check Conn.");
    } else {
      display->print("Sensor 1: ");
      display->print(t1, 1);
      display->println(tempType == 1 ? " F" : " C");
    }
  } else {
    display->println("Sensor 1 off");
  }

  display->println(); // small spacing

  // Sensor 2
  if (sensor2_active) {
    float t2 = readSensor(2);
    if (!isTempValid(t2)) {
      display->println("Sensor 2 Error");
      display->println("Check Conn.");
    } else {
      display->print("Sensor 2: ");
      display->print(t2, 1);
      display->println(tempType == 1 ? " F" : " C");
    }
  } else {
    display->println("Sensor 2 off");
  }

  display->display();
}

void HardwareManager::readSensorValues() {
  sensors->requestTemperatures(); // Always request temps

  // Always update cache, regardless of active flags
  if(tempType == 1) { // Fahrenheit
    cachedTemp1 = sensors->getTempF(sensor1Addr);
    cachedTemp2 = sensors->getTempF(sensor2Addr);
  } else { // Celsius
    cachedTemp1 = sensors->getTempC(sensor1Addr);
    cachedTemp2 = sensors->getTempC(sensor2Addr);
  }
}
float HardwareManager::readSensor(int sensorNum) {
  // Return cached values instead of querying sensors directly
  if (sensorNum == 1) {
    return cachedTemp1;
  } else if (sensorNum == 2) {
    return cachedTemp2;
  }
  return tempType == 1 ? DEVICE_DISCONNECTED_F : DEVICE_DISCONNECTED_C;
}

void HardwareManager::handleButton1Press() {
    sensor1_active = !sensor1_active;
    
    static const int btn = GPIO_PIN25;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE; 
    xQueueSendFromISR(buttonQueue, &btn, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
}

void HardwareManager::handleButton2Press() {
  unsigned long currentTime = millis();
    sensor2_active = !sensor2_active;
    static const int btn = GPIO_PIN26;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(buttonQueue, &btn, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
}

void HardwareManager::rescanSensors() {
  Serial.println("\n=== Rescanning Temperature Sensors ===");
  
  // Re-initialize the sensor library
  sensors->begin();
  
  int deviceCount = sensors->getDeviceCount();
  Serial.print("Found ");
  Serial.print(deviceCount);
  Serial.println(" temperature sensor(s)");
  
  // Store previous states
  bool prevSensor1Active = sensor1_active;
  bool prevSensor2Active = sensor2_active;
  
  // Try to get sensor 1 address
  if (sensors->getAddress(sensor1Addr, 0)) {
    Serial.print("Sensor 1: Found at index 0 - Address: ");
    for (uint8_t i = 0; i < 8; i++) {
      if (sensor1Addr[i] < 16) Serial.print("0");
      Serial.print(sensor1Addr[i], HEX);
    }
    Serial.println();
    sensor1_active = prevSensor1Active;
  } else {
    Serial.println("Sensor 1: Not found");
    sensor1_active = false;
  }
  
  // Try to get sensor 2 address
  if (sensors->getAddress(sensor2Addr, 1)) {
    Serial.print("Sensor 2: Found at index 1 - Address: ");
    for (uint8_t i = 0; i < 8; i++) {
      if (sensor2Addr[i] < 16) Serial.print("0");
      Serial.print(sensor2Addr[i], HEX);
    }
    Serial.println();
    sensor2_active = prevSensor2Active;
  } else {
    Serial.println("Sensor 2: Not found");
    sensor2_active = false;
  }
  
  Serial.println("Rescan complete!");
  Serial.println("=====================================\n");
  
  updateDisplay();
}
// In hardware.cpp, add this helper function:
bool HardwareManager::hasSensorAddress(const DeviceAddress addr) const {
  // Check if address is all zeros (invalid)
  for(uint8_t i = 0; i < 8; i++) {
    if(addr[i] != 0) return true;
  }
  return false;
}