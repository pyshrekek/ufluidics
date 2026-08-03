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
// Pin map - ESP32-S3-WROOM-1
//
// The module exposes GPIO0-21 and GPIO35-48 only - 36 pins. There is no
// GPIO22-34 on the package at all; the pad numbering jumps from IO21 to IO35.
// Check any pin against that before assigning it.
//
// Reserved: GPIO19/20 (native USB D-/D+), GPIO0/3 (strapping), GPIO43/44
// (UART0, kept free as a serial recovery port). GPIO45/46 are strapping pins
// but carry the indicator LEDs - see the LED_*_PIN note below for why that is
// safe. That leaves 30 comfortable pins on N16.
//
// Avoids GPIO35-37 for everything except the optional SPI display, so the map
// works on BOTH N16 and the octal-PSRAM N16R8. R8 consumes 35-37 for the
// SPI0/1 data lines, and a map that used them would fail on the wrong part
// with no symptom beyond pumps that never move.
//
// Budget: 26 pins used. 6 spare on N16 (35, 36, 37, 43, 44, 48), 3 on R8
// (43, 44, 48). GPIO43/44 are spare by choice, not by accident - they are the
// UART0 pair and are worth more as a recovery port than as two more IO.
// ---------------------------------------------------------------------------

// STEP pins must stay contiguous and all above GPIO32, so the ISR sets and
// clears every pump with a single GPIO_OUT1 register write.
#define STEP_GPIO_BASE  38
#define STEP_GPIO_MASK  0x7C0u         // bits 6..10 of OUT1 = GPIO38..42

#define EN_PIN          16             // shared across all 5 drivers, active LOW
#define TMC_UART_A_PIN  17             // single-wire UART, drivers 0-3
#define TMC_UART_B_PIN  18             // single-wire UART, driver 4

#define POT_A_PIN       1              // ADC1_CH0 - OUT1 fraction
#define POT_B_PIN       2              // ADC1_CH1 - OUT2 fraction

#define RUN_SWITCH_PIN  13             // maintained, closed to GND
#define ESTOP_SENSE_PIN 14             // sense only; the cut is hardware
// LEDs sit on the two strapping pins, which is safe here and deliberate.
//
// Both are anode-driven through a series resistor to GND, so the pin sources
// current when lit and is hi-Z during reset. GPIO45 and GPIO46 both have an
// internal pulldown enabled at reset, so at strap-sample time they read 0 -
// which is exactly the state both need: 45 low selects a 3.3 V VDD_SPI flash,
// 46 low enables ROM message printing. An LED cannot pull them high because
// its cathode is at ground. The firmware driving them high later is after the
// straps are latched, and a reset re-tristates the pin before the next sample.
//
// The alternative, GPIO43/44, is UART0. Those pins stay free so there is a
// serial recovery port if native USB is ever unusable, and so the ROM boot log
// does not flicker an indicator LED on every reset.
#define LED_RUN_PIN     45             // VDD_SPI strap, pulled low at reset
#define LED_FAULT_PIN   46             // ROM-print strap, pulled low at reset

#define I2C_SDA_PIN     47
#define I2C_SCL_PIN     21

// SPI display header, N16 ONLY.
//
// GPIO35/36/37 are the only spare pins on an N16 part once everything above is
// placed, and octal PSRAM claims exactly those - so on an R8 module there is no
// room and the I2C display header is the option. Compiled out rather than left
// as a comment, because pins that fight the memory bus fail at run time.
//
// Only four GPIO, not five: display RESET ties to the board reset net so the
// panel comes up with the MCU. That is standard practice and it is what makes
// the header fit at all.
//
// NOTE: GPIO33 and GPIO34 do NOT exist on the WROOM-1 module. It exposes
// GPIO0-21 and GPIO35-48 only - the pad numbering jumps straight from IO21 to
// IO35. Anything in 22-34 is either absent or bonded to the internal flash.
#ifndef BOARD_HAS_PSRAM
  #define TFT_SPI_AVAILABLE 1
  #define TFT_SCK_PIN     35
  #define TFT_MOSI_PIN    36
  #define TFT_CS_PIN      37
  #define TFT_DC_PIN      48   // free; DevKitC-1 wires it to an RGB LED
                               // RST -> board reset net, no GPIO
