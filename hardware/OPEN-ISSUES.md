# Schematic open issues

Worklist against `ufluidics.kicad_sch` as of **2026-08-02**, from `kicad-cli sch erc` plus a netlist review.
State at that point: **0 ERC violations**, and several problems ERC cannot see.

Reproduce both checks with:

```
cd hardware
kicad-cli sch erc --output /tmp/erc.rpt --severity-all ufluidics.kicad_sch
kicad-cli sch export netlist --format kicadsexpr --output /tmp/net.net ufluidics.kicad_sch
```

ERC catches dangling and unconnected pins.
It does not catch a net that is wired to the wrong pin, so the netlist review is the one that found the VSW and USB faults.
A clean ERC run therefore closes out the first category and says nothing about the second - everything below survived a clean run.

**No E-stop is a deliberate choice on this board.**
`DRIVER_EN` runs straight from U1 pad 9 (GPIO16) to all five driver EN pins, GPIO14 carries a no-connect flag, and there is no J-ESTOP connector.
Do not re-file this as a defect.
The consequence is that stopping depends on firmware, which raises the value of two things that are not the E-stop: R12 holding the drivers disabled from reset, and `DRV_STATUS` readback over the driver UART. Both are present and working.

---

## Blocking

### No open blocking items

The driver socket wiring was the last one, and it is resolved: **the pin locations are correct as routed.** Pin 4 carries the single-wire PDN_UART, pin 5 is the alternate PDN pad and correctly No-Connect, pin 6 is CLK tied to GND, and the power row runs GND on pin 1 with VIO on pin 2. `CONNECTIONS.md` now carries the full as-wired table under "As-wired socket pinout - verified, do not re-derive"; read that instead of re-deriving it from the generic StepStick order, which is what produced two wrong findings during review.

Register readback therefore works. `test_connection()`, `DRV_STATUS`, CRC checking and StallGuard are all available over the existing bus, with no added parts. Do **not** fit an external 1k between pins 4 and 5 - the module already has one, and a second in series makes 2k.

---

## Important

### 1. Most parts have no footprint

**60 of 84 components.** Blocks layout, not review. Footprints that are set and worth keeping: D1 `D_SMA`, D2 `D_SMA`, D7 `D_MiniMELF`, D8 `PCM_JLCPCB:D_SMB`, Q1 `TO-252-2`, U3 `SOT-223-3_TabPin2`, U5 `SOIC-8`, F1 5x20 mm holder.

### 2. C9 is an electrolytic on an unpolarized symbol

`C9`, 470 uF on `+24V`, still uses `Device:C`. That symbol draws no polarity marking and lets a footprint be chosen with no anode. `Cout1`/`Cout2` had the same problem and are fixed; C9 was left alone because it was outside the reported scope.

470 uF on a 24 V rail is an aluminium electrolytic, and fitted backwards it vents. Move it to `Device:C_Polarized` with a `CP_*` footprint, and confirm pin 1 lands on `+24V` rather than GND after the swap - that is the step that bit `Cout1`/`Cout2`.

`C20` (1 uF at the LDO input) is fine as `Device:C`. At that value it is a ceramic, and unpolarized is correct.

### 3. Annotation errors on export

`kicad-cli` still warns on every export: `schematic has annotation errors`. The non-numeric refdes are `J_ENDSTOP_IN1`, `J_ENDSTOP_IN2`, `J_ENDSTOP_OUT1`-`OUT3`, `J_LED_RUN1`, `J_LED_FAULT1`, `J_LED_PWR1`, `J_RUN_SWITCH` and `J_DEBUG_UART`. KiCad treats each as unannotated.

Renaming them to plain numbered refdes is the fix; the descriptive names belong in the symbol's Description or a sheet note, not the reference field.

---

## Verify, do not assume

Things the netlist cannot tell apart, worth checking against the parts you actually order.

