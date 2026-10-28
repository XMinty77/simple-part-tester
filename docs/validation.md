# Verification and limits

## Reproduce

On Debian/Ubuntu:

```sh
sudo apt-get install g++ python3 ngspice gcc-avr avr-libc arduino-core-avr libsimavr-dev
make test
make simavr
```

`make test` compiles the actual `.ino` and measurement core with an Arduino GPIO shim, then runs 12 unittest methods with many parameterized cases. ADC conversion uses 10-bit quantized voltages from **ngspice DC operating-point solutions** of the resistor fixture and DUT. Identification is performed by firmware, not by the model. Repeated circuit states are cached. The simulator supports `SIM_DEVICE`, `SIM_PINS` (zero-based C/D, B/G, E/S), `SIM_BETA`, `SIM_OHMS`, and `SIM_PROBES` environment variables. Gate models supply an explicitly defined Boolean transfer function rather than an analog IC circuit.

```sh
SIM_DEVICE=npn python3 host/tester.py --simulate build/tester-native --mode transistor
SIM_DEVICE=gate SIM_TRUTH=1110 python3 host/tester.py --simulate build/tester-native --mode gate --expect NAND
```

`make simavr` builds the real Arduino AVR core and sketch with avr-gcc, then executes that **same AVR ELF** in simavr at 16 MHz. The harness sends UART commands, obtains actual GPIO direction/output registers, feeds ngspice solutions into the emulated ADC, and checks that all fixture pins return to input with pull-ups disabled after each command. This checks AVR execution, serial framing, ADC acquisition, 16-bit integer behavior and GPIO cleanup, beyond native compilation. The CLI's `--simulate` option uses the faster native harness, not simavr. Simulation tooling is intended for Linux; USB serial control also supports Windows/macOS.

## Results recorded for this implementation

- Native suite: **12/12 test methods passed**.
- NPN, PNP, N-MOSFET and P-MOSFET: correct identification and terminal assignment for all six permutations each (24 cases).
- BJT gain sweep: beta 20, 60, 300 for both polarities, within 20% of model BF. Default BF=180 produced about hFE=185 and Vbe=665 mV.
- Open, short, single diode and resistor not labeled as transistors.
- Diode polarity identified for all six terminal pairs.
- Resistance: 100 Ω, 680 Ω, 1 kΩ, 10 kΩ, 470 kΩ and 1 MΩ within 6% in ideal DC models; open/asymmetric diode rejected and short near zero.
- Non-default ADC/drive pin mapping verified against a remapped fixture.
- All eight gate functions checked at supported input counts; faulty/undefined outputs, custom truth tables, protocol errors and overlong lines covered.
- Actual AVR ELF: all four transistor families with correct terminal assignments; gate AND table; invalid configuration blocks RUN; resistor calculations at 680 Ω, 470 kΩ, 1 MΩ; GPIO cleanup checks passed.
- Uno build using Arduino AVR core 1.8.6 / avr-gcc 7.3: **11,200 bytes flash, 507 bytes static RAM**, comfortably within Uno's flash/static RAM limits. Runtime stack usage is additional.

## What this does not establish

This is an educational **adaptation of the published method and gain calculation**, not the full extensively developed upstream tester firmware. Its simplified recognition does not inherit upstream device coverage or accuracy. Tests are reproducible evidence for the listed models, not a claim that every physical transistor is supported.

The models cover ordinary silicon BJTs and enhancement MOSFETs with body diodes, Vth around ±1.8 V, and BF up to 300. High-Vth MOSFETs that do not conduct at 5 V, Darlingtons, digital transistors with internal resistors, unusual protection networks, JFETs/depletion devices, IGBTs, SCRs/TRIACs and damaged/leaky devices may return unknown or misleading results. Low-holding-current latch devices are not explicitly discriminated. A classification is not proof of transistor health under load. hFE is a low-current estimate; collector saturation limits high gains, and close forward/reverse gains can make C/E assignment ambiguous.

DC models omit temperature, contact resistance, component spread, ADC offset/noise/input leakage, real GPIO resistance, gate charge/transient behavior, wiring capacitance and analog sample/hold acquisition errors. The 470 kΩ source impedance deserves particular attention on real ATmega ADCs. Measurements discard the first ADC reading and average sixteen readings, but that is not a substitute for calibration or a precision ADC front end. The resistance range is a practical educational range around 100 Ω–1 MΩ, not a calibrated specification. Diode scan reports any qualifying directed conduction path, not a guarantee of an isolated diode.

No physical board was available. A real build still requires wiring inspection and first-use checks with an empty socket, a known diode, a known resistor, a known BJT in several orientations and a known gate. Measure Vcc and reference resistors before relying on numeric results. Simulation reduces debugging risk; it cannot honestly guarantee a one-shot physical build.
