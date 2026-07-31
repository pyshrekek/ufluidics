# ufluidics - PCB hardware specification

Target: a single board carrying five stepper drivers, an MCU, and a web interface, replacing the Mega + A4988 breakout build.

KiCad project lives in `hardware/`. Net-by-net wiring is in [hardware/CONNECTIONS.md](hardware/CONNECTIONS.md).

## MCU selection

### First: the constraint everyone assumes is binding, is not

The obvious way to choose is by counting hardware step channels, since FastAccelStepper's support varies sharply by chip:

| Variant | Hardware step channels (FastAccelStepper) |
|---|---|
| ESP32 (classic) | 14 (6 MCPWM/PCNT + 8 RMT) |
| ESP32-S3 | 8 on IDF 4.x, **4** on IDF 5.3+ |
| ESP32-S2 / C3 / C6 | 4 / 2 / 2 |
| RP2040 (Pico W) | 4 under the Arduino core |
| RP2350 (Pico 2 W) | 8 under the Arduino core |

By that table only classic ESP32 and RP2350 clear five pumps, and ESP32-S3 is a trap - it advertises 8 but drops to 4 on IDF 5.3+, so a routine toolchain bump breaks the build.

**But this project does not need any of it.** Two facts collapse the constraint:

1. **Drive the TMC2209 at 1/16 with MicroPlyer interpolation to 1/256**, rather than 1/256 natively. The driver interpolates internally, so you get 1/256 smoothness at 1/16 step rates. Native 1/256 would multiply every rate by 16 for no benefit.
2. At that setting the **peak rate across the whole system is 181 steps/s** (OUT3 at a 100% split, current flow config).

A DDS step generator on a 20 kHz hardware-timer ISR - five 32-bit phase accumulators, one step per accumulator wrap - gives **111 timer ticks per step at peak** and a rate resolution of 5e-6 steps/s. On a 240 MHz dual-core part with the motion task pinned to core 1, that is not close to a limit.

This design was written and bench-compiled for the Mega early in this project, then replaced by AccelStepper before the repository existed - so it predates git history and has to be rewritten rather than recovered. It is roughly 40 lines.

So: **use a timer DDS, not FastAccelStepper**, and the per-variant channel table stops mattering. The MCU can then be chosen on the things that actually shape the board - GPIO count, ADC behaviour, USB, ecosystem.

### The choice

| | ESP32-WROOM-32E | **ESP32-S3-WROOM-1-N16** | RP2350B |
|---|---|---|---|
| Usable GPIO | 19 out + 4 in-only | **30** | 48 |
| I/O expander needed | **Yes** | No | No |
| ADC usable with WiFi | ADC1 only, 6 ch | **ADC1, 10 ch** | All, no conflict |
| Native USB | No - needs CP2102N | **Yes** | Yes |
| WiFi | Mature, on-die | **Mature, on-die** | External CYW43439 |
| Web stack maturity | Best | Best | Weaker (lwIP via arduino-pico) |
| Cost | ~$3 | ~$4 | ~$2 + radio |

**Recommendation: ESP32-S3-WROOM-1-N16.**

It wins on three concrete things:

- **30 usable GPIO deletes the MCP23017.** The module exposes GPIO0-21 and GPIO35-48 (36 pins); reserving GPIO19/20 for USB and GPIO0/3/45/46 as strapping leaves 30. The pin budget that forced an expander was a classic-ESP32 problem, not an ESP32 problem.
- **Native USB deletes the CP2102N**, its crystal and its support parts. Programming and console come straight off USB-C, and DFU becomes available.
- **ADC1 has 10 channels on GPIO1-10 and works with WiFi up.** Classic ESP32 gives you 6, overlapping the input-only pins the endstops want. That collision was the other half of the pin problem.

**N16 is preferred, but the firmware pin map works on either.** Octal PSRAM (the R8 parts) consumes GPIO35-37 for the SPI0/1 data lines, so `firmware/include/config.h` keeps everything except the optional SPI display header clear of that range. A `static_assert` guarded by `BOARD_HAS_PSRAM` fails the build if a pin ever strays into it, rather than letting the board fight its own memory bus at run time.

