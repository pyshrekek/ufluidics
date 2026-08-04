# ufluidics - connection reference

Net-by-net wiring for the ESP32-S3 board. Every MCU pin here is derived from `firmware/include/config.h` - if you change a pin there, change it here, and vice versa. The `static_assert`s in that file will catch a pin that does not exist on the module, but nothing will catch this document drifting out of date.

This file describes the intended design. For where the schematic currently departs from it, see [OPEN-ISSUES.md](OPEN-ISSUES.md).

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
| J-TFT | SPI display, 9-pin (N16 only) |
| J-DBG | UART0 serial console, 3-pin 0.1 in header |
| D-RUN / D-FAULT / D-PWR | Status LEDs, 0805 |

---

## U1 - ESP32-S3-WROOM-1

| Pad | Net | Function | Connects to | Dir |
|---|---|---|---|---|
| 1, 40, 41 | GND | Ground | Ground plane. **Pad 41 is the EPAD** - thermal vias | - |
| 2 | +3V3 | Logic supply | 3.3 V rail, 22 uF + 100 nF at the pad | - |
| 3 | ~RESET | Chip enable | 10k to 3V3, **1 uF** to GND, SW-RST to GND | in |
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
| 15 | GPIO3 | LED_RUN | D-RUN anode, 1k | out |
| 16 | GPIO46 | LED_FAULT | D-FAULT anode, 1k | out |
| 17 | GPIO9 | ENDSTOP IN2 | J-ES2 pin 2 | in, pullup |
| 18 | GPIO10 | ENDSTOP OUT1 | J-ES3 pin 2 | in, pullup |
| 19 | GPIO11 | ENDSTOP OUT2 | J-ES4 pin 2 | in, pullup |
| 20 | GPIO12 | ENDSTOP OUT3 | J-ES5 pin 2 | in, pullup |
| 21 | GPIO13 | RUN switch | J-RUN pin 2 | in, pullup |
| 22 | GPIO14 | ESTOP_SENSE | E-stop aux contact | in, pullup |
| 23 | GPIO21 | I2C_SCL | J-DISP pin 4, 4k7 to 3V3 | bidir |
| 24 | GPIO47 | I2C_SDA | J-DISP pin 3, 4k7 to 3V3 | bidir |
| 25 | GPIO48 | TFT_DC | J-TFT pin 6 | out |
| 26 | GPIO45 | unused | No-Connect | - |
| 27 | GPIO0 | BOOT strap | 10k to 3V3 + SW-BOOT to GND | in |
| 28 | GPIO35 | TFT_SCK  (N16 only) | J-TFT pin 3, via 0R jumper | out |
| 29 | GPIO36 | TFT_MOSI (N16 only) | J-TFT pin 4, via 0R jumper | out |
| 30 | GPIO37 | TFT_CS   (N16 only) | J-TFT pin 5, via 0R jumper | out |
| 31 | GPIO38 | STEP IN1 | U2 TMC2209 STEP | out |
| 32 | GPIO39 | STEP IN2 | U3 TMC2209 STEP | out |
| 33 | GPIO40 | STEP OUT1 | U4 TMC2209 STEP | out |
| 34 | GPIO41 | STEP OUT2 | U5 TMC2209 STEP | out |
| 35 | GPIO42 | STEP OUT3 | U6 TMC2209 STEP | out |
| 36 | GPIO44 | UART0_RX (spare) | J-DBG pin 3 | in |
| 37 | GPIO43 | UART0_TX (spare) | J-DBG pin 2 | out |
| 38 | GPIO2 | POT_B (ADC1_CH1) | J-POT-B wiper | analog in |
| 39 | GPIO1 | POT_A (ADC1_CH0) | J-POT-A wiper | analog in |

Unlisted pads (GPIO22-34) do not exist on this module - the numbering jumps from IO21 to IO35.

**Pads 36 and 37 appear in the schematic as `RXD0` and `TXD0`, not as `IO44` and `IO43`.** KiCad's `RF_Module:ESP32-S3-WROOM-1` symbol names those two pins by their UART0 function, matching Espressif's own pinout table. They are the same pins: pad 36 = RXD0 = GPIO44, pad 37 = TXD0 = GPIO43. Every other pad is named `IOnn`, so these two are the only place where the symbol label and this table disagree on wording.

### Status LEDs - D-RUN, D-FAULT, D-PWR

All three are wired the same way: **anode toward the source, cathode to ground.**

```
  GPIO3  --[R 1k]--|>|-- GND      D-RUN, green
  GPIO46 --[R 1k]--|>|-- GND      D-FAULT, red
  +3V3   --[R 2k2]--|>|-- GND     D-PWR, green - always on, no GPIO
```

| Ref | Net | Series R | Current | Meaning |
|---|---|---|---|---|
| D-RUN | GPIO3 | 1k | ~1.2 mA | Solid = pumping |
| D-FAULT | GPIO46 | 1k | ~1.4 mA | Blinking = fault |
| D-PWR | +3V3 | 2k2 | ~0.6 mA | Rail is up |

At 3.3 V with a green Vf of about 2.1 V, 1k gives (3.3 - 2.1) / 1000 = 1.2 mA; a red Vf of 1.9 V gives 1.4 mA. That is dim by 1990s standards and perfectly visible on any modern high-efficiency part. Drop to 470R if the enclosure window is tinted - about 3 mA, still an order of magnitude under the 20 mA the ESP32-S3 wants as a per-pin ceiling, and still inside the 10 mA the power budget allots to indicators.

Put D-PWR on **+3V3**, not on +5V or +24V. A 3.3 V indicator proves the buck *and* the LDO are both alive, which is the question you actually have when a board does nothing.

**Do not wire these active-low.** The tempting alternative - 3V3 through the resistor to the LED, GPIO sinking to turn it on - would work on an ordinary pin and is wrong here: it holds GPIO3 and GPIO46 *high* through the resistor while the pin is hi-Z at reset. That is a mis-strapped JTAG source select and a mis-strapped boot log setting, on every power-up. The anode-to-GPIO topology is what makes these two pins usable at all.

Firmware matches: `digitalWrite(LED_RUN_PIN, HIGH)` lights it.

### Why the status LEDs sit on strapping pins

The indicator LEDs are on **GPIO3 and GPIO46**, both strapping pins, rather than on the UART0 pair. That is deliberate in both directions.

Putting the LEDs on strapping pins is safe because of how they are driven. Each LED is anode-driven through a 1k series resistor to ground, so the MCU pin sources current when the LED is lit and is high-impedance during reset. An LED wired this way can never pull its pin high, because its cathode is at ground - the worst it can do is offer a weak path to GND, which is the low state every strap here wants anyway. Firmware drives them long after the straps are latched, and any reset re-tristates the pin before the next sample.

GPIO46 has a documented internal pulldown at reset, so it reads 0 regardless - low leaves ROM message printing enabled. GPIO3 selects JTAG signal source (0 = USB-JTAG, 1 = external JTAG pins); Espressif does not document an internal pulldown on this pin the way it does for 45/46, so the LED's resistor-to-GND path is doing more of the work. It should still sample low - 0, USB-JTAG, the default this board wants since debugging goes over native USB - but **this was moved here for routing convenience and has not been bench-verified.** Check it with a meter or scope on first power-up. If it ever sampled high, the consequence is USB-JTAG being deselected in favor of external JTAG pins this board does not expose - inconvenient, not boot-breaking.

GPIO45 (the original home of LED_RUN, VDD_SPI strap) is unpopulated now - No-Connect, per the pin table.

Keeping the LEDs *off* GPIO43/44 buys two things:

- **A serial recovery port.** The console normally runs over native USB (GPIO19/20). If that is ever unusable - a bad USB descriptor, a bricked CDC config, a bootloader problem - UART0 on a 3-pin header is the way back in. Spending it on an LED removes the only fallback.
- **No boot flicker.** The ROM bootloader prints its log on TXD0 at every reset. An indicator LED on that pin flashes on every power-up, which reads as a fault to anyone watching the panel.

Bring GPIO43/44 out to **J-DBG**, a 3-pin 0.1 in header: pin 1 GND, pin 2 TX, pin 3 RX. No population cost if left unstuffed.

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

##### As-wired socket pinout - verified, do not re-derive

This is what the schematic routes, checked against the module and confirmed correct. Both rows are `Connector_Generic:Conn_01x08`. Reviewers have twice tried to re-derive this from the generic StepStick order and reached the wrong answer; the table below is the authority.

