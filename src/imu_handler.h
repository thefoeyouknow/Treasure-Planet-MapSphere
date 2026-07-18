#ifndef IMU_HANDLER_H
#define IMU_HANDLER_H

#include <Arduino.h>

extern volatile bool imuWakeFlag;

void initIMU();
void processIMUPhysicalInputs();
void configureIMUForSleep();
void getAveragedGravityVector(float* gx, float* gy, float* gz);

#endif
