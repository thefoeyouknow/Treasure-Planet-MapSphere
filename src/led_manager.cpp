#include "led_manager.h"
#include <Adafruit_NeoPixel.h>

#define NUM_LEDS 36
#define LED_PIN D10

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

struct RGB { uint8_t r, g, b; };
RGB background[NUM_LEDS];
RGB overlay[NUM_LEDS];

uint32_t lastFrameTime = 0;
const uint32_t FRAME_DELAY_MS = 16; // ~60 FPS

bool blackoutMode = false;
uint8_t globalBrightness = 255;

const uint8_t distToNorth[NUM_LEDS] = {
    // Loop A (0..19). North is 10.
    10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    // Loop B (20..35). North is 27 and 28.
    7, 6, 5, 4, 3, 2, 1, 0, // 20..27
    0, 1, 2, 3, 4, 5, 6, 7  // 28..35
};

AnimationMode currentMode = AnimationMode::AUTONOMOUS;
RGB manualColor = {0, 0, 255};
float currentBaseHue = 160.0f;
uint8_t currentProxSpeed = 30;
uint8_t gyroSensitivityConfig = 50; // 0-100

float currentRippleRadius = -1.0f;
float currentRippleOrigin[3] = {0.0f, 0.0f, 0.0f};
float ledPos[NUM_LEDS][3];

// --- Math Helpers (FastLED replacements) ---
uint8_t beatsin8(uint8_t beats_per_minute, uint8_t lowest = 0, uint8_t highest = 255) {
    uint32_t time_ms = millis();
    uint32_t beat = (time_ms * beats_per_minute * 256) / 60000;
    uint8_t beat8 = beat & 0xFF;
    // Simple triangle wave approximation of sine
    uint8_t wave = beat8;
    if (wave & 0x80) {
        wave = ~wave;
    }
    wave = wave << 1; 
    return lowest + ((uint16_t)wave * (highest - lowest)) / 255;
}

void fadeToBlackBy(RGB* leds, uint16_t count, uint8_t fadeBy) {
    for (uint16_t i = 0; i < count; i++) {
        leds[i].r = (leds[i].r * (255 - fadeBy)) / 256;
        leds[i].g = (leds[i].g * (255 - fadeBy)) / 256;
        leds[i].b = (leds[i].b * (255 - fadeBy)) / 256;
    }
}

void fill_solid(RGB* leds, uint16_t count, RGB color) {
    for (uint16_t i = 0; i < count; i++) {
        leds[i] = color;
    }
}

RGB CHSV(uint8_t h, uint8_t s, uint8_t v) {
    RGB rgb;
    uint32_t c = strip.ColorHSV((uint16_t)h * 256, s, v);
    rgb.r = (c >> 16) & 0xFF;
    rgb.g = (c >> 8) & 0xFF;
    rgb.b = c & 0xFF;
    return rgb;
}
// -------------------------------------------

void initLEDs() {
    strip.begin();
    strip.clear();
    strip.show();
    
    // Generate 3D LED mapping based on distToNorth
    // Assuming radius = 10.0 for easier math
    for (int i = 0; i < NUM_LEDS; i++) {
        // angle from 0 (North) to PI (South)
        float theta = distToNorth[i] * (PI / 10.0f);
        
        if (i < 20) {
            // Loop A: X-Y plane
            if (i <= 10) {
                // Going South to North
                ledPos[i][0] = sin(theta) * 10.0f;
                ledPos[i][1] = cos(theta) * 10.0f;
                ledPos[i][2] = 0.0f;
            } else {
                // Going North back down to South
                ledPos[i][0] = -sin(theta) * 10.0f;
                ledPos[i][1] = cos(theta) * 10.0f;
                ledPos[i][2] = 0.0f;
            }
        } else {
            // Loop B: Z-Y plane
            if (i <= 27) {
                // Going up to North
                ledPos[i][0] = 0.0f;
                ledPos[i][1] = cos(theta) * 10.0f;
                ledPos[i][2] = sin(theta) * 10.0f;
            } else {
                // Going down from North
                ledPos[i][0] = 0.0f;
                ledPos[i][1] = cos(theta) * 10.0f;
                ledPos[i][2] = -sin(theta) * 10.0f;
            }
        }
    }
}

void setGyroSensitivity(uint8_t sens) {
    gyroSensitivityConfig = sens;
}

void applyGyroHueShift(float rotationSpeed) {
    // scale 0-100 to a multiplier (e.g., 50 -> 2.0x, 100 -> 4.0x)
    float multiplier = (float)gyroSensitivityConfig / 25.0f;
    currentBaseHue += rotationSpeed * multiplier;
    if (currentBaseHue > 255.0f) currentBaseHue -= 255.0f;
}

void setAnimationMode(AnimationMode mode) {
    currentMode = mode;
}

void setManualColor(uint8_t r, uint8_t g, uint8_t b) {
    manualColor = {r, g, b};
}

void updateLEDs() {
    if (blackoutMode || millis() - lastFrameTime < FRAME_DELAY_MS) {
        return;
    }
    lastFrameTime = millis();

    // Process background based on mode
    if (currentMode == AnimationMode::AUTONOMOUS) {
        runAutonomousAnimation();
    } else if (currentMode == AnimationMode::SOLID_COLOR) {
        fill_solid(background, NUM_LEDS, manualColor);
    } else if (currentMode == AnimationMode::PROXIMITY_SYNC) {
        runSyncProximityAnimation();
    }

    // Fade the overlay layer (taps)
    fadeToBlackBy(overlay, NUM_LEDS, 20); // Fade faster so trails look cleaner

    // Update wave radius
    if (currentRippleRadius >= 0.0f) {
        currentRippleRadius += 0.7f; // expansion speed
        for (int i = 0; i < NUM_LEDS; i++) {
            float dx = ledPos[i][0] - currentRippleOrigin[0];
            float dy = ledPos[i][1] - currentRippleOrigin[1];
            float dz = ledPos[i][2] - currentRippleOrigin[2];
            float dist = sqrt(dx*dx + dy*dy + dz*dz);
            
            if (abs(dist - currentRippleRadius) < 2.5f) {
                overlay[i] = {255, 200, 50}; // Bright gold impact
            }
        }
        if (currentRippleRadius > 30.0f) currentRippleRadius = -1.0f;
    }

    // Composite: Overlay adds to background
    for (int i = 0; i < NUM_LEDS; i++) {
        uint16_t r = background[i].r + overlay[i].r;
        uint16_t g = background[i].g + overlay[i].g;
        uint16_t b = background[i].b + overlay[i].b;
        
        // Apply global brightness
        r = (r * globalBrightness) / 255;
        g = (g * globalBrightness) / 255;
        b = (b * globalBrightness) / 255;
        
        strip.setPixelColor(i, min(r, (uint16_t)255), min(g, (uint16_t)255), min(b, (uint16_t)255));
    }

    strip.show();
}

void turnOffLEDs() {
    strip.clear();
    fill_solid(background, NUM_LEDS, {0, 0, 0});
    fill_solid(overlay, NUM_LEDS, {0, 0, 0});
    strip.show();
}

void setBlackoutMode(bool enable) {
    blackoutMode = enable;
    if (blackoutMode) {
        turnOffLEDs();
    }
}

void setGlobalBrightness(uint8_t brightness) {
    globalBrightness = brightness;
}

void triggerImpactRipple(float ix, float iy, float iz) {
    if (blackoutMode) return;
    
    // Orthogonal Basis Matrix derived from Gram-Schmidt calibration
    const float calX[3] = {-0.490f, 0.662f, -0.567f}; // Loop A Equator Axis
    const float calY[3] = {0.581f, 0.734f, 0.353f};   // North Pole Axis
    const float calZ[3] = {0.650f, -0.157f, -0.744f}; // Loop B Equator Axis
    
    float mappedX = ix * calX[0] + iy * calX[1] + iz * calX[2];
    float mappedY = ix * calY[0] + iy * calY[1] + iz * calY[2];
    float mappedZ = ix * calZ[0] + iy * calZ[1] + iz * calZ[2];

    float mag = sqrt(mappedX*mappedX + mappedY*mappedY + mappedZ*mappedZ);
    if (mag > 0.001f) {
        // Project to the sphere shell (radius 10.0)
        currentRippleOrigin[0] = (mappedX / mag) * 10.0f;
        currentRippleOrigin[1] = (mappedY / mag) * 10.0f;
        currentRippleOrigin[2] = (mappedZ / mag) * 10.0f;
        currentRippleRadius = 0.0f;
    }
}

void setProximitySpeed(uint8_t speed) {
    currentProxSpeed = speed;
}

void runSyncProximityAnimation() {
    if (blackoutMode) return;
    uint8_t pos = beatsin8(currentProxSpeed, 0, NUM_LEDS - 1);
    fill_solid(background, NUM_LEDS, {0, 0, 0});
    background[pos] = {0, 255, 0};
}

void runAutonomousAnimation() {
    if (blackoutMode) return;
    
    // Add a pulsing brightness to make it breathe
    uint8_t bright = beatsin8(15, 80, 255); 
    fill_solid(background, NUM_LEDS, CHSV((uint8_t)currentBaseHue, 255, bright));
}

void runBootChase() {
    uint32_t delayTime = 2000 / NUM_LEDS; // Total time 2000ms distributed evenly
    strip.clear();
    for (int i = 0; i < NUM_LEDS; i++) {
        strip.setPixelColor(i, 0, 0, 255); // Blue chase using RGB values directly
        strip.show();
        delay(delayTime);
        strip.setPixelColor(i, 0, 0, 0); // Turn off trailing pixel
    }
    strip.show();
}
