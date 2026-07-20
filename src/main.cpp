#include "ble_manager.h"
#include "imu_handler.h"
#include "led_manager.h"
#include "power_manager.h"
#include "version.h"
#include <Arduino.h>

void setup() {
  // Give TinyUSB time to enumerate on the host before we do anything that could
  // crash.
  delay(2000);

  Serial.begin(115200);
  // Print a few blank lines to clear monitor
  Serial.println("\n\n--- BOOTING ---");

  Serial.println("Init Power Manager...");
  initPowerManager();

  Serial.println("Init LEDs...");
  initLEDs();
  runBootChase();

  Serial.println("Init IMU...");
  initIMU();

  Serial.println("Init BLE...");
  initBLE();

  Serial.println("--- SETUP COMPLETE ---");
}

void loop() {
  processIMUPhysicalInputs();
  processBLE();

  updateLEDs(); // Shows FastLED buffer

  checkInactivityAndSleep();
}