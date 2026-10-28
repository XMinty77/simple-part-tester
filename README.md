# Arduino device tester

Educational 5 V Uno/Nano tester: NPN/PNP, enhancement MOSFETs, gate truth tables, diodes, resistors.

1. Wire the [probe network or gate fixture](docs/hardware.md); upload `firmware/tester/tester.ino`.
2. `python3 -m pip install -r requirements.txt`
3. `python3 host/tester.py --port /dev/ttyACM0 --mode transistor`
4. Gates: `python3 host/tester.py --port /dev/ttyACM0 --mode gate --inputs D8 D9 --output A3 --expect NAND`

`make test` runs ngspice simulations; `make avr` builds Uno firmware. See [usage](docs/usage.md), [validation limits](docs/validation.md), and [upstream attribution](vendor/NOTICE.md).

**Unpowered, discharged, out-of-circuit parts only. Simulation does not guarantee hardware accuracy.**
