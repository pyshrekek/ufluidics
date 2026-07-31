// ufluidics.ino - zero-sum five-pump syringe controller for an Elegoo Mega 2560 R3.
//
// Two input pumps push a fixed total flow into the microfluidic device at a
// fixed ratio to each other. Three output pumps withdraw exactly that same
// total, split three ways. Two potentiometers set the output split live; a
// maintained switch arms the system.
//
// Because every pump is derived from one total, mass balance holds by
// construction rather than by tuning: the three output fractions sum to 1.0
// and both inputs and outputs scale from the same number.
//
// Requires the AccelStepper library (Mike McCauley).
// Arduino IDE: Tools > Manage Libraries > search "AccelStepper".
//
// All tunables live in config.h.

#include <AccelStepper.h>

#include "config.h"

// ---------------------------------------------------------------------------
// Steppers
// ---------------------------------------------------------------------------
//
// Constant-speed mode only: runSpeed() is polled every loop iteration and no
// acceleration ramp is used. Steady volumetric flow is the entire point, and a
// ramp would corrupt the mass balance every time a pot moved.

static AccelStepper g_pump[N_PUMPS] = {
  AccelStepper(AccelStepper::DRIVER, STEP_PIN[0], DIR_PIN[0]),
  AccelStepper(AccelStepper::DRIVER, STEP_PIN[1], DIR_PIN[1]),
  AccelStepper(AccelStepper::DRIVER, STEP_PIN[2], DIR_PIN[2]),
  AccelStepper(AccelStepper::DRIVER, STEP_PIN[3], DIR_PIN[3]),
  AccelStepper(AccelStepper::DRIVER, STEP_PIN[4], DIR_PIN[4]),
};

// ---------------------------------------------------------------------------
// Controller state
// ---------------------------------------------------------------------------

enum State : uint8_t { ST_IDLE, ST_RUNNING, ST_FAULT };

static State g_state = ST_IDLE;

// Blocks arming until the RUN switch has been seen open at least once since
// boot. See the power-up note in loop().
static bool g_armInhibit = true;

static float g_flowUlS[N_PUMPS];       // commanded flow per pump, uL/s
static float g_fraction[N_PUMPS];      // each pump's share of total flow

static int32_t  g_potAcc[N_POTS];      // EMA accumulators, raw << POT_EMA_SHIFT
static uint16_t g_potApplied[N_POTS];  // last value that actually moved the rates

// ---------------------------------------------------------------------------
// Non-blocking serial
//
// Serial.print() blocks once the 64-byte UART buffer fills, and anything that
// blocks stops runSpeed() from being polled. AccelStepper timestamps each step
// as it happens rather than against a fixed schedule, so a stalled loop turns
// directly into lost volume that is never made up. Telemetry is therefore
// staged in a buffer and drained only as fast as the UART has room.
// ---------------------------------------------------------------------------

#define TX_BUF_SIZE 256

static char    s_tx[TX_BUF_SIZE];
static uint16_t s_txLen = 0;
static uint16_t s_txPos = 0;

static bool txIdle() { return s_txPos >= s_txLen; }

static void txBegin() { s_txLen = 0; s_txPos = 0; }

// Always keeps two bytes in reserve for the terminator. Without that, an
// over-long line silently loses its newline instead of its tail, and every
// subsequent line merges into one unreadable run - which looks exactly like
// telemetry having stopped.
static void txStr(const char *s) {
  while (*s && s_txLen < TX_BUF_SIZE - 2) s_tx[s_txLen++] = *s++;
}

static void txEol() {
  s_tx[s_txLen++] = '\r';
  s_tx[s_txLen++] = '\n';
}

static void txFloat(float v, uint8_t decimals) {
  char b[14];
  dtostrf(v, 0, decimals, b);
  txStr(b);
}

// Push only what fits in the UART buffer right now, then return.
static void txService() {
  int room = Serial.availableForWrite();
  while (s_txPos < s_txLen && room-- > 0) Serial.write(s_tx[s_txPos++]);
}

// ---------------------------------------------------------------------------
// Speed control
// ---------------------------------------------------------------------------

