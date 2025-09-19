#include "hardware.h"
struct Button {
  const uint8_t PIN;
  bool pressed;
};
QueueHandle_t buttonQueue;
Button button1 = {GPIO_PIN18, false};
Button button2 = {GPIO_PIN19, false};


void hardwareTask(void * parameter) {
  int btn;
  for(;;) {
    if(xQueueReceive(buttonQueue, &btn, portMAX_DELAY)) {
      Serial.print("Button on pin ");
      Serial.print(btn);
      Serial.println(" pressed!");
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