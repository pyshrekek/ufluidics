# ufluidics - connection reference

Net-by-net wiring for the ESP32-S3 board. Every MCU pin here is derived from `firmware/include/config.h` - if you change a pin there, change it here, and vice versa. The `static_assert`s in that file will catch a pin that does not exist on the module, but nothing will catch this document drifting out of date.

Reference designators used below:

| Ref | Part |
|---|---|
| U1 | ESP32-S3-WROOM-1 |
| U2-U6 | TMC2209, one per pump: U2=IN1, U3=IN2, U4=OUT1, U5=OUT2, U6=OUT3 |
| U7 | 24 V -> 5 V buck (40 V+ rated) |
| U8 | 5 V -> 3.3 V LDO |
| J1 | 24 V input, 2-pin screw terminal |
| J2 | USB-C |
| J-M1..M5 | Motor outputs, JST-XH 4-pin |
| J-ES1..ES5 | Endstops, JST-XH 3-pin |
| J-POT-A/B | Potentiometers, JST-XH 3-pin |
| J-RUN | RUN switch, JST-XH 2-pin |
| J-ESTOP | E-stop, JST-XH 4-pin |
| J-DISP | I2C display, 4-pin |
| J-TFT | SPI display, 8-pin (N16 only) |

---

## U1 - ESP32-S3-WROOM-1

| Pad | Net | Function | Connects to | Dir |
|---|---|---|---|---|
| 1, 40, 41 | GND | Ground | Ground plane. **Pad 41 is the EPAD** - thermal vias | - |
| 2 | +3V3 | Logic supply | 3.3 V rail, 22 uF + 100 nF at the pad | - |
| 3 | ~RESET | Chip enable | 10k to 3V3, 100 nF to GND, SW-RST to GND | in |
| 4 | GPIO4 | DIR IN1 | U2 TMC2209 DIR | out |
| 5 | GPIO5 | DIR IN2 | U3 TMC2209 DIR | out |
| 6 | GPIO6 | DIR OUT1 | U4 TMC2209 DIR | out |
| 7 | GPIO7 | DIR OUT2 | U5 TMC2209 DIR | out |
| 8 | GPIO15 | DIR OUT3 | U6 TMC2209 DIR | out |
| 9 | GPIO16 | DRV_EN (all 5) | U2-U6 TMC2209 EN, via E-stop | out |
| 10 | GPIO17 | TMC_UART_A | U2-U5 PDN_UART, 1k series each | bidir |
| 11 | GPIO18 | TMC_UART_B | U6 PDN_UART, 1k series | bidir |
| 12 | GPIO8 | ENDSTOP IN1 | J-ES1 pin 2 | in, pullup |
| 13 | GPIO19 | USB_D- | USB-C D- (CC 5k1 each) | bidir |
| 14 | GPIO20 | USB_D+ | USB-C D+ | bidir |
| 15 | GPIO3 | unused | No-Connect | - |
| 16 | GPIO46 | unused (boot strap) | No-Connect, internal pulldown | - |
| 17 | GPIO9 | ENDSTOP IN2 | J-ES2 pin 2 | in, pullup |
| 18 | GPIO10 | ENDSTOP OUT1 | J-ES3 pin 2 | in, pullup |
| 19 | GPIO11 | ENDSTOP OUT2 | J-ES4 pin 2 | in, pullup |
| 20 | GPIO12 | ENDSTOP OUT3 | J-ES5 pin 2 | in, pullup |
| 21 | GPIO13 | RUN switch | J-RUN pin 2 | in, pullup |
| 22 | GPIO14 | ESTOP_SENSE | E-stop aux contact | in, pullup |
| 23 | GPIO21 | I2C_SCL | J-DISP pin 4, 4k7 to 3V3 | bidir |
| 24 | GPIO47 | I2C_SDA | J-DISP pin 3, 4k7 to 3V3 | bidir |
| 25 | GPIO48 | TFT_DC | J-TFT pin 6 | out |
| 26 | GPIO45 | unused (VDD_SPI strap) | No-Connect, internal pulldown | - |
| 27 | GPIO0 | BOOT strap | 10k to 3V3 + SW-BOOT to GND | in |
| 28 | GPIO35 | TFT_SCK  (N16 only) | J-TFT pin 3, via 0R jumper | out |
| 29 | GPIO36 | TFT_MOSI (N16 only) | J-TFT pin 4, via 0R jumper | out |
| 30 | GPIO37 | TFT_CS   (N16 only) | J-TFT pin 5, via 0R jumper | out |
| 31 | GPIO38 | STEP IN1 | U2 TMC2209 STEP | out |
| 32 | GPIO39 | STEP IN2 | U3 TMC2209 STEP | out |
| 33 | GPIO40 | STEP OUT1 | U4 TMC2209 STEP | out |
| 34 | GPIO41 | STEP OUT2 | U5 TMC2209 STEP | out |
| 35 | GPIO42 | STEP OUT3 | U6 TMC2209 STEP | out |
| 36 | GPIO44 | LED_FAULT | D-FAULT anode, 1k | out |
| 37 | GPIO43 | LED_RUN | D-RUN anode, 1k | out |
| 38 | GPIO2 | POT_B (ADC1_CH1) | J-POT-B wiper | analog in |
| 39 | GPIO1 | POT_A (ADC1_CH0) | J-POT-A wiper | analog in |

