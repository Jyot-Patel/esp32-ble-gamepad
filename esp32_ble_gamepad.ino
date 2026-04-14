/*
 * ============================================================
 *  ESP32 Bluetooth HID Gamepad
 *  2 Joysticks + 8 buttons via 2x4 matrix
 * ============================================================
 *
 *  JOYSTICK WIRING
 *    Left  : VRx->34, VRy->35, SW->25
 *    Right : VRx->32, VRy->33, SW->26
 *
 *  MATRIX LAYOUT (2 rows x 4 cols = 8 buttons)
 *
 *            GP13   GP12   GP14   GP27   <- COL pins
 *  GP5        B1     B2     B3     B4    <- ROW 0
 *  GP18       B5     B6     B7     B8    <- ROW 1
 *
 *  Each button: one leg to ROW, other leg to COL.
 *  No resistors needed (INPUT_PULLUP on COL pins).
 *
 *  BLE BUTTON MAP
 *    1 = Left  joystick click
 *    2 = Right joystick click
 *    3..10 = Matrix B1..B8
 *
 *  VOLTAGE NOTE
 *    Joystick modules expect 5V but are powered at 3.6V from ESP32.
 *    ADC full-scale at 11dB attenuation = 3.9V.
 *    Effective ADC max = (3.6 / 3.9) * 4095 ≈ 3780.
 *    ADC_MAX below reflects this — do not change to 4095.
 *
 *  Library: "ESP32-BLE-Gamepad" by lemmingDev
 * ============================================================
 */

#include <BleGamepad.h>

// ── Joystick pins ────────────────────────────────────────────
#define L_JOY_X    34
#define L_JOY_Y    35
#define L_JOY_BTN  25
#define R_JOY_X    32
#define R_JOY_Y    33
#define R_JOY_BTN  26

// ── Matrix pins ──────────────────────────────────────────────
#define ROWS 2
#define COLS 4
const uint8_t rowPins[ROWS]              = { 5,  18 };
const uint8_t colPins[COLS]              = { 13, 12, 14, 27 };
const uint8_t matrixBleIndex[ROWS][COLS] = {
  { 3, 4, 5,  6  },
  { 7, 8, 9, 10  },
};

// ── Misc ─────────────────────────────────────────────────────
#define LED_PIN      2
#define DEBOUNCE_MS  20
#define DEADZONE     350    // dead band around center (~8% of range)
#define POLL_MS      10

// ── Voltage-corrected ADC max ─────────────────────────────────
// Joystick VCC = 3.6V, ADC full-scale at 11dB attenuation = 3.9V
// Effective max reading: (3.6 / 3.9) * 4095 ≈ 3780
// Stick physically cannot push ADC beyond this value.
#define ADC_MAX      3780

// Calibrated center per axis (sampled at boot, joysticks at rest)
int centerLX, centerLY, centerRX, centerRY;

// ── BLE ──────────────────────────────────────────────────────
BleGamepad pad("ESP32 Gamepad", "PRO", 100);

// ── Button debounce helpers ───────────────────────────────────
struct Btn {
  uint8_t       pin;
  uint8_t       index;       // 1-based BLE button number
  bool          last;
  bool          stable;
  unsigned long changed;
};

Btn directBtns[] = {
  { L_JOY_BTN, 1, HIGH, HIGH, 0 },
  { R_JOY_BTN, 2, HIGH, HIGH, 0 },
};

bool          mLast[ROWS][COLS];
bool          mStable[ROWS][COLS];
unsigned long mChanged[ROWS][COLS];

// ── Axis helper ───────────────────────────────────────────────
// Applies a cubic response curve so small nudges near center
// stay small, and full deflection requires deliberate push.
// Voltage-corrected: uses ADC_MAX (3780) not 4095.
int16_t toAxis(int raw, int center) {
  int v = raw - center;
  if (abs(v) < DEADZONE) return 0;

  float t, sign;
  if (v > 0) {
    sign = 1.0f;
    // Normalize: 0.0 at deadzone edge, 1.0 at physical max
    t = (float)(v - DEADZONE) / (float)(ADC_MAX - center - DEADZONE);
  } else {
    sign = -1.0f;
    t = (float)(-v - DEADZONE) / (float)(center - DEADZONE);
  }

  // Clamp to [0, 1] to handle slight ADC over-range noise
  if (t < 0.0f) t = 0.0f;
  if (t > 1.0f) t = 1.0f;

  // Cubic curve: gentle near center, needs real push for max output
  // t=0.5 deflection → 0.125 output (12.5%)
  // t=0.8 deflection → 0.512 output (51.2%)
  // t=1.0 deflection → 1.000 output (100%)
  //
  // If this feels too sluggish, soften to quadratic: t * t
  // For a blend: 0.3*t + 0.7*t*t*t
  float curve = t * t * t;

  return (int16_t)(sign * curve * 32767.0f);
}

