#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <Arduino.h>

void initPowerManager();
void resetInactivityTimer();
void checkInactivityAndSleep();
void goToSleep();
void setSleepTimeout(uint32_t seconds);

#endif
