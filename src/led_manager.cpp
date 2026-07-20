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
float gyroHueOffset = 0.0f;
uint8_t currentProxSpeed = 30;
uint8_t gyroSensitivityConfig = 50; // 0-100

float polarWaveRadius = -1.0f;
float equatorialWaveRadius = -1.0f;

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
}

void setGyroSensitivity(uint8_t sens) {
    gyroSensitivityConfig = sens;
}

void applyGyroHueShift(float rotationSpeed) {
    // scale 0-100 to a multiplier (e.g., 50 -> 2.0x, 100 -> 4.0x)
    float multiplier = (float)gyroSensitivityConfig / 25.0f;
    gyroHueOffset += rotationSpeed * multiplier;
    if (gyroHueOffset > 255.0f) gyroHueOffset -= 255.0f;
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

    // Update wave radiuses
    if (polarWaveRadius >= 0.0f) {
        polarWaveRadius += 0.4f; // speed
        for (int i = 0; i < NUM_LEDS; i++) {
            float dist = abs((float)distToNorth[i] - polarWaveRadius);
            if (dist < 1.5f) {
                overlay[i] = {255, 200, 50}; // Dramatic Gold
            }
        }
        if (polarWaveRadius > 12.0f) polarWaveRadius = -1.0f;
    }

    if (equatorialWaveRadius >= 0.0f) {
        equatorialWaveRadius += 0.4f; // speed
        for (int i = 0; i < NUM_LEDS; i++) {
            float distFromEq = abs((float)distToNorth[i] - 5.0f);
            float dist = abs(distFromEq - equatorialWaveRadius);
            if (dist < 1.5f) {
                overlay[i] = {0, 255, 255}; // Cyan
            }
        }
        if (equatorialWaveRadius > 7.0f) equatorialWaveRadius = -1.0f;
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

void triggerPolarCascade(int top1, int top2, int top3) {
    if (blackoutMode) return;
    polarWaveRadius = 0.0f; // Spawns the wave at the north pole
}

void triggerEquatorialRipple(int eq1, int eq2) {
    if (blackoutMode) return;
    equatorialWaveRadius = 0.0f; // Spawns the wave at the equator
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
    // Ethereal space background: Slowly shifting base color
    // We add gyroHueOffset to make it spin colors when rolling!
    uint8_t baseHue = beatsin8(2, 140, 180); // Slowly drifts between purple and blue
    baseHue = (uint8_t)(baseHue + (int)gyroHueOffset);
    
    // Add a pulsing brightness to make it breathe
    uint8_t bright = beatsin8(15, 30, 120); 
    fill_solid(background, NUM_LEDS, CHSV(baseHue, 255, bright));
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