| Pin | Logic row (J1, J4, J7, J10, J13) | Power row (J2, J5, J8, J11, J14) |
|---|---|---|
| 1 | EN -> `DRIVER_EN` | GND |
| 2 | MS1 -> strapped, see address table | VIO -> **+3V3** |
| 3 | MS2 -> strapped, see address table | 1B |
| 4 | PDN_UART -> `TMC_UART_A` / `TMC_UART_B` | 1A |
| 5 | Alternate PDN pad - **No-Connect** | 2A |
| 6 | CLK -> GND | 2B |
| 7 | STEP | GND |
| 8 | DIR | VMOT -> **+24V** |

Two points that look like mistakes and are not:

- **Power row pin 1 is GND and pin 2 is VIO**, not the other way round. The generic StepStick order puts VDD on the end pin; this module does not.
- **Logic row pin 5 is No-Connect.** See the PDN section above - it is the alternate position for the same single-wire line, not a second UART pin.

UART addresses, set by the MS1/MS2 straps:

| Socket | Bus | MS1 (pin 2) | MS2 (pin 3) | Address |
|---|---|---|---|---|
| J1 | `TMC_UART_A` -> GPIO17 | GND | GND | 0 |
| J4 | `TMC_UART_A` | +3V3 | GND | 1 |
| J7 | `TMC_UART_A` | GND | +3V3 | 2 |
| J10 | `TMC_UART_A` | +3V3 | +3V3 | 3 |
| J13 | `TMC_UART_B` -> GPIO18 | GND | GND | 0 |

Four addresses is the TMC2209's limit per bus, which is why the fifth driver gets its own pin.

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

**`VIO` in that table means the +3V3 rail** - the same net that feeds the module's VIO/VDD pin, nothing else. Not +5V, not VMOT, and not the module's `5VOUT` pad. A logic input above VIO is out of spec, and the reason VIO is 3.3 V in the first place is the section above.

MS1 and MS2 have internal pull-downs, so a pin strapped to GND could in principle be left open. **Strap it anyway.** These are high-impedance inputs a few millimetres from a 24 V chopper, the same argument that grounds CLK, and a floating pin records no intent - the next person cannot tell address 0 from a forgotten net. Some vendors' modules also add their own pull-ups, which silently inverts the table.

Route each strap through its own **solder jumper or 0R pad to +3V3 and to GND**. The drivers are socketed and interchangeable, so the address lives on this board, not on the module - and a reworkable pad is the difference between changing an address and cutting a trace. Current through a strap is the pull-down's, tens of microamps, so the pads carry nothing.

**Maximum resolution does not come from these pins.** The firmware commands 1/16 and enables MicroPlyer, which interpolates to 256 microsteps inside the driver: the motor sees 1/256 motion while the step rate stays at 181 steps/s peak. Commanding 1/256 natively would give identical smoothness at 2894 steps/s - 16x the interrupt load and EMI for nothing. Interpolation is also at its best here, since MicroPlyer predicts from the last step interval and this system only ever runs at constant velocity.

> **Verify the UART config at startup.** `CHOPCONF.MRES` resets to 0, which is 256 microsteps. A driver that never received its configuration therefore runs at 1/256 while the firmware sends 1/16-rate pulses, delivering **16x too little flow** with nothing reporting a fault. A mis-strapped address does this. Check `test_connection()` on every driver and fault the system if any does not answer.

> **Validate the single-wire UART early.** One GPIO per bus relies on the ESP32 GPIO matrix mapping both U*TXD and U*RXD onto the same pad. It is used successfully in ESP32 Marlin builds, but prove it on a devkit before committing copper. If it does not behave, the fallback is separate TX and RX per bus - two extra pins, which N16 has spare and **R8 does not**.

---

## Power

```
J18 (24 V) --[F1 2A T]--[Q1 P-FET]--+-- +24V
                                    |
              D6 TVS SMBJ26A -------+
              C9 470 uF 35 V --------+
                                    |
                    +---------------+---------------+
                    |                               |
      driver VMOT (5x sockets)          U5 buck 24 -> 5 V
                                                    |
                                                +5V rail
                                                    |
                                        U3 LDO 5 -> 3.3 V
                                                    |
                            +3V3: U1, all VIO, pots, display
```

