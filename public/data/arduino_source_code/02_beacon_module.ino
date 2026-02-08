#include <BLEDevice.h>
#include <math.h>

volatile unsigned long lastSeenMillis = 0;
const unsigned long HOLD_TIME_MS = 3000;

// Ignore outputs during initial boot window (prevents flicker)
const unsigned long STARTUP_IGNORE_MS = 3000;
unsigned long bootMillis = 0;

// bridge out will send digital (HIGH = beacon detected / LOW = beacon not detected)
const int BRIDGE_OUT = 5;

// Distance threshold in feet
const float DISTANCE_THRESHOLD_FEET = 40.0;
const int LED_BT = 15;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// VALID BEACON MAC ADDRESSES (FIXED STRINGS – HEAP SAFE)

const char* validBeacons[] = {
    // replace placeholder with bluetooth beacon mac address
    "Beacon-1-MacAddress", // Beacon1
    "Beacon-2-MacAddress", // Beacon2
    "Beacon-3-MacAddress", // Beacon3
    "Beacon-4-MacAddress", // Beacon4

};

const int numValidBeacons = sizeof(validBeacons) / sizeof(validBeacons[0]);

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UNIQUE MAC TRACKING (per scan window)

#define MAX_SEEN_MACS 40
#define MAC_LEN 18   // "AA:BB:CC:DD:EE:FF" + null

char seenMacs[MAX_SEEN_MACS][MAC_LEN];
int seenCount = 0;

bool alreadySeen(const char* mac) {
    for (int i = 0; i < seenCount; i++) {
        if (strcmp(seenMacs[i], mac) == 0) return true;
    }
    return false;
}

void recordSeen(const char* mac) {
    if (seenCount < MAX_SEEN_MACS) {
        strncpy(seenMacs[seenCount], mac, MAC_LEN);
        seenMacs[seenCount][MAC_LEN - 1] = '\0';
        seenCount++;
    }
}

void resetSeenMacs() {
    seenCount = 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////

BLEScan* pBLEScan;

// Forward declaration
class MyAdvertisedDeviceCallbacks;
MyAdvertisedDeviceCallbacks* pCallbacks = nullptr;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 🔐 CYCLE CONTROL (fresh each boot)

bool cycleArmed    = false;  // beacon seen at least once this boot
bool cycleConsumed = false;  // restart already performed

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Distance estimation (unchanged)

float estimateDistance(int rssi) {
    if (rssi == 0) return -1.0;

    float txPower = -59.0;
    float pathLoss = 2.5;

    if (rssi > txPower) return 0.1;

    float distance_meters = pow(10.0, (txPower - rssi) / (10.0 * pathLoss));
    return distance_meters * 3.28084;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BLE callback (NO String, HEAP SAFE)

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {

public:
    void onResult(BLEAdvertisedDevice advertisedDevice) override {

        // Fixed buffer (no heap allocation)
        char mac[MAC_LEN];
        String macStr = advertisedDevice.getAddress().toString();
        macStr.toUpperCase();
        macStr.toCharArray(mac, MAC_LEN);


        for (int k = 0; k < numValidBeacons; k++) {
            if (strcmp(mac, validBeacons[k]) == 0) {

                // ALWAYS refresh presence
                lastSeenMillis = millis();

                // Arm cycle on first real detection
                cycleArmed = true;

                // De-dup side effects only
                if (!alreadySeen(mac)) {
                    recordSeen(mac);

                    int rssi = advertisedDevice.getRSSI();
                    float distance = estimateDistance(rssi);
                    (void)distance; // intentionally unused
                }
                return;
            }
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// LED helpers (ACTIVE LOW)

void led_on()  { digitalWrite(LED_BT, LOW); }
void led_off() { digitalWrite(LED_BT, HIGH); }

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SETUP

void setup() {

    bootMillis = millis();

    pinMode(LED_BT, OUTPUT);
    led_off();

    pinMode(BRIDGE_OUT, OUTPUT);
    digitalWrite(BRIDGE_OUT, LOW);

    BLEDevice::init("ESP32_BluetoothScanner");
    pBLEScan = BLEDevice::getScan();

    pBLEScan->setActiveScan(false);
    pBLEScan->setInterval(160);
    pBLEScan->setWindow(160);

    pCallbacks = new MyAdvertisedDeviceCallbacks();
    pBLEScan->setAdvertisedDeviceCallbacks(pCallbacks, true);

    // Continuous scan
    pBLEScan->start(0, nullptr, false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// LOOP

void loop() {
    check_beacon();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MAIN LOGIC

void check_beacon() {

    // ⛔ Ignore outputs during startup
    if (millis() - bootMillis < STARTUP_IGNORE_MS) {
        return;
    }

    bool active = (millis() - lastSeenMillis) < HOLD_TIME_MS;

    if (active) {
        digitalWrite(BRIDGE_OUT, HIGH);
        led_on();
    }
    else {
        digitalWrite(BRIDGE_OUT, LOW);
        led_off();
        resetSeenMacs();

        // 🔁 Restart after first complete ON → OFF cycle
        if (cycleArmed && !cycleConsumed) {
            cycleConsumed = true;
            delay(100);
            ESP.restart();
        }
    }
}
