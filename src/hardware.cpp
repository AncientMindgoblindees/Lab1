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
    tempType(1),
    button1{GPIO_PIN25, false, 0, 20},
    button2{GPIO_PIN26, false, 0, 20},
    buttonQueue(nullptr),
    lastOledUpdate(0),
    lastServerDataUpdate(0)
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
  
  // Check for button presses (non-blocking)
  if(xQueueReceive(buttonQueue, &btn, 0)) {
    //processButtonPress(btn);
    // Update display immediately when button is pressed
    updateDisplay();
  } else {
    // Update OLED periodically
    if(millis() - lastOledUpdate > OLED_UPDATE_INTERVAL) {
      lastOledUpdate = millis();
      updateDisplay();
    }
  }
  
  // Update sensor readings periodically
  if(millis() - lastServerDataUpdate > SENSOR_UPDATE_INTERVAL) {
    lastServerDataUpdate = millis();
    readSensorValues();
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
  /*if (!display) return;
  
  display->clearDisplay();
  display->setTextSize(1);
  display->setTextColor(SSD1306_WHITE);
  display->setCursor(0, 0);
  
  if (sensor1_active && sensor2_active) {
    // Both sensors active - show average
    float t1 = readSensor(1);
    float t2 = readSensor(2);
    if (t1 == DEVICE_DISCONNECTED_C || t2 == DEVICE_DISCONNECTED_C) {
      display->println("Sensor Error");
      display->println("Check Conn.");
    } else {
      float avg = (t1 + t2) / 2.0;
      display->println("Both Sensors:");
      display->print("Avg: ");
      display->print(avg, 1);
      if(tempType == 1) {
        display->println(" F");
      } else
        display->println(" C");
    }
  }
  else if (sensor1_active) {
    // Only sensor 1 active
    float temp1 = readSensor(1);
    if(temp1 == DEVICE_DISCONNECTED_C) {
      display->println("Sensor 1 Error");
      display->println("Check Conn.");
    } else {
      display->println("Sensor 1:");
      display->print("Temp: ");
      display->print(temp1, 1);
      if(tempType == 1) {
        display->println(" F");
      } else
        display->println(" C");
    }
  } 
  else if (sensor2_active) {
    // Only sensor 2 active
    float temp2 = readSensor(2);
    if (temp2 == DEVICE_DISCONNECTED_C) {
      display->println("Sensor 2 Error");
      display->println("Check Conn.");
    } else {
      display->println("Sensor 2:");
      display->print("Temp: ");
      display->print(temp2, 1);
      if(tempType == 1) {
        display->println(" F");
      } else
        display->println(" C");
    }
  } 
  else {
    // No sensors active
    display->println("No Sensors");
    display->println("Active");
    display->println("Press button");
    display->println("to activate");
  }
  
  display->display();*/
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
  
  // Check if enough time has passed since last trigger
  //if (currentTime - button2.lastDebounceTime > button2.debounceDelay) {
    //button2.lastDebounceTime = currentTime;
    sensor2_active = !sensor2_active;
    
    static const int btn = GPIO_PIN26;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(buttonQueue, &btn, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
  //}
}