// ── Setup ─────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  pinMode(L_JOY_BTN, INPUT_PULLUP);
  pinMode(R_JOY_BTN, INPUT_PULLUP);

  for (uint8_t r = 0; r < ROWS; r++) {
    pinMode(rowPins[r], OUTPUT);
    digitalWrite(rowPins[r], HIGH);
  }
  for (uint8_t c = 0; c < COLS; c++)
    pinMode(colPins[c], INPUT_PULLUP);

  for (uint8_t r = 0; r < ROWS; r++)
    for (uint8_t c = 0; c < COLS; c++) {
      mLast[r][c] = mStable[r][c] = HIGH;
      mChanged[r][c] = 0;
    }

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  // ── Boot calibration ─────────────────────────────────────
  // Average 64 samples with joysticks at rest to find true center.
  // Keep sticks untouched during power-on for ~0.13 seconds.
  Serial.println("Calibrating joystick centers...");
  long lx = 0, ly = 0, rx = 0, ry = 0;
  for (int i = 0; i < 64; i++) {
    lx += analogRead(L_JOY_X);
    ly += analogRead(L_JOY_Y);
    rx += analogRead(R_JOY_X);
    ry += analogRead(R_JOY_Y);
    delay(2);
  }
  centerLX = lx / 64;
  centerLY = ly / 64;
  centerRX = rx / 64;
  centerRY = ry / 64;
  Serial.printf("Centers — LX:%d LY:%d RX:%d RY:%d\n",
                centerLX, centerLY, centerRX, centerRY);

  BleGamepadConfiguration cfg;
  cfg.setAutoReport(false);
  cfg.setControllerType(CONTROLLER_TYPE_GAMEPAD);
  cfg.setButtonCount(10);
  cfg.setHatSwitchCount(0);
  cfg.setAxesMin(-32767);
  cfg.setAxesMax( 32767);

  pad.begin(&cfg);
  Serial.println("Waiting for BLE connection...");
}

// ── Scan matrix ───────────────────────────────────────────────
void scanMatrix(unsigned long now) {
  for (uint8_t r = 0; r < ROWS; r++) {
    digitalWrite(rowPins[r], LOW);
    delayMicroseconds(10);

    for (uint8_t c = 0; c < COLS; c++) {
      bool reading = digitalRead(colPins[c]);

      if (reading != mLast[r][c]) {
        mChanged[r][c] = now;
        mLast[r][c]    = reading;
      }

      if ((now - mChanged[r][c]) >= DEBOUNCE_MS && reading != mStable[r][c]) {
        mStable[r][c] = reading;
        uint8_t idx   = matrixBleIndex[r][c];
        if (reading == LOW) {
          pad.press(idx);
          Serial.printf("B%d pressed\n", idx - 2);
        } else {
          pad.release(idx);
          Serial.printf("B%d released\n", idx - 2);
        }
      }
    }

    digitalWrite(rowPins[r], HIGH);
  }
}

// ── Main loop ─────────────────────────────────────────────────
void loop() {
  if (!pad.isConnected()) {
    static unsigned long lb = 0;
    if (millis() - lb > 500) { digitalWrite(LED_PIN, !digitalRead(LED_PIN)); lb = millis(); }
    return;
  }

  digitalWrite(LED_PIN, HIGH);
  unsigned long now = millis();

  // Axes — cubic curve applied inside toAxis()
  pad.setLeftThumb (toAxis(analogRead(L_JOY_X), centerLX),
                    toAxis(analogRead(L_JOY_Y), centerLY));
  pad.setRightThumb(toAxis(analogRead(R_JOY_X), centerRX),
                    toAxis(analogRead(R_JOY_Y), centerRY));

  // Direct buttons (joystick clicks)
  for (uint8_t i = 0; i < 2; i++) {
    bool r = digitalRead(directBtns[i].pin);
    if (r != directBtns[i].last) { directBtns[i].changed = now; directBtns[i].last = r; }
    if ((now - directBtns[i].changed) >= DEBOUNCE_MS && r != directBtns[i].stable) {
      directBtns[i].stable = r;
      if (r == LOW) pad.press(directBtns[i].index);
      else          pad.release(directBtns[i].index);
    }
  }

  // Matrix buttons
  scanMatrix(now);

  pad.sendReport();
  delay(POLL_MS);
}
