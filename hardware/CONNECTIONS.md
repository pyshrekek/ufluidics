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
| 10 | GPIO17 | TMC_UART_A | U2-U5 PDN_UART pin 4 (1k on module) | bidir |
| 11 | GPIO18 | TMC_UART_B | U6 PDN_UART pin 4 (1k on module) | bidir |
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

#### SilentStepStick-style module (socketed - what this board uses)

| Module pin | Connects to | Notes |
|---|---|---|
| VMOT | +24 V | 100 uF electrolytic + 100 nF at the module |
| GND | Ground plane | |
| VIO / VDD | **+3.3 V** | Logic reference, not the operating supply - see below |
| EN | DRV_EN net | Active LOW |
| STEP, DIR | Hierarchical sheet pins | |
| PDN_UART | Bus, directly (BTT) | BTT Pin 4; the 1k is on the module. Pin 5 stays NC |
| MS1, MS2 | Strapped per instance | |
| VREF | No-Connect | The onboard trimpot already drives it |
| DIAG, INDEX | No-Connect | Optional |
| 1A, 1B / 2A, 2B | J-Mx | Motor coils |

Charge pump, sense resistors and regulator capacitors are all on the module - roughly forty passives that do not appear on this board's BOM. Still fit **100 uF electrolytic + 100 nF per socket** on VMOT: the module's own decoupling is small, and the socket adds inductance between it and the bulk capacitor.

Footprint: the standard 2x8 0.1 in StepStick outline, `Module:Pololu_Breakout-16_15.2x20.3mm` in the KiCad library. Use female headers so a failed driver is a swap rather than a rework.

**Pin order differs between Watterott, BigTreeTech and FYSETC** even though all three claim the A4988 footprint. Once you socket them, the board is committed to whichever pinout you route - pick the exact part first and wire by signal name against its datasheet.

##### Two PDN pads (BigTreeTech TMC2209 V1.2/V1.3)

The two pads are **not** TX and RX. They are two alternative positions for the same single-wire PDN_UART line, and an on-board 1k resistor selects which one is live.

From BigTreeTech's own documentation: *"The factory has connected the UART Pin to the fourth Pin, namely the PDN_UART Pin... If the fifth Pin is used as the UART Pin, the resistance shall be removed and welded to the following two pads."*

| Pad | Wiring |
|---|---|
| **Pin 4** (PDN_UART) | To the UART bus. Factory default, nothing to modify |
| **Pin 5** | **No-Connect** - physically open on the module unless you relocate the resistor |

**Do not add an external 1k series resistor.** BigTreeTech already fitted it; another in series makes 2k. This differs from a bare TMC2209 or some other vendors' modules, where the resistor is the host board's job.

Klipper drives these with a single `uart_pin`, which confirms single-wire half-duplex operation - so one GPIO per bus is correct and the pin budget is unchanged.

> If you switch to a different vendor's module, re-check this. Watterott SilentStepSticks and bare ICs do **not** carry the resistor, and some modules genuinely do split TX and RX. An ohmmeter between the pads tells you which you have: ~0 ohm means duplicated pinout, ~1k means the resistor sits between them.

##### CLK

**CLK ties to GND**, selecting the internal ~12 MHz oscillator.

- Do not leave it floating. It is a clock input, and floating next to a 24 V chopper it will pick up switching noise and upset the chopper.
- Oscillator accuracy does not affect flow. Position comes entirely from STEP pulse count, which the DDS controls; CLK only sets chopper and StealthChop PWM frequency, so its tolerance is irrelevant here.
- Some modules ground CLK on-board and still expose the pad. Check yours - if so it is a No-Connect on your side.

##### VIO must be 3.3 V, and this is not a preference

VIO sets the driver's logic reference only; the chip runs from VM through its own regulator, so 3.3 V costs nothing.

At 5 V two things break in opposite directions:

- **Inbound:** the input-high threshold is roughly 0.7 x VIO, so 3.5 V at a 5 V VIO. The ESP32 drives 3.3 V, which lands below it. Missed STEP pulses with no error anywhere.
- **Outbound:** `PDN_UART` is bidirectional. The driver answers register reads at VIO, so a 5 V VIO drives 5 V into GPIO17/GPIO18. **The ESP32-S3 is not 5 V tolerant**, and both pins fan out to five drivers - one wrong rail damages the MCU on the first UART read.

Do not tie VIO to the module's `5VOUT` pin either, where one is exposed; that is the internal regulator and puts you in the same place. VIO draw is microamps to low milliamps, so five drivers are nothing for the 3.3 V LDO.

##### Two hazards the socket introduces

**A module inserted backwards is destroyed instantly**, and nothing stops it mechanically - both rows are 8 pins at the same pitch. Mitigate on silkscreen: outline the module body, mark pin 1, and label which end carries VMOT. A reversed driver puts 24 V onto the logic pins and can take the ESP32 with it.

**Never hot-plug a driver.** Removing or inserting one with VMOT live kills the output stage. Power down completely before swapping - which is the whole reason for sockets, so it is worth putting on the silkscreen too.

#### Bare TMC2209 (QFN-28) - reference only, not used on this board

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
| PDN_UART | Bus, directly (BTT) | BTT Pin 4; the 1k is on the module. Pin 5 stays NC |
| MS1, MS2 | Strapped per instance - see the address table | |
| SPREAD | GND | StealthChop |
| CLK | GND | Selects the internal oscillator. Never leave floating |
| DIAG | No-Connect, or a spare GPIO for StallGuard | Optional |
| INDEX | No-Connect | Optional |
| NC | No-Connect | |
| GND, exposed pad | Ground plane | **Yes.** Solder the EPAD with thermal vias - it is the only heat path |

Leaving VS, VCP, CPI/CPO or 5VOUT open means the part does not run at all: the charge pump generates the high-side gate drive. Leaving BRA/BRB open means no current sense, which is worse than not running.

**Per-driver passive count, bare IC:** 2x 0.11 ohm sense, 22 nF, 2x 100 nF, 2.2 uF, 100 nF, plus 100 nF + 100 uF on VS. About eight parts each, forty across the board. That is the real cost of bare ICs over modules - the silicon is cheaper, the BOM line count is not.

Sense resistor value sets the current ceiling: 0.11 ohm gives roughly 1.4 A RMS at full scale, which is what makes the 800 mA in `TMC_RMS_CURRENT_MA` a comfortable setting rather than a stretch.

### Per-driver

| Ref | Pump | STEP from | DIR from | PDN_UART | MS1 | MS2 | Addr |
|---|---|---|---|---|---|---|---|
| U2 | IN1 | GPIO38 | GPIO4 | TMC_UART_A (GPIO17) | GND | GND | 0 |
| U3 | IN2 | GPIO39 | GPIO5 | TMC_UART_A | VIO | GND | 1 |
| U4 | OUT1 | GPIO40 | GPIO6 | TMC_UART_A | GND | VIO | 2 |
| U5 | OUT2 | GPIO41 | GPIO7 | TMC_UART_A | VIO | VIO | 3 |
| U6 | OUT3 | GPIO42 | GPIO15 | TMC_UART_B (GPIO18) | GND | GND | 0 |

**On BigTreeTech modules the 1k series resistor is already fitted** - wire Pin 4 straight to the bus and add nothing. Bare ICs and some other vendors' modules need an external 1k per driver. See the two-pad note above.

A TMC2209 takes only a 2-bit address, so one bus reaches four drivers. That is the entire reason for the second bus - U6 is alone on it and can keep address 0.

**MS1/MS2 are not microstepping select.** That is their standalone-mode role. Once PDN_UART is used for UART they become the address, and microstepping moves to `CHOPCONF.MRES` in software. So the four drivers on bus A must be strapped *differently* - identical straps collide the bus.

