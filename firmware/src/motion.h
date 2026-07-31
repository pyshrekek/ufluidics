// motion.h - hardware-timed step generation.
//
// Five 32-bit phase accumulators on a DDS_TICK_HZ timer interrupt; one step per
// accumulator wrap. Average rate is exact to DDS_TICK_HZ / 2^32 (about 5e-6
// steps/s at 20 kHz), independent of what the main loop or the WiFi stack is
// doing.
//
// This is the whole reason the ESP32 build does not need the non-blocking
// telemetry and round-robin ADC gymnastics the AVR build required: a polled
// generator loses volume permanently whenever the loop stalls, and this one
// cannot stall.

#pragma once

#include <stdint.h>

#include "config.h"

// Configures the STEP/DIR/EN pins and starts the timer. Call from the task
// pinned to MOTION_TASK_CORE so the ISR lands on that core.
void motionBegin();

// Commanded magnitudes in steps/s. Direction is fixed per pump by DIR_SIGN and
// set once at init, so a pump never reverses mid-run.
void motionSetRates(const float stepsPerSec[N_PUMPS]);

// Drivers on/off via the shared active-low ENABLE line.
void motionEnable(bool on);

// Steps emitted since the last motionResetSteps(), per pump.
uint32_t motionSteps(uint8_t pump);

void motionResetSteps();
