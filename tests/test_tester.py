import importlib.util
import itertools
import json
import os
from pathlib import Path
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]
BINARY = ROOT / 'build/tester-native'
spec = importlib.util.spec_from_file_location('controller', ROOT / 'host/tester.py')
controller = importlib.util.module_from_spec(spec)
spec.loader.exec_module(controller)
DEFAULT = '14 2 3 15 4 5 16 6 7'


def firmware(commands, device='npn', **model):
    environment = dict(os.environ, SIM_DEVICE=device)
    environment.update({'SIM_' + k.upper(): str(v) for k, v in model.items()})
    proc = subprocess.run([str(BINARY)], input='\n'.join(commands) + '\n',
                          capture_output=True, text=True, env=environment, timeout=30)
    if proc.returncode:
        raise AssertionError(proc.stderr)
    return [json.loads(line) for line in proc.stdout.splitlines()]


def measure(mode='transistor', device='npn', mapping=DEFAULT, **model):
    responses = firmware([f'CONFIG {mode} {mapping} 5000 680 470000', 'RUN'],
                         device, **model)
    assert responses[0]['ok'], responses
    return responses[1]


class SpiceTests(unittest.TestCase):
    def test_all_transistor_pin_permutations(self):
        for device, kind in [('npn', 'NPN'), ('pnp', 'PNP'),
                             ('nmos', 'N-MOSFET'), ('pmos', 'P-MOSFET')]:
            for c, b, e in itertools.permutations(range(3)):
                with self.subTest(device=device, pins=(c, b, e)):
                    result = measure(device=device, pins=f'{c} {b} {e}')
                    self.assertEqual(result['kind'], kind)
                    self.assertEqual(result['control'], b + 1)
                    self.assertEqual(result['collector_or_drain'], c + 1)
                    self.assertEqual(result['emitter_or_source'], e + 1)
                    if device in ('npn', 'pnp'):
                        self.assertTrue(150 < result['hfe_estimate'] < 220, result)
                        self.assertTrue(500 < result['vbe_mv'] < 850, result)

    def test_gain_sweep(self):
        for device in ('npn', 'pnp'):
            for beta in (20, 60, 300):
                with self.subTest(device=device, beta=beta):
                    result = measure(device=device, beta=beta)
                    self.assertEqual(result['kind'], device.upper())
                    self.assertLess(abs(result['hfe_estimate']/beta-1), .20)

    def test_open_short_passives_not_transistors(self):
        for device in ('open', 'short', 'resistor', 'diode'):
            with self.subTest(device=device):
                self.assertEqual(measure(device=device)['kind'], 'unknown')

    def test_diode_polarities(self):
        for a, c in itertools.permutations(range(3), 2):
            result = measure('diode', 'diode', pins=f'{a} {3-a-c} {c}')
            self.assertEqual(len(result['junctions']), 1)
            junction = result['junctions'][0]
            self.assertEqual((junction['anode'], junction['cathode']), (a+1, c+1))
            self.assertTrue(450 < junction['mv'] < 900)

    def test_resistance_dual_range(self):
        for ohms in (100, 680, 1000, 10000, 470000, 1000000):
            with self.subTest(ohms=ohms):
                result = measure('resistor', 'resistor', pins='0 2 1', ohms=ohms)
                self.assertIsNotNone(result['ohms'])
                self.assertLess(abs(result['ohms']/ohms-1), .06)
        self.assertIsNone(measure('resistor', 'open')['ohms'])
        self.assertLess(measure('resistor', 'short', pins='0 2 1')['ohms'], 5)
        self.assertIsNone(measure('resistor', 'diode', pins='0 2 1')['ohms'])

    def test_remapped_probe_network(self):
        mapping = '19 11 10 18 9 8 17 7 6'
        result = measure(mapping=mapping, probes=mapping, pins='2 0 1')
        self.assertEqual(result['kind'], 'NPN')
        self.assertEqual(result['control'], 1)
        self.assertEqual(result['collector_or_drain'], 3)
        self.assertEqual(result['emitter_or_source'], 2)


class ProtocolTests(unittest.TestCase):
    def test_all_gate_families(self):
        for name in ('AND', 'NAND', 'OR', 'NOR', 'XOR', 'XNOR', 'NOT', 'BUFFER'):
            for n in ((1,) if name in ('NOT', 'BUFFER') else range(2, 5)):
                expected = controller.expected_levels(name, n)
                pins = ' '.join(str(i) for i in range(8, 8+n))
                result = firmware([f'CONFIG gate {n} {pins} 17', 'RUN'], 'gate',
                                  inputs=pins, truth=''.join(map(str, expected)))[1]
                controller.assess_gate(result, expected)
                self.assertTrue(result['pass'])

    def test_bad_or_undefined_gate(self):
        result = firmware(['CONFIG gate 2 8 9 17', 'RUN'], 'gate', truth='001x')[1]
        self.assertEqual(result['rows'][3]['level'], -1)
        self.assertFalse(controller.assess_gate(result, [0, 0, 0, 1])['pass'])

    def test_protocol_errors_and_recovery(self):
        commands = ['HELLO', 'RUN', 'BOGUS', 'CONFIG gate 0 17',
                    'CONFIG gate 2 8 8 17', 'CONFIG gate 2 0 9 17',
                    'CONFIG gate 2 8 9 12', 'CONFIG gate 2 8 9 99',
                    'CONFIG transistor ' + DEFAULT + ' 5000 680 99999999999999999999999',
                    'X'*200, 'RUN', 'CONFIG gate 2 8 9 17', 'RUN', 'STOP']
        results = firmware(commands, 'gate')
        self.assertEqual(results[0]['protocol'], 1)
        for reply in results[1:11]:
            self.assertFalse(reply['ok'], reply)
        self.assertTrue(all(r['ok'] for r in results[11:]))

    def test_invalid_reconfiguration_cannot_run_old_mode(self):
        results = firmware(['CONFIG gate 2 8 9 17', 'CONFIG bogus 1', 'RUN'], 'gate')
        self.assertFalse(results[-1]['ok'])

    def test_python_validation_and_end_to_end(self):
        args = controller.parser().parse_args(['--simulate', str(BINARY), '--mode', 'gate',
                                               '--inputs', 'D8', 'D9', '--output', 'A3', '--expect', 'AND'])
        command, expected = controller.configure(args)
        self.assertEqual(command, 'CONFIG gate 2 8 9 17')
        self.assertEqual(expected, [0, 0, 0, 1])
        args.output = 8
        with self.assertRaises(ValueError):
            controller.configure(args)
        for truth, code in [('0001', 0), ('1111', 1), ('00x1', 1)]:
            proc = subprocess.run(['python3', 'host/tester.py', '--simulate', str(BINARY),
                                   '--mode', 'gate', '--expect', 'AND'], cwd=ROOT,
                                  capture_output=True, text=True,
                                  env=dict(os.environ, SIM_DEVICE='gate', SIM_TRUTH=truth), timeout=10)
            self.assertEqual(proc.returncode, code, proc.stderr)
            self.assertEqual(json.loads(proc.stdout)['pass'], code == 0)

    def test_protocol_client_rejects_bad_responses(self):
        class Stream:
            def __init__(self, response): self.response = response
            def write(self, _): pass
            def flush(self): pass
            def readline(self): return self.response
        for response in (b'', b'garbage', b'[]', b'{"ok":false}', b'{"ok":true,"protocol":9}'):
            with self.assertRaises(controller.ProtocolError):
                controller.Client(Stream(response)).hello()


if __name__ == '__main__':
    unittest.main()
