#include <Arduino.h>
#ifndef HARDWARE_H
#define HARDWARE_H


void hardware_setup();
void IRAM_ATTR button18_pressed();
void IRAM_ATTR button19_pressed();
void hardwareTask(void * parameter);
void testdrawchar();
void updateOLED(float sensor_value);
bool isSensorConnected(DeviceAddress addr);
void rescanAndUpdate(DeviceAddress addr, uint8_t index);

//Sensor Parameters
#define ONE_WIRE_BUS 4 // GPIO where the sensors are connected
OneWire oneWire_in(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire_in);
DeviceAddress sensor1Addr, sensor2Addr;
bool sensor1_active = false;
bool sensor2_active = false;


// OLED display parameters
// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// The pins for I2C are defined by the Wire-library. 
// On an ESP32: 21(SDA), 22(SCL)

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
const int GPIO_PIN26=26;
const int GPIO_PIN25=25;
Button button1 = {GPIO_PIN25, false};
Button button2 = {GPIO_PIN26, false};
#endif