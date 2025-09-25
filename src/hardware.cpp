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
Button button1 = {GPIO_PIN25, false};
Button button2 = {GPIO_PIN26, false};

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
  
  // Initial display update
  testdrawchar();
  
  for(;;) {
    // Check for button presses (non-blocking)
    if(xQueueReceive(buttonQueue, &btn, 0)) {
      Serial.print("Button on pin ");
      Serial.print(btn);
      Serial.println(" pressed!");
      
      // Update display immediately when button is pressed
      updateDisplayBasedOnSensors();
    }
    
    // Update OLED every 2 seconds
    if(millis() - lastOledUpdate > 2000) {
      lastOledUpdate = millis();
      updateDisplayBasedOnSensors();
    }
    
    // Update sensor readings every 5 seconds
    if(millis() - lastServerDataUpdate > 5000) {
      lastServerDataUpdate = millis();
      readSensorValues();
    }
    
    vTaskDelay(100 / portTICK_PERIOD_MS); // Delay 100ms instead of 1ms
  }
}

void updateDisplayBasedOnSensors() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  
  if (sensor1_active && sensor2_active) {
    // Both sensors active - show average
    float avg = (readSensor(1) + readSensor(2)) / 2.0;
    display.println("Both Sensors:");
    display.print("Avg: ");
    display.print(avg, 1);
    display.println(" C");
  } else if (sensor1_active) {
    // Only sensor 1 active
    float temp1 = readSensor(1);
    display.println("Sensor 1:");
    display.print("Temp: ");
    display.print(temp1, 1);
    display.println(" C");
  } else if (sensor2_active) {
    // Only sensor 2 active
    float temp2 = readSensor(2);
    display.println("Sensor 2:");
    display.print("Temp: ");
    display.print(temp2, 1);
    display.println(" C");
  } else {
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
  return -999.0; // Error value
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
      displayDebugTest(); // Run debug test
    }
  } else {
    Serial.println(" SUCCESS on 0x3C");
    displayDebugTest(); // Run debug test
  }
  
  Serial.println("Hardware setup complete!");
}

void IRAM_ATTR button25_pressed() {
  sensor1_active = !sensor1_active;
  static const int btn = GPIO_PIN25;
  BaseType_t xHigherPriorityTaskWoken = pdFALSE; 
  xQueueSendFromISR(buttonQueue, &btn, &xHigherPriorityTaskWoken);
  if (xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
}

void IRAM_ATTR button26_pressed() {
  sensor2_active = !sensor2_active;
  static const int btn = GPIO_PIN26;
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  xQueueSendFromISR(buttonQueue, &btn, &xHigherPriorityTaskWoken);
  if (xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
}

void updateOLED(float data) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.print("Data: ");
  display.println(data, 1);
  display.display();
}

void testdrawchar(void) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.cp437(true);
  display.println("Hello!");
  display.display();
}

void changeSensorState(int sensorNum) {
  if (sensorNum == 1) {
    sensor1_active = !sensor1_active;
  } else if (sensorNum == 2) {
    sensor2_active = !sensor2_active;
  }
}

void rescanAndUpdate(DeviceAddress addr, uint8_t index) {
  sensors.begin();
  if (sensors.getAddress(addr, index)) {
    Serial.print("Sensor re-found at index ");
    Serial.println(index);
  } else {
    Serial.print("Sensor still not found at index ");
    Serial.println(index);
  }
}

bool isSensorConnected(DeviceAddress addr) {
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

void displayDebugTest() {
  Serial.println("Starting display debug test...");
  
  // Test 1: Basic pixel test
  display.clearDisplay();
  display.drawPixel(10, 10, SSD1306_WHITE);
  display.display();
  Serial.println("Test 1: Single pixel drawn");
  delay(2000);
  
  // Test 2: Fill screen
  display.fillScreen(SSD1306_WHITE);
  display.display();
  Serial.println("Test 2: Screen filled white");
  delay(2000);
  
  // Test 3: Clear and draw rectangle
  display.clearDisplay();
  display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
  display.display();
  Serial.println("Test 3: Border rectangle drawn");
  delay(2000);
  
  // Test 4: Text at different positions
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Line 1");
  display.setCursor(0, 10);
  display.println("Line 2");
  display.setCursor(0, 20);
  display.println("Line 3");
  display.display();
  Serial.println("Test 4: Multi-line text drawn");
  delay(2000);
  
  // Test 5: Large text
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("BIG TEXT");
  display.display();
  Serial.println("Test 5: Large text drawn");
  delay(2000);
  
  // Test 6: Invert display
  display.invertDisplay(true);
  Serial.println("Test 6: Display inverted");
  delay(2000);
  display.invertDisplay(false);
  Serial.println("Test 6: Display normal");
  
  Serial.println("Display debug test complete!");
}
