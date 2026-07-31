#include "flow.h"

#include <stdlib.h>

void flowCompute(float potA, float potB, Flows &out) {
  const float f2 = potA * OUT_POT_MAX_FRAC;  // OUT1
  const float f3 = potB * OUT_POT_MAX_FRAC;  // OUT2
  const float f4 = 1.0f - f2 - f3;           // OUT3, the remainder

  out.fraction[0] = IN1_SHARE;
  out.fraction[1] = IN2_SHARE;
  out.fraction[2] = f2;
  out.fraction[3] = f3;
  out.fraction[4] = f4;

  for (uint8_t i = 0; i < N_PUMPS; i++) {
    out.ulPerSec[i]    = TOTAL_IN_UL_S * out.fraction[i];
    const float sps    = out.ulPerSec[i] * STEPS_PER_UL;
    out.stepsPerSec[i] = (sps < MIN_SPEED_SPS) ? 0.0f : sps;
  }
}

void PotFilter::seed(uint16_t raw) {
  acc_ = (int32_t)raw << POT_EMA_SHIFT;
  applied_ = raw;
}

bool PotFilter::update(uint16_t raw) {
  acc_ += (int32_t)raw - (acc_ >> POT_EMA_SHIFT);
  const uint16_t filtered = (uint16_t)(acc_ >> POT_EMA_SHIFT);

  if (abs((int)filtered - (int)applied_) > POT_DEADBAND) {
    applied_ = filtered;
    return true;
  }
  return false;
}

float PotFilter::fraction() const {
  if (applied_ <= POT_RAW_MIN) return 0.0f;
  if (applied_ >= POT_RAW_MAX) return 1.0f;
  return (float)(applied_ - POT_RAW_MIN) / (float)(POT_RAW_MAX - POT_RAW_MIN);
}
