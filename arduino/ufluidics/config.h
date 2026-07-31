// config.h - all tunable constants for the ufluidics five-pump controller.
//
// This file holds every number a user would reasonably want to change.
// ufluidics.ino contains logic only and reads everything from here.

#ifndef UFLUIDICS_CONFIG_H
#define UFLUIDICS_CONFIG_H

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Mechanics
// ---------------------------------------------------------------------------

#define MOTOR_STEPS_PER_REV   200      // 1.8 deg/step NEMA17

// IMPORTANT: this must match the MS1/MS2/MS3 jumpers on the driver boards.
// 16 is the A4988 hardware maximum (MS1/MS2/MS3 all high).
// A mismatch here silently scales every flow rate with no error indication.
#define MICROSTEPS            16

#define LEADSCREW_PITCH_MM    2.0f     // mm of carriage travel per motor revolution

// ---------------------------------------------------------------------------
// Syringe
//
// MEASURE YOUR OWN BARREL INNER DIAMETER WITH CALIPERS.
// This single number sets the volume-per-millimetre for the whole system, so a
// 1% error here is a 1% error on all five pumps. See README.md step 5.
// ---------------------------------------------------------------------------

#define SYRINGE_ID_MM         14.5f    // BD 10 mL Luer-Lok
#define SYRINGE_VOLUME_UL     10000.0f // usable stroke volume

// ---------------------------------------------------------------------------
// Process
// ---------------------------------------------------------------------------

// IN1 flow is fixed in firmware - it is the single source of truth. IN2 follows
// by a fixed ratio, and all three output pumps divide the resulting total. That
// is what keeps the system zero-sum.
//
// uL/min throughout, so nothing in this file mixes mL and uL.
#define IN1_FLOW_UL_MIN       70.0f    // IN1 takes 1/16 = 6.25% of total
#define IN2_TO_IN1_RATIO      15.0f    // -> IN2 1050, total 1120 uL/min

// Per-pot ceiling for the two pot-controlled outputs. OUT3 takes whatever is
// left, so its floor is 1 - 2*OUT_POT_MAX_FRAC. Must stay strictly below 0.5.
#define OUT_POT_MAX_FRAC      0.45f

// ---------------------------------------------------------------------------
// Stepper driver
// ---------------------------------------------------------------------------

// Ceiling handed to AccelStepper::setMaxSpeed(). setSpeed() clamps against it,
// so it must exceed the fastest rate the flow math can ever ask for. Generous
// here; the real limit is checked at boot against the configured flow.
#define STEPPER_MAX_SPS       2000.0f

// STEP high time. A4988 needs 1 us minimum; 2 us gives margin over wiring.
#define MIN_PULSE_WIDTH_US    2

// Floor on commanded speed, and it is a correctness guard, not a preference.
// AccelStepper::setSpeed() computes _stepInterval = 1000000.0/speed into an
// unsigned long. Below ~0.00023 steps/s that exceeds 2^32 us and silently
// wraps, producing a step rate many times FASTER than commanded. At these flow
// rates a pot near the bottom of its travel can reach that region, so anything
// slower than one step per ~33 min is commanded to a true stop instead.
#define MIN_SPEED_SPS         0.0005f

// ---------------------------------------------------------------------------
// Control loop timing
// ---------------------------------------------------------------------------

#define CONTROL_HZ            50UL     // pot read + rate recompute rate
#define TELEMETRY_MS          1000UL   // serial status period
#define TELEMETRY_HEADER_ROWS 20       // re-emit the column header this often
#define DEBOUNCE_MS           25UL     // button debounce window

#define POT_EMA_SHIFT         4        // EMA alpha = 1/16
#define POT_DEADBAND          8        // ADC counts of hysteresis before recompute

// Usable span of pot travel. Anything at or below POT_RAW_MIN reads as 0%,
// at or above POT_RAW_MAX as 100%.
//
// This margin is required, not cosmetic. POT_DEADBAND means the applied value
// only moves once the reading shifts by more than 8 counts, and at the ends of
// travel there is nothing beyond to push it those last counts - so it strands
// short of the rail and 0%/100% become unreachable. Real pots compound this:
// track end resistance often keeps the wiper from ever reading a true 0 or
// 1023. Both are absorbed by ignoring the outer counts of the sweep.
#define POT_RAW_MIN           16
#define POT_RAW_MAX           1007

// ---------------------------------------------------------------------------
// Pin map
// ---------------------------------------------------------------------------

#define EN_PIN      28    // shared across all 5 drivers, active LOW
#define BTN_PIN     2     // maintained RUN switch, closed to GND, internal pullup
#define POT_A_PIN   A0    // sets OUT1 fraction
#define POT_B_PIN   A1    // sets OUT2 fraction
                          // A2 is free: input flow is fixed, not pot-controlled
#define LED_PIN     13    // status

// ---------------------------------------------------------------------------
// Pump layout
//
// Index 0,1 = inputs  (push into the device)
// Index 2,3,4 = outputs (withdraw from the device); 2 and 3 are pot-controlled,
//               4 (OUT3) absorbs the remainder so the split always sums to 1.
// ---------------------------------------------------------------------------

#define N_PUMPS     5
#define N_INPUTS    2
#define N_POTS      2

// Both pots set the output split. Input flow is fixed in firmware.
static const uint8_t POT_PIN[N_POTS] = { POT_A_PIN, POT_B_PIN };

static const uint8_t STEP_PIN[N_PUMPS] = { 22, 23, 24, 25, 26 };
static const uint8_t DIR_PIN[N_PUMPS]  = { 30, 31, 32, 33, 34 };

// Sign of the commanded speed, which is what sets rotation direction under
// AccelStepper. Inputs dispense (+1), outputs withdraw (-1).
// If a pump physically runs the wrong way, flip its entry here. Do not rewire.
static const int8_t DIR_SIGN[N_PUMPS] = { +1, +1, -1, -1, -1 };

static const char *const PUMP_NAME[N_PUMPS] = { "IN1", "IN2", "OUT1", "OUT2", "OUT3" };

// ---------------------------------------------------------------------------
// Derived values - do not edit
// ---------------------------------------------------------------------------

#define STEPS_PER_MM   ((MOTOR_STEPS_PER_REV * (float)MICROSTEPS) / LEADSCREW_PITCH_MM)
#define UL_PER_MM      (PI * SYRINGE_ID_MM * SYRINGE_ID_MM / 4.0f)
#define STEPS_PER_UL   (STEPS_PER_MM / UL_PER_MM)
// Fixed share of total input taken by each input pump.
#define IN1_SHARE      (1.0f / (1.0f + IN2_TO_IN1_RATIO))
#define IN2_SHARE      (IN2_TO_IN1_RATIO / (1.0f + IN2_TO_IN1_RATIO))

#define IN2_FLOW_UL_MIN  (IN1_FLOW_UL_MIN * IN2_TO_IN1_RATIO)
#define TOTAL_IN_UL_MIN  (IN1_FLOW_UL_MIN * (1.0f + IN2_TO_IN1_RATIO))
#define TOTAL_IN_UL_S    (TOTAL_IN_UL_MIN / 60.0f)

// uL/min -> steps/s, for turning configured flow into rate limits.
#define UL_MIN_TO_SPS(f)  ((f) / 60.0f * STEPS_PER_UL)

// The fastest any single pump can run is the full total (OUT3 at a 100% split);
// the slowest is always IN1.
#define MAX_PUMP_SPS   UL_MIN_TO_SPS(TOTAL_IN_UL_MIN)
#define MIN_IN1_SPS    UL_MIN_TO_SPS(IN1_FLOW_UL_MIN)

// Steps for a full stroke. Same for every pump, so the end-of-travel check is a
// single integer comparison with no floating point in the safety path.
#define STEPS_FULL_SYRINGE ((uint32_t)(SYRINGE_VOLUME_UL * STEPS_PER_UL))

// ---------------------------------------------------------------------------
// Compile-time sanity checks
// ---------------------------------------------------------------------------

static_assert(OUT_POT_MAX_FRAC > 0.0f && OUT_POT_MAX_FRAC < 0.5f,
              "OUT_POT_MAX_FRAC must be in (0, 0.5) so OUT3 keeps a positive share");
static_assert(IN1_FLOW_UL_MIN > 0.0f, "IN1 flow must be positive");
static_assert(IN2_TO_IN1_RATIO > 0.0f, "input ratio must be positive");

// The fastest pump must fit under the AccelStepper speed ceiling, and the
// slowest (always IN1) must stay above the interval-overflow floor. A wide
// ratio squeezes both ends of that window at once.
static_assert(MAX_PUMP_SPS < STEPPER_MAX_SPS,
              "IN1_FLOW_UL_MIN x (1+ratio) exceeds STEPPER_MAX_SPS");
static_assert(MIN_IN1_SPS > MIN_SPEED_SPS,
              "IN1_FLOW_UL_MIN too low: IN1 falls below MIN_SPEED_SPS and stops");
static_assert(MIN_PULSE_WIDTH_US >= 1, "A4988 needs at least 1 us of STEP high time");

// The end margins must clear the deadband, or the applied value can strand
// inside the margin and the endpoints stay unreachable.
static_assert(POT_RAW_MIN > POT_DEADBAND,
              "POT_RAW_MIN must exceed POT_DEADBAND or 0% is unreachable");
static_assert(POT_RAW_MAX < 1023 - POT_DEADBAND,
              "POT_RAW_MAX must clear POT_DEADBAND or 100% is unreachable");
static_assert(POT_RAW_MAX > POT_RAW_MIN, "pot span must be positive");

#endif  // UFLUIDICS_CONFIG_H
