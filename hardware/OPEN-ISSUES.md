# Schematic open issues

Worklist against `ufluidics.kicad_sch` as of **2026-08-02**, from `kicad-cli sch erc` plus a netlist review.
State at that point: **8 ERC violations**, and several problems ERC cannot see.

Reproduce both checks with:

```
cd hardware
kicad-cli sch erc --output /tmp/erc.rpt --severity-all ufluidics.kicad_sch
kicad-cli sch export netlist --format kicadsexpr --output /tmp/net.net ufluidics.kicad_sch
```

ERC catches dangling and unconnected pins.
It does not catch a net that is wired to the wrong pin, so the netlist review is the one that found the VSW and USB faults.

---

## Blocking

### 1. No E-stop anywhere

`DRIVER_EN` runs straight from U1 pad 9 (GPIO16) to all five driver EN pins.
`ESTOP_SENSE` has exactly one node - U1 pad 22 - and goes nowhere; ERC reports pad 22 as `pin_not_connected`.
There is no J-ESTOP connector in the design.

Fix: add a 4-pin J-ESTOP, put the NC contact in series between GPIO16 and the driver EN net, and **add a 10k pull-up from the driver side of that contact to +3V3**.

```
  GPIO16 ----[ J-ESTOP pins 1-2, NC ]----+---- DRV_EN -> 5x driver EN
                                         |
                                     [10k] to +3V3
```

The pull-up is the part that matters. TMC2209 EN is active LOW, so a bare series break leaves EN floating and the drivers may stay live.
Aux contact to pin 3 -> GPIO14, GND on pin 4.

The RUN switch does not cover this. It is a firmware-polled input, so it fails in exactly the scenario the E-stop exists for.

### 2. The input TVS is on the gate node, not on the rail

D6 (SMBJ26A) connects GND to `Q1_GATE` - in parallel with R11 - instead of GND to `+24V`. The `+24V` net contains C9, Cin/Cin1/Cin2/Cinx, the five VMOT pins, Q1 source and U5 pin 7, and no TVS.

The rail therefore has no transient protection, and nothing about that is visible on the bench: the gate sits about 12 V below the rail, far under the part's 26 V standoff, so it never conducts and every measurement looks correct. Meanwhile motor back-EMF reaches U5 and the drivers unclamped.

Fix: move D6's non-ground end from `Q1_GATE` to `+24V`.

**The symbol needs attention too, and it is not the mistake it first looks like.** D6 uses `Diode:SM6T33A`, and KiCad's whole SM6T family inherits from `SM6T6V8A`, whose pins are named `A1` and `A2` - anode/anode - even though the parts are unidirectional and the symbol description says so. The netlist therefore exports no cathode at all.

That has a concrete consequence rather than a cosmetic one. `Diode_SMD:D_SMB` puts the band, and therefore the cathode, on **pad 1**. D6 pin 1 currently sits on GND. Move only pin 2 to `+24V` and the physical part is fitted cathode-to-ground - a forward diode across the supply that opens F1 on first power-up.

So the correct wiring is **pin 1 to `+24V`, pin 2 to GND**. Confirm pad 1 is the banded end in the footprint editor first; everything downstream rests on it.

**Separately, the symbol and value disagree about which part this is.**

| Field | Says |
|---|---|
| Value | SMBJ26A |
| Symbol | `Diode:SM6T33A` |
| Description | 600W unidirectional Transil, **33 Vrwm** |
| Datasheet | `st.com/.../sm6t.pdf` |

A BOM built from Value gets the right part. Anyone reading the description or following the datasheet link orders an SM6T33A, which clamps near **45.7 V** - above the LM2675's 45 V absolute maximum, and precisely the selection SMBJ26A exists to avoid. Correct the Description and Datasheet fields, or derive a symbol with real `K`/`A` pins and retire the ambiguity for good.

Everything else in the protection block is correct and verified against the netlist: J18 pin 2 broken off `+24V` and routed through F1, F1 into Q1's **drain**, Q1 **source** to `+24V`, R11 100k gate to GND, D7 (BZV55C15) cathode to `+24V` and anode to the gate, C9 470 uF on the rail. Q1 is a SUD50P06-15 at -60 V and F1 is 2 A.

---

## Important

### 3. No per-driver VMOT decoupling

Input bulk is now covered by C9 at 470 uF, which also settles the `Cin` question - 3x 4.7 uF at U5's input is correct *local* decoupling once the bulk sits upstream.

What is still missing is **100 uF electrolytic + 100 nF at each of the five driver sockets**, on `+24V` at the socket rather than at the input. That placement is the whole point: the socket's own inductance is what makes a distant bulk capacitor useless to the driver, and local absorption is also the practical mitigation for the TMC2209's 30 V absolute maximum, which no input TVS can protect (see `HARDWARE.md`).

