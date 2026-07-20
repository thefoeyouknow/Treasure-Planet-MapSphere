#include "ble_manager.h"
#include "led_manager.h"
#include "power_manager.h"
#include "version.h"
#include <ArduinoBLE.h>
#include "imu_handler.h"

const char *customServiceUuidStr = "a1b2c3d4-e5f6-7a8b-9c0d-1e2f3a4b5c6d";

BLEService customService(customServiceUuidStr);
BLEByteCharacteristic commandChar("a1b2c3d4-e5f6-7a8b-9c0d-1e2f3a4b5c6f", BLEWrite);
BLECharacteristic configChar("a1b2c3d4-e5f6-7a8b-9c0d-1e2f3a4b5c70", BLERead | BLEWrite, 4);
BLECharacteristic imuTelemetryChar("a1b2c3d4-e5f6-7a8b-9c0d-1e2f3a4b5c71", BLENotify, 12);

BLEByteCharacteristic animationModeChar("a1b2c3d4-e5f6-7a8b-9c0d-1e2f3a4b5c72", BLERead | BLEWrite);
BLECharacteristic colorOverrideChar("a1b2c3d4-e5f6-7a8b-9c0d-1e2f3a4b5c73", BLERead | BLEWrite, 3);
BLECharacteristic fwVersionChar("a1b2c3d4-e5f6-7a8b-9c0d-1e2f3a4b5c74", BLERead, 16);

float filteredRSSI = -80.0;
const float ALPHA = 0.15;
int proxWakeGate = -65;
int proxSleepGate = -71;
const int PROX_SATURATION = -45;

bool proximityActive = false;
bool bleActive = false;

uint32_t lastRoleSwitch = 0;
bool isScanning = false;

void commandCharWritten(BLEDevice central, BLECharacteristic characteristic) {
  uint8_t cmd = commandChar.value();
  switch (cmd) {
    case 0x01:
      setBlackoutMode(!blackoutMode);
      break;
    case 0x02:
      triggerPolarCascade(10, 27, 28);
      break;
    case 0x03:
      triggerEquatorialRipple(5, 15);
      break;
    case 0x04:
      triggerEquatorialRipple(24, 33);
      break;
  }
}

void configCharWritten(BLEDevice central, BLECharacteristic characteristic) {
  const uint8_t* val = configChar.value();
  if (configChar.valueLength() == 4) {
    uint8_t timeoutSec = val[0];
    uint8_t brightness = val[1];
    uint8_t sensOffset = val[2]; // 0-100 mapped to an offset
    uint8_t gyroSens = val[3];
    
    setSleepTimeout((uint32_t)timeoutSec);
    setGlobalBrightness(brightness);
    setGyroSensitivity(gyroSens);
    
    // Default gates: Wake -65, Sleep -71
    // Let's use offset (0-100) where 50 is default
    int offset = (int)sensOffset - 50;
    proxWakeGate = -65 + offset;
    proxSleepGate = -71 + offset;
  }
}

void animationModeCharWritten(BLEDevice central, BLECharacteristic characteristic) {
    uint8_t mode = animationModeChar.value();
    setAnimationMode((AnimationMode)mode);
}

void colorOverrideCharWritten(BLEDevice central, BLECharacteristic characteristic) {
    const uint8_t* val = colorOverrideChar.value();
    if (colorOverrideChar.valueLength() == 3) {
        setManualColor(val[0], val[1], val[2]);
    }
}