What you give up on R8:

| | N16 | N16R8 |
|---|---|---|
| Usable GPIO | 30 | 27 |
| Pins used by this design | 26 | 26 |
| Spare | 4 (35, 36, 37, 48) | 1 (48) |
| SPI display header (GPIO35-37) | Available | **Not available - use I2C** |

The 8 MB of PSRAM buys nothing here. A single-page web UI is a few hundred KB, the build currently uses 5.9% of SRAM, and an ILI9341 driven by Adafruit_GFX draws directly rather than from a framebuffer. So R8 costs five GPIO and the SPI display option in exchange for memory this design does not use - but if it is what you can get, it works.

RP2350B is technically the most elegant - PIO state machines are purpose-built for exactly this - but the WiFi is a separate CYW43439 die and the Arduino web stack is meaningfully less mature. Not worth it when step generation is a solved non-problem.

**Fallback:** if you would rather not move, classic ESP32 + MCP23017 works and is fully specified below. It is a proven, boring design. It just costs an extra chip and eight pins of headroom to reach the same place.

### Do you need a Raspberry Pi?

No, and adding one makes things worse.

Linux has no hardware step generation on GPIO and no real-time guarantee, so a Pi driving steppers directly produces visible flow jitter. You would end up with a Pi for the UI plus an MCU for motion - two processors, an inter-processor protocol, and two firmware images to keep in sync.

The ESP32 serves the web UI itself. Add a Pi only if you later want camera capture, long-term database logging, or on-box Python analysis - and then it talks to the ESP32 over HTTP/MQTT and stays out of the motion path.

## Bill of materials

### Controller

| Item | Part | Qty | Notes |
|---|---|---|---|
| MCU module | **ESP32-S3-WROOM-1-N16** | 1 | 16 MB flash. N16R8 also works but costs GPIO35-37, hence the SPI display header |
| USB-serial | **none** | 0 | ESP32-S3 has native USB. Wire D+/D- to GPIO20/19 |
| USB connector | USB-C receptacle | 1 | 5.1k pulldowns on both CC pins |
| Auto-reset | 2x transistor (DTR/RTS) | 1 set | Only if you also fit a serial header; native USB does not need it |
| Boot/reset buttons | Tactile SMD | 2 | GPIO0 needs 10k to 3V3; EN needs 10k + 100 nF |
| Crystal / oscillator | **none** | 0 | 40 MHz crystal, flash and RF matching are inside the module |

### Stepper drivers

**TMC2209, five off.** This is a significant upgrade over the A4988 and fixes several problems in the current build at once.

| Advantage | What it fixes |
|---|---|
| 1/256 microstepping with MicroPlyer interpolation | 16x finer than the A4988's 1/16 ceiling. Step quantum drops from 0.103 uL to 0.0065 uL |
| Current set over UART, in software | **Removes the Vref trimpot entirely** - the single most dangerous step in the current build, where a mis-set pot destroys drivers at 24 V |
| StealthChop2 | Near-silent. Matters in a lab, and reduces vibration coupling into the fluidics |
| StallGuard4 | Sensorless endstop detection, optional backup to mechanical switches |
| CoolStep | Cuts idle current and heat |
| 4.75-29 V VM | 24 V is comfortably in range |

Current rating is 1.4 A RMS / 2 A peak, which suits a 42 mm NEMA17 driving a 2 mm lead screw. Syringe plunger force is modest and the screw's mechanical advantage is large, so this is not marginal.

If you later need more force, TMC5160 (SPI, external MOSFETs) is the step up. Do not use it unless measurement says you need it.

**UART addressing detail that affects the schematic:** a TMC2209 takes a 2-bit address from MS1/MS2, so **one UART line addresses four drivers**. For five you need a second bus. On the ESP32-S3 the console runs over native USB, which frees all three hardware UARTs - use two of them and do not try to put all five drivers on one bus.

