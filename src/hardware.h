#include <Arduino.h>
#ifndef HARDWARE_H
#define HARDWARE_H



void hardware_setup();
void IRAM_ATTR button25_pressed();
void IRAM_ATTR button26_pressed();
void hardwareTask(void * parameter);
void testdrawchar();
void updateDisplayBasedOnSensors();
void readSensorValues();
float readSensor(int sensorNum);
void displayDebugTest();
//bool isSensorConnected(DeviceAddress addr);
//void rescanAndUpdate(DeviceAddress addr, uint8_t index);


#define GPIO_PIN26 26
#define GPIO_PIN25 25

#endif