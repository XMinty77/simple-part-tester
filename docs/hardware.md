# Hardware

Target: **5 V ATmega328P Uno R3 or classic Nano**, 16 MHz. Other Arduino families, Nano Every, 3.3 V boards and 3.3 V DUTs are unsupported. Pin names refer to Arduino headers, not ATmega or DUT package numbers. USB supplies the board and serial connection; no LCD is required. The firmware reserves D0/D1 for UART and releases D2–A5 after each test/error. Disconnect shields and unrelated peripherals from these pins.

## Three-probe fixture

Build this network once for **each** test point:

```text
Arduino ADC sense ---------------------- TP (DUT terminal)
Arduino low-resistor drive ---- 680 Ω ---+
Arduino high-resistor drive --- 470 kΩ --+
```

| Test point | Direct ADC sense | Through 680 Ω | Through 470 kΩ |
|---|---|---|---|
| TP1 | A0 | D2 | D3 |
| TP2 | A1 | D4 | D5 |
| TP3 | A2 | D6 | D7 |

Use six 1% resistors, short clean wires, and a socket/breadboard. The two resistors in each row join **at the same TP** as its sense wire. They are external hardware; setting pins in software does not replace them. Attach the three transistor terminals in any order to TP1/2/3. Results use TP numbers, starting at 1. Diodes can use any two TPs; resistor mode uses TP1 and TP2, leaving TP3 empty.

The Arduino can clamp the sense pins directly to ground or Vcc; the other current path always includes a resistor. The nominal test current is at most about 7.4 mA. This is not a protected multimeter input. Do not connect batteries, charged capacitors, live circuits, externally driven signals, or devices still installed in a circuit. Discharge and check parts before insertion. Only small parts compatible with 5 V test excitation are in scope.

Remap all nine physical connections with three `--probe ADC:LOW:HIGH` groups, matching the actual wiring. Every pin must be unique; sense pins must be A0–A5. Avoid boards with extra loads on chosen pins. Default fixtures deliberately avoid D13's onboard LED.

## Gate fixture (separate from the transistor fixture)

Use a combinational **5 V CMOS** gate such as one gate of a 74HC00:

| DUT (74HC00 DIP-14 example) | Connection |
|---|---|
| Pin 14 Vcc | Arduino 5 V |
| Pin 7 GND | Arduino GND |
| Pin 1 input A | D8 through 1 kΩ |
| Pin 2 input B | D9 through 1 kΩ |
| Pin 3 output Y | A3 through 1 kΩ |

Add 100 nF decoupling at the IC supply. Tie every unused IC input to a defined rail per its datasheet. Leave unused outputs unconnected. Power off before changing wiring. The IC power comes from the supply pins, never GPIO. Series resistors limit accidental contention; they do not make arbitrary voltages safe. Disconnect the transistor fixture when reusing its pins for gates.

Test one output with 1–4 inputs at a time. For open-drain/open-collector outputs, fit a suitable external pull-up to 5 V (e.g. 10 kΩ where the DUT permits it). Read the DUT datasheet for enable pins and their required states. Sequential logic, tristate enable characterization, propagation delay, TTL-specific thresholds, fan-out and maximum-frequency tests are outside this mode.

Gate thresholds: ≤0.3 Vcc is low, ≥0.7 Vcc is high, the interval between is `-1` (undefined). This is a slow DC functional test. A floating wire can settle to either rail; a passing table does not establish a sound physical connection or electrical compliance.
