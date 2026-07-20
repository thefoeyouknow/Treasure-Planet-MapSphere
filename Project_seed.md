PROJECT SEED: FIRMWARE SPECIFICATION & SYSTEM ARCHITECTURE

Target Platform: PlatformIO / C++ (Arduino Framework)

Microcontroller: Seeed Studio Xiao nRF52840 Sense

Primary Libraries: FastLED, ArduinoBLE, LSM6DS3 (or Adafruit_LSM6DS)

1. System Topology & Electrical Architecture

1.1 Microcontroller Environment

MCU: Nordic nRF52840 (ARM Cortex-M4F running at $64\text{ MHz}$).

SRAM / Flash: $256\text{ KB}$ RAM, $1\text{ MB}$ Flash.

Onboard Peripherals: LSM6DS3TR-C 6-DOF IMU, Bluetooth Low Energy 2.4GHz Radio.

1.2 Power Supply & Safety Constraints (DFR1026 & Floating VBUS)

Active Draw Constraints: Battery-backed 5V boosted rail. Software must enforce a strict current ceiling of $800\text{ mA}$ at $5\text{ V}$ using FastLED.setMaxPowerInVoltsAndMilliamps(5, 800).

Hardware Safe Mode (Floating VBUS): * Quirk: The DFR1026 boost converter constantly back-feeds $5\text{ V}$ onto the Xiao's $5\text{ V}$ rail from the battery.

Implication: The USB-C breakout has its VBUS trace physically severed to prevent secondary $5\text{ V}$ collisions when programming.

Software Caution: Do not implement blocking loops waiting for while(!Serial) or checking USBDevice.attached() to run initialization routines, as the MCU operates purely on battery-boosted power even while plugged in for serial flashing.

2. Low-Level Peripheral Configuration

2.1 Critical Onboard I2C Subsystem

The onboard LSM6DS3TR-C IMU is wired to a secondary, internal $I^2C$ bus that is completely separate from the external hardware pins (SDA/SCL). Standard initialization of the Wire object will fail to discover the sensor.

High-Side Power Switch Pin: P0_14 (Hardware pin name PIN_LSM6DS3_TR_C_POWER). This pin controls a MOSFET supplying power to the IMU sensor. It must be written HIGH prior to initializing the bus.

I2C Interface: Use the Wire1 object rather than Wire.

Sensor I2C Address: 0x6A on Wire1.

Standard Hardware Initialization Boilerplate

#include <Wire.h>
#include <LSM6DS3.h> // Ensure target is compatible with LSM6DS3TR-C

#define IMU_PWR_PIN P0_14 
#define IMU_I2C_ADDR 0x6A

LSM6DS3 myIMU(I2C_MODE, IMU_I2C_ADDR);

void initCoreIMU() {
    // 1. Energize the high-side power rail to the sensor
    pinMode(IMU_PWR_PIN, OUTPUT);
    digitalWrite(IMU_PWR_PIN, HIGH);
    delay(50); // Allow sensor VCC to settle

    // 2. Initialize secondary I2C bus
    Wire1.begin(); 

    // 3. Initialize IMU register map over Wire1
    if (myIMU.begin() != 0) {
        // Critical Error: Handle peripheral offline state
    }
}



3. LED Array Mapping & Data Flow

The physical strip is composed of 36 WS2812B NeoPixels running at 144 LEDs/meter, wired as a single continuous sequential data line via jumpers over the mechanical crossover points.

With the equatorial center support ring level to the ground, the LED array is shaped like an X. It consists of two diagonal meridian hoops that intersect at the North Pole (Apex) and South Pole (Base).

 [ Xiao Data Pin ] ---> [ Loop A: Continuous Diagonal (0..19) ]
                                      |
                 (Solder Bridge Jumper at South Pole)
                                      V
                        [ Loop B: Left Diagonal Arc (20..27) ]
                                      |
                 (Solder Bridge Jumper at North Pole)
                                      V
                        [ Loop B: Right Diagonal Arc (28..35) ]



3.1 Logical Index Allocations

Loop A: Continuous Diagonal Meridian Hoop (Indices 0 to 19)

Index 0: South Pole (Base / Start of Loop A).

Index 5: Equatorial Crossing (East Hemisphere).

Index 10: North Pole (Apex of Loop A / Physical crossover).

Index 15: Equatorial Crossing (West Hemisphere).

Index 19: South Pole (Base / End of Loop A).

Loop B: Conjugate Diagonal Meridian Hoop (Indices 20 to 35)

Due to the mechanical interlocking of the core, the second leg of the "X" is split into two equal half-circles:

Left Diagonal Arc (Indices 20 to 27):

Index 20: South Pole (Base / Start of Arc 1).

