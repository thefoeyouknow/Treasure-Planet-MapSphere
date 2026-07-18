#include "led_manager.h"
#include <Adafruit_NeoPixel.h>

#define NUM_LEDS 38
#define LED_PIN 10

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

struct RGB { uint8_t r, g, b; };
RGB background[NUM_LEDS];
RGB overlay[NUM_LEDS];

uint32_t lastFrameTime = 0;
const uint32_t FRAME_DELAY_MS = 16; // ~60 FPS

bool blackoutMode = false;
uint8_t globalBrightness = 255;

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

void updateLEDs() {
    if (blackoutMode || millis() - lastFrameTime < FRAME_DELAY_MS) {
        return;
    }
    lastFrameTime = millis();

    // Fade the overlay layer (taps)
    fadeToBlackBy(overlay, NUM_LEDS, 10);

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
    if (top1 < NUM_LEDS) overlay[top1] = {255, 255, 255};
    if (top2 < NUM_LEDS) overlay[top2] = {255, 255, 255};
    if (top3 < NUM_LEDS) overlay[top3] = {255, 255, 255};
}

void triggerEquatorialRipple(int eq1, int eq2) {
    if (blackoutMode) return;
    if (eq1 < NUM_LEDS) overlay[eq1] = {0, 0, 255};
    if (eq2 < NUM_LEDS) overlay[eq2] = {0, 0, 255};
}

void runSyncProximityAnimation(uint8_t speed) {
    if (blackoutMode) return;
    uint8_t pos = beatsin8(speed, 0, NUM_LEDS - 1);
    fill_solid(background, NUM_LEDS, {0, 0, 0});
    background[pos] = {0, 255, 0};
}

void runAutonomousAnimation() {
    if (blackoutMode) return;
    uint8_t bright = beatsin8(15, 20, 80);
    fill_solid(background, NUM_LEDS, CHSV(160, 255, bright));
}
