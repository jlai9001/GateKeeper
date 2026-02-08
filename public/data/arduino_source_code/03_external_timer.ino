#include <WiFi.h>
#include "time.h"
#include <Wire.h>
#include <RTClib.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>

// ======================================================
// WIFI / TIME
// ======================================================

// replace placeholder with your network SSID and network password
const char* ssid     = "Wifi_SSID";
const char* password = "Wifi_Password";

const char* ntpServer = "pool.ntp.org";
const char* timezone  = "PST8PDT,M3.2.0,M11.1.0";

// ======================================================
// TIMERS
// ======================================================

const unsigned long RTC_SYNC_INTERVAL   = 60000;
const unsigned long WIFI_RETRY_INTERVAL = 15000;
const unsigned long BACKLIGHT_TIMEOUT   = 10000;

unsigned long lastRtcSync = 0;
unsigned long lastWifiAttempt = 0;
unsigned long backlightStartMillis = 0;

// ======================================================
// I2C BUSES
// ======================================================

TwoWire I2C_RTC(0);
TwoWire I2C_LCD(1);

const int LCD_SDA_PIN = 26;
const int LCD_SCL_PIN = 27;
const int RTC_SDA_PIN = 32;
const int RTC_SCL_PIN = 33;

// ======================================================
// LCD / RTC
// ======================================================

const int LCD_COLS = 16;
const int LCD_ROWS = 2;

RTC_DS3231 rtc;
hd44780_I2Cexp lcd;

bool rtcReady   = false;
bool ntpSynced  = false;
bool backlightOff = false;

// ======================================================
// OUTPUT
// ======================================================

const int RELAY = 12;
bool outputState = false;

// ======================================================
// CONNECTION STATUS
// ======================================================

enum ConnStatus : uint8_t {
  CS_INIT,
  CS_CONN,
  CS_ONLINE,
  CS_OFFLINE
};

ConnStatus connectionStatus = CS_INIT;

const char* connStatusToStr(ConnStatus s) {
  switch (s) {
    case CS_CONN:    return "CONN";
    case CS_ONLINE:  return "ONLINE";
    case CS_OFFLINE: return "OFFLINE";
    default:         return "INIT";
  }
}

// ======================================================
// DISPLAY STATE (ANTI-FLICKER)
// ======================================================

bool lastOutputState = false;
ConnStatus lastConnStatus = CS_INIT;
uint8_t lastMinute = 255;

// ======================================================
// SCHEDULE
// ======================================================

const char* scheduleStrings[] = {
  // replace with your schedule
  "MON 8:00-9:00",
  "TUE 18:00-19:00",
  "WED 7:00-8:00",
};

const int SCHEDULE_SIZE =
  sizeof(scheduleStrings) / sizeof(scheduleStrings[0]);

// ======================================================
// SETUP
// ======================================================

void setup() {
  Serial.begin(115200);
  pinMode(RELAY, OUTPUT);

  // I2C
  I2C_RTC.begin(RTC_SDA_PIN, RTC_SCL_PIN);
  I2C_LCD.begin(LCD_SDA_PIN, LCD_SCL_PIN);

  // LCD
  Wire = I2C_LCD;
  lcd.begin(LCD_COLS, LCD_ROWS);
  lcd.clear();
  lcd.backlight();
  backlightStartMillis = millis();

  // RTC (authoritative at boot)
  Wire = I2C_RTC;
  if (rtc.begin()) {
    delay(20);
    rtcReady = true;
  }

  // Draw immediately from RTC
  if (rtcReady) {
    DateTime now = rtc.now();
    drawDisplay(now);
    lastMinute = now.minute();
  }

  // WiFi (background)
  WiFi.setSleep(false);
  WiFi.begin(ssid, password);
  lastWifiAttempt = millis();

  // SNTP (non-blocking)
  configTime(0, 0, ntpServer);
  setenv("TZ", timezone, 1);
  tzset();
}

// ======================================================
// LOOP
// ======================================================