| Net | Sources | Loads |
|---|---|---|
| +24V | J18 via F1/Q1 | Driver VMOT x5, U5 VIN |
| +5V | U5 | U3 input, VBUS Schottky D1 |
| +3V3 | U3 | U1 (22 uF + 100 nF at pad 2), all driver VIO, pot high side, display |

### Input protection, net by net

Refdes match the schematic. D1 and D2 were already the VBUS Schottky and the buck catch diode, so the protection parts start at D6.

| Ref | Value | Pin | Net |
|---|---|---|---|
| J18 | Screw terminal 2-pin | 1 | GND |
| | | 2 | `VIN_RAW` |
| F1 | 2 A time-lag, 5x20 holder | 1 | `VIN_RAW` |
| | | 2 | `VIN_FUSED` |
| Q1 | P-channel, >= -60 V | **D (drain)** | `VIN_FUSED` |
| | | **S (source)** | `+24V` |
| | | **G (gate)** | `Q1_GATE` |
| R11 | 100k | 1 | `Q1_GATE` |
| | | 2 | GND |
| D7 | 15 V Zener, BZV55C15 | **A (anode)** | `Q1_GATE` |
| | | **K (cathode)** | `+24V` |
| D6 | TVS SMBJ26A | **cathode / pad 1** | `+24V` |
| | | **anode / pad 2** | GND |
| C9 | 470-1000 uF, 35 V, low ESR | + | `+24V` |
| | | - | GND |

Three nets are new: `VIN_RAW`, `VIN_FUSED`, `Q1_GATE`. Everything already on `+24V` - U5 pin 7, the five socket VMOT pins, `Cin`/`Cin1`/`Cin2`/`Cinx` - stays where it is and simply ends up downstream of the protection.

**The one edit to existing wiring:** J18 pin 2 currently connects straight to `+24V`. Break that and relabel it `VIN_RAW`. If you miss this the whole block is shorted out and does nothing, while the board still works perfectly on the bench - which is the worst possible failure to ship.

Watch three orientations, because all three are silently wrong rather than obviously wrong:

- **Q1 drain faces the input, source faces the load.** Reversed, the body diode conducts a backwards supply straight through the board.
- **D7 cathode to source**, anode to gate. Reversed it is a forward diode clamping the gate 0.7 V below the rail and the FET never turns on.
- **D6 cathode to +24V**, anode to GND. Reversed it is a forward diode across the supply and blows F1 on first power-up. Watch this one: KiCad's `Diode:SM6T*` symbols name both pins `A1`/`A2` even though the parts are unidirectional, so the schematic shows no cathode. `Diode_SMD:D_SMB` puts the band on **pad 1**, which makes pin 1 the end that goes to `+24V`.
- **D6 belongs on the rail, not on the gate.** Wired gate-to-GND it never conducts, nothing looks wrong on the bench, and the rail has no protection at all.

Keep the return unfused: J18 pin 1 straight to the ground plane.

The per-socket **100 uF + 100 nF on VMOT** hang off `+24V` at each driver, not at the input. That placement is the point - the socket's inductance is what makes a distant bulk capacitor useless to the driver.

#### Bench check before trusting it

1. No load, correct polarity: `+24V` should read within a few tens of millivolts of `VIN_FUSED`. A 0.6 V drop means Q1 never enhanced and current is going through the body diode - check D7's orientation.
2. Reverse the supply leads deliberately, current-limited: `+24V` should read 0 V and the fuse should survive.
3. Measure `Q1_GATE` in normal operation. Expect roughly 10-12 V below `+24V`. Near 0 V difference means the Zener is backwards or R11 is open.
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

**Breaking the wire is not sufficient - the pull-up is what makes it work.**

```
  GPIO16 ----[ J-ESTOP pins 1-2, NC contact ]----+---- DRV_EN -> 5x driver EN
                                                 |
                                             [10k] to +3V3
```

TMC2209 EN is active LOW. An open net floats, and a floating EN input may read as enabled - so a bare series contact can leave the drivers live. The 10k to VIO is what turns an open contact into a defined *disabled*. Use a latching mushroom head with **NC** contacts so a cut cable also reads as tripped.