#else
  #define TFT_SPI_AVAILABLE 0
#endif

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

static const uint8_t STEP_PIN[N_PUMPS]  = { 38, 39, 40, 41, 42 };
static const uint8_t DIR_PIN[N_PUMPS]   = { 4, 5, 6, 7, 15 };
static const uint8_t ENDSTOP_PIN[N_PUMPS] = { 8, 9, 10, 11, 12 };
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

// The WROOM-1 module exposes GPIO0-21 and GPIO35-48 only. GPIO22-34 are not on
// the package - 22-25 do not exist on the die and 26-32 are bonded to the
// internal flash. Assigning one produces a schematic with a net going nowhere,
// which is invisible until the board is built.
#define PIN_EXISTS(p)  (((p) <= 21) || ((p) >= 35 && (p) <= 48))

static_assert(PIN_EXISTS(EN_PIN)          && PIN_EXISTS(TMC_UART_A_PIN)  &&
              PIN_EXISTS(TMC_UART_B_PIN)  && PIN_EXISTS(POT_A_PIN)       &&
              PIN_EXISTS(POT_B_PIN)       && PIN_EXISTS(RUN_SWITCH_PIN)  &&
              PIN_EXISTS(ESTOP_SENSE_PIN) && PIN_EXISTS(LED_RUN_PIN)     &&
              PIN_EXISTS(LED_FAULT_PIN)   && PIN_EXISTS(I2C_SDA_PIN)     &&
              PIN_EXISTS(I2C_SCL_PIN),
              "pin is not exposed on the ESP32-S3-WROOM-1 module");
static_assert(PIN_EXISTS(STEP_GPIO_BASE) &&
              PIN_EXISTS(STEP_GPIO_BASE + N_PUMPS - 1),
              "STEP range is not exposed on the ESP32-S3-WROOM-1 module");
#if TFT_SPI_AVAILABLE
static_assert(PIN_EXISTS(TFT_SCK_PIN) && PIN_EXISTS(TFT_MOSI_PIN) &&
              PIN_EXISTS(TFT_CS_PIN)  && PIN_EXISTS(TFT_DC_PIN),
              "SPI display pin is not exposed on the module");
#endif

// Octal PSRAM (the R8 parts) claims GPIO35-37 for the SPI0/1 data lines. A pin
// map that used them would build fine and then fight the memory bus at run
// time, so catch it here instead. Everything except the optional SPI display
// header stays clear of that range, which is what lets one map serve N16 and
// N16R8 alike.
#ifdef BOARD_HAS_PSRAM
  #define PIN_CLEARS_PSRAM(p) ((p) < 35 || (p) > 37)
  static_assert(PIN_CLEARS_PSRAM(EN_PIN)          &&
                PIN_CLEARS_PSRAM(TMC_UART_A_PIN)  &&
                PIN_CLEARS_PSRAM(TMC_UART_B_PIN)  &&
                PIN_CLEARS_PSRAM(RUN_SWITCH_PIN)  &&
                PIN_CLEARS_PSRAM(ESTOP_SENSE_PIN) &&
                PIN_CLEARS_PSRAM(LED_RUN_PIN)     &&
                PIN_CLEARS_PSRAM(LED_FAULT_PIN)   &&
                PIN_CLEARS_PSRAM(I2C_SDA_PIN)     &&
                PIN_CLEARS_PSRAM(I2C_SCL_PIN),
                "pin collides with octal PSRAM (GPIO35-37) on an R8 module");
  static_assert(PIN_CLEARS_PSRAM(STEP_GPIO_BASE) &&
                PIN_CLEARS_PSRAM(STEP_GPIO_BASE + N_PUMPS - 1),
                "STEP range collides with octal PSRAM (GPIO35-37)");
#endif

static_assert(POT_RAW_MIN > POT_DEADBAND,
              "POT_RAW_MIN must exceed POT_DEADBAND or 0% is unreachable");
static_assert(POT_RAW_MAX < ADC_MAX_COUNT - POT_DEADBAND,
              "POT_RAW_MAX must clear POT_DEADBAND or 100% is unreachable");
