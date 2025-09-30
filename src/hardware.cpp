#include "hardware.h"

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Add these variables near the top with your other button declarations
struct Button {
  const uint8_t PIN;
  bool pressed;
  unsigned long lastDebounceTime;  // Add this
  unsigned long debounceDelay;     // Changed from const
};
Button button1 = {GPIO_PIN25, false, 0, 200};
Button button2 = {GPIO_PIN26, false, 0, 200};

//Sensor Parameters
#define ONE_WIRE_BUS 4 // GPIO where the sensors are connected
OneWire oneWire_in(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire_in);
DeviceAddress sensor1Addr, sensor2Addr;
bool sensor1_active = false;
bool sensor2_active = false;

// OLED display parameters
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address;  0x3C for 128x32
#define NUMFLAKES     10 // Number of snowflakes in the animation example
#define LOGO_HEIGHT   16
#define LOGO_WIDTH    16
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

//Button Parameters
QueueHandle_t buttonQueue;

static const unsigned char PROGMEM logo_bmp[] =
{ 0b00000000, 0b11000000,
  0b00000001, 0b11000000,
  0b00000001, 0b11000000,
  0b00000011, 0b11100000,
  0b11110011, 0b11100000,
  0b11111110, 0b11111000,
  0b01111110, 0b11111111,
  0b00110011, 0b10011111,
  0b00011111, 0b11111100,
  0b00001101, 0b01110000,
  0b00011011, 0b10100000,
  0b00111111, 0b11100000,
  0b00111111, 0b11110000,
  0b01111100, 0b11110000,
  0b01110000, 0b01110000,
  0b00000000, 0b00110000 };

// Task that runs via FreeRTOS to handle hardware related tasks
void hardwareTask(void * parameter) {
  int btn;
  uint32_t lastOledUpdate = millis();
  uint32_t lastServerDataUpdate = millis();
  float sensor1_value = 0.0;
  float sensor2_value = 0.0;
  
  
  for(;;) {
    // Check for button presses (non-blocking)
    if(xQueueReceive(buttonQueue, &btn, 0)) {
      Serial.print("Button on pin ");
      Serial.print(btn);
      Serial.println(" pressed!");
      
      // Update display immediately when button is pressed
      updateDisplayBasedOnSensors();
    }
    else{
      // Update OLED every 1 seconds
      if(millis() - lastOledUpdate > 1000) {
        lastOledUpdate = millis();
        updateDisplayBasedOnSensors();
      }
    }
    // Update sensor readings every 1 seconds
      if(millis() - lastServerDataUpdate > 1000) {
        lastServerDataUpdate = millis();
        readSensorValues();
      }
    vTaskDelay(10 / portTICK_PERIOD_MS); // Delay 10ms instead of 1ms
  }
}

void updateDisplayBasedOnSensors() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  
  if (sensor1_active && sensor2_active) {
    // Both sensors active - show average
    float t1 = readSensor(1);
    float t2 = readSensor(2);
    if (t1 == DEVICE_DISCONNECTED_C || t2 == DEVICE_DISCONNECTED_C) {
      display.println("Sensor Error");
      display.println("Check Conn.");
    }
    else{
      float avg = (t1 + t2) / 2.0;
      display.println("Both Sensors:");
      display.print("Avg: ");
      display.print(avg, 1);
      display.println(" C"); //Need to add function for changing the unit (F -> C)
    }
  }
  else if (sensor1_active) {
    // Only sensor 1 active
    float temp1 = readSensor(1);
    if(temp1 == DEVICE_DISCONNECTED_C) {
      display.println("Sensor 1 Error");
      display.println("Check Conn.");
    }
    else{
      display.println("Sensor 1:");
      display.print("Temp: ");
      display.print(temp1, 1);
      display.println(" C");
    }
  } 
  else if (sensor2_active) {
    // Only sensor 2 active
    float temp2 = readSensor(2);
    if (temp2 == DEVICE_DISCONNECTED_C) {
      display.println("Sensor 2 Error");
      display.println("Check Conn.");
    }
    else{
      display.println("Sensor 2:");
      display.print("Temp: ");
      display.print(temp2, 1);
      display.println(" C");
    }
  } 
  else {
    // No sensors active
    display.println("No Sensors");
    display.println("Active");
    display.println("Press button");
    display.println("to activate");
  }
  
  display.display(); // Actually update the screen
}

