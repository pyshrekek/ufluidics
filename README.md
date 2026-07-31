# ufluidics - zero-sum five-pump syringe controller

Firmware for an Elegoo Mega 2560 R3 driving five NEMA17 syringe pumps on a microfluidic device.

Two input pumps push a fixed total flow into the chip at a fixed ratio to each other.
Three output pumps withdraw exactly that same total, split three ways.
Two potentiometers set the output split live, and a maintained switch arms and disarms the system.

Mass balance holds by construction, not by tuning.
No pump is ever commanded directly: one total flow constant drives both inputs (by a fixed ratio) and all three outputs (by fractions that sum to 1.0).
There is one source of truth for flow in the entire program.

## Dependency

**AccelStepper** by Mike McCauley.
Arduino IDE: `Tools > Manage Libraries`, search "AccelStepper", install.

Used in constant-speed mode only.
`runSpeed()` is polled for all five pumps on every loop pass and no acceleration ramp is configured, because steady volumetric flow is the whole point and a ramp would corrupt the mass balance every time a pot moved.

## Repository layout

```
ufluidics/              <- must keep this directory name, see below
  ufluidics.ino         firmware: stepper setup, state machine, flow math, telemetry
  config.h              every tunable constant, plus compile-time sanity checks
  README.md             this file - wiring, calibration, verification
  HARDWARE.md           PCB spec for the ESP32-S3 revision with a web interface
  hardware/             KiCad project
    ufluidics.kicad_pro
    ufluidics.kicad_sch
    ufluidics.kicad_pcb
```

**The top-level directory name matters.** The Arduino IDE requires a sketch folder to be named exactly after its `.ino` file, so cloning this repo as anything other than `ufluidics` will make the IDE refuse to open it:

```sh
git clone <url> ufluidics
```

The three KiCad files must also keep a shared basename - that is a KiCad requirement, not a convention. Rename all three together or none.

## Configuration at a glance

At 1/16 microstepping with a 2 mm lead screw and a 14.5 mm syringe bore:

| Quantity | Value |
|---|---|
| steps/mm | 1600 |
| uL/mm | 165.13 |
| steps/uL | 9.69 |
| uL per step | 0.103 |
| Full 10 mL stroke | 60.6 mm, 96 893 steps |

Input flow is **fixed in firmware**, not pot-controlled:

| Pump | Flow | Step rate |
|---|---|---|
| IN1 | 0.1 mL/min (100 uL/min) | 16.1 steps/s |
| IN2 | 1 mL/min (1000 uL/min) | 161.5 steps/s |
| **Total** | **1.1 mL/min** | - |
| OUT3 at a 100% split (worst case) | 1.1 mL/min | 177.6 steps/s |

The two pots only set how that fixed total is divided between the three outputs.

Flow is smooth: the 0.103 uL step quantum is delivered 16 to 178 times a second, so pulsatility is not a concern.
Peak rate is 178 steps/s against the 2000 steps/s ceiling - an 11x margin, and the shortest step interval is 5.6 ms.
That is comfortably inside where a polled step generator is accurate, so AccelStepper's loop-latency behaviour is no longer a practical concern (it was, at the 11 mL/min ceiling this replaced).

### Barrel capacity is the binding constraint - read this

The mechanics are comfortable at these rates; the syringes are what limit a run.
Every barrel is currently 10 mL, and the 10:1 input ratio means they do not last remotely the same time:

| Pump | Flow | 10 mL barrel lasts |
|---|---|---|
| IN1 | 0.1 mL/min | 100 min |
| IN2 | 1 mL/min | **10 min** |
| OUT3 at a 100% split | 1.1 mL/min | **9 min** |

**IN2 is the limiting pump.** A 10 mL barrel gives it a 10 minute run.

For both inputs to exhaust together, barrel volumes want the same 10:1 proportion as the flows - 10 mL on IN1 against 100 mL on IN2 both run 100 minutes, and a 60 mL syringe on IN2 gives 60 minutes.
Nothing in firmware can extend this; it is a straight volume-over-rate limit.

The output side needs the same thought.
The three outputs collectively receive the full 1.1 mL/min, so whichever fills first ends the run - and OUT3 can be handed up to 100% of it.

**Mixing barrel sizes needs a small firmware change.** `SYRINGE_ID_MM` and `SYRINGE_VOLUME_UL` are global today, one geometry shared by all five pumps, which is also what the end-of-travel check uses.
Making them per-pump arrays in the shape of `STEP_PIN[]` and `DIR_SIGN[]` is roughly a 20 line change across both files.

### Note on resolution