Unlisted pads (GPIO22-34) do not exist on this module - the numbering jumps from IO21 to IO35.

**No external crystal or oscillator.** The 40 MHz crystal, its load capacitors, the SPI flash and the RF matching network are all inside the module - that is the main thing you are buying over a bare ESP32-S3 chip. Nothing external is needed beyond the reset RC, the boot strap and decoupling.

A 32.768 kHz crystal is optional on ESP32-S3 for RTC accuracy in deep sleep, and is not wanted here: this system runs continuously, and its pins (XTAL_32K_P/N = GPIO15/16) already carry DIR OUT3 and DRV_EN. The internal 40 MHz reference is typically +/-10 ppm, which is four orders of magnitude below the syringe-bore uncertainty that dominates volumetric error.

**Antenna keepout:** the module's antenna end must overhang the board edge with no copper on any layer.

---

## U2-U6 - TMC2209 stepper drivers

Identical wiring except for STEP, DIR and the UART address straps.

### Common to all five

Two very different parts share the name TMC2209. A **SilentStepStick-style module** carries the charge pump, sense resistors and regulator caps on board, so it needs almost nothing external. A **bare QFN-28** needs all of that from you.

The pin names decide which you have: if your symbol shows `VS`, `VCP`, `CPI`, `CPO`, `5VOUT`, `BRA` and `BRB`, it is the bare IC.

#### Bare TMC2209 (QFN-28)

| Pin | Connects to | Required? |
|---|---|---|
| VS | +24 V | **Yes.** This is the motor supply. 100 nF at the pin plus 100 uF electrolytic |
| VCP | **100 nF to VS** | **Yes.** Charge-pump buffer. Note it returns to VS, not GND |
| CPI, CPO | **22 nF between the two pins** | **Yes.** Charge-pump flying capacitor |
| 5VOUT | 2.2 uF X7R to GND | **Yes.** Internal LDO output; do not load it externally |
| VCC | 5VOUT, with its own 100 nF to GND | **Yes**, if your symbol breaks it out separately |
| VCC_IO | +3.3 V, 100 nF to GND | **Yes.** Sets the logic level. 3V3, not 5 V |
| BRA | Sense resistor to GND, 0.11 ohm 1% | **Yes.** Bridge A current sense |
| BRB | Sense resistor to GND, 0.11 ohm 1% | **Yes.** Bridge B current sense |
| VREF | VCC_IO | **Yes.** Analog input, must not float. IRUN scales from it in UART mode |
| OA1, OA2 | J-Mx pins 1, 2 | Motor coil A |
| OB1, OB2 | J-Mx pins 3, 4 | Motor coil B |
| EN (ENN) | DRV_EN net, active LOW | |
| STEP, DIR | Hierarchical sheet pins | |
| PDN_UART | Bus, through a 1k series resistor | |
| MS1, MS2 | Strapped per instance - see the address table | |
| SPREAD | GND | StealthChop |
| CLK | GND | Selects the internal oscillator |
| DIAG | No-Connect, or a spare GPIO for StallGuard | Optional |
| INDEX | No-Connect | Optional |
| NC | No-Connect | |
| GND, exposed pad | Ground plane | **Yes.** Solder the EPAD with thermal vias - it is the only heat path |