void readSensorValues() {
  if (sensor1_active || sensor2_active) {
    sensors.requestTemperatures(); // Request readings from all sensors
  }
}

float readSensor(int sensorNum) {
  if (sensorNum == 1 && sensor1_active) {
    return sensors.getTempC(sensor1Addr);
  } else if (sensorNum == 2 && sensor2_active) {
    return sensors.getTempC(sensor2Addr);
  }
  return -127.0; // Error value
}

void hardware_setup(){
  Serial.println("Starting hardware setup...");
  
  // Initialize I2C with explicit pins
  Wire.begin(21, 22); // SDA=21, SCL=22 for ESP32
  Wire.setClock(100000); // Set I2C clock to 100kHz (slower, more reliable)
  
  Serial.println("I2C initialized on pins 21(SDA), 22(SCL)");
  
  // Scan I2C bus to see what's connected
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
  
  //Set up buttons with interrupts
  pinMode(button1.PIN, INPUT_PULLUP);
  pinMode(button2.PIN, INPUT_PULLUP);
  buttonQueue = xQueueCreate(10, sizeof(int));
  attachInterrupt(digitalPinToInterrupt(button1.PIN), button25_pressed, FALLING);
  attachInterrupt(digitalPinToInterrupt(button2.PIN), button26_pressed, FALLING);
  Serial.println("Buttons configured");

  // Set up Sensors
  sensors.begin();
  Serial.print("Found ");
  Serial.print(sensors.getDeviceCount());
  Serial.println(" temperature sensors");
  
  if (sensors.getAddress(sensor1Addr, 0)) {
    Serial.println("Sensor 1 found!");
  } else {
    Serial.println("Sensor 1 not found.");
    sensor1_active = false;
  }

  if (sensors.getAddress(sensor2Addr, 1)) {
    Serial.println("Sensor 2 found!");
  } else {
    Serial.println("Sensor 2 not found.");
    sensor2_active = false;
  }

  // Set up display with enhanced error checking
  Serial.println("Initializing OLED display...");
  Serial.print("Attempting address 0x3C...");
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(" FAILED");
    Serial.print("Attempting address 0x3D...");
    
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
      Serial.println(" FAILED");
      Serial.println("Display initialization failed completely!");
      Serial.println("Possible issues:");
      Serial.println("1. Wrong I2C address (try I2C scanner)");
      Serial.println("2. Bad wiring");
      Serial.println("3. Insufficient power");
      Serial.println("4. Faulty display");
      
      // Continue anyway for other hardware
    } else {
      Serial.println(" SUCCESS on 0x3D");
    }
  } else {
    Serial.println(" SUCCESS on 0x3C");
  }
  
  Serial.println("Hardware setup complete!");
}



// Replace your existing ISR functions with these debounced versions:

void IRAM_ATTR button25_pressed() {
  unsigned long currentTime = millis();
  
  // Check if enough time has passed since last trigger
  if (currentTime - button1.lastDebounceTime > button1.debounceDelay) {
    button1.lastDebounceTime = currentTime;
    sensor1_active = !sensor1_active;
    
    static const int btn = GPIO_PIN25;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE; 
    xQueueSendFromISR(buttonQueue, &btn, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
  }
}

void IRAM_ATTR button26_pressed() {
  unsigned long currentTime = millis();
  
  // Check if enough time has passed since last trigger
  if (currentTime - button2.lastDebounceTime > button2.debounceDelay) {
    button2.lastDebounceTime = currentTime;
    sensor2_active = !sensor2_active;
    
    static const int btn = GPIO_PIN26;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(buttonQueue, &btn, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
  }
}
