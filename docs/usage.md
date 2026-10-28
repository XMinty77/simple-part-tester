# Usage

Open `firmware/tester/tester.ino` in the Arduino IDE, select Uno or classic Nano (ATmega328P, 5 V), and upload. Alternatively use `arduino-cli compile --fqbn arduino:avr:uno firmware/tester` then `arduino-cli upload --fqbn arduino:avr:uno --port /dev/ttyACM0 firmware/tester`. The Debian-core build `make avr` produces `build/avr/tester.hex` and `.elf`; it does not flash or change fuses.

Install the Python dependency with `python3 -m pip install -r requirements.txt`. Python 3.9+ is required. Each invocation opens the serial device at 115200 baud, waits for the Arduino's reset, verifies protocol version, configures the fixture, runs once, and prints JSON. Use `COM3` on Windows or the appropriate `/dev/tty...` on Linux/macOS. Close any IDE serial monitor first.

```sh
python3 host/tester.py --port /dev/ttyACM0 --mode transistor
python3 host/tester.py --port /dev/ttyACM0 --mode transistor \
  --probe A5:11:10 --probe A4:9:8 --probe A3:7:6
python3 host/tester.py --port /dev/ttyACM0 --mode diode
python3 host/tester.py --port /dev/ttyACM0 --mode resistor
python3 host/tester.py --port /dev/ttyACM0 --mode gate --inputs D8 D9 --output A3 --expect NAND
python3 host/tester.py --port /dev/ttyACM0 --mode gate --inputs D8 --output A3 --expect NOT
python3 host/tester.py --port /dev/ttyACM0 --mode gate --inputs D8 D9 --output A3 --truth 0001
```

`--expect` supports AND/NAND/OR/NOR/XOR/XNOR/NOT/BUFFER. Without it or `--truth`, gate mode measures without assigning pass/fail. Custom truth strings contain exactly 2^N bits. Row index counts upward from zero; the **first input is bit 0** (changes fastest). For two inputs the order is `(0,0), (1,0), (0,1), (1,1)`. XOR/XNOR use parity for more than two inputs. Exit status: 0 successful measurement, 1 expected table mismatch/undefined level, 2 configuration/transport/protocol error. An `unknown` transistor or `null` resistance is a successful measurement with no supported identification, not proof of a broken part.

Transistor JSON reports `control` (base/gate), `collector_or_drain`, `emitter_or_source`, approximate `hfe_estimate`, and `vbe_mv`. Pins are TP1–TP3; zero means unidentified. MOSFET hFE/Vbe fields are zero and not applicable. Diode mode reports directed junctions and forward voltage; transistor junctions can also appear here. Resistance is in ohms, TP1–TP2, or null if open/outside range/asymmetric. A small value is a short/low resistance, with poor accuracy near zero.

Probe modes accept `--vcc-mv`, `--low-ohms`, `--high-ohms` for measured calibration values (defaults 5000/680/470000). These are common values for all three resistor branches; fit matched resistors. Firmware assumes 25 Ω GPIO output resistance. Calibration is volatile and resent by each command invocation. Gate mode assumes nominal 5 V. This is educational identification, not a precision meter.

## Protocol v1

ASCII lines, LF terminated (CRLF accepted), max 159 characters, one JSON response per command. No startup chatter. A test blocks command parsing until it completes; STOP releases the fixture between tests, and cannot interrupt an active test. Outputs are released at completion. Only one request may be outstanding.

```text
HELLO
CONFIG transistor 14 2 3 15 4 5 16 6 7 5000 680 470000
RUN
CONFIG gate 2 8 9 17
RUN
STOP
```

Non-gate CONFIG: `CONFIG MODE ADC1 LOW1 HIGH1 ADC2 LOW2 HIGH2 ADC3 LOW3 HIGH3 VCC_MV RL_OHM RH_OHM`, with MODE transistor/diode/resistor. Gate CONFIG: `CONFIG gate N INPUT1 ... INPUTN OUTPUT_ADC`. Numeric pins: A0=14 through A5=19; D2–D13 retain their numbers. A failed CONFIG or overlong line invalidates configuration; RUN then requires a fresh valid CONFIG. Invalid requests return `{"ok":false,"error":"..."}`. HELLO reports board `uno328p`, protocol `1`. Configuration never energizes a fixture; only RUN does.

## Possible extensions

Capacitance using timed RC charge/discharge is a natural next mode, followed by capacitor leakage, LED forward voltage at several currents, and optocoupler current-transfer ratio with a dedicated four-terminal fixture. Inductance/ESR need different timing and analog calibration; do not infer their accuracy from these DC tests. JFETs, depletion MOSFETs, IGBTs, thyristors and TRIACs need separate recognition paths and tests. They are not implemented here.
