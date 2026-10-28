"""Separate, optional integration checks for the actual ATmega328P binary."""
import json
import os
import subprocess


def run(commands, **model):
    result = subprocess.run(['build/tester-avr-sim', 'build/avr/tester.elf'],
                            input='\n'.join(commands)+'\n', text=True, capture_output=True,
                            timeout=90, env=dict(os.environ, **model))
    assert result.returncode == 0, (result.stdout, result.stderr)
    return [json.loads(s) for s in result.stdout.splitlines() if s.startswith('{')]


for device, kind in [('npn', 'NPN'), ('pnp', 'PNP'), ('nmos', 'N-MOSFET'), ('pmos', 'P-MOSFET')]:
    replies = run(['HELLO', 'CONFIG transistor 14 2 3 15 4 5 16 6 7 5000 680 470000', 'RUN'], SIM_DEVICE=device)
    assert replies[0]['protocol'] == 1
    assert all(r['ok'] for r in replies), replies
    assert replies[2]['kind'] == kind, replies
    assert replies[2]['control'] == 2, replies
    assert replies[2]['collector_or_drain'] == 1, replies
    assert replies[2]['emitter_or_source'] == 3, replies
    print(f'AVR/ngspice {device}: {replies[2]}', flush=True)
replies = run(['CONFIG gate 2 8 9 17', 'RUN', 'CONFIG gate 2 8 8 17', 'RUN'], SIM_DEVICE='gate')
assert [r['level'] for r in replies[1]['rows']] == [0, 0, 0, 1], replies
assert not replies[2]['ok'] and not replies[3]['ok'], replies
print('AVR gate truth table, invalid CONFIG recovery, GPIO release: passed')

for resistance in (680, 470000, 1000000):
    replies = run(['CONFIG resistor 14 2 3 15 4 5 16 6 7 5000 680 470000', 'RUN'],
                  SIM_DEVICE='resistor', SIM_PINS='0 2 1', SIM_OHMS=str(resistance))
    assert replies[1]['ohms'] is not None, replies
    assert abs(replies[1]['ohms']/resistance-1) < .06, replies
print('AVR resistor arithmetic and 470k calibration: passed')
