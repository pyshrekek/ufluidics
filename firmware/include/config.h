// config.h - all tunable constants for the ufluidics ESP32-S3 controller.
//
// This file holds every number a user would reasonably want to change.
// Everything else in src/ contains logic only and reads from here.
//
// Ported from the AVR build in ../../arduino/ufluidics/. The mechanics,
// syringe and process sections are identical by design - only the pin map,
// the ADC width and the step generator differ.

#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Mechanics
// ---------------------------------------------------------------------------

#define MOTOR_STEPS_PER_REV   200      // 1.8 deg/step NEMA17

// Step resolution the TMC2209 is COMMANDED at. Configure the driver for 1/16
// with MicroPlyer interpolation to 1/256: the motion is as smooth as 1/256
// while the step rate stays 16x lower. Setting 256 here instead would multiply
// every rate by 16 for no gain in smoothness.
#define MICROSTEPS            16

#define LEADSCREW_PITCH_MM    2.0f     // mm of carriage travel per motor revolution

// ---------------------------------------------------------------------------
// Syringe
//
// MEASURE YOUR OWN BARREL INNER DIAMETER WITH CALIPERS.
// This single number sets volume-per-millimetre for the whole system, so a
// 1% error here is a 1% error on all five pumps.
//
// NOTE: still one geometry shared by all five pumps. With a 15:1 input ratio
// that gives wildly different barrel lifetimes - see HARDWARE.md. Making these
// per-pump arrays is the outstanding firmware item.
// ---------------------------------------------------------------------------

#define SYRINGE_ID_MM         14.5f    // BD 10 mL Luer-Lok
#define SYRINGE_VOLUME_UL     10000.0f // usable stroke volume

// ---------------------------------------------------------------------------
// Process
// ---------------------------------------------------------------------------

// IN1 flow is fixed - it is the single source of truth. IN2 follows by a fixed
// ratio, and all three output pumps divide the resulting total. That is what
// keeps the system zero-sum.
//
// uL/min throughout, so nothing in this file mixes mL and uL.
#define IN1_FLOW_UL_MIN       70.0f    // -> IN2 1050, total 1120 uL/min
#define IN2_TO_IN1_RATIO      15.0f

// Per-pot ceiling for the two pot-controlled outputs. OUT3 takes whatever is
// left, so its floor is 1 - 2*OUT_POT_MAX_FRAC. Must stay strictly below 0.5.
#define OUT_POT_MAX_FRAC      0.45f

// ---------------------------------------------------------------------------
// Step generator
//
// DDS: each pump owns a 32-bit phase accumulator advanced every timer tick,
// and one step is emitted per accumulator wrap. Average rate is exact to
// DDS_TICK_HZ / 2^32, regardless of how slow the pump runs.
//
// This replaces AccelStepper entirely. No polling, so nothing the main loop or
// the WiFi stack does can cost a step - which was the dominant accuracy
// concern on the AVR build.
// ---------------------------------------------------------------------------

#define DDS_TICK_HZ           20000UL  // also the hard ceiling on steps/s

// Below this a pump is commanded to a true stop rather than an absurd rate.
// Unlike the AVR build this is not an overflow guard - the DDS has no such
// failure mode - it just avoids meaningless sub-microstep-per-hour rates.
#define MIN_SPEED_SPS         0.001f

// TMC2209 needs only ~100 ns of STEP high. The ISR holds it for a few hundred.
#define STEP_PULSE_SPINS      8

// ---------------------------------------------------------------------------
// TMC2209 (UART configured - no Vref trimpot)
// ---------------------------------------------------------------------------

#define TMC_RMS_CURRENT_MA    800      // per motor; 42 mm NEMA17 on a 2 mm screw
#define TMC_UART_BAUD         115200
#define TMC_STEALTHCHOP       true
#define TMC_INTERPOLATE       true     // MicroPlyer: 1/16 commanded -> 1/256 motion

// ---------------------------------------------------------------------------
// Control loop timing
// ---------------------------------------------------------------------------

