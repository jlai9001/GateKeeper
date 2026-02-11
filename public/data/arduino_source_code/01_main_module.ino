// ======================================================
// GateKeeper FINAL – 24/7 Stable (NO String, NO reboot)
// Works with EDGE101 + BLE beacon controller
// ======================================================

#include <Wire.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>

// ======================================================
// LCD
// ======================================================

hd44780_I2Cexp lcd;

// ======================================================
// PINS
// ======================================================

// Inputs
const int LCD_LIGHT         = 34;
const int LIMIT_OPEN        = 12;
const int LIMIT_SHUT        = 14;
const int IR_IN             = 23;
const int IR_OUT            = 18;
const int BRIDGE_IN_BEACON  = 5;

// Outputs
const int relay  = 33;
const int LED_BT = 15;

// ======================================================
// ENUMS (MUST BE DECLARED BEFORE USE)
// ======================================================

// Gate state (replaces String gate states)
enum GateState : uint8_t {
  GS_BEGIN = 0,
  GS_SHUT,
  GS_OPEN,
  GS_SHUTTING,
  GS_OPENING
};

// LCD messages
enum LcdMsg : uint8_t {
  LCD_GATE_BEGIN = 0,
  LCD_GATE_SHUT,
  LCD_GATE_OPEN,
  LCD_GATE_SHUTTING,
  LCD_GATE_OPENING,
  LCD_BEACON_VALID,
  LCD_BEACON_NONE,
  LCD_IR_DET,
  LCD_IR_NON
};

// ======================================================
// GLOBAL STATE
// ======================================================

GateState prev_gate_state = GS_BEGIN;

// ======================================================
// HELPERS
// ======================================================

const char* gateStateToStr(GateState s) {
  switch (s) {
    case GS_SHUT:     return "shut";
    case GS_OPEN:     return "open";
    case GS_SHUTTING: return "shutting";
    case GS_OPENING:  return "opening";
    case GS_BEGIN:
    default:          return "begin";
  }
}

// ======================================================
// LED (ACTIVE LOW)
// ======================================================

void led_light(bool on) {
  digitalWrite(LED_BT, on ? LOW : HIGH);
}

// ======================================================
// LCD BACKLIGHT BUTTON
// ======================================================

void check_lcd_light_button() {
  if (digitalRead(LCD_LIGHT) == LOW) lcd.backlight();
  else lcd.noBacklight();
}

// ======================================================
// LCD UPDATE (NO String, CHANGE-ONLY)
// ======================================================

void lcdClearLine(uint8_t row) {
  lcd.setCursor(0, row);
  lcd.print("                    "); // 20 chars
}

void update_lcd(LcdMsg msg) {

  static GateState last_gate_state = GS_BEGIN;
  static char last_beacon = '?';
  static char last_ir     = '?';

  static bool beacon = false;
  static bool ir     = false;

  // Update flags
  switch (msg) {
    case LCD_BEACON_VALID: beacon = true;  break;
    case LCD_BEACON_NONE:  beacon = false; break;
    case LCD_IR_DET:       ir = true;      break;
    case LCD_IR_NON:       ir = false;     break;

    case LCD_GATE_BEGIN:    last_gate_state = GS_BEGIN;    break;
    case LCD_GATE_SHUT:     last_gate_state = GS_SHUT;     break;
    case LCD_GATE_OPEN:     last_gate_state = GS_OPEN;     break;
    case LCD_GATE_SHUTTING: last_gate_state = GS_SHUTTING; break;
    case LCD_GATE_OPENING:  last_gate_state = GS_OPENING;  break;
  }

  // Gate line
  if (msg <= LCD_GATE_OPENING) {
    lcdClearLine(0);
    lcd.setCursor(0, 0);
    lcd.print("Gate: ");
    lcd.print(gateStateToStr(last_gate_state));
  }

  // Beacon line
  char b = beacon ? 'T' : 'F';
  if (b != last_beacon) {
    last_beacon = b;
    lcdClearLine(1);
    lcd.setCursor(0, 1);
    lcd.print("Beacon: ");
    lcd.print(b);
  }

  // IR line
  char i = ir ? 'T' : 'F';
  if (i != last_ir) {
    last_ir = i;
    lcdClearLine(2);
    lcd.setCursor(0, 2);
    lcd.print("IR: ");
    lcd.print(i);
  }
}

// ======================================================
// INIT
// ======================================================

void init_outputs() {
  pinMode(LED_BT, OUTPUT);
  digitalWrite(LED_BT, HIGH);

  pinMode(relay, OUTPUT);
  digitalWrite(relay, LOW);
}

void init_limit_switches() {
  pinMode(LIMIT_OPEN, INPUT_PULLUP);
  pinMode(LIMIT_SHUT, INPUT_PULLUP);
}

void init_ir() {
  pinMode(IR_IN, INPUT_PULLUP);
  pinMode(IR_OUT, INPUT_PULLUP);
}

void init_bridges() {
  pinMode(BRIDGE_IN_BEACON, INPUT_PULLUP);
}

void init_lcd_backlight_button() {
  pinMode(LCD_LIGHT, INPUT_PULLUP);
}