Index 23/24: Equatorial Crossing (South-West Hemisphere).

Index 27: North Pole (Apex / Physical crossover).

Right Diagonal Arc (Indices 28 to 35):

Index 28: North Pole (Apex / Physical crossover).

Index 31/32: Equatorial Crossing (North-East Hemisphere).

Index 35: South Pole (Base / End of Loop B).

3.2 Physical Intersection Points

North Pole Intersection (Apex): Index 10 of Loop A is physically adjacent to Indices 27 and 28 of Loop B.

South Pole Intersection (Base): Indices 0 and 19 of Loop A are physically adjacent to Indices 20 and 35 of Loop B.

4. Firmware State Machine & Power Management

To maintain a standby shelf life of 75 days on a 1000mAh cell, the firmware must drop the average sleep current to $0.55\text{ mA}$ by placing both the MCU and the radio stack into true deep sleep.

       +------------------------------------+
       |             BOOT/INIT              |
       |  (Configures IMU Hardware Inter.)  |
       +-----------------+------------------+
                         |
                         V
       +------------------------------------+
       |            ACTIVE STATE            | <------+
       |  - Execute FastLED Animations      |        |
       |  - Continuous motion calculations  |        |
       |  - BLE Scan & Non-Conn Advertise   |        | Hardware
       +-----------------+------------------+        | Interrupt
                         |                           | (Wake-on-Shake)
                         | 35s Inactivity Timeout    |
                         V                           |
       +------------------------------------+        |
       |             SLEEP STATE            | -------+
       |  - Write FastLED [0, 0, 0] (0mA)   |
       |  - Disable Radio (BLE.end())       |
       |  - Sleep MCU (System Off / Sleep)  |
       +---------------- +------------------+



4.1 Transition Specifications

Active Mode: MCU is fully awake, polling the IMU, scanning/broadcasting BLE packets, and updating the LED buffer.

Inactivity Timer: If the calculated motion delta does not cross the dynamic wake threshold for 35 seconds, trigger goToSleep().

Power Down Sequence (goToSleep()):

Write all WS2812B channels to absolute dark CRGB::Black. Power draw of the strip drops to $0\text{ mA}$.

Shutdown the Bluetooth stack using BLE.end() to turn off the internal high-frequency 2.4GHz oscillators.

Configure the LSM6DS3TR-C to map its internal interrupt engine (Wake-on-Shake / Significant Motion) to physical pin INT1 on the Xiao.

Force the nRF52840 into deep sleep mode. Depending on the BSP, use:
sd_power_system_off(); (SoftDevice core) or NRF_POWER->SYSTEMOFF = 1; (Bare-metal registers).

Wake-on-Shake Trigger: Physical movement triggers the IMU's hardware interrupt pin, sending a rising edge to the Xiao, cycling the MCU through a clean boot reset or interrupt service routine (ISR) to wake up into the Active State.

5. Mathematical Models & DSP Code

5.1 Three-Dimensional Tap & Magnitude Math

To catch rapid mechanical impacts on the outer shell, compute the instantaneous vector magnitude of the 3-axis accelerometer and run it through a high-pass filter to decouple gravity ($1.0\text{ G}$):

$$\text{Magnitude} = \sqrt{x^2 + y^2 + z^2}$$

$$\text{Dynamic G-Force} = \left\vert{} \text{Magnitude} - 1.0\text{ G} \right\vert{}$$

Active Tap and Axis Mapping Routine

Because both loops cross at the poles to form an "X", their orientations map beautifully to directional impacts:

Polar Tap (Y-Axis Impact): A strike on the vertical axis of the globe. Because both loops intersect at the poles, this triggers a symmetric 4-way ripple starting at the top apex (Indices 10, 28, and 29) and rushing down all legs of the "X" toward the base.

Equatorial Tap (X/Z-Axis Impact): A strike on the level horizontal plane. This triggers a wave starting at the respective loop's equatorial crossings (e.g., Indices 5/15 or 24/33) and ripples outward along the diagonals.

void processIMUPhysicalInputs() {
    float x, y, z;
    if (!myIMU.readAcceleration(x, y, z)) return;

    // 1. Vector Magnitude Calculation
    float totalMag = sqrt((x * x) + (y * y) + (z * z));

    // 2. High-Pass Isolation (Subtract static gravity vector)
    float dynamicG = abs(totalMag - 1.0);

    const float IMPACT_THRESHOLD = 0.85; // Target threshold for structural taps

    if (dynamicG > IMPACT_THRESHOLD) {
        // Evaluate dominant vector component to isolate physical hit location
        float absX = abs(x);
        float absY = abs(y - 1.0); // Assuming gravity rests on Y in default orientation
        float absZ = abs(z);

        if (absY > absX && absY > absZ) {
            // Polar Tap: Trigger symmetric 4-way pulse from the Apex downward
            triggerPolarCascade(10, 27, 28); 
        } else if (absX > absY && absX > absZ) {
            // Equatorial Tap (X-Axis dominant): Target Loop A's equator crossings
            triggerEquatorialRipple(5, 15); 
        } else if (absZ > absY && absZ > absX) {
            // Equatorial Tap (Z-Axis dominant): Target Loop B's equator crossings
            triggerEquatorialRipple(24, 33);
        }
    }
}



5.2 BLE Proximity Filtering (Hysteresis & Smoothing)

To smooth out high-frequency RF noise and multipath propagation of the 2.4GHz signal, apply an Exponential Moving Average (EMA) filter to incoming RSSI values:

$$\text{Filtered RSSI}_t = (\alpha \times \text{Raw RSSI}_t) + ((1.0 - \alpha) \times \text{Filtered RSSI}_{t-1})$$

Alpha ($\alpha$): Set to $0.15$ to achieve smooth transition profiles without introducing sluggish temporal lag.

Hysteresis Implementation: Prevent LED toggle flickering by implementing separate entry and exit gates.

Tuned Proximity Calibration Parameters

Link Budget Assumptions: $P_{TX} = +4\text{ dBm}$, $3.05\text{ m}$ ($10\text{ ft}$) distance, $5\text{ mm}$ PETG wall obstruction loss ($-4\text{ dB}$ total), internal battery/chassis shadowing ($-6\text{ dB}$ total), ceramic antenna loss ($-5\text{ dB}$ total).

Expected baseline RSSI at target threshold boundary: $-61\text{ dBm}$.

float filteredRSSI = -80.0;
const float ALPHA = 0.15;

// Calibration parameters tuned for 5mm PETG shell at 10-foot boundary
const int PROX_WAKE_GATE = -65;  // High entry gate to trigger proximity mode (~10 to 12 feet)
const int PROX_SLEEP_GATE = -71; // Low exit gate to terminate proximity mode (~15 feet)
const int PROX_SATURATION  = -45; // Saturation boundary representing near-contact (under 1 foot)

bool proximityActive = false;

void updateBLEProximity(int rawRSSI) {
    // 1. Apply EMA Filter
    filteredRSSI = (ALPHA * rawRSSI) + ((1.0 - ALPHA) * filteredRSSI);

    // 2. Apply Hysteresis Check
    if (!proximityActive && filteredRSSI > PROX_WAKE_GATE) {
        proximityActive = true;
    } else if (proximityActive && filteredRSSI < PROX_SLEEP_GATE) {
        proximityActive = false;
    }

    // 3. Translate signal data to dynamic animation factors
    if (proximityActive) {
        // Map the decibel range smoothly to an 8-bit scale
        uint8_t effectSpeed = map(constrain(filteredRSSI, PROX_SLEEP_GATE, PROX_SATURATION), 
                                  PROX_SLEEP_GATE, PROX_SATURATION, 
                                  30, 255);
        runSyncProximityAnimation(effectSpeed);
    } else {
        runAutonomousAnimation();
    }
}



6. Bluetooth Low Energy (BLE) Radio Strategy

Mode: Concurrent Broadcaster (Non-connectable Advertising) and Observer (Passive Scanning).

Connection Protocol: Do not establish formal BLE central-to-peripheral connections. This avoids connection handshaking overhead and latency, allowing instant, multiple-node proximity interactions.

TX Power Configuration: The BLE stack must be explicitly configured to transmit at $+4\text{ dBm}$ to reliably punch through the internal battery block, closely integrated electronics, and the thick $5\text{ mm}$ outer PETG shell.

Advertisement Payload: Broadcast a constant 128-bit Custom UUID unique to your device profile. Include raw data fields representing the local Device ID and active color palette indices to synchronize visual schemes across nodes over the air.

BLE Initialization and Transmission Power Setup

#include <ArduinoBLE.h>

const char* customServiceUuid = "a1b2c3d4-e5f6-7a8b-9c0d-1e2f3a4b5c6d";

void initRadioBLE() {
    if (!BLE.begin()) {
        // Handle BLE hardware failure state
        while (1);
    }

    // Explicitly set TX Power to max (+4 dBm) to meet 10-foot PETG link budget
    // Options: -40, -20, -16, -12, -8, -4, 0, 2, 3, 4 (dBm)
    if (BLE.setTxPower(4)) {
        // TX Power successfully written to nRF52840 register
    }

    // Set local name and advertised service
    BLE.setLocalName("MapSphereNode");
    BLE.setAdvertisedServiceUuid(customServiceUuid);

    // Start advertising as a non-connectable beacon to conserve energy
    BLE.advertise();
}

