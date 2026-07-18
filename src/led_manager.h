#ifndef LED_MANAGER_H
#define LED_MANAGER_H

#include <Arduino.h>

void initLEDs();
void updateLEDs();
void turnOffLEDs();

extern bool blackoutMode;
void setBlackoutMode(bool enable);
void setGlobalBrightness(uint8_t brightness);

void triggerPolarCascade(int top1, int top2, int top3);
void triggerEquatorialRipple(int eq1, int eq2);
void runSyncProximityAnimation(uint8_t speed);
void runAutonomousAnimation();

#endif