The A4988 microstep table tops out at 1/16 (MS1/MS2/MS3 all high); 1/32 is not reachable on that chip.

The 2 mm lead screw compensates for that.
Dropping from 8 mm to 2 mm pitch quadruples resolution at the same microstepping: each step displaces **0.103 uL** instead of 0.41 uL.
That is finer than a DRV8825 at 1/32 would have given on the old 8 mm screw, so the driver is not the limiting factor.

Set `MICROSTEPS` and `LEADSCREW_PITCH_MM` in `config.h` to match the hardware and every derived value follows automatically.

## Wiring

Pins are freely reassignable in `config.h`; nothing depends on a particular port.

| Signal | Pin | Notes |
|---|---|---|
| STEP 1-5 | D22, D23, D24, D25, D26 | `STEP_PIN[]` |
| DIR 1-5 | D30, D31, D32, D33, D34 | `DIR_PIN[]` |
| ENABLE | D28 | Shared across all five drivers, active LOW |
| RUN switch | D2 | Maintained/latching, closed to GND. Internal pullup |
| Pot A | A0 | Sets OUT1's share |
| Pot B | A1 | Sets OUT2's share |
| Status LED | D13 | Off = idle, solid = running, blinking = fault |

The RUN switch needs no pulldown or external pullup - `INPUT_PULLUP` handles it, and an open switch reads HIGH (idle).

Pots wire as dividers: outer legs to 5 V and GND, wiper to the analog pin.

Pump index order is `IN1, IN2, OUT1, OUT2, OUT3`.
OUT1 and OUT2 are pot-controlled; OUT3 absorbs the remainder, so it is the largest stream at typical settings.

### Power

- **Set each driver's Vref before connecting any motor.** For an A4988 with 0.1 ohm sense resistors, `Vref = Imax * 8 * Rsense`; a 1.5 A NEMA17 wants roughly 1.2 V. Getting this wrong at 24 V destroys drivers and cooks motors.
- 24 V goes to driver VMOT only. The Mega runs from USB or its own barrel jack.
- **Grounds must be common** between the 24 V supply and the Mega.
- Put a 100 uF electrolytic across VMOT/GND at each driver, close to the pins.
- Never hot-plug a stepper into a powered driver. It kills the output stage.
- Confirm the MS jumpers match `MICROSTEPS`. A mismatch scales every flow rate by 2x to 16x with no error indication anywhere.

## Operation

Load **input syringes full** and **output syringes empty** before arming.
The firmware tracks travel by counting steps from this assumed starting state; it has no way to sense actual plunger position.

The RUN control is a **maintained (latching) switch**, so its position is the request - the firmware follows the level rather than toggling on an edge.

- **Close** the switch to arm. The LED goes solid and the drivers energise.
- **Open** it to disarm.
- Turn any pot at any time. Total flow and the split update live without stopping flow.
- If any syringe reaches the end of its stroke, the system halts itself, disables the drivers, blinks the LED, and prints which pump hit the limit. Open the switch to acknowledge - that resets the volume accounting and returns to idle, so it must not happen without an actual syringe reload.

**The system will not arm on power-up from an already-closed switch.** The switch has to be seen open at least once first (`g_armInhibit`). Without that, restoring power to a rig left switched on would start pumping with nobody present and with syringe positions assumed rather than known. The banner prints `[arm] RUN switch open, ready to arm` once the interlock clears.

Serial telemetry runs at **115200 baud**, one line per second:

```
state   OUT1/OUT2/OUT3 split   then per pump: uL/min (syringe uL)
RUN     50.0/30.0/20.0%   IN1 100.0(9987) IN2 1000.0(9871) OUT1 550.0(129) OUT2 330.0(77) OUT3 220.0(51)
```

The column header re-emits every 20 rows (`TELEMETRY_HEADER_ROWS`), so opening the serial monitor after boot still tells you what the numbers are - no reset needed.
Inputs report volume **remaining** in the barrel, outputs report volume **accumulated**.

The boot banner prints `uL per step`, both fixed input flows with their step rates, the input ratio, total flow, and the fastest pump's rate. Those are the numbers that tell you whether the mechanics suit the configured flow.

## Details worth knowing

**Telemetry is non-blocking.**
`Serial.print()` blocks once the 64-byte UART buffer fills, and anything that blocks stops `runSpeed()` from being polled.
AccelStepper timestamps each step as it happens rather than against a fixed schedule (its own source comments "does not account for costs in step()"), so a stalled loop turns directly into lost volume that is never made up.
Status lines are therefore staged in a buffer and drained only as fast as the UART has room.
A slow terminal throttles telemetry instead of corrupting flow.