Leaving VS, VCP, CPI/CPO or 5VOUT open means the part does not run at all: the charge pump generates the high-side gate drive. Leaving BRA/BRB open means no current sense, which is worse than not running.

**Per-driver passive count, bare IC:** 2x 0.11 ohm sense, 22 nF, 2x 100 nF, 2.2 uF, 100 nF, plus 100 nF + 100 uF on VS. About eight parts each, forty across the board. That is the real cost of bare ICs over modules - the silicon is cheaper, the BOM line count is not.

Sense resistor value sets the current ceiling: 0.11 ohm gives roughly 1.4 A RMS at full scale, which is what makes the 800 mA in `TMC_RMS_CURRENT_MA` a comfortable setting rather than a stretch.

#### SilentStepStick-style module

| Module pin | Connects to | Notes |
|---|---|---|
| VMOT | +24 V | 100 uF electrolytic + 100 nF at the module |
| GND | Ground plane | |
| VIO / VDD | +3.3 V | Logic reference |
| EN | DRV_EN net | Active LOW |
| STEP, DIR | Hierarchical sheet pins | |
| PDN_UART | Bus, through a 1k series resistor | |
| MS1, MS2 | Strapped per instance | |
| VREF | No-Connect | The onboard trimpot already drives it |
| DIAG, INDEX | No-Connect | Optional |
| 1A, 1B / 2A, 2B | J-Mx | Motor coils |

Charge pump, sense resistors and regulator capacitors are all on the module. Pin *order* differs between Watterott, BigTreeTech and FYSETC even though all three claim the A4988 footprint - wire by signal name against the datasheet for the exact part.

### Per-driver

| Ref | Pump | STEP from | DIR from | PDN_UART | MS1 | MS2 | Addr |
|---|---|---|---|---|---|---|---|
| U2 | IN1 | GPIO38 | GPIO4 | TMC_UART_A (GPIO17) | GND | GND | 0 |
| U3 | IN2 | GPIO39 | GPIO5 | TMC_UART_A | VIO | GND | 1 |
| U4 | OUT1 | GPIO40 | GPIO6 | TMC_UART_A | GND | VIO | 2 |
| U5 | OUT2 | GPIO41 | GPIO7 | TMC_UART_A | VIO | VIO | 3 |
| U6 | OUT3 | GPIO42 | GPIO15 | TMC_UART_B (GPIO18) | GND | GND | 0 |

**Every PDN_UART gets its own 1k series resistor** to the shared bus line.

A TMC2209 takes only a 2-bit address, so one bus reaches four drivers. That is the entire reason for the second bus - U6 is alone on it and can keep address 0.

> **Validate the single-wire UART early.** One GPIO per bus relies on the ESP32 GPIO matrix mapping both U*TXD and U*RXD onto the same pad. It is used successfully in ESP32 Marlin builds, but prove it on a devkit before committing copper. If it does not behave, the fallback is separate TX and RX per bus - two extra pins, which N16 has spare and **R8 does not**.

---

## Power

```
J1 (24 V) --[F1 5A]--[Q1 reverse-polarity P-FET]--[D1 TVS SMBJ26A]--+-- +24V
                                                                    |
                                              C1 470-1000 uF 35 V ---+
                                                                    |
                                                    +---------------+---------------+
                                                    |                               |
                                        U2-U6 VM (5x)                    U7 buck 24->5 V
                                                                                    |
                                                                                +5V rail
                                                                                    |
                                                                        U8 LDO 5 -> 3.3 V
                                                                                    |
                                                            +3V3: U1, all VIO, pots, display
```