// Push the commanded flows into the steppers, or zero them when not running.
static void applySpeeds() {
  for (uint8_t i = 0; i < N_PUMPS; i++) {
    float sps = (g_state == ST_RUNNING)
                  ? g_flowUlS[i] * STEPS_PER_UL * DIR_SIGN[i]
                  : 0.0f;

    // See MIN_SPEED_SPS: below this AccelStepper's step-interval arithmetic
    // overflows and runs FASTER than asked, so stop the pump outright.
    if (fabsf(sps) < MIN_SPEED_SPS) sps = 0.0f;

    g_pump[i].setSpeed(sps);
  }
}

// Travel is tracked as steps from the assumed starting position. Position is
// signed because the outputs run backwards, so compare the magnitude.
static uint32_t stepsTaken(uint8_t i) {
  const long pos = g_pump[i].currentPosition();
  return (uint32_t)(pos < 0 ? -pos : pos);
}

// ---------------------------------------------------------------------------
// Flow math
// ---------------------------------------------------------------------------
//
// Total input flow is fixed, split between the inputs by IN2_TO_IN1_RATIO.
// POT_A sets OUT1's share of that total, POT_B sets OUT2's, and OUT3 absorbs
// the remainder. Each pot is capped at OUT_POT_MAX_FRAC, so OUT3 can never be
// starved below 1 - 2*OUT_POT_MAX_FRAC.
//
// Inputs and outputs are both derived from the same total, so mass balance
// holds at every pot position rather than only at the calibrated one.

// Map a pot reading to 0.0-1.0 across POT_RAW_MIN..POT_RAW_MAX. The outer
// counts of the sweep are deliberately discarded so both rails stay reachable;
// see POT_RAW_MIN in config.h for why that margin has to exist.
static float potFraction(uint8_t p) {
  const int16_t v = (int16_t)g_potApplied[p];
  if (v <= POT_RAW_MIN) return 0.0f;
  if (v >= POT_RAW_MAX) return 1.0f;
  return (float)(v - POT_RAW_MIN) / (float)(POT_RAW_MAX - POT_RAW_MIN);
}

static void recomputeFlows() {
  const float f2 = potFraction(0) * OUT_POT_MAX_FRAC;  // OUT1
  const float f3 = potFraction(1) * OUT_POT_MAX_FRAC;  // OUT2
  const float f4 = 1.0f - f2 - f3;                     // OUT3

  g_fraction[0] = IN1_SHARE;
  g_fraction[1] = IN2_SHARE;
  g_fraction[2] = f2;
  g_fraction[3] = f3;
  g_fraction[4] = f4;

  for (uint8_t i = 0; i < N_PUMPS; i++) g_flowUlS[i] = TOTAL_IN_UL_S * g_fraction[i];
}

// ---------------------------------------------------------------------------
// State transitions
// ---------------------------------------------------------------------------

static void enterIdle() {
  g_state = ST_IDLE;
  applySpeeds();
  digitalWrite(EN_PIN, HIGH);   // drivers off
  digitalWrite(LED_PIN, LOW);
  Serial.println(F("[state] IDLE"));
}

static void enterRunning() {
  g_state = ST_RUNNING;
  digitalWrite(EN_PIN, LOW);    // drivers on
  digitalWrite(LED_PIN, HIGH);
  applySpeeds();
  Serial.println(F("[state] RUNNING"));
}

static void enterFault(uint8_t pump) {
  g_state = ST_FAULT;
  applySpeeds();
  digitalWrite(EN_PIN, HIGH);
  Serial.print(F("[state] FAULT - end of travel on "));
  Serial.print(PUMP_NAME[pump]);
  Serial.println(F(". Reload syringes, then open the RUN switch to acknowledge."));
}

// ---------------------------------------------------------------------------
// Inputs
// ---------------------------------------------------------------------------

// Debounced position of the maintained RUN switch. Closed to GND on an
// internal pullup reads LOW, so closed = run.
//
// This is a latching switch, not a momentary button: its position IS the
// request, so the state machine follows the level rather than toggling on an
// edge. That also means the switch can be closed while the board is powered
// up, which g_armInhibit handles.
static bool runSwitchClosed() {
  static uint8_t  stable = HIGH;
  static uint8_t  last = HIGH;
  static uint32_t lastChange = 0;

  const uint8_t raw = digitalRead(BTN_PIN);
  const uint32_t now = millis();

  if (raw != last) {
    last = raw;
    lastChange = now;
  } else if ((now - lastChange) >= DEBOUNCE_MS) {
    stable = raw;
  }
  return (stable == LOW);
}