**Maximum resolution does not come from these pins.** The firmware commands 1/16 and enables MicroPlyer, which interpolates to 256 microsteps inside the driver: the motor sees 1/256 motion while the step rate stays at 181 steps/s peak. Commanding 1/256 natively would give identical smoothness at 2894 steps/s - 16x the interrupt load and EMI for nothing. Interpolation is also at its best here, since MicroPlyer predicts from the last step interval and this system only ever runs at constant velocity.

> **Verify the UART config at startup.** `CHOPCONF.MRES` resets to 0, which is 256 microsteps. A driver that never received its configuration therefore runs at 1/256 while the firmware sends 1/16-rate pulses, delivering **16x too little flow** with nothing reporting a fault. A mis-strapped address does this. Check `test_connection()` on every driver and fault the system if any does not answer.

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

A 16-pin USB 2.0 receptacle is enough - the SBU and SS pairs of a 24-pin part go unused. Wired as a **device** (UFP), data only, no power delivery negotiation.

| Receptacle pin(s) | Net | Notes |
|---|---|---|
| A1, B1, A12, B12 | GND | All four to the plane |
| A4, B4, A9, B9 | VBUS | All four tied together |
| **A5** | **CC1** | **5.1k to GND - its own resistor** |
| **B5** | **CC2** | **5.1k to GND - its own resistor** |
| A6, B6 | D+ | Tie together, then to U1 GPIO20 |
| A7, B7 | D- | Tie together, then to U1 GPIO19 |
| A8, B8 | SBU1, SBU2 | No-Connect for USB 2.0 |

### The CC resistors are the classic mistake

**Each CC pin needs its own 5.1k pulldown.** One shared resistor across both is the single most common USB-C error, and it produces a port that works with some cables and hosts and not others - which reads as a flaky cable rather than a board fault.

The host uses CC1 and CC2 to detect plug orientation. Tie them together and orientation detection fails; many hosts then refuse to enumerate or never turn on VBUS. Two separate 5.1k (Rd) advertises "device, USB default current" and is correct for anything drawing 500 mA or less.

### D+ and D- tie together, deliberately

A USB-C receptacle carries the D pair twice, once per orientation. A USB 2.0 device shorts A6 to B6 and A7 to B7 so the link works either way up. That is correct, not a shortcut.

Route them to U1 GPIO20 (D+) and GPIO19 (D-) as a differential pair - matched length, ~90 ohm differential, no stubs, and keep them away from the switcher and the motor traces. Espressif reference designs connect these straight to the module with no series resistors; 0 ohm placeholders are cheap insurance if you want tuning options later.

### VBUS on a board that also has 24 V

Two supplies can reach the 5 V rail, so decide which wins:

| Approach | Behaviour |
|---|---|
| **VBUS through a Schottky to the 5 V rail** (recommended) | USB alone powers the logic for flashing and configuration; the buck powers it when 24 V is present. The diode stops the buck back-feeding the host |
| Leave VBUS unconnected | Simplest and fully safe, but the board cannot be flashed or configured without the 24 V supply on |

The Schottky option has a real safety benefit worth naming: with USB only, the logic runs and the web UI is reachable while **the motors cannot move**, because the drivers have no VM. That is a good state for firmware work and configuration.

Set the buck feedback for about 5.1 V rather than exactly 5.0 V, so when both sources are present the buck reliably wins and the VBUS diode stays reverse biased.

### ESD

Fit a **USBLC6-2SC6** (or equivalent) on D+, D- and VBUS, close to the connector. A USB port is the most exposed net on the board and the ESP32-S3's USB pins are not otherwise protected.

### No auto-reset circuit needed

The ESP32-S3 enumerates natively over USB Serial/JTAG, so the DTR/RTS transistor pair that a CP2102-based design needs is not required. Keep the **BOOT (GPIO0) and RESET (EN) buttons** anyway - they are the recovery path if firmware ever wedges the USB stack.