| Net | Sources | Loads |
|---|---|---|
| +24V | J1 via F1/Q1/D1 | U2-U6 VM |
| +5V | U7 | U8 input, fan header |
| +3V3 | U8 | U1 (22 uF + 100 nF at pad 2), U2-U6 VIO, pot high side, display |
| GND | - | Single-point join between power and signal ground near C1 |

**Never linear-regulate 3.3 V from 24 V.** At the ESP32's 500 mA WiFi peak that dissipates 10.4 W. The intermediate 5 V rail exists for exactly this reason.

---

## Connectors

### J-M1..M5 - motors (JST-XH 4-pin)

| Pin | Net |
|---|---|
| 1 | Coil A+ (driver 1A) |
| 2 | Coil A- (driver 1B) |
| 3 | Coil B+ (driver 2A) |
| 4 | Coil B- (driver 2B) |

Never hot-plug a stepper into a powered driver.

### J-ES1..ES5 - endstops (JST-XH 3-pin)

| Pin | Net |
|---|---|
| 1 | GND |
| 2 | Signal -> U1 GPIO8/9/10/11/12 |
| 3 | +3V3 |

Use normally-closed switches so a broken wire reads as triggered. Internal pullups are enabled in firmware; no external resistor needed.

### J-POT-A / J-POT-B (JST-XH 3-pin)

| Pin | Net |
|---|---|
| 1 | +3V3 |
| 2 | Wiper -> U1 GPIO1 / GPIO2, **100 nF to GND at the board** |
| 3 | GND |

**Pot low side goes to GND, never to AREF.** These are ratiometric against the 3.3 V rail, so supply droop cancels out. Return the pot ground near the analog section, not to the driver power ground - motor return current through a shared path shows up as split drift under load.

### J-RUN (2-pin) and J-ESTOP (4-pin)

| Connector | Pin | Net |
|---|---|---|
| J-RUN | 1 | GND |
| J-RUN | 2 | U1 GPIO13, internal pullup. Maintained switch, closed = run |
| J-ESTOP | 1, 2 | **NC contact in series with the DRV_EN net.** Hardware path, no firmware |
| J-ESTOP | 3 | Aux contact -> U1 GPIO14 (sense only, for the UI) |
| J-ESTOP | 4 | GND |

The E-stop must break DRV_EN in hardware. GPIO14 exists only so the display and web UI can report the state - it is not the interlock.

### J-DISP - I2C display (4-pin, works on N16 and R8)

| Pin | Net |
|---|---|
| 1 | +3V3 |
| 2 | GND |
| 3 | SDA -> U1 GPIO47, 4k7 pullup to 3V3 |
| 4 | SCL -> U1 GPIO21, 4k7 pullup to 3V3 |

### J-TFT - SPI display (8-pin, **N16 only**)

| Pin | Net |
|---|---|
| 1 | +3V3 |
| 2 | GND |
| 3 | SCK -> GPIO35, **via 0R jumper R-J1** |
| 4 | MOSI -> GPIO36, **via 0R jumper R-J2** |
| 5 | CS -> GPIO37, **via 0R jumper R-J3** |
| 6 | DC -> GPIO48 |
| 7 | RST -> ~RESET net (shares the MCU reset) |
| 8 | BL -> +3V3, or a transistor for dimming |

**Leave R-J1..R-J3 unpopulated when fitting an N16R8 module.** GPIO35/36/37 are bonded to the PSRAM die on that part, and driving them is bus contention, not a wasted pin.

---

## USB-C (J2)

| Pin | Net |
|---|---|
| VBUS | +5V (through a Schottky if the 24 V supply may also be present) |
| GND | Ground |
| D+ | U1 GPIO20 |
| D- | U1 GPIO19 |
| CC1, CC2 | 5.1k pulldown each |

Both CC pins need their own 5.1k. A single shared resistor is a common error that makes the port work with some hosts and not others.

No USB-serial chip: the ESP32-S3 enumerates natively, which is what removes the CP2102N from the BOM.