**Both speed limits are enforced at compile time.**
`AccelStepper::setSpeed()` silently clamps against `setMaxSpeed()`, so a too-fast configuration would quietly under-deliver with no visible symptom.
`static_assert`s in `config.h` check the fastest possible pump (`MAX_PUMP_SPS`, OUT3 at a 100% split) and the slowest (`MIN_IN1_SPS`, always IN1) against the configured flow.
A wide input ratio squeezes both ends of that window at once, which is why the checks are paired: raising `IN2_TO_IN1_RATIO` pushes IN2 toward the ceiling and IN1 toward the floor simultaneously.

**The pot span has margins at both ends, and they are required.**
`POT_DEADBAND` only lets the applied value move once the reading shifts by more than 8 counts.
At the ends of travel there is nothing beyond to push it those last counts, so it strands short of the rail and 0%/100% become unreachable - the symptom is OUT3 bottoming out around 10.6% instead of 10.0% and topping out at 99.5% instead of 100%.
Real pots compound it, since track end resistance often stops the wiper reading a true 0 or 1023.
`POT_RAW_MIN`/`POT_RAW_MAX` discard the outer counts of the sweep so both rails are always reachable, and a `static_assert` enforces that the margin clears the deadband.

**There is a minimum speed, and it is a correctness limit.**
`setSpeed()` computes `_stepInterval = 1000000.0 / speed` into an `unsigned long`.
At 1 uL/min a pot near the bottom of its travel can command about 7.1e-5 steps/s, which works out to an interval of 1.4e10 us against a uint32 ceiling of 4.29e9.
It wraps, and the pump then runs roughly 3x **faster** than commanded rather than slower.
`applySpeeds()` therefore floors anything below `MIN_SPEED_SPS` to a true stop.
Verified against the library source, not assumed.

## Tuning

Everything of interest is in `config.h`:

```c
#define IN1_FLOW_UL_MIN       100.0f  // fixed, 0.1 mL/min
#define IN2_TO_IN1_RATIO      10.0f   // so IN2 = 1 mL/min, total = 1.1 mL/min
#define OUT_POT_MAX_FRAC   0.45f   // per-pot ceiling; OUT3 floor is 1 - 2x this
#define SYRINGE_ID_MM      14.5f   // MEASURE THIS
#define MICROSTEPS         16      // must match the driver jumpers
#define LEADSCREW_PITCH_MM 2.0f    // must match the actual screw
#define POT_RAW_MIN        16      // pot end margins; must exceed POT_DEADBAND
#define POT_RAW_MAX        1007
```

If a pump runs backwards, flip its entry in `DIR_SIGN[]`.
Do not rewire.

## Verification

1. **Compile.** Arduino IDE with the board set to Arduino Mega 2560, or `arduino-cli compile --fqbn arduino:avr:mega .`
2. **Bench test with no motors and no 24 V.** USB power only. Open the serial monitor, confirm the arm interlock reports the switch open, close the switch, sweep both pots. The three output percentages must always sum to 100.0. Both pots at minimum must give OUT3 exactly 100.0%, both at maximum exactly 10.0%. If the endpoints come up short (10.6%, 99.5%), the pot end margins are too small - raise `POT_RAW_MIN` and lower `POT_RAW_MAX`.
3. **Watch the STEP pins** on D22-D26 before any liquid is involved. At 1 uL/min this is not a scope measurement - it is one pulse every 12.4 s on each input pump, so an LED on the pin or a logic analyzer in single-shot is the practical tool. Count the interval against the boot banner. This is the only way to catch a microstepping jumper mismatch or a wrong `LEADSCREW_PITCH_MM`, which are otherwise completely invisible.
4. **Dry motor run** with motors off the lead screws and 24 V on. The two input motors turn one way, the three outputs the other. Expect visibly discrete steps, not rotation - that is the flow rate, not a fault.
4b. **Sanity-check the timing before trusting a long run.** Not needed at these rates - the pumps step 16 to 291 times a second, so behaviour is observable in real time. Debugging a system that steps twice a minute is otherwise very slow going.
5. **Gravimetric calibration.** This is the test that validates the whole chain. Run one input pump alone for a timed 60 s into a tared weigh boat and compare mass in mg against expected uL. A consistent percentage error means `SYRINGE_ID_MM` is wrong; correct it and the error disappears from all five pumps at once, because everything derives from that one constant.
6. **Fault check.** Temporarily set `SYRINGE_VOLUME_UL` to 200, run, and confirm the system halts and blinks on schedule.