| Item | Part | Qty |
|---|---|---|
| Driver | TMC2209 (bare IC for integrated PCB, or SilentStepStick modules on headers) | 5 |
| Sense resistors | 0.11 ohm 1% (if bare IC) | 10 |
| Driver decoupling | 100 uF electrolytic + 100 nF, per driver, close to VM | 5 sets |

Bare ICs give a cleaner board and better thermals; socketed modules let you swap a failed driver in the field. For a first spin, **socket them** - you will want to swap drivers while debugging.

### Power

24 V in, with 5 V and 3.3 V derived on board.

| Item | Part | Notes |
|---|---|---|
| Input connector | 5.08 mm screw terminal, 2-pin | Or barrel jack; terminal is more secure |
| Fuse | 5 A slow-blow, holder or polyfuse | |
| Reverse polarity | P-MOSFET (e.g. SI2333) in the high side | Better than a Schottky - no 0.5 V drop, no heat |
| Input TVS | SMBJ26A | Clamps inductive kickback from motor leads |
| Bulk cap | 470-1000 uF, 35 V, low ESR | Plus the per-driver 100 uF above |
| 24 V -> 5 V | **Buck rated 40 V or higher**: TPS54360 (60 V, 3.5 A) or LMR14030 (40 V, 3 A). LM2596 (40 V) as the cheap ubiquitous fallback | See voltage rating note below |
| 5 V -> 3.3 V | AMS1117-3.3 (1 A) with copper pour, or AP2112K-3.3 | ESP32 WiFi peaks near 500 mA |
| 3.3 V bulk | 22 uF + 100 nF at the module | Espressif minimum is 10 uF; WiFi TX is bursty |

### Power topology

The ESP32-WROOM module has **no onboard regulator** - that is a dev-board feature. On a custom PCB you need both stages:

```
24 V --[buck, 40V+ rated]--> 5 V --[LDO]--> 3.3 V --> ESP32
```

If you socket an ESP32 DevKitC instead of reflowing a bare module, it brings its own 3.3 V LDO, and you only need the 24 V -> 5 V stage feeding its 5 V/VIN pin. That is the lower-risk option for a first board spin.

**Never linear-regulate from 24 V.** At the ESP32's 500 mA WiFi peak, a 24 V -> 3.3 V linear regulator dissipates **10.4 W**. The same LDO from 5 V dissipates 0.85 W, which is why the intermediate rail exists. This is not an efficiency preference; it is the difference between a warm part and a fire.

**Buck input rating is not a place to economise.** Common parts like TPS54331 and MP1584 are rated 28 V max. On a 24 V rail shared with five inductive loads, motor back-EMF and hot-unplug transients will exceed that, and the failure mode is a shorted high-side FET putting 24 V onto the 5 V rail and through everything downstream. Specify **40 V minimum**, and keep the input TVS.

**Logic current is negligible.** The whole 5 V rail - ESP32, USB-serial, OLED, LEDs - runs about 800 mA, which is roughly 200 mA drawn from 24 V. Size the buck for 2-3 A anyway; the margin is nearly free and covers a fan.

**PSU sizing:** five motors at 24 V, chopper-driven, average maybe 0.5 A each into the drivers. Specify a **24 V 5 A (120 W)** supply. That is generous, and generous is correct for inductive loads.

### I/O and safety

| Item | Qty | Notes |
|---|---|---|
| **E-stop** | 1 | **Hardware path that pulls all five driver EN high, independent of firmware.** Not a GPIO the software polls |
| Endstops | 5 | One per pump, NC microswitch or optical. See below |
| RUN switch | 1 | Maintained, as the current firmware expects |
| Pots | 0-2 | Optional once there is a web UI. **ADC1 pins only** - see gotcha below |
| Status LEDs | 3-5 | Power, run, fault |
| Display | 1 | On a header - see the display section below |
| I/O expander | 0 | Not needed on ESP32-S3. Required only on the classic-ESP32 fallback |
| Motor connectors | 5 | JST-XH 4-pin |
| Endstop connectors | 5 | JST-XH 3-pin |