void initBLE() {
  if (!BLE.begin()) {
    return;
  }

  bleActive = true;

  BLE.setLocalName("MapSphereNode");
  BLE.setAdvertisedService(customService);
  
  customService.addCharacteristic(commandChar);
  customService.addCharacteristic(configChar);
  customService.addCharacteristic(imuTelemetryChar);
  customService.addCharacteristic(animationModeChar);
  customService.addCharacteristic(colorOverrideChar);
  customService.addCharacteristic(fwVersionChar);
  
  BLE.addService(customService);
  
  commandChar.setEventHandler(BLEWritten, commandCharWritten);
  configChar.setEventHandler(BLEWritten, configCharWritten);
  animationModeChar.setEventHandler(BLEWritten, animationModeCharWritten);
  colorOverrideChar.setEventHandler(BLEWritten, colorOverrideCharWritten);

  // Initial config value
  uint8_t initialConfig[4] = {35, 255, 50, 50}; // 35s, max brightness, default prox, default gyro
  configChar.writeValue(initialConfig, 4);
  
  uint8_t initialTelemetry[12] = {0};
  imuTelemetryChar.writeValue(initialTelemetry, 12);
  
  // Initialize Version Char
  fwVersionChar.writeValue(FW_VERSION);
  
  animationModeChar.writeValue((uint8_t)0); // Autonomous mode default
  uint8_t initialColor[3] = {0, 0, 255};
  colorOverrideChar.writeValue(initialColor, 3);

  // Initial state: Advertising
  BLE.advertise();
  isScanning = false;
  lastRoleSwitch = millis();
}

void processBLE() {
  if (!bleActive)
    return;

  BLE.poll();

  BLEDevice central = BLE.central();
  bool centralConnected = central && central.connected();

  // Suspend role switching if a central (like a phone) is connected
  if (centralConnected) {
    if (isScanning) {
      BLE.stopScan();
      isScanning = false;
    }
    // We can also reset the inactivity timer to keep the device awake while connected
    resetInactivityTimer();
  } else {
    // Time-slice scanning and advertising because ArduinoBLE doesn't natively do
    // both concurrently 500ms advertise, 1500ms scan
    if (isScanning && (millis() - lastRoleSwitch >= 1500)) {
      BLE.stopScan();
      BLE.advertise();
      isScanning = false;
      lastRoleSwitch = millis();
    } else if (!isScanning && (millis() - lastRoleSwitch >= 500)) {
      BLE.stopAdvertise();
      BLE.scanForUuid(customServiceUuidStr);
      isScanning = true;
      lastRoleSwitch = millis();
    }
  }

  if (isScanning) {
    BLEDevice peripheral = BLE.available();
    if (peripheral) {
      if (peripheral.hasAdvertisedServiceUuid()) {
        // We found a match
        int rawRSSI = peripheral.rssi();
        filteredRSSI =
            (ALPHA * (float)rawRSSI) + ((1.0 - ALPHA) * filteredRSSI);

        if (!proximityActive && filteredRSSI > proxWakeGate) {
          proximityActive = true;
          setAnimationMode(AnimationMode::PROXIMITY_SYNC);
          Serial.println("[LED] Mode: PROXIMITY SYNC (Peer Node Found)");
        }
        if (proximityActive) {
          resetInactivityTimer();
        }
      }
    } else {
      // Decay RSSI if no device seen during this scan tick
      filteredRSSI = (ALPHA * -100.0) + ((1.0 - ALPHA) * filteredRSSI);
      if (proximityActive && filteredRSSI < proxSleepGate) {
        proximityActive = false;
        setAnimationMode((AnimationMode)animationModeChar.value());
        Serial.println("[LED] Mode: Restored from Proximity");
      }
    }
  }

  static uint32_t lastTelemetryTime = 0;
  if (centralConnected && (millis() - lastTelemetryTime > 66)) { // ~15 Hz
    lastTelemetryTime = millis();
    float gVec[3];
    getAveragedGravityVector(&gVec[0], &gVec[1], &gVec[2]);
    imuTelemetryChar.writeValue((const uint8_t*)gVec, 12);
  }

  if (proximityActive) {
    uint8_t effectSpeed =
        map(constrain((int)filteredRSSI, proxSleepGate, PROX_SATURATION),
            proxSleepGate, PROX_SATURATION, 30, 255);
    setProximitySpeed(effectSpeed);
  }
}

void shutdownBLE() {
  if (bleActive) {
    BLE.stopAdvertise();
    BLE.stopScan();
    BLE.end();
    bleActive = false;
  }
}
