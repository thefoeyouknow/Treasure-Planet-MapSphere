#ifndef LED_MANAGER_H
#define LED_MANAGER_H

#include <Arduino.h>

void initLEDs();
void updateLEDs();
void turnOffLEDs();

extern bool blackoutMode;
void setBlackoutMode(bool enable);
void setGlobalBrightness(uint8_t brightness);

void triggerImpactRipple(float ix, float iy, float iz);
void runSyncProximityAnimation();
void runAutonomousAnimation();

void runBootChase();

enum class AnimationMode {
    AUTONOMOUS = 0,
    SOLID_COLOR = 1,
    PROXIMITY_SYNC = 2
};

void setAnimationMode(AnimationMode mode);
void setManualColor(uint8_t r, uint8_t g, uint8_t b);
void applyGyroHueShift(float rotationSpeed);
void setGyroSensitivity(uint8_t sens);
void setProximitySpeed(uint8_t speed);

#endif