**Add the endstops.** The current firmware assumes plunger positions at boot and counts steps from there - it has no way to know where a plunger actually is, and a skipped step is silent permanent error. Five endstops let the system home on startup and turn the travel limit from an assumption into a measurement. This is the single biggest reliability gain available.

## Display

Put it on a header rather than soldering it down - it lets you change your mind about size, and a failed display never bricks the board.

| Option | Interface | Header | Cost | Verdict |
|---|---|---|---|---|
| 0.96" OLED SSD1306, 128x64 | I2C | 4-pin | ~$3 | Workable, but 8 lines of ~21 chars is tight for five pumps plus ratios |
| 1.3" OLED SH1106, 128x64 | I2C | 4-pin | ~$5 | Same pixels, easier to read across a bench |
| 2.42" OLED SSD1309, 128x64 | I2C | 4-pin | ~$12 | Same layout code, genuinely readable at arm's length |
| **2.4" TFT ILI9341, 240x320** | SPI | 8-pin | ~$8 | **Best fit.** Room for all five pumps, colour-codes state, still cheap |
| 20x4 character LCD | I2C | 4-pin | ~$8 | Very readable, but no graphics and an awkward aspect for this data |

For a bench instrument you glance at while doing something else, **the 2.4" colour TFT is the right call.** Five pump rows with names, uL/min, percentage and syringe volume is about 20 lines of text - which a 128x64 OLED can only show by paging, and paging is exactly what you do not want when checking a running experiment at a glance.

Fit **both headers** on the board. An I2C display header costs four pins that are already routed for the expander, and the SPI header costs five GPIO you will have spare once the expander is in place.

| Header | Pins | Availability |
|---|---|---|
| I2C display | 3.3 V, GND, SDA, SCL (4.7k pullups on board) | Both N16 and N16R8 |
| SPI display | 3.3 V, GND, SCK, MOSI, CS, DC, BL. RST ties to the board reset net | **N16 only** - sits on GPIO35-37 |

Drive both at **3.3 V logic**. Most OLED modules are fine; check any ILI9341 module, as some carry 5 V level shifters and some do not.

Show on it what the serial telemetry already computes: state, the three output split percentages, and per pump the commanded uL/min with remaining or accumulated syringe volume. The data model is identical - only the renderer differs.

## Pin budget

On **ESP32-S3-WROOM-1-N16** everything fits directly, with headroom:

| Function | Pins |
|---|---|
| STEP x5 | 5 |
| DIR x5 | 5 |
| ENABLE, shared | 1 |
| TMC2209 UART, single-wire, 2 buses | 2 |
| SPI display | 5 |
| I2C (spare, sensors) | 2 |
| Endstops x5 | 5 |
| RUN switch, E-stop sense | 2 |
| Status LEDs | 3 |
| Pots x2 (ADC1, GPIO1-10) | 2 |
| **Total** | **32 of ~34** |

Swap the SPI display for I2C and it drops to 27, which is comfortable.

Two notes that apply either way:

- **Use single-wire UART for the TMC2209s.** TX and RX bridged through a 1k resistor means one GPIO per bus instead of two. Standard practice on 3D printer boards. A TMC2209 takes a 2-bit address from MS1/MS2, so one bus addresses four drivers and the fifth needs a second bus.
- **The E-stop is never a GPIO decision.** Firmware senses its state so the UI and display can report it; the actual cut is a hardware path to driver ENABLE with no software involvement.

### Fallback: classic ESP32 pin budget

Classic ESP32-WROOM has 19 output-capable GPIO plus 4 input-only (GPIO6-11 are flash, 34-39 are input-only, 0/2/12/15 are strapping). Motion alone takes 15, leaving 4 output-capable and 4 input-only - and the pots need ADC1 on GPIO32-39, colliding with the pins the endstops want.

That is why the classic part needs an **MCP23017 I2C expander** (~$1.50) to absorb the endstops, switches and LEDs. It works fine; it is simply a problem the S3 does not have.

## Unused pins on the schematic

Seven module GPIOs go unused by this design. They do not all get the same treatment.

| Pin | Handling |
|---|---|
| **GPIO0** | **Not a No-Connect.** 10k pullup to 3V3 plus a button to GND - it is the boot strap, and NC-ing it means no download mode |
| GPIO3 | No-Connect. JTAG source select; floating is fine |
| GPIO45 | No-Connect. VDD_SPI select, internal pulldown holds it LOW, which is what a 3.3 V flash needs |
| GPIO46 | No-Connect. Boot strap, internal pulldown holds it LOW |
| GPIO35, 36, 37 | See below - this is a decision, not a default |

KiCad's No-Connect flag is an ERC annotation, not a physical statement. It creates no copper and no net; it only suppresses the "pin not connected" warning so real omissions still surface.

### GPIO35/36/37: the variant decision

These are the only spare pins on an N16, and exactly the pins octal PSRAM claims on an R8. Three options:

| Approach | N16 | N16R8 | SPI display |
|---|---|---|---|
| No-Connect all three | Safe | Safe | **Foreclosed** |
| Route to the SPI display header | Works | **Hardware fault** | Available |
| **0 ohm series jumpers to the header** | Populate | Leave DNP | **Available on N16** |

**Do not route them and then fit an R8 module.** Those pads are bonded to the PSRAM die on that part, so external circuitry is bus contention rather than a merely wasted pin, and it can damage the module.

The series-jumper approach is the recommended one: three cheap parts turn the module variant into a BOM line instead of a board respin. Note that it means the pins are genuinely connected, so they get no NC flags - ERC reads the schematic, not the populate list.

## ESP32-S3 gotchas that must be designed around

**GPIO22-34 do not exist on the WROOM-1 module.** The pad numbering jumps straight from IO21 to IO35: 22-25 are absent from the die, 26-32 are bonded to the internal flash, and 33/34 are not brought out to the module at all. Assigning one produces a schematic net that goes nowhere and a board that is wrong in a way no DRC will catch. `config.h` carries a `PIN_EXISTS()` `static_assert` over every pin for this reason.

**ADC2 does not work while WiFi is active.** Silicon limitation on every ESP32 variant, not something firmware can route around - the pots read garbage the moment the radio comes up. On the S3, put them on **ADC1: GPIO1-10**. Ten channels, all normal bidirectional pins.

**GPIO26-32 are the SPI flash.** Unusable.

**GPIO33-37 are only free on the non-PSRAM part.** This is the whole reason for specifying N16 over N16R8 - octal PSRAM consumes them.

**GPIO19 and GPIO20 are native USB D- and D+.** Reserve them; that is what deletes the USB-serial chip.

**Strapping pins: GPIO0, GPIO3, GPIO45, GPIO46.** Anything pulled the wrong way at reset stops the board booting. Keep them off driver ENABLE, which is precisely the kind of net that sits at a defined level at power-up. GPIO45 is VDD_SPI and GPIO46 is input-only at boot - treat both as awkward and use them last.

**No input-only pins.** Unlike classic ESP32's GPIO34-39, every S3 GPIO is bidirectional. This is a real simplification for endstop and LED placement.

**Antenna keepout.** The WROOM module's antenna end must overhang the board edge with no copper - no ground pour, no traces, on any layer.

For the classic-ESP32 fallback the equivalents shift: flash is GPIO6-11, GPIO34-39 are input-only, ADC1 is GPIO32-39, and the strapping pins are GPIO0, 2, 12, 15.

## Layout notes

The hard part of this board is that 24 V chopper currents and a 12-bit ADC share it.

- **Separate the power and signal grounds**, joined at exactly one point near the input capacitor.
- **Keep motor connector traces away from analog and step/dir lines.** The chopper switches at tens of kHz into an inductive load and radiates well.
- **Per-driver bulk capacitance close to the VM pin**, short and fat traces. This is the most common cause of driver death.
- **Thermal vias under each driver's exposed pad.** TMC2209s throttle when hot.
- 4-layer if the budget allows: signal / ground / power / signal. It solves most of the above by itself.