// Filter both pots and report whether either moved enough to matter. Raw ADC
// dither would otherwise make the output rates wobble continuously, so an EMA
// is followed by a deadband: a still pot produces a perfectly still flow.
// One pot per call, round-robin. analogRead() takes ~112 us, so reading all
// three back to back was a ~340 us hole in the loop - more than half a step
// interval at the top of the flow range, which shows up as lost volume because
// AccelStepper never makes up a late step. Staggering caps the hole at ~112 us.
//
// Each pot is then sampled at CONTROL_HZ/N_POTS, so the EMA settles over about
// a second. That is invisible for a hand-turned control.
static bool readPots() {
  static uint8_t p = 0;

  const int raw = analogRead(POT_PIN[p]);

  g_potAcc[p] += raw - (g_potAcc[p] >> POT_EMA_SHIFT);
  const uint16_t filtered = (uint16_t)(g_potAcc[p] >> POT_EMA_SHIFT);

  bool changed = false;
  if (abs((int)filtered - (int)g_potApplied[p]) > POT_DEADBAND) {
    g_potApplied[p] = filtered;
    changed = true;
  }

  if (++p >= N_POTS) p = 0;
  return changed;
}

// ---------------------------------------------------------------------------
// Safety
// ---------------------------------------------------------------------------
//
// Inputs start full and count down to empty; outputs start empty and count up
// to full. Both reach the same step count, so one integer comparison covers
// every pump and no floating point sits in the safety path.

static void checkTravel() {
  for (uint8_t i = 0; i < N_PUMPS; i++) {
    if (stepsTaken(i) >= STEPS_FULL_SYRINGE) {
      enterFault(i);
      return;
    }
  }
}

// ---------------------------------------------------------------------------
// Telemetry
// ---------------------------------------------------------------------------

static void printBanner() {
  Serial.println();
  Serial.println(F("=== ufluidics zero-sum five-pump controller ==="));
  Serial.print(F("microsteps        : 1/"));   Serial.println(MICROSTEPS);
  Serial.print(F("steps per mm      : "));     Serial.println(STEPS_PER_MM, 2);
  Serial.print(F("uL per mm         : "));     Serial.println(UL_PER_MM, 3);
  Serial.print(F("steps per uL      : "));     Serial.println(STEPS_PER_UL, 4);
  Serial.print(F("full stroke steps : "));     Serial.println(STEPS_FULL_SYRINGE);
  Serial.print(F("uL per step       : "));     Serial.println(1.0f / STEPS_PER_UL, 4);
  Serial.print(F("IN1 flow (fixed)  : "));     Serial.print(IN1_FLOW_UL_MIN, 1);
  Serial.print(F(" uL/min   at "));            Serial.print(MIN_IN1_SPS, 2);
  Serial.println(F(" steps/s"));
  Serial.print(F("IN2 flow (fixed)  : "));     Serial.print(IN2_FLOW_UL_MIN, 1);
  Serial.print(F(" uL/min   at "));            Serial.print(UL_MIN_TO_SPS(IN2_FLOW_UL_MIN), 2);
  Serial.println(F(" steps/s"));
  Serial.print(F("input ratio       : IN2 = "));
  Serial.print(IN2_TO_IN1_RATIO, 1);
  Serial.print(F("x IN1  -> IN1 takes "));
  Serial.print(IN1_SHARE * 100.0f, 2);
  Serial.println(F("% of total"));
  Serial.print(F("total flow        : "));     Serial.print(TOTAL_IN_UL_MIN, 1);
  Serial.println(F(" uL/min"));

  // Worst case for the step generator: OUT3 handed a 100% split.
  Serial.print(F("fastest pump      : "));     Serial.print(MAX_PUMP_SPS, 1);
  Serial.println(F(" steps/s (OUT3 at 100% split)"));
  Serial.println();
  Serial.println(F("Load INPUT syringes FULL and OUTPUT syringes EMPTY before arming."));
  Serial.println(F("Open the RUN switch, then close it to arm."));
  Serial.println();
}