void init_lcd() {
  Wire.begin();
  lcd.begin(20, 4);
  lcd.backlight();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("    SYSTEM STARTUP   ");
  lcd.setCursor(0, 1);
  lcd.print("   Initializing...   ");

  // startup delay
  delay(3000);
  lcd.noBacklight();
  update_lcd(LCD_GATE_BEGIN);
}

// ======================================================
// SENSOR CHECKS
// ======================================================

bool check_beacon() {
  bool state = digitalRead(BRIDGE_IN_BEACON) == HIGH;
  update_lcd(state ? LCD_BEACON_VALID : LCD_BEACON_NONE);
  led_light(state);
  return state;
}

bool check_ir() {
  static bool detected = false;
  static unsigned long clear_time = 0;
  const unsigned long HOLD = 500;

  if (digitalRead(IR_IN) == LOW || digitalRead(IR_OUT) == LOW) {
    if (!detected) {
      detected = true;
      update_lcd(LCD_IR_DET);
    }
    return true;
  } else {
    if (detected) {
      if (!clear_time) clear_time = millis();
      else if (millis() - clear_time >= HOLD) {
        detected = false;
        clear_time = 0;
        update_lcd(LCD_IR_NON);
      }
      return true;
    }
    return false;
  }
}

// ======================================================
// GATE STATE
// ======================================================

GateState check_gate_state() {
  bool shut = digitalRead(LIMIT_SHUT) == LOW;
  bool open = digitalRead(LIMIT_OPEN) == LOW;

  if (shut) {
    prev_gate_state = GS_SHUT;
    update_lcd(LCD_GATE_SHUT);
  }
  else if (open) {
    prev_gate_state = GS_OPEN;
    update_lcd(LCD_GATE_OPEN);
  }
  else if (prev_gate_state == GS_OPEN) {
    update_lcd(LCD_GATE_SHUTTING);
    return GS_SHUTTING;
  }
  else if (prev_gate_state == GS_SHUT) {
    update_lcd(LCD_GATE_OPENING);
    return GS_OPENING;
  }
  return prev_gate_state;
}

// ======================================================
// REMOTE CONTROL
// ======================================================

void remote_click() {
  digitalWrite(relay, HIGH);
  delay(500);
  digitalWrite(relay, LOW);
  delay(100);
}

// ======================================================
// MAIN LOGIC
// ======================================================

void shutting() {

  // ------------------------------------------------------
  // *** ADDED: Wait up to 3000ms for OPEN to release
  //            (prevents auto-close from skipping safety)
  // ------------------------------------------------------
  unsigned long waitOpenDeadline = millis() + 3000;
  while (check_gate_state() == GS_OPEN) {
    if ((long)(millis() - waitOpenDeadline) >= 0) {
      return; // never left OPEN; don't hang
    }
    delay(10);
    yield();
  }

  // ------------------------------------------------------
  // *** ADDED: Confirm we are actually SHUTTING or already SHUT
  // ------------------------------------------------------
  {
    GateState g = check_gate_state();
    if (g != GS_SHUTTING && g != GS_SHUT) return;
  }

  // ------------------------------------------------------
  // Monitor while shutting
  // ------------------------------------------------------
  while (check_gate_state() == GS_SHUTTING) {

    if (check_ir()) {
      // Reverse
      remote_click();

      // ----------------------------------------------
      // *** ADDED: Wait up to 3000ms for OPEN (timeout)
      // ----------------------------------------------
      unsigned long waitOpenAgainDeadline = millis() + 20000;
      while (check_gate_state() != GS_OPEN) {
        if ((long)(millis() - waitOpenAgainDeadline) >= 0) {
          return; // gave up waiting to reach OPEN; don't hang
        }
        delay(10);
        yield();
      }

      break; // done handling IR reversal
    }

    // *** ADDED: keep ESP32 healthy in long loop
    delay(10);
    yield();
  }

  // ------------------------------------------------------
  // Reboot after ALL confirmed SHUTS (manual or automatic)
  // ------------------------------------------------------
  if (check_gate_state() == GS_SHUT) {
    ESP.restart();
  }
}

void open_gate() {
  remote_click();
  while (check_gate_state() == GS_OPENING) delay(100);
  delay(2000);
}

void shut_gate() {
  remote_click();
  shutting();
}

bool int_delay(unsigned long ms) {
  GateState start = check_gate_state();
  unsigned long t0 = millis();

  while (millis() - t0 <= ms) {
    if (check_beacon() || check_ir()) return false;
    if (check_gate_state() != start) return false;
    delay(50);
    yield();
  }
  return true;
}

void main_conditional() {
  GateState gate = check_gate_state();
  bool beacon = check_beacon();
  bool ir = check_ir();

  if (gate == GS_SHUTTING) shutting();

  if (gate == GS_OPEN && !beacon && !ir) {
    if (int_delay(18000)) shut_gate();
  }

  if (gate == GS_SHUT && beacon) open_gate();
}

// ======================================================
// SETUP / LOOP
// ======================================================

void setup() {
  init_outputs();
  init_limit_switches();
  init_lcd();
  init_lcd_backlight_button();
  init_ir();
  init_bridges();
}

void loop() {
  check_lcd_light_button();
  main_conditional();
}
