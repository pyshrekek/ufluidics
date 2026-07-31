#include "motion.h"

#include <Arduino.h>
#include <esp_arduino_version.h>
#include <soc/gpio_reg.h>

// ---------------------------------------------------------------------------
// Shared with the ISR
// ---------------------------------------------------------------------------

static volatile uint32_t s_phase[N_PUMPS];
static volatile uint32_t s_inc[N_PUMPS];
static volatile uint32_t s_steps[N_PUMPS];

static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static hw_timer_t  *s_timer = nullptr;

// Bit position of each STEP pin within the GPIO_OUT1 bank (GPIO32-48).
static uint32_t s_stepBit[N_PUMPS];

// ---------------------------------------------------------------------------
// ISR
//
// Must stay in IRAM and must not touch anything in flash - no digitalWrite, no
// gpio_set_level, no logging. The register writes below are macros over a
// volatile store, which is safe.
// ---------------------------------------------------------------------------

static void IRAM_ATTR ddsIsr() {
  uint32_t pulse = 0;

  for (uint8_t i = 0; i < N_PUMPS; i++) {
    const uint32_t inc = s_inc[i];
    if (inc == 0) continue;

    const uint32_t prev = s_phase[i];
    const uint32_t next = prev + inc;
    s_phase[i] = next;
    if (next < prev) pulse |= s_stepBit[i];   // accumulator wrapped
  }

  if (pulse) {
    REG_WRITE(GPIO_OUT1_W1TS_REG, pulse);     // rising edges, all pumps at once

    for (uint8_t i = 0; i < N_PUMPS; i++) {
      if (pulse & s_stepBit[i]) s_steps[i]++;
    }

    // TMC2209 wants ~100 ns of STEP high. The counting above is not guaranteed
    // to take that long, so pad deterministically rather than hope.
    for (volatile int d = 0; d < STEP_PULSE_SPINS; d++) {
    }

    REG_WRITE(GPIO_OUT1_W1TC_REG, pulse);     // falling edges
  }
}

// ---------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------

void motionBegin() {
  for (uint8_t i = 0; i < N_PUMPS; i++) {
    s_stepBit[i] = 1u << (STEP_PIN[i] - 32);

    pinMode(STEP_PIN[i], OUTPUT);
    digitalWrite(STEP_PIN[i], LOW);

    // Direction is fixed for the life of the run, so DIR is stable long before
    // any STEP edge and the driver's setup time is never in question.
    pinMode(DIR_PIN[i], OUTPUT);
    digitalWrite(DIR_PIN[i], DIR_SIGN[i] > 0 ? HIGH : LOW);

    s_phase[i] = 0;
    s_inc[i]   = 0;
    s_steps[i] = 0;
  }

  pinMode(EN_PIN, OUTPUT);
  digitalWrite(EN_PIN, HIGH);   // drivers off until armed

#if ESP_ARDUINO_VERSION_MAJOR >= 3
  // 3.x: frequency in, alarm counted in ticks of that frequency.
  s_timer = timerBegin(DDS_TICK_HZ);
  timerAttachInterrupt(s_timer, &ddsIsr);
  timerAlarm(s_timer, 1, true, 0);
#else
  // 2.x: prescaler from the 80 MHz APB clock, alarm counted in microseconds.
  s_timer = timerBegin(0, 80, true);                       // -> 1 MHz
  timerAttachInterrupt(s_timer, &ddsIsr, true);
  timerAlarmWrite(s_timer, 1000000UL / DDS_TICK_HZ, true);
  timerAlarmEnable(s_timer);
#endif
}

void motionSetRates(const float stepsPerSec[N_PUMPS]) {
  uint32_t inc[N_PUMPS];

  for (uint8_t i = 0; i < N_PUMPS; i++) {
    const float sps = stepsPerSec[i];
    inc[i] = (sps < MIN_SPEED_SPS) ? 0u : (uint32_t)(sps * PHASE_PER_SPS);
  }

  // 32-bit stores are atomic on this core, but the five must land as a set so
  // the ratios between pumps are never briefly wrong.
  portENTER_CRITICAL(&s_mux);
  for (uint8_t i = 0; i < N_PUMPS; i++) s_inc[i] = inc[i];
  portEXIT_CRITICAL(&s_mux);
}

void motionEnable(bool on) {
  digitalWrite(EN_PIN, on ? LOW : HIGH);   // active low
}

uint32_t motionSteps(uint8_t pump) {
  portENTER_CRITICAL(&s_mux);
  const uint32_t v = s_steps[pump];
  portEXIT_CRITICAL(&s_mux);
  return v;
}

void motionResetSteps() {
  portENTER_CRITICAL(&s_mux);
  for (uint8_t i = 0; i < N_PUMPS; i++) {
    s_steps[i] = 0;
    s_phase[i] = 0;
  }
  portEXIT_CRITICAL(&s_mux);
}
