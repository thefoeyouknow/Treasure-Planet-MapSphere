#include "imu_handler.h"
#include "led_manager.h"
#include "power_manager.h"
#include <Wire.h>
#include <Adafruit_LSM6DS3TRC.h>

#ifndef PIN_LSM6DS3TR_C_INT1
#define PIN_LSM6DS3TR_C_INT1 11
#endif

Adafruit_LSM6DS3TRC lsm6ds;
volatile bool imuWakeFlag = false;

float currentG_X = 0, currentG_Y = 0, currentG_Z = 0;

void getAveragedGravityVector(float* gx, float* gy, float* gz) {
    *gx = currentG_X;
    *gy = currentG_Y;
    *gz = currentG_Z;
}

void imuInterruptISR() {
    imuWakeFlag = true;
}

void writeRegisterRaw(uint8_t reg, uint8_t value) {
    Wire1.beginTransmission(0x6A);
    Wire1.write(reg);
    Wire1.write(value);
    Wire1.endTransmission();
}

bool imuInitialized = false;

void initIMU() {
    // 1. Energize the high-side power rail to the sensor
    // P1_8 is mapped to index 15 in variant.cpp
    pinMode(15, OUTPUT);
    digitalWrite(15, HIGH);
    
    // CRITICAL: Set P1.08 to high drive strength (H0H1) to provide enough current for the IMU!
    // Standard drive strength limits current and prevents the IMU from booting.
    NRF_P1->PIN_CNF[8] = ((uint32_t)1 << 0)   // DIR_OUTPUT
                       | ((uint32_t)1 << 1)   // INPUT_DISCONNECT
                       | ((uint32_t)0 << 2)   // NOPULL
                       | ((uint32_t)3 << 8)   // H0H1 (High Drive)
                       | ((uint32_t)0 << 16); // NOSENSE
                       
    // Enable I2C Pullups if they are on D29 (optional, but good measure)
    pinMode(29, OUTPUT);
    digitalWrite(29, HIGH);

    delay(100); // Allow sensor VCC to settle

    // Explicitly begin Wire1
    Wire.begin();
    Wire1.begin();
    delay(50);

    Serial.println("[IMU] Scanning Wire (External I2C)...");
    for (uint8_t i = 1; i < 127; i++) {
        Wire.beginTransmission(i);
        if (Wire.endTransmission() == 0) {
            Serial.print("  Found device at 0x");
            Serial.println(i, HEX);
        }
    }
    
    Serial.println("[IMU] Scanning Wire1 (Internal I2C)...");
    bool foundDevice = false;
    for (uint8_t i = 1; i < 127; i++) {
        Wire1.beginTransmission(i);
        if (Wire1.endTransmission() == 0) {
            Serial.print("  Found device at 0x");
            Serial.println(i, HEX);
            if (i == 0x6A || i == 0x6B) {
                foundDevice = true;
            }
        }
    }

    if (lsm6ds.begin_I2C(0x6A, &Wire1)) {
        imuInitialized = true;
        Serial.println("[IMU] Successfully Initialized!");
    } else {
        Serial.println("[IMU] Initialization failed. Sensor not found!");
        return; // Headless mode
    }

    // Configure LSM6DS3TR-C for accelerometer and gyroscope at low power
    lsm6ds.setAccelRange(LSM6DS_ACCEL_RANGE_4_G);
    lsm6ds.setAccelDataRate(LSM6DS_RATE_104_HZ);
    lsm6ds.setGyroRange(LSM6DS_GYRO_RANGE_1000_DPS);
    lsm6ds.setGyroDataRate(LSM6DS_RATE_12_5_HZ); // Enable gyro for spin detection

    // PIN_LSM6DS3TR_C_INT1 is 18 on Mbed
    pinMode(18, INPUT);
    attachInterrupt(digitalPinToInterrupt(18), imuInterruptISR, RISING);
}

uint32_t lastIMUPrintTime = 0;