#define CONTROL_HZ            50UL     // pot read + rate recompute rate
#define TELEMETRY_MS          1000UL   // serial status period
#define TELEMETRY_HEADER_ROWS 20       // re-emit the column header this often
#define DEBOUNCE_MS           25UL     // RUN switch debounce window

#define MOTION_TASK_CORE      1        // core 0 runs the WiFi/TCP stack
#define MOTION_TASK_STACK     4096
#define MOTION_TASK_PRIO      3

// ---------------------------------------------------------------------------
// ADC
//
// The ESP32 ADC is 12-bit, not the AVR's 10-bit. Every pot constant below is
// derived from ADC_MAX_COUNT for exactly this reason: hardcoding 10-bit values
// here would make potFraction() saturate at a quarter of pot travel, pinning
// OUT1/OUT2 to maximum over three quarters of the sweep with no error anywhere.
//
// Pots MUST sit on ADC1 (GPIO1-10). ADC2 does not work while WiFi is active.
// ---------------------------------------------------------------------------

#define ADC_BITS              12
#define ADC_MAX_COUNT         ((1 << ADC_BITS) - 1)   // 4095

#define POT_EMA_SHIFT         4        // EMA alpha = 1/16
#define POT_DEADBAND          (ADC_MAX_COUNT / 128)   // ~32 counts

// Usable span of pot travel; outside it reads as a hard 0% or 100%.
//
// This margin is required, not cosmetic. POT_DEADBAND means the applied value
// only moves once the reading shifts by more than the deadband, and at the ends
// of travel there is nothing beyond to push it those last counts - so it
// strands short of the rail and 0%/100% become unreachable. Real pots compound
// it: track end resistance often stops the wiper ever reading a true 0 or full
// scale. Both are absorbed by discarding the outer counts of the sweep.
#define POT_RAW_MIN           (ADC_MAX_COUNT / 64)          // ~64
#define POT_RAW_MAX           (ADC_MAX_COUNT - POT_RAW_MIN) // ~4031

// ---------------------------------------------------------------------------
// Pin map - ESP32-S3-WROOM-1-N16
//
// Avoided: GPIO19/20 (native USB D-/D+), GPIO26-32 (SPI flash),
//          GPIO0/3/45/46 (strapping).
// GPIO33-37 are only free because this is the N16 part; N16R8's octal PSRAM
// would take them.
// ---------------------------------------------------------------------------

// STEP pins must stay contiguous and all above GPIO32, so the ISR sets and
// clears every pump with a single GPIO_OUT1 register write.
#define STEP_GPIO_BASE  35
#define STEP_GPIO_MASK  0xF8u          // bits 3..7 of OUT1 = GPIO35..39

#define EN_PIN          34             // shared across all 5 drivers, active LOW
#define TMC_UART_A_PIN  33             // single-wire UART, drivers 0-3
#define TMC_UART_B_PIN  21             // single-wire UART, driver 4

#define POT_A_PIN       1              // ADC1_CH0 - OUT1 fraction
#define POT_B_PIN       2              // ADC1_CH1 - OUT2 fraction

#define RUN_SWITCH_PIN  9              // maintained, closed to GND
#define ESTOP_SENSE_PIN 10             // sense only; the cut is hardware
#define LED_RUN_PIN     43
#define LED_FAULT_PIN   44

#define I2C_SDA_PIN     11
#define I2C_SCL_PIN     12

#define TFT_SCK_PIN     13
#define TFT_MOSI_PIN    14
#define TFT_CS_PIN      16
#define TFT_DC_PIN      17
#define TFT_RST_PIN     18

// ---------------------------------------------------------------------------
// Pump layout
//
// Index 0,1 = inputs  (push into the device)
// Index 2,3,4 = outputs (withdraw); 2 and 3 are pot-controlled, 4 (OUT3)
//               absorbs the remainder so the split always sums to 1.
// ---------------------------------------------------------------------------

#define N_PUMPS   5
#define N_INPUTS  2
#define N_POTS    2

