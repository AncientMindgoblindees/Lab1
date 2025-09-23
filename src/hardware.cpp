#include "hardware.h"
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// The pins for I2C are defined by the Wire-library. 
// On an ESP32: 21(SDA), 22(SCL)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address;  0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define NUMFLAKES     10 // Number of snowflakes in the animation example

#define LOGO_HEIGHT   16
#define LOGO_WIDTH    16
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


struct Button {
  const uint8_t PIN;
  bool pressed;
};
QueueHandle_t buttonQueue;
Button button1 = {GPIO_PIN18, false};
Button button2 = {GPIO_PIN19, false};


void hardwareTask(void * parameter) {
  int btn;
  uint32_t lastOledUpdate = millis();
  uint32_t lastServerDataUpdate = millis();
  float sensor1_value;
  float sensor2_value;
  int sensorActive; // 1 = sensor1, 2 = sensor2 
  for(;;) {
    if(xQueueReceive(buttonQueue, &btn, portMAX_DELAY)) {
      Serial.print("Button on pin ");
      Serial.print(btn);
      Serial.println(" pressed!");
    }
    if (btn == GPIO_PIN18) { //updates place holder sensorActive variable to simulate which sensor is data gathering
        sensorActive = 1; // Simulate sensor 1 active
      }
    if (btn == GPIO_PIN19){ //updates place holder sensorActive variable to simulate which sensor is data gathering
        sensorActive = 2; // Simulate sensor 2 active
      }
    if(millis() - lastOledUpdate > 1000 || xQueueReceive(buttonQueue, &btn, portMAX_DELAY)) { // Update OLED every 1 seconds or also on button press to update OLED immediately
      lastOledUpdate = millis();
      testdrawchar(); //draws on screen every second for demo purposes
      if (sensorActive == 1) {
        //sensor1_value = readSensor(sensorActive); // Simulate sensor 1 data change
        updateOLED(sensor1_value);
      } else if (sensorActive == 2) {
        //sensor2_value = readSensor(sensorActive); // Simulate sensor 2 data change
        updateOLED(sensor2_value);
      }
    }
    // Placeholder for hardware-related tasks
    vTaskDelay(1);
  }
}


void hardware_setup(){
  
  pinMode(button1.PIN, INPUT_PULLUP); //Will read a HIGH to Low transition
  pinMode(button2.PIN, INPUT_PULLUP);
  buttonQueue = xQueueCreate(10, sizeof(int)); // up to 10 button events, shovels events into queue during interrupts to be processed in task
  attachInterrupt(button1.PIN, button18_pressed, FALLING);
  attachInterrupt(button2.PIN, button19_pressed, FALLING);
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  display.clearDisplay(); //clear display for first use
  display.display();
}

void IRAM_ATTR button18_pressed() {
  // Puts the function in fast internal ram to ensure quick response
  // then pushes the button pin number to the queue (i.e. which button was pressed)
  // lets the scheduler immediately wake up the tasks thats waiting on the queue (FreeRTOS)
  static const int btn = GPIO_PIN18;
  BaseType_t xHigherPriorityTaskWoken = pdFALSE; 
  xQueueSendFromISR(buttonQueue, &btn, &xHigherPriorityTaskWoken);
  if (xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
}
void IRAM_ATTR button19_pressed() {
    // Handle button press interrupt
    // This is just a placeholder; actual implementation may vary
    static const int btn = GPIO_PIN19;
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