---

## Hierarchical sheet layout (KiCad)

Five identical drivers is exactly what hierarchical sheets are for: draw the driver **once**, instantiate the same `.sch` file five times. KiCad annotates each instance separately, so U2-U6 fall out automatically.

### What crosses the hierarchy, and what does not

| Signal | How | Why |
|---|---|---|
| +24V, +3V3, GND | **Power symbols** (global) | Never route power through sheet pins; it clutters the parent for no information |
| DRV_EN | **Global label** | One net to all five drivers. A hierarchical pin would add five parent wires that all say the same thing |
| STEP, DIR | **Hierarchical pin** | Unique per instance |
| UART | **Hierarchical pin** | Bus A for U2-U5, bus B for U6 |
| MS1, MS2 | **Hierarchical pin** | The address straps - see below |
| Motor 1A/1B/2A/2B | **Stay inside the sheet** | Put the JST connector in the sheet too, so each instance gets its own and nothing crosses |

So the sheet symbol on the parent has just **five pins**: STEP, DIR, UART, MS1, MS2.

### The address straps are the catch

A TMC2209 takes its UART address from MS1/MS2, and each of the four drivers on bus A needs a *different* address. You cannot hardwire that inside a sheet you are reusing - all five instances would come out identical, every driver on bus A would answer to address 0, and the bus would collide.

Fix: expose **MS1 and MS2 as hierarchical pins** and strap them on the parent sheet, per instance. A reused sheet may connect its pins to different nets in each instance - that is precisely what hierarchy is for.

| Instance | Pump | MS2 | MS1 | Address | Bus |
|---|---|---|---|---|---|
| Sheet 1 | IN1 | GND | GND | 0 | A |
| Sheet 2 | IN2 | GND | +3V3 | 1 | A |
| Sheet 3 | OUT1 | +3V3 | GND | 2 | A |
| Sheet 4 | OUT2 | +3V3 | +3V3 | 3 | A |
| Sheet 5 | OUT3 | GND | GND | 0 | **B** |

MS1/MS2 have internal pulldowns, so a GND strap could be left off - do not. An explicit tie is one symbol and it makes the address readable straight off the schematic.

### Inside the driver sheet

```
  +24V ──┬──────────────── VM
         │
    C1 100uF/35V          GND ─── GND
    C2 100nF                     (both caps close to VM)
         │
        GND

  +3V3 ──┬──────────────── VIO        3.3 V, not 5 V
    C3 100nF
         │
        GND

  DRV_EN ───────────────── EN         global label, active LOW
  STEP ─────────────────── STEP       hier pin
  DIR ──────────────────── DIR        hier pin
  UART ────[ R 1k ]─────── PDN_UART   hier pin, own resistor per driver
  MS1 ──────────────────── MS1        hier pin, strapped on parent
  MS2 ──────────────────── MS2        hier pin, strapped on parent

  GND ──────────────────── SPREAD     StealthChop
  GND ──────────────────── CLK        internal oscillator
                           VREF ───── see the VREF note above
                           DIAG ───── NC (or a hier pin for StallGuard)
                           INDEX ──── NC

                     1A,1B,2A,2B ──── J-Mx, JST-XH 4-pin, inside the sheet
```

**Physical pin order varies by vendor.** Watterott SilentStepStick, BigTreeTech and FYSETC modules are all "A4988 footprint" but do not agree pin-for-pin. Wire by signal name against the datasheet for the exact part you are buying, not against a generic StepStick pinout.

### Mechanics

1. Draw the driver sheet once and save it, e.g. `tmc2209.kicad_sch`.
2. On the root sheet, place a sheet symbol and point it at that **existing file** four more times. KiCad reuses the file rather than copying it - editing one edits all five, which is the point.
3. Annotate. Each instance gets its own designators; the sheet path disambiguates them.
4. Strap MS1/MS2 per instance on the root sheet, per the table above.
5. ERC will flag any sheet pin left unconnected on any instance - useful, since a missed strap is otherwise silent until the UART does not answer.