static const uint8_t STEP_PIN[N_PUMPS]  = { 35, 36, 37, 38, 39 };
static const uint8_t DIR_PIN[N_PUMPS]   = { 40, 41, 42, 47, 48 };
static const uint8_t ENDSTOP_PIN[N_PUMPS] = { 4, 5, 6, 7, 8 };
static const uint8_t POT_PIN[N_POTS]    = { POT_A_PIN, POT_B_PIN };

// Direction each pump runs in. Inputs dispense, outputs withdraw.
// If a pump physically runs the wrong way, flip its entry here. Do not rewire.
static const int8_t DIR_SIGN[N_PUMPS] = { +1, +1, -1, -1, -1 };

static const char *const PUMP_NAME[N_PUMPS] = { "IN1", "IN2", "OUT1", "OUT2", "OUT3" };

// ---------------------------------------------------------------------------
// Derived values - do not edit
// ---------------------------------------------------------------------------

#define STEPS_PER_MM   ((MOTOR_STEPS_PER_REV * (float)MICROSTEPS) / LEADSCREW_PITCH_MM)
#define UL_PER_MM      (PI * SYRINGE_ID_MM * SYRINGE_ID_MM / 4.0f)
#define STEPS_PER_UL   (STEPS_PER_MM / UL_PER_MM)

#define IN1_SHARE      (1.0f / (1.0f + IN2_TO_IN1_RATIO))
#define IN2_SHARE      (IN2_TO_IN1_RATIO / (1.0f + IN2_TO_IN1_RATIO))

#define IN2_FLOW_UL_MIN  (IN1_FLOW_UL_MIN * IN2_TO_IN1_RATIO)
#define TOTAL_IN_UL_MIN  (IN1_FLOW_UL_MIN * (1.0f + IN2_TO_IN1_RATIO))
#define TOTAL_IN_UL_S    (TOTAL_IN_UL_MIN / 60.0f)

#define UL_MIN_TO_SPS(f) ((f) / 60.0f * STEPS_PER_UL)

// Fastest any single pump can run is the full total (OUT3 at a 100% split);
// slowest is always IN1.
#define MAX_PUMP_SPS   UL_MIN_TO_SPS(TOTAL_IN_UL_MIN)
#define MIN_IN1_SPS    UL_MIN_TO_SPS(IN1_FLOW_UL_MIN)

// Phase increment per step/s for the DDS accumulators.
#define PHASE_PER_SPS  (4294967296.0f / (float)DDS_TICK_HZ)

// Steps for a full stroke. Same for every pump, so the end-of-travel check is
// a single integer comparison with no floating point in the safety path.
#define STEPS_FULL_SYRINGE ((uint32_t)(SYRINGE_VOLUME_UL * STEPS_PER_UL))

// ---------------------------------------------------------------------------
// Compile-time sanity checks
// ---------------------------------------------------------------------------

static_assert(OUT_POT_MAX_FRAC > 0.0f && OUT_POT_MAX_FRAC < 0.5f,
              "OUT_POT_MAX_FRAC must be in (0, 0.5) so OUT3 keeps a positive share");
static_assert(IN1_FLOW_UL_MIN > 0.0f, "IN1 flow must be positive");
static_assert(IN2_TO_IN1_RATIO > 0.0f, "input ratio must be positive");

// A DDS degrades as it approaches one step per tick; stay well clear.
static_assert(MAX_PUMP_SPS < (float)DDS_TICK_HZ * 0.5f,
              "peak pump rate too close to DDS_TICK_HZ - raise the tick rate");
static_assert(MIN_IN1_SPS > MIN_SPEED_SPS,
              "IN1 falls below MIN_SPEED_SPS and would be commanded to a stop");

// The single-register ISR write depends on every STEP pin living above GPIO32.
static_assert(STEP_GPIO_BASE >= 32, "STEP pins must be in the GPIO_OUT1 bank");

static_assert(POT_RAW_MIN > POT_DEADBAND,
              "POT_RAW_MIN must exceed POT_DEADBAND or 0% is unreachable");
static_assert(POT_RAW_MAX < ADC_MAX_COUNT - POT_DEADBAND,
              "POT_RAW_MAX must clear POT_DEADBAND or 100% is unreachable");