// Stage one status line. Returns false if the previous one has not drained yet,
// so a slow terminal throttles telemetry instead of stalling the steppers - and
// the caller retries rather than forfeiting the slot.
static bool stageStatus() {
  if (!txIdle()) return false;

  txBegin();

  // Re-emit the column header periodically. Opening the serial monitor after
  // boot otherwise leaves you with unlabelled numbers and no way to see the
  // units short of resetting the board.
  static uint8_t rows = 0;
  if (rows == 0) {
    txStr("\r\nstate   OUT1/OUT2/OUT3 split   then per pump: uL/min (syringe uL)");
    txEol();
  }
  if (++rows >= TELEMETRY_HEADER_ROWS) rows = 0;

  switch (g_state) {
    case ST_IDLE:    txStr("IDLE    "); break;
    case ST_RUNNING: txStr("RUN     "); break;
    case ST_FAULT:   txStr("FAULT   "); break;
  }

  for (uint8_t i = N_INPUTS; i < N_PUMPS; i++) {
    txFloat(g_fraction[i] * 100.0f, 1);
    txStr(i == N_PUMPS - 1 ? "%  " : "/");
  }

  for (uint8_t i = 0; i < N_PUMPS; i++) {
    const float moved = stepsTaken(i) / STEPS_PER_UL;
    // Inputs report what is left in the barrel, outputs what they have taken in.
    const float vol = (i < N_INPUTS) ? (SYRINGE_VOLUME_UL - moved) : moved;

    txStr(" ");
    txStr(PUMP_NAME[i]);
    txStr(" ");
    txFloat(g_flowUlS[i] * 60.0f, 1);
    txStr("(");
    txFloat(vol, 0);
    txStr(")");
  }
  txEol();
  return true;
}

// ---------------------------------------------------------------------------
// Setup / loop
// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);

  for (uint8_t i = 0; i < N_PUMPS; i++) {
    g_pump[i].setMaxSpeed(STEPPER_MAX_SPS);
    g_pump[i].setMinPulseWidth(MIN_PULSE_WIDTH_US);
    g_pump[i].setSpeed(0.0f);
  }

  pinMode(EN_PIN, OUTPUT);
  digitalWrite(EN_PIN, HIGH);   // drivers off until armed
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  pinMode(BTN_PIN, INPUT_PULLUP);

  // Seed the pot filters from the current position so the first reading is
  // already correct instead of ramping in from zero.
  for (uint8_t p = 0; p < N_POTS; p++) {
    const int raw = analogRead(POT_PIN[p]);
    g_potAcc[p] = (int32_t)raw << POT_EMA_SHIFT;
    g_potApplied[p] = (uint16_t)raw;
  }

  recomputeFlows();
  printBanner();

  // Both speed limits are enforced by static_assert in config.h rather than at
  // runtime, since flow is now fixed. See MAX_PUMP_SPS and MIN_IN1_SPS.
}

void loop() {
  static uint32_t lastControl = 0;
  static uint32_t lastTelemetry = 0;
  static uint32_t lastBlink = 0;

  // Highest priority: every pump gets a chance to step on every pass.
  for (uint8_t i = 0; i < N_PUMPS; i++) g_pump[i].runSpeed();

  txService();

  const uint32_t now = millis();

  const bool wantRun = runSwitchClosed();

  // Never arm on power-up from an already-closed switch: the operator has to
  // open it once first. Without this, restoring power to a rig left switched on
  // starts pumping with nobody present and with the syringe positions assumed
  // rather than known.
  if (!wantRun && g_armInhibit) {
    g_armInhibit = false;
    Serial.println(F("[arm] RUN switch open, ready to arm"));
  }

  switch (g_state) {
    case ST_IDLE:
      if (wantRun && !g_armInhibit) enterRunning();
      break;

    case ST_RUNNING:
      if (!wantRun) enterIdle();
      break;

    case ST_FAULT:
      // A maintained switch has to be physically returned to OFF to clear a
      // fault, which guarantees someone is present for the syringe reload that
      // resetting the volume accounting assumes.
      if (!wantRun) {
        for (uint8_t i = 0; i < N_PUMPS; i++) g_pump[i].setCurrentPosition(0);
        enterIdle();
      }
      break;
  }

  if ((now - lastControl) >= (1000UL / CONTROL_HZ)) {
    lastControl = now;

    if (readPots()) {
      recomputeFlows();
      applySpeeds();      // no-op while idle; applySpeeds() commands zero
    }
    if (g_state == ST_RUNNING) checkTravel();
  }

  if (g_state == ST_FAULT && (now - lastBlink) >= 200) {
    lastBlink = now;
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }

  if ((now - lastTelemetry) >= TELEMETRY_MS && stageStatus()) {
    lastTelemetry = now;
  }
}
