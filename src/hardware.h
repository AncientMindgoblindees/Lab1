#include <Arduino.h>
#ifndef HARDWARE_H
#define HARDWARE_H


void hardware_setup();
void IRAM_ATTR button18_pressed();
void IRAM_ATTR button19_pressed();
void hardwareTask(void * parameter);
void testdrawchar();
//void gather_tempData();
//void send_tempData();
//void send_toLCD();

const int GPIO_PIN19=19;
const int GPIO_PIN18=18;



#endif