void loop() {
  unsigned long nowMillis = millis();

  // Backlight off after 10s (independent)
  if (!backlightOff &&
      nowMillis - backlightStartMillis >= BACKLIGHT_TIMEOUT) {
    Wire = I2C_LCD;
    lcd.noBacklight();
    backlightOff = true;
  }

  handleWiFi(nowMillis);

  if (!rtcReady) return;

  // First successful NTP → RTC
  if (!ntpSynced && WiFi.status() == WL_CONNECTED) {
    struct tm t;
    if (getLocalTime(&t, 0)) {
      syncRTCFromNTP(t);
      ntpSynced = true;
      lastRtcSync = nowMillis;
    }
  }

  // Periodic RTC correction
  if (ntpSynced &&
      nowMillis - lastRtcSync >= RTC_SYNC_INTERVAL) {
    struct tm t;
    if (getLocalTime(&t, 0)) {
      syncRTCFromNTP(t);
      lastRtcSync = nowMillis;
    }
  }

  // Output
  updateOutputState();

  // Display update (minute-based)
  Wire = I2C_RTC;
  DateTime now = rtc.now();

  bool needRedraw =
    (outputState != lastOutputState) ||
    (connectionStatus != lastConnStatus) ||
    (now.minute() != lastMinute);

  if (needRedraw) {
    drawDisplay(now);
    lastOutputState = outputState;
    lastConnStatus = connectionStatus;
    lastMinute = now.minute();
  }
}

// ======================================================
// WIFI (NON-BLOCKING)
// ======================================================

void handleWiFi(unsigned long nowMillis) {
  if (WiFi.status() == WL_CONNECTED) {
    connectionStatus = CS_ONLINE;
    return;
  }

  if (nowMillis - lastWifiAttempt >= WIFI_RETRY_INTERVAL) {
    WiFi.begin(ssid, password);
    lastWifiAttempt = nowMillis;
    connectionStatus = CS_CONN;
  } else {
    connectionStatus = CS_OFFLINE;
  }
}

// ======================================================
// DISPLAY (UNCHANGED SEMANTICS)
// ======================================================

void drawDisplay(DateTime now) {
  Wire = I2C_LCD;
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print(outputState ? "OUT:T" : "OUT:F");

  const char* status = connStatusToStr(connectionStatus);
  lcd.setCursor(LCD_COLS - strlen(status), 0);
  lcd.print(status);

  const char* days[] =
    {"SUN","MON","TUE","WED","THU","FRI","SAT"};

  char line[17];
  snprintf(line, sizeof(line), "%s %02d:%02d",
           days[now.dayOfTheWeek()],
           now.hour(),
           now.minute());

  lcd.setCursor((LCD_COLS - strlen(line)) / 2, 1);
  lcd.print(line);
}

// ======================================================
// RTC / OUTPUT
// ======================================================

void syncRTCFromNTP(const struct tm& t) {
  Wire = I2C_RTC;
  rtc.adjust(DateTime(
    t.tm_year + 1900,
    t.tm_mon + 1,
    t.tm_mday,
    t.tm_hour,
    t.tm_min,
    t.tm_sec
  ));
}

void updateOutputState() {
  Wire = I2C_RTC;
  DateTime now = rtc.now();
  outputState = isWithinSchedule(now);
  digitalWrite(RELAY, outputState ? HIGH : LOW);
}

bool isWithinSchedule(DateTime now) {
  const char* days[] = {"SUN","MON","TUE","WED","THU","FRI","SAT"};
  const char* today = days[now.dayOfTheWeek()];
  int curMin = now.hour() * 60 + now.minute();

  for (int i = 0; i < SCHEDULE_SIZE; i++) {
    char d[4]; int sh, sm, eh, em;
    if (sscanf(scheduleStrings[i], "%3s %d:%d-%d:%d",
               d, &sh, &sm, &eh, &em) == 5) {
      if (!strcmp(d, today)) {
        int s = sh * 60 + sm;
        int e = eh * 60 + em;
        if (curMin >= s && curMin < e) return true;
      }
    }
  }
  return false;
}