## Firmware implications

Moving to this board changes three things in the current code:

1. **Step generation** switches from AccelStepper polling to FastAccelStepper on hardware peripherals. All the latency mitigation in the current sketch - non-blocking telemetry, round-robin `analogRead` - becomes unnecessary. Keep the flow math and the state machine exactly as they are.
2. **Per-pump syringe geometry** stops being optional. `SYRINGE_ID_MM` and `SYRINGE_VOLUME_UL` are global today; with a web UI they become per-pump fields, which is also what a 15:1 input ratio needs to give both inputs a sensible barrel life.
3. **Homing** replaces the "load the syringes and trust the assumption" startup, once the endstops exist.

## Web interface

The ESP32 hosts this itself. No Pi, no external server.

### Resource reality check

| Resource | Available | Needed |
|---|---|---|
| Flash | 16 MB (`-N16` part) | ~200 KB for a single-page UI in LittleFS |
| SRAM | 520 KB, roughly 300 KB usable heap | ~50 KB for WiFi + async server |
| Concurrent clients | Several | 1-3 |

A pump interface is a handful of numbers and a couple of sliders. This is not a demanding workload - the ESP32 is over-specified for it, which is the position you want to be in.

### Stack

| Layer | Choice |
|---|---|
| HTTP/WebSocket | ESPAsyncWebServer (use the actively maintained **ESP32Async** fork, not the original me-no-dev repo) + AsyncTCP. `PsychicHttp` is a good alternative built on the ESP-IDF server |
| Live telemetry | WebSocket push at 1 Hz - the same cadence the serial telemetry uses now |
| Control | REST `POST` for setpoints, so a dropped socket cannot leave a command half-applied |
| UI assets | LittleFS. Small single-page app; no CDN, no framework needed |
| Discovery | mDNS - answers at `ufluidics.local` |
| Settings | NVS via `Preferences` - syringe geometry, flow constants, calibration |

### Pin the motion task to core 1

The ESP32 has two cores, and the Arduino core puts the WiFi/TCP stack on core 0.
Create the control task with `xTaskCreatePinnedToCore(..., 1)` so rate recomputation never waits behind network work.

Step generation is already immune - it runs on MCPWM/RMT hardware - but the control loop is ordinary code and should not share a core with the radio.

### Keep the web UI out of the safety path

This is the part that matters for an instrument.

- **Arm/disarm stays a physical switch.** A browser tab is not an interlock.
- **E-stop stays hardware**, cutting driver ENABLE with no firmware involvement.
- **Decide what a WiFi dropout means** and implement it deliberately. Continuing at the last commanded rate is usually right for a running experiment; stopping mid-run may waste the sample. Either is defensible - silently doing whichever falls out of the code is not.
- **A client disconnect must never stall the step generator.** With hardware stepping and a pinned control task, it cannot.

### Access control

Default HTTP has no authentication, which means **anyone on the same network can change your flow rates mid-experiment**.

Two reasonable answers:

- **AP mode with a WPA2 password.** The board makes its own network. Best for an instrument - no dependency on lab WiFi, no captive portal, no IT involvement. Connect a phone or laptop directly.
- **STA mode with HTTP basic auth**, if it must live on the lab network for remote monitoring.

Supporting both, with STA attempted first and AP as fallback, is about twenty lines and worth it - lab networks are frequently locked down in ways you find out about on demo day.

## Cost estimate

| Block | Approx |
|---|---|
| ESP32-S3-WROOM-1-N16 | $4 |
| 5x TMC2209 modules | $25 |
| Power stage (buck, LDO, protection, caps) | $12 |
| Connectors, switches, LEDs | $15 |
| 2.4" TFT display | $8 |
| PCB, 4-layer, small qty | $30 |
| **Board total** | **~$90** |
| 24 V 5 A PSU | $25 |
| 5x NEMA17 | $60 |

Excludes mechanics, syringes, and enclosure.