void processIMUPhysicalInputs() {
    if (!imuInitialized) return; // Prevent hardfault

    sensors_event_t accel, gyro, temp;
    lsm6ds.getEvent(&accel, &gyro, &temp);

    float y = accel.acceleration.x / 9.80665;
    float x = accel.acceleration.y / 9.80665;
    float z = accel.acceleration.z / 9.80665;

    float gyroMag = sqrt((gyro.gyro.x * gyro.gyro.x) + (gyro.gyro.y * gyro.gyro.y) + (gyro.gyro.z * gyro.gyro.z));
    applyGyroHueShift(gyroMag);

    currentG_X = x;
    currentG_Y = y;
    currentG_Z = z;

    float totalMag = sqrt((x * x) + (y * y) + (z * z));
    float dynamicG = abs(totalMag - 1.0);

    // Keep awake as long as the device is moving (not still)
    // 0.15G is a good threshold for "stillness"
    if (dynamicG > 0.15) {
        resetInactivityTimer();
    }

    // Calculate jerk (change in acceleration) to distinguish a sharp tap from a smooth lift
    static float last_x = 0, last_y = 0, last_z = 0;
    float jerkX = x - last_x;
    float jerkY = y - last_y;
    float jerkZ = z - last_z;
    last_x = x; last_y = y; last_z = z;

    float jerkMag = sqrt((jerkX * jerkX) + (jerkY * jerkY) + (jerkZ * jerkZ));

    // --- Event-Driven Prints (Rate limited to avoid spam) ---
    static uint32_t lastGForcePrint = 0;
    if (dynamicG > 0.15 && (millis() - lastGForcePrint > 1000)) {
        lastGForcePrint = millis();
        Serial.println("[IMU] Event: G-Force Detected");
    }

    static uint32_t lastGyroPrint = 0;
    if ((abs(gyro.gyro.x) > 15 || abs(gyro.gyro.y) > 15 || abs(gyro.gyro.z) > 15) && (millis() - lastGyroPrint > 1000)) {
        lastGyroPrint = millis();
        Serial.println("[IMU] Event: Gyro Movement Detected");
    }

    static uint32_t lastJerkPrint = 0;
    if (jerkMag > 0.15 && jerkMag <= 0.85 && (millis() - lastJerkPrint > 1000)) {
        lastJerkPrint = millis();
        Serial.println("[IMU] Event: Minor Jerk Detected");
    }
    // --------------------------------------------------------

    // A tap creates a high frequency jerk. A lift is smooth.
    const float TAP_JERK_THRESHOLD = 0.85; 
    
    // Cooldown timer to prevent multiple triggers from one physical tap
    static uint32_t lastTapTime = 0;
    const uint32_t TAP_COOLDOWN_MS = 300;
    
    // For double tap detection on Z axis
    static uint32_t lastZTapTime = 0;
    const uint32_t DOUBLE_TAP_WINDOW_MS = 600;

    if (jerkMag > TAP_JERK_THRESHOLD) {
        if (millis() - lastTapTime > TAP_COOLDOWN_MS) {
            lastTapTime = millis();
            Serial.println("[IMU] Tap Detected!");

            float absX = abs(jerkX);
            float absY = abs(jerkY);
            float absZ = abs(jerkZ);

            if (absY > absX && absY > absZ) {
                Serial.println("[LED] State: Polar Cascade (Y-axis Tap)");
                triggerPolarCascade(10, 27, 28); 
            } else if (absX > absY && absX > absZ) {
                Serial.println("[LED] State: Equatorial Ripple (X-axis Tap)");
                triggerEquatorialRipple(5, 15); 
            } else if (absZ > absY && absZ > absX) {
                if (millis() - lastZTapTime < DOUBLE_TAP_WINDOW_MS) {
                    // Double tap detected
                    Serial.println(blackoutMode ? "[LED] State: Blackout Disabled" : "[LED] State: Blackout Enabled");
                    setBlackoutMode(!blackoutMode);
                    lastZTapTime = 0; // Reset to prevent triple-tap triggering again immediately
                } else {
                    lastZTapTime = millis();
                    Serial.println("[LED] State: Equatorial Ripple (Z-axis Tap)");
                    triggerEquatorialRipple(24, 33);
                }
            }
        }
    }
}

void configureIMUForSleep() {
    if (!imuInitialized) return; // Prevent hardfault

    // 0. Disable Gyroscope to save power and stop gyro interrupts
    writeRegisterRaw(0x11, 0x00); // CTRL2_G = 0

    // 0.5 CRITICAL: Disable Data Ready (DRDY) interrupts on INT1!
    // If DRDY is still routed to INT1, the NRF will wake up instantly at 104Hz.
    writeRegisterRaw(0x0D, 0x00); // INT1_CTRL = 0

    // 1. Set Accel to low power 26Hz, 2g FS
    writeRegisterRaw(0x10, 0x20); // CTRL1_XL = 26Hz

    // 2. CRITICAL: Enable basic interrupts in TAP_CFG (Bit 7 = INTERRUPTS_ENABLE)
    // Bit 0 = LIR (Latch Interrupt). Let's set it to 0 so it pulses, avoiding stuck-high issues.
    writeRegisterRaw(0x58, 0x80); 

    // 3. Set Wake-up threshold (0 to 63)
    writeRegisterRaw(0x5B, 0x08); // WAKE_UP_THS (~250mg)

    // 4. Set Wake-up duration (debounce)
    // Bits [6:5] = WAKE_DUR. Setting to 1 (0x20) means the motion must last for 1*ODR (~38ms)
    // This ignores instantaneous shocks/bounces from setting the sphere down!
    writeRegisterRaw(0x5C, 0x20); 

    // 5. Route Wake-up event to INT1 (overwrite to ensure ONLY wake-up is routed)
    writeRegisterRaw(0x5E, 0x20); // MD1_CFG -> INT1_WU = 1

    // 6. CLEAR any pending interrupts so the INT1 pin drops low before we sleep!
    Wire1.beginTransmission(0x6A);
    Wire1.write(0x1A); // ALL_INT_SRC
    Wire1.endTransmission(false);
    Wire1.requestFrom((uint8_t)0x6A, (uint8_t)2);
    if (Wire1.available() == 2) {
        Wire1.read(); // ALL_INT_SRC
        Wire1.read(); // WAKE_UP_SRC
    }
    
    // Give the IMU INT1 pin time to physically drop low
    delay(50);
}
