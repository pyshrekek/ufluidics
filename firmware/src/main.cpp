// ufluidics - zero-sum five-pump syringe controller, ESP32-S3.
//
// Two input pumps push a fixed total flow into the microfluidic device at a
// fixed ratio to each other. Three output pumps withdraw exactly that same
// total, split three ways. Two potentiometers set the output split; a
// maintained switch arms the system.
//
// Because every pump derives from one total, mass balance holds by
// construction rather than by tuning: the three output fractions sum to 1.0
// and both inputs and outputs scale from the same number.
//
// Structure:
//   flow.*    portable arithmetic - ported unchanged from the AVR build
//   motion.*  DDS step generator on a hardware timer
//   main.cpp  I/O, state machine, telemetry
//
// All tunables live in include/config.h.

#include <Arduino.h>

#include "config.h"
#include "flow.h"
#include "motion.h"

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

static State g_state = ST_IDLE;
static Flows g_flows;

// Blocks arming until the RUN switch has been seen open at least once since
// boot. Without it, restoring power to a rig left switched on starts pumping
// with nobody present and with syringe positions assumed rather than known.
static bool g_armInhibit = true;

static PotFilter g_pot[N_POTS];

// ---------------------------------------------------------------------------
// Inputs
// ---------------------------------------------------------------------------

// Debounced position of the maintained RUN switch. Closed to GND on an
// internal pullup reads LOW, so closed = run.
//
// This is a latching switch, not a momentary button: its position IS the
// request, so the state machine follows the level rather than toggling on an
// edge.
static bool runSwitchClosed() {
  static uint8_t  stable = HIGH;
  static uint8_t  last = HIGH;
  static uint32_t lastChange = 0;

  const uint8_t raw = digitalRead(RUN_SWITCH_PIN);
  const uint32_t now = millis();

  if (raw != last) {
    last = raw;
    lastChange = now;
  } else if ((now - lastChange) >= DEBOUNCE_MS) {
    stable = raw;
  }
  return (stable == LOW);
}

// One pot per call, round-robin. Not strictly necessary here - the DDS cannot
// lose a step to a slow loop the way AccelStepper could - but it keeps the
// control task's worst-case iteration short and costs nothing.
static bool readPots() {
  static uint8_t p = 0;

  const bool changed = g_pot[p].update((uint16_t)analogRead(POT_PIN[p]));

  if (++p >= N_POTS) p = 0;
  return changed;
}

static void applyFlows() {
  flowCompute(g_pot[0].fraction(), g_pot[1].fraction(), g_flows);

  if (g_state == ST_RUNNING) {
    motionSetRates(g_flows.stepsPerSec);
  } else {
    static const float stopped[N_PUMPS] = {0, 0, 0, 0, 0};
    motionSetRates(stopped);
  }
}

// ---------------------------------------------------------------------------
// State transitions
// ---------------------------------------------------------------------------

static void enterIdle() {
  g_state = ST_IDLE;
  applyFlows();
  motionEnable(false);
  digitalWrite(LED_RUN_PIN, LOW);
  digitalWrite(LED_FAULT_PIN, LOW);
  Serial.println(F("[state] IDLE"));
}

static void enterRunning() {
  g_state = ST_RUNNING;
  motionEnable(true);
  digitalWrite(LED_RUN_PIN, HIGH);
  applyFlows();
  Serial.println(F("[state] RUNNING"));
}

static void enterFault(uint8_t pump) {
  g_state = ST_FAULT;
  applyFlows();
  motionEnable(false);
  digitalWrite(LED_RUN_PIN, LOW);
  Serial.print(F("[state] FAULT - end of travel on "));
  Serial.print(PUMP_NAME[pump]);
  Serial.println(F(". Reload syringes, then open the RUN switch to acknowledge."));
}

// ---------------------------------------------------------------------------
// Safety
//
// Inputs start full and count down to empty; outputs start empty and count up
// to full. Both reach the same step count, so one integer comparison covers
// every pump with no floating point in the safety path.
//
// TODO: once endstops are fitted, home on startup and turn this from an
// assumption about the starting position into a measurement.
// ---------------------------------------------------------------------------

static void checkTravel() {
  for (uint8_t i = 0; i < N_PUMPS; i++) {
    if (motionSteps(i) >= STEPS_FULL_SYRINGE) {
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
  Serial.println(F("=== ufluidics zero-sum five-pump controller (ESP32-S3) ==="));
  Serial.print(F("microsteps        : 1/"));   Serial.print(MICROSTEPS);
  Serial.println(F(" commanded, interpolated to 1/256 by the driver"));
  Serial.print(F("steps per mm      : "));     Serial.println(STEPS_PER_MM, 2);
  Serial.print(F("uL per mm         : "));     Serial.println(UL_PER_MM, 3);
  Serial.print(F("steps per uL      : "));     Serial.println(STEPS_PER_UL, 4);
  Serial.print(F("uL per step       : "));     Serial.println(1.0f / STEPS_PER_UL, 4);
  Serial.print(F("full stroke steps : "));     Serial.println(STEPS_FULL_SYRINGE);
  Serial.print(F("IN1 flow (fixed)  : "));     Serial.print(IN1_FLOW_UL_MIN, 1);
  Serial.print(F(" uL/min   at "));            Serial.print(MIN_IN1_SPS, 2);
  Serial.println(F(" steps/s"));
  Serial.print(F("IN2 flow (fixed)  : "));     Serial.print(IN2_FLOW_UL_MIN, 1);
  Serial.print(F(" uL/min   at "));            Serial.print(UL_MIN_TO_SPS(IN2_FLOW_UL_MIN), 2);
  Serial.println(F(" steps/s"));
  Serial.print(F("input ratio       : IN2 = ")); Serial.print(IN2_TO_IN1_RATIO, 1);
  Serial.print(F("x IN1  -> IN1 takes "));       Serial.print(IN1_SHARE * 100.0f, 2);
  Serial.println(F("% of total"));
  Serial.print(F("total flow        : "));     Serial.print(TOTAL_IN_UL_MIN, 1);
  Serial.println(F(" uL/min"));
  Serial.print(F("fastest pump      : "));     Serial.print(MAX_PUMP_SPS, 1);
  Serial.print(F(" steps/s  (DDS tick "));     Serial.print(DDS_TICK_HZ);
  Serial.println(F(" Hz)"));
  Serial.println();
  Serial.println(F("Load INPUT syringes FULL and OUTPUT syringes EMPTY before arming."));
  Serial.println(F("Open the RUN switch, then close it to arm."));
  Serial.println();
}

static void printStatus() {
  static uint8_t rows = 0;

  // Re-emit the column header periodically. Opening the monitor after boot
  // otherwise leaves unlabelled numbers and no way to see the units.
  if (rows == 0) {
    Serial.println();
    Serial.println(F("state   OUT1/OUT2/OUT3 split   then per pump: uL/min (syringe uL)"));
  }
  if (++rows >= TELEMETRY_HEADER_ROWS) rows = 0;

  switch (g_state) {
    case ST_IDLE:    Serial.print(F("IDLE    ")); break;
    case ST_RUNNING: Serial.print(F("RUN     ")); break;
    case ST_FAULT:   Serial.print(F("FAULT   ")); break;
  }

  for (uint8_t i = N_INPUTS; i < N_PUMPS; i++) {
    Serial.print(g_flows.fraction[i] * 100.0f, 1);
    Serial.print(i == N_PUMPS - 1 ? F("%  ") : F("/"));
  }

  for (uint8_t i = 0; i < N_PUMPS; i++) {
    const float moved = motionSteps(i) / STEPS_PER_UL;
    // Inputs report what is left in the barrel, outputs what they have taken in.
    const float vol = (i < N_INPUTS) ? (SYRINGE_VOLUME_UL - moved) : moved;

    Serial.print(' ');
    Serial.print(PUMP_NAME[i]);
    Serial.print(' ');
    Serial.print(g_flows.ulPerSec[i] * 60.0f, 1);
    Serial.print('(');
    Serial.print(vol, 0);
    Serial.print(')');
  }
  Serial.println();
}

// ---------------------------------------------------------------------------
// Control task
//
// Pinned to core 1. The Arduino core runs the WiFi/TCP stack on core 0, and
// rate recomputation should never queue behind network work.
// ---------------------------------------------------------------------------

static void controlTask(void *) {
  motionBegin();

  for (uint8_t p = 0; p < N_POTS; p++) {
    g_pot[p].seed((uint16_t)analogRead(POT_PIN[p]));
  }
  applyFlows();
  printBanner();

  uint32_t lastControl = 0;
  uint32_t lastTelemetry = 0;
  uint32_t lastBlink = 0;

  for (;;) {
    const uint32_t now = millis();
    const bool wantRun = runSwitchClosed();

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
        // A maintained switch must be physically returned to OFF to clear a
        // fault, which guarantees someone is present for the syringe reload
        // that resetting the volume accounting assumes.
        if (!wantRun) {
          motionResetSteps();
          enterIdle();
        }
        break;
    }

    if ((now - lastControl) >= (1000UL / CONTROL_HZ)) {
      lastControl = now;
      if (readPots()) applyFlows();
      if (g_state == ST_RUNNING) checkTravel();
    }

    if (g_state == ST_FAULT && (now - lastBlink) >= 200) {
      lastBlink = now;
      digitalWrite(LED_FAULT_PIN, !digitalRead(LED_FAULT_PIN));
    }

    if ((now - lastTelemetry) >= TELEMETRY_MS) {
      lastTelemetry = now;
      printStatus();
    }

    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);

  pinMode(RUN_SWITCH_PIN, INPUT_PULLUP);
  pinMode(ESTOP_SENSE_PIN, INPUT_PULLUP);
  pinMode(LED_RUN_PIN, OUTPUT);
  pinMode(LED_FAULT_PIN, OUTPUT);
  digitalWrite(LED_RUN_PIN, LOW);
  digitalWrite(LED_FAULT_PIN, LOW);

  for (uint8_t i = 0; i < N_PUMPS; i++) pinMode(ENDSTOP_PIN[i], INPUT_PULLUP);

  analogReadResolution(ADC_BITS);
  // Full 0-3.3 V span. Without this the pots clip near the top of their sweep.
  analogSetAttenuation(ADC_11db);

  xTaskCreatePinnedToCore(controlTask, "control", MOTION_TASK_STACK, nullptr,
                          MOTION_TASK_PRIO, nullptr, MOTION_TASK_CORE);

  // TODO: TMC2209 UART init - RMS current, 1/16 + MicroPlyer, StealthChop.
  //       Pins TMC_UART_A_PIN (drivers 0-3) and TMC_UART_B_PIN (driver 4);
  //       a 2-bit MS1/MS2 address means one bus addresses only four.
  //
  //       MUST call driver.I_scale_analog(false) before setting the current.
  //       GCONF bit 0 defaults to 1, which makes VREF scale the run current -
  //       so on a SilentStepStick the onboard trimpot silently overrides
  //       whatever rms_current() was asked for. Clearing it selects the
  //       internal reference and makes TMC_RMS_CURRENT_MA mean what it says.
  //
  //       MUST also verify every driver answered, e.g. test_connection() == 0,
  //       and fault if not. CHOPCONF.MRES resets to 0 = 256 microsteps, so a
  //       driver that never received its config runs at 1/256 while this code
  //       sends 1/16-rate pulses - every pump then delivers 16x too little
  //       flow, silently. A mis-strapped MS1/MS2 address produces exactly that,
  //       and nothing downstream would notice.
  // TODO: WiFi (AP mode + WPA2), async web server, WebSocket telemetry at 1 Hz.
  // TODO: endstop homing on startup.
  // TODO: persist calibration in NVS via Preferences.
}

void loop() {
  // Everything runs in controlTask on core 1. Leaving loop() empty keeps the
  // Arduino housekeeping task on core 0 with the network stack, where it
  // cannot interfere with motion.
  vTaskDelay(pdMS_TO_TICKS(1000));
}
