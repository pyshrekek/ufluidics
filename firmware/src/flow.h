// flow.h - the portable core: flow math, pot conditioning, state machine.
//
// Deliberately free of any hardware call. Everything here is arithmetic on
// plain floats and integers, which is why it moved from the AVR build to the
// ESP32 build without modification. Keep it that way: if this file ever needs
// <Arduino.h> for anything but the config constants, something belongs
// elsewhere.

#pragma once

#include <stdint.h>

#include "config.h"

enum State : uint8_t { ST_IDLE, ST_RUNNING, ST_FAULT };

// Per-pump commanded flow and its share of the total.
struct Flows {
  float fraction[N_PUMPS];   // share of total input flow; sums to 1.0 over outputs
  float ulPerSec[N_PUMPS];   // commanded flow
  float stepsPerSec[N_PUMPS];// magnitude only; direction comes from DIR_SIGN
};

// Total input flow is fixed, split between the inputs by IN2_TO_IN1_RATIO.
// potA sets OUT1's share of that total, potB sets OUT2's, and OUT3 absorbs the
// remainder. Each pot is capped at OUT_POT_MAX_FRAC, so OUT3 can never be
// starved below 1 - 2*OUT_POT_MAX_FRAC.
//
// Inputs and outputs both derive from the same total, so mass balance holds at
// every pot position rather than only at a calibrated one.
void flowCompute(float potA, float potB, Flows &out);

// Exponential moving average plus a deadband, and saturation at both ends of
// the usable pot span. Returns 0.0-1.0.
//
// The EMA kills ADC dither, the deadband stops the rates wobbling on a still
// pot, and the saturation margin is what makes 0% and 100% actually reachable
// - without it the deadband strands the applied value short of each rail.
class PotFilter {
 public:
  void seed(uint16_t raw);
  // Returns true when the conditioned value moved enough to matter.
  bool update(uint16_t raw);
  float fraction() const;

 private:
  int32_t  acc_ = 0;       // raw << POT_EMA_SHIFT
  uint16_t applied_ = 0;   // last value that actually moved the rates
};