- **L1 is 47 uH; the 5 V nomograph at 24 V in gives 68 uH.** 47 uH is defensible, but confirm Isat clears the 2.2 A max current limit rather than the load current.
- **D1 (SS34) ORs USB VBUS onto `+5V`** with the buck output, cathode on `+5V`, anode on the VBUS net. Nothing limits how much the host supplies, and nothing prevents both sources being live at once. Confirm the buck tolerates back-feed at 5 V minus a Schottky drop, and that a USB host alone cannot brown out the +3V3 rail under motor load.
- **Panel LED headers share a resistor with the on-board LED.** `J_LED_RUN1` sits in parallel with D3 across R4, and likewise for FAULT and PWR. Two LEDs on one resistor split current by forward voltage, so the lower-Vf part takes most of it and the other looks dim. Give the panel header its own resistor, or accept that only one of the pair is the real indicator.
- **Endstop connectors are 2-pin here, 3-pin in the docs.** 2-pin is correct with firmware pullups and NC switches - fix `CONNECTIONS.md`, not the schematic.
- **Motor coil polarity.** Pairing is correct on all five, so no damage risk. One coil's polarity is inverted relative to the documented J-Mx order, which shows up as a reversed direction, not a fault.

---

## Already fixed

Recorded so the next review does not re-flag them.

| Was | Now |
|---|---|
| `Cout1`/`Cout2` dielectric unspecified | Both are now `Device:C_Polarized`, footprint `Capacitor_Tantalum_SMD:CP_EIA-7343-31_Kemet-D`, with `Dielectric`, `Voltage`, `ESR` and `MPN` fields carrying the constraint into the BOM. Pin 1 (+) sits on `+5V`, pin 2 on GND - verified in the netlist after the symbol swap. **`MPN` is still `TBD`** - fill it from a distributor page once you have confirmed the part's ESR |
| No input protection at all | F1, Q1, R11 100k, D7 (BZV55C15, cathode on `+24V`) and C9 470 uF all present and correctly wired |
| Input TVS on the gate node, wrong part, ambiguous symbol | D6 retired. **D8** replaces it: `TVS-Uni,SMBJ26A` from the JLCPCB library, footprint `PCM_JLCPCB:D_SMB`, pin 1 on `+24V` and pin 2 on `GND`. Value, symbol and description now agree - 26 V standoff, 42.1 V clamp, under the LM2675's 45 V absmax. Worth one look in the footprint editor that pad 1 is the banded end, since the symbol pins are unnamed |
| `DRIVER_EN` floating from reset | R12, 10k from `DRIVER_EN` to `+3V3`. Drivers are disabled until firmware pulls GPIO16 low |
| No local input cap at the LDO | C20, 1 uF on `+5V` at U3 VI |
| No J-DBG header | `J_DEBUG_UART`, 3-pin: `/DEBUG_RX` (U1 pad 36), `/DEBUG_TX` (pad 37), GND |
| `J_ENDSTOP_OUT4` was really the RUN switch | Renamed `J_RUN_SWITCH`, on `/RUN_SWITCH` to GPIO13 |
| `single_global_label` disabled | Back on. Only `footprint_filter`, `four_way_junction` and `simulation_model_issue` remain ignored, all harmless. `erc_exclusions` is empty |
| 24 V bulk at ~14 uF | C9 470 uF at the input |
| No per-driver VMOT decoupling | C10-C19, ten parts on `+24V`, one pair per driver socket |
| Three `power_pin_not_driven` errors | PWR_FLAGs added in `power.kicad_sch`. `+3V3` is additionally driven by U3 pin 3 (`power_out`) |
| I2C pullups unset | R9 and R10 both 4.7k |
| J19 pin order reversed against the docs | Pin 3 = SDA, pin 4 = SCL, matching `CONNECTIONS.md` |
| Stray `POT_B` label | Deleted |
| Stray `ESTOP_SENSE` label | Deleted; GPIO14 (pad 22) now carries a no-connect flag |
| Missing no-connect flags | U1 pads 15, 22, 36, 37; U5 pads 2, 3 and 5; J20 pad 9; J21 SBU1/SBU2. Floating ON/OFF on U5 is correct - the pin sources its own bias - and now reads as intent |
| Off-grid endpoints around J17 | Cleared |
| U5 pad 8 (VSW) unconnected, FB wired to the switch node | VSW joins C1/D2/L1; FB on `+5V` |
| USB D+/D- crossed at the MCU | `USB_DP` and `USB_DN` both straight through |
| No capacitance at all on +3V3 | C6 22 uF, C7 100 nF, C8 1 uF on the LDO output |
| EN cap 100 nF | C2 = 1 uF |
| No pot wiper filtering | C4, C5 at 100 nF |
| `USB_C_Plug` symbol, single 5.1k | `USB_C_Receptacle_USB2.0_16P`, R1 and R8 as separate Rd on CC1 and CC2 |
