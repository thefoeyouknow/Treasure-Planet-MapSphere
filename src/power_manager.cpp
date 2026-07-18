#include "power_manager.h"
#include "led_manager.h"
#include "ble_manager.h"
#include "imu_handler.h"

// Sleep timeout
uint32_t inactivityTimeoutMs = 35000;
uint32_t lastActivityTime = 0;

void initPowerManager() {
    lastActivityTime = millis();
}

void resetInactivityTimer() {
    lastActivityTime = millis();
}

uint32_t lastCountdownPrintTime = 0;

void checkInactivityAndSleep() {
    uint32_t elapsed = millis() - lastActivityTime;
    
    // Print countdown once per second
    if (millis() - lastCountdownPrintTime >= 1000) {
        lastCountdownPrintTime = millis();
        uint32_t remaining = (inactivityTimeoutMs > elapsed) ? (inactivityTimeoutMs - elapsed) / 1000 : 0;
        Serial.print("[POWER] State: ACTIVE | Sleep in: ");
        Serial.print(remaining);
        Serial.println("s");
    }

    if (elapsed >= inactivityTimeoutMs) {
        Serial.println("[POWER] Timeout reached. Entering SYSTEMOFF Deep Sleep...");
        delay(100); // Allow serial buffer to flush
        goToSleep();
    }
}

void setSleepTimeout(uint32_t seconds) {
    inactivityTimeoutMs = seconds * 1000;
}

void goToSleep() {
    // 1. Turn off LEDs
    turnOffLEDs();

    // 2. Shutdown BLE
    shutdownBLE();

    // 3. Configure IMU to wake on interrupt
    configureIMUForSleep();

    // 4. Force nRF52840 into deep sleep
    // Clear wake flag just in case
    imuWakeFlag = false;

    // Enable wakeup from pin
    #ifndef PIN_LSM6DS3TR_C_INT1
    #define PIN_LSM6DS3TR_C_INT1 11
    #endif

    // Set pin to wake up the system
    nrf_gpio_cfg_sense_input((uint32_t)digitalPinToPinName(PIN_LSM6DS3TR_C_INT1), NRF_GPIO_PIN_PULLDOWN, NRF_GPIO_PIN_SENSE_HIGH);

    // Call system off
    NRF_POWER->SYSTEMOFF = 1;

    // This point should not be reached as SYSTEMOFF resets upon waking up
    while(1);
}