### 4. Driver socket pin 5 floats on all five sockets

`unconnected-(J1-Pin_5-Pad5)`, and the same for J4, J7, J10, J13.

Identify what pin 5 is on the module you actually chose - vendor pin order differs between Watterott, BigTreeTech and FYSETC. If it is **CLK or SPREAD, it must go to GND**: a floating clock input next to a 24 V chopper picks up switching noise and upsets the chopper.

### 5. I2C pullup values are unset

R9 and R10 both carry the literal value `R`. Set both to **4.7k**.

### 6. J19 pin order is reversed against the documentation

Schematic has pin 3 = SCL, pin 4 = SDA. `CONNECTIONS.md` specifies pin 3 = SDA, pin 4 = SCL.
Either is workable, but the cable is built from one of them - pick one and make both agree.

### 7. Stray `POT_B` label

ERC: `label_dangling` at (0.159 mm, 2.096 mm) - near the sheet origin, so it is a dropped stray, not a real connection.
The `/POT_B` net itself is correct (C5, J17 pin 2, U1 pad 38). Delete the orphan label.

---

## Hygiene

Nothing here breaks the board, but each one hides a real fault the next time ERC runs.

| Item | Detail |
|---|---|
| No-connect flags | U1 pad 15 (IO3), pad 36 (RXD0), pad 37 (TXD0); U5 pads 2 and 3 (NC) and pad 5 (ON/OFF). Floating ON/OFF is correct - the pin sources its own bias - but flag it so it reads as intent |
| PWR_FLAG missing | Three `power_pin_not_driven` errors: `#PWR01`, `#PWR030`, `#PWR04` |
| Annotation errors | `kicad-cli` warns on export. The `J_LED_*` and `J_ENDSTOP_*` refdes are non-numeric, which KiCad treats as unannotated |
| `J_ENDSTOP_OUT4` is the RUN switch | Wired to GPIO13. Rename to `J_RUN` before someone plugs an endstop into it |
| Footprints | Most parts have none. Blocks layout, not review |
| J-DBG header | UART0 pads 36/37 have no header. Specified in `CONNECTIONS.md`; the recovery path if native USB ever fails |
| LDO input cap | No local 1 uF at U3 VI. `Cout`/`Cout1` are doing the job from the buck side; a local part is still wanted |

---

## Verify, do not assume

Things the netlist cannot tell apart, worth checking against the parts you actually order.

- **`Cout` / `Cout1` dielectric.** 2x 68 uF is right, but the LM2675 is voltage-mode and compensated around output-capacitor ESR. Every capacitor in the datasheet's selection tables is a solid tantalum. If these are X5R/X7R ceramics the loop can ring or oscillate - use tantalum or polymer, or add series resistance.
- **L1 is 47 uH; the 5 V nomograph at 24 V in gives 68 uH.** 47 uH is defensible, but confirm Isat clears the 2.2 A max current limit rather than the load current.
- **Panel LED headers share a resistor with the on-board LED.** `J_LED_RUN` sits in parallel with D3 across R4, and likewise for FAULT and PWR. Two LEDs on one resistor split current by forward voltage, so the lower-Vf part takes most of it and the other looks dim. Give the panel header its own resistor, or accept that only one of the pair is the real indicator.
- **Endstop connectors are 2-pin here, 3-pin in the docs.** 2-pin is correct with firmware pullups and NC switches - fix `CONNECTIONS.md`, not the schematic.
- **Motor coil polarity.** Pairing is correct on all five, so no damage risk. One coil's polarity is inverted relative to the documented J-Mx order, which shows up as a reversed direction, not a fault.

---

## Already fixed

Recorded so the next review does not re-flag them.

| Was | Now |
|---|---|
| No input protection at all | F1, Q1, R11, D7 and C9 all present and correctly wired. Only D6's placement is outstanding, see above |
| 24 V bulk at ~14 uF | C9 470 uF at the input |
| Off-grid endpoints around J17 | Cleared |
| Stray `ESTOP_SENSE` label | Deleted. GPIO14 now reports as `pin_not_connected`, which is expected until J-ESTOP exists |
| U5 pad 8 (VSW) unconnected, FB wired to the switch node | VSW joins C1/D2/L1; FB on +5V |
| USB D+/D- crossed at the MCU | `USB_DP` and `USB_DN` both straight through |
| No capacitance at all on +3V3 | C6 22 uF, C7 100 nF, C8 1 uF on the LDO output |
| EN cap 100 nF | C2 = 1 uF |
| No pot wiper filtering | C4, C5 at 100 nF |
| No I2C pullups | R9, R10 present - values still unset, see above |
| `USB_C_Plug` symbol, single 5.1k | `USB_C_Receptacle_USB2.0_16P`, R1 and R8 as separate Rd on CC1 and CC2 |