Holding torque disappears when EN goes high. That is acceptable here: a 2 mm lead screw is not back-drivable, so line pressure cannot push a plunger back.

**The RUN switch does not replace this.** GPIO13 is an input firmware polls; stopping depends on firmware running, the ISR being alive and the main loop being reached. The E-stop exists for the case where none of that is true - a hung ISR, a watchdog that did not fire, a crash that left STEP toggling. In that state the RUN switch is an input nobody is reading, and a syringe pump keeps driving a plunger into a closed line.

### J-DISP - I2C display (4-pin, works on N16 and R8)

| Pin | Net |
|---|---|
| 1 | +3V3 |
| 2 | GND |
| 3 | SDA -> U1 GPIO47, 4k7 pullup to 3V3 |
| 4 | SCL -> U1 GPIO21, 4k7 pullup to 3V3 |

### J-TFT - SPI display (9-pin, **N16 only**)

Pin order matches the 2.4" ILI9341 module exactly, so the cable is straight-through with no crossed conductors.

| Pin | Module label | Net |
|---|---|---|
| 1 | VCC | +3V3 |
| 2 | GND | GND |
| 3 | CS | GPIO37, **via 0R jumper R-J3** |
| 4 | RESET | Own RC: 10k to +3V3, 100 nF to GND. **Not the MCU ~RESET net** |
| 5 | DC | GPIO48 |
| 6 | SDI (MOSI) | GPIO36, **via 0R jumper R-J2** |
| 7 | SCK | GPIO35, **via 0R jumper R-J1** |
| 8 | LED | +3V3 through solder jumper R-BL |
| 9 | SDO (MISO) | **No-Connect** |

**Leave R-J1..R-J3 unpopulated when fitting an N16R8 module.** GPIO35/36/37 are bonded to the PSRAM die on that part, and driving them is bus contention, not a wasted pin.

Three things to check on the specific module before wiring it:

- **VCC voltage.** Modules carrying an AMS1117 and a level shifter accept 5 V; bare ones are 3.3 V only. This header supplies 3.3 V, which is safe for both - an AMS1117 part simply runs in dropout on its regulator and takes logic level directly. Never move this pin to 5 V to "help" a regulated module: the logic pins on a bare module are not 5 V tolerant, and you cannot tell the two apart from the connector.
- **SDO is left open deliberately.** The firmware never reads registers back, and nothing else shares this SPI bus, so a module whose SDO does not tri-tri-state properly cannot cause trouble. If you later add a second SPI device, revisit this.
- **Touch, if the module has it.** Many 2.4" ILI9341 boards add an XPT2046 with T_CLK, T_CS, T_DIN, T_DO and T_IRQ. Those stay unconnected: touch would want five more GPIO plus a second CS, and there is no room in the pin budget. The pots and the web UI are the input path.

##### RST gets its own RC, not the MCU reset net

Sharing the board's ~RESET net looks free and costs nothing in GPIO, but it loads the one node on this board whose time constant was chosen deliberately. EN carries 10k and 1 uF for a ~10 ms power-up delay. Most ILI9341 modules already fit their own pull-up on RST, typically 10k to their 3.3 V; tie the two together and that pull-up lands in parallel with the board's, halving R and halving the EN delay. The EN cap was sized to 1 uF for a reason, and this quietly undoes it.

Give the display its own **10k to +3V3 and 100 nF to GND** instead. That is a ~1 ms low pulse at power-up, comfortably past the 10 us minimum the controller asks for, and it is independent of anything the MCU does.

Losing the shared reset costs nothing in practice: every driver library issues a software reset (`SWRESET`, 0x01) during init, so a display that missed a hardware reset still comes up clean when firmware restarts. The hardware pulse only has to cover the power-up case, which is exactly what the RC does.

If the module turns out to have no pull-up of its own, tying RST to the MCU ~RESET net becomes safe again - but check with an ohmmeter before assuming it, not after.

##### LED / BL is a backlight, not a logic pin

The backlight is four white LEDs and it dominates the module's draw - roughly **60-100 mA** on a 2.4" panel against the 80 mA this design budgets for the display as a whole. It is worth measuring on your actual module rather than trusting a listing. Two consequences:

- **Never drive it from a GPIO directly** unless the pin is a transistor base. 60 mA is three times the ESP32-S3's 20 mA per-pin ceiling; the pin survives at first and degrades.
- **Check what the pin actually is before connecting it.** An ohmmeter from the pin to GND tells you: a few hundred ohms means a series resistor is fitted, open means it is a logic input to an on-board driver, near-zero means a bare LED anode and the resistor is your job.

Simplest correct wiring is **the LED pin to +3V3, backlight always on**, which is what a bench instrument wants anyway. Wire it as a **2-pad solder jumper to +3V3, strapped by default** (R-BL), with the free side stubbed to a spare GPIO pad. Dimming then costs a jumper move rather than a respin.

Which pin drives it depends on which of the three types you have:

- **Logic input to an on-board driver** (the common case on modules whose documentation says "connect to a GPIO"): strap to +3V3 for always-on, or drive the pin directly and PWM it with LEDC. No external transistor - the module already has one. **Check polarity first**: some modules switch a PNP or P-FET, so LOW is on, and strapping that kind to +3V3 gives a dark screen that reads as a dead display.
- **LED anode with a series resistor on-board:** +3V3 only. A GPIO cannot sink or source this.
- **Bare LED anode:** you supply the series resistor, then +3V3.

For the last two, dimming needs a low-side N-channel FET (2N7002 or similar): BL to +3V3, LED return through the FET drain, source to GND, gate to a GPIO with a **100k pull-up to +3V3** so the backlight defaults ON if firmware never touches it. Nothing on this board is worth a dark screen because a pin failed to initialize.

Either way the backlight current comes from the module's VCC pin - J-TFT pin 1, off the 3.3 V rail, through the LDO. Switching it with a GPIO gates that current; it does not move it somewhere cheaper.

There is no spare GPIO for backlight control today: GPIO43/44 are the debug UART. Leave the pad and the jumper, populate when a pin frees up.

### J-DBG - UART0 console (3-pin 0.1 in header)

| Pin | Net |
|---|---|
| 1 | GND |
| 2 | UART0_TX -> GPIO43 (pad 37) |
| 3 | UART0_RX -> GPIO44 (pad 36) |

Unpopulated by default - the pads cost nothing and the header goes on only when it is needed. This is the recovery path when the native USB console is not available, and it is where the ROM bootloader prints at 115200 baud on every reset. Levels are 3.3 V; do not connect a 5 V adapter without a level shifter.

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

### The VBUS Schottky

**SS34** (40 V, 3 A, SMA) or a Nexperia PMEG equivalent.

| Spec | Target | Why |
|---|---|---|
| Vf | as low as practical, ~0.35 V at 250 mA | It comes straight off the LDO's headroom on USB-only power. Read it off the Vf-vs-If curve at 250 mA, not the summary table, which quotes full rating |
| If | >=1 A | Draw is at most 500 mA - the 5.1k CC resistors advertise USB default current - but a bigger die also means lower Vf at 250 mA |
| **VR** | **>=40 V** | Fault case, not normal operation |
| Package | SMA or SMB | Lower thermal resistance, bigger die, lower Vf |

The reverse-voltage target is the non-obvious one. In normal operation this diode sees about 5 V reverse, so a 20 V part looks generous. But if the buck ever fails short, 24 V lands on the 5 V rail, and **this diode is the only thing between that fault and the host USB port**. A 20 V part breaks down and passes it through. A 40 V part costs the same and turns "kills the laptop" into "blows the fuse".

Pair it with the **AP7361C-33E-13** LDO rather than a 1117-family part. The Schottky drop plus a 1 V dropout leaves only ~100 mV of margin from a 4.75 V VBUS; the AP7361C's 140 mV dropout leaves ~960 mV. See HARDWARE.md.

### ESD

Fit a **USBLC6-2SC6** (or equivalent) on D+, D- and VBUS, close to the connector. A USB port is the most exposed net on the board and the ESP32-S3's USB pins are not otherwise protected.

### No auto-reset circuit needed

The ESP32-S3 enumerates natively over USB Serial/JTAG, so the DTR/RTS transistor pair that a CP2102-based design needs is not required. Keep the **BOOT (GPIO0) and RESET (EN) buttons** anyway - they are the recovery path if firmware ever wedges the USB stack.
