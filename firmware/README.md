# ufluidics firmware - ESP32-S3

PlatformIO project for the PCB revision. See `../HARDWARE.md` for the board spec and `../arduino/ufluidics/` for the original Mega firmware this was ported from.

## Build and flash

```sh
cd firmware
pio run                 # build
pio run -t upload       # flash over native USB
pio device monitor      # 115200, console is USB CDC
```

No USB-serial adapter is involved - the ESP32-S3 enumerates directly, which is what removes the CP2102N from the BOM.

## Layout

| Path | Contents |
|---|---|
| `include/config.h` | Every tunable constant, the pin map, and the compile-time sanity checks |
| `src/flow.{h,cpp}` | **Portable core**: flow math, pot conditioning, state enum. No hardware calls |
| `src/motion.{h,cpp}` | DDS step generator on a hardware timer |
| `src/main.cpp` | I/O, state machine, telemetry, task setup |
| `data/` | LittleFS image for web assets (`pio run -t uploadfs`) |

`flow.cpp` moved from the AVR build without modification. That is deliberate - it is the part that took the most iterations to get right, and keeping it free of hardware calls is what let it survive the port. If it ever needs `<Arduino.h>` for anything beyond the config constants, something belongs in `main.cpp` instead.

## Why a DDS instead of a stepper library

Each pump owns a 32-bit phase accumulator advanced on a 20 kHz timer interrupt; one step is emitted per accumulator wrap. Average rate is exact to about 5e-6 steps/s.

The AVR build used AccelStepper, which steps only when the main loop polls it and timestamps from the moment the step actually happened - its own source notes `_lastStepTime = time; // Caution: does not account for costs in step()`. Lateness therefore accumulates rather than averaging out, so anything that stalls the loop becomes permanently lost volume. That drove real complexity in the AVR firmware: non-blocking telemetry staging, round-robin ADC reads, a minimum-speed guard against interval overflow.

None of that is needed here. With a WiFi stack and a web server sharing the chip, a polled generator would have been a liability.

At peak the system runs 181 steps/s - about 111 timer ticks per step. FastAccelStepper's per-variant channel limits (4 steppers on ESP32-S3 under IDF 5.3+) are what would otherwise have forced the MCU choice; a DDS makes them irrelevant.

## Notes that will bite if forgotten

**Pots must be on ADC1 (GPIO1-10).** ADC2 does not work while WiFi is active. Silicon limitation, not a firmware workaround.

**The ADC is 12-bit here, 10-bit on the Mega.** Every pot constant derives from `ADC_MAX_COUNT` for that reason. Hardcoding the AVR values would make `PotFilter::fraction()` saturate at a quarter of pot travel, pinning OUT1/OUT2 to maximum over three quarters of the sweep with no error reported anywhere.

**STEP pins must stay above GPIO32 and contiguous.** The ISR writes `GPIO_OUT1_W1TS`/`W1TC` once for all five pumps. A `static_assert` enforces the bank; the contiguity is by convention.

**GPIO22-34 do not exist on the WROOM-1 module** - the pads jump from IO21 to IO35. A `PIN_EXISTS()` `static_assert` covers every assigned pin, because a non-existent pin yields a schematic net going nowhere rather than any build or DRC error.

**The pin map avoids GPIO35-37 so it serves both N16 and N16R8.** Octal PSRAM claims that range for the SPI0/1 data lines. Only the optional SPI display header uses it, and that header is compiled out when `BOARD_HAS_PSRAM` is defined. A guarded `static_assert` fails the build if any other pin strays in there - verified by deliberately breaking it, not just written and hoped for.

Build for an R8 part with:

```sh
PLATFORMIO_BUILD_FLAGS="-DBOARD_HAS_PSRAM" pio run
```

**The ISR must stay in IRAM and out of flash.** No `digitalWrite`, no `gpio_set_level`, no logging inside `ddsIsr()`. The register macros used there are volatile stores and are safe.

**The platform version is pinned on purpose.** arduino-esp32 3.x renamed the entire timer API; `motion.cpp` compiles against both via `ESP_ARDUINO_VERSION_MAJOR`, but which one you build with should be a decision rather than an accident.

## Not yet implemented

Marked `TODO` in `main.cpp`:

- TMC2209 UART setup - RMS current, 1/16 with MicroPlyer interpolation, StealthChop. Two buses, since a 2-bit MS1/MS2 address means one bus reaches only four drivers.
- WiFi (AP mode with WPA2) and the async web server with WebSocket telemetry.
- Endstop homing, which turns the travel limit from an assumption about starting position into a measurement.
- Per-pump syringe geometry. Currently one `SYRINGE_ID_MM` for all five, which with a 15:1 input ratio gives very different barrel lifetimes.
- Calibration persistence in NVS.
