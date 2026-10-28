#!/usr/bin/env python3
"""Configure and run the educational tester; pyserial is needed only for USB."""
import argparse
import json
import subprocess
import selectors
import sys
import time


class ProtocolError(Exception):
    pass


def pin(text):
    text = str(text).upper()
    try:
        number = 14 + int(text[1:]) if text.startswith('A') else int(text.removeprefix('D'))
    except ValueError as exc:
        raise argparse.ArgumentTypeError(f'invalid Arduino pin: {text}') from exc
    if not 2 <= number <= 19:
        raise argparse.ArgumentTypeError('use D2–D13 or A0–A5; D0/D1 are serial')
    return number


def probe(text):
    parts = text.split(':')
    if len(parts) != 3:
        raise argparse.ArgumentTypeError('probe format: ADC:680-ohm-pin:470k-pin')
    result = tuple(pin(p) for p in parts)
    if result[0] < 14:
        raise argparse.ArgumentTypeError('probe sense pin must be A0–A5')
    return result


class Client:
    def __init__(self, stream):
        self.stream = stream

    def request(self, command):
        self.stream.write((command + '\n').encode('ascii'))
        self.stream.flush()
        line = self.stream.readline()
        if not line:
            raise ProtocolError('no firmware response (check port, baud, wiring, or timeout)')
        try:
            reply = json.loads(line)
        except (ValueError, UnicodeError) as exc:
            raise ProtocolError(f'invalid firmware response: {line!r}') from exc
        if not isinstance(reply, dict) or reply.get('ok') is not True:
            raise ProtocolError(f'firmware rejected command: {reply}')
        return reply

    def hello(self):
        result = self.request('HELLO')
        if result.get('protocol') != 1 or result.get('board') != 'uno328p':
            raise ProtocolError(f'unsupported firmware: {result}')


def expected_levels(name, n):
    if name in ('NOT', 'BUFFER') and n != 1:
        raise ValueError(f'{name} requires one input')
    result = []
    for row in range(1 << n):
        bits = [(row >> i) & 1 for i in range(n)]
        value = {'AND': all(bits), 'OR': any(bits), 'XOR': sum(bits) % 2,
                 'NAND': not all(bits), 'NOR': not any(bits),
                 'XNOR': not (sum(bits) % 2), 'NOT': not bits[0],
                 'BUFFER': bits[0]}[name]
        result.append(int(value))
    return result


def assess_gate(result, expected):
    rows = result.get('rows', [])
    if len(rows) != len(expected) or [r.get('bits') for r in rows] != list(range(len(expected))):
        raise ProtocolError('incomplete or reordered truth table')
    result['pass'] = all(r.get('level') == expected[i] for i, r in enumerate(rows))
    result['expected'] = expected
    return result


def parser():
    p = argparse.ArgumentParser(description=__doc__)
    connection = p.add_mutually_exclusive_group(required=True)
    connection.add_argument('--port', help='USB serial device, e.g. /dev/ttyACM0 or COM3')
    connection.add_argument('--simulate', help='path to native firmware executable')
    p.add_argument('--mode', choices=['transistor', 'gate', 'diode', 'resistor'], required=True)
    p.add_argument('--probe', type=probe, action='append', help='repeat three times; default A0:2:3 A1:4:5 A2:6:7')
    p.add_argument('--inputs', type=pin, nargs='+', default=[8, 9])
    p.add_argument('--output', type=pin, default=17, help='gate output sense pin (A0–A5), default A3')
    p.add_argument('--expect', choices=['AND', 'NAND', 'OR', 'NOR', 'XOR', 'XNOR', 'NOT', 'BUFFER'])
    p.add_argument('--truth', help='custom expected output bits in binary-count row order, e.g. 0001')
    p.add_argument('--vcc-mv', type=int, default=5000)
    p.add_argument('--low-ohms', type=int, default=680)
    p.add_argument('--high-ohms', type=int, default=470000)
    p.add_argument('--timeout', type=float, default=30)
    return p


def configure(args):
    expected = None
    if args.expect and args.truth:
        raise ValueError('choose --expect or --truth')
    if args.mode == 'gate':
        pins = args.inputs + [args.output]
        if not 1 <= len(args.inputs) <= 4 or args.output < 14:
            raise ValueError('gate requires 1–4 inputs and an analog output sense pin')
        if args.vcc_mv != 5000:
            raise ValueError('gate mode requires a nominal 5 V supply')
        command = 'CONFIG gate ' + ' '.join(map(str, [len(args.inputs)] + pins))
        if args.expect:
            expected = expected_levels(args.expect, len(args.inputs))
        if args.truth:
            if len(args.truth) != 1 << len(args.inputs) or set(args.truth) - {'0', '1'}:
                raise ValueError('--truth must contain exactly 2**inputs binary digits')
            expected = list(map(int, args.truth))
    else:
        if args.expect or args.truth:
            raise ValueError('--expect/--truth apply to gate mode')
        probes = args.probe or [(14, 2, 3), (15, 4, 5), (16, 6, 7)]
        if len(probes) != 3:
            raise ValueError('provide exactly three --probe groups')
        pins = [p for group in probes for p in group]
        if not (4500 <= args.vcc_mv <= 5500 and 600 <= args.low_ohms <= 800 and 400000 <= args.high_ohms <= 550000):
            raise ValueError('calibration outside supported 5 V / 680 ohm / 470k range')
        command = f'CONFIG {args.mode} ' + ' '.join(map(str, pins + [args.vcc_mv, args.low_ohms, args.high_ohms]))
    if len(set(pins)) != len(pins):
        raise ValueError('physical pins must be unique')
    if args.timeout <= 0:
        raise ValueError('timeout must be positive')
    return command, expected


class ProcessStream:
    def __init__(self, process, timeout):
        self.process = process
        self.timeout = timeout
    def write(self, data):
        return self.process.stdin.write(data)
    def flush(self):
        self.process.stdin.flush()
    def readline(self):
        with selectors.DefaultSelector() as selector:
            selector.register(self.process.stdout, selectors.EVENT_READ)
            if not selector.select(self.timeout):
                raise ProtocolError("native firmware response timed out")
            return self.process.stdout.readline()


def main(argv=None):
    p = parser()
    args = p.parse_args(argv)
    try:
        command, expected = configure(args)
    except ValueError as exc:
        p.error(str(exc))
    process = None
    stream = None
    try:
        if args.simulate:
            process = subprocess.Popen([args.simulate], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
            stream = ProcessStream(process, args.timeout)
        else:
            try:
                import serial
            except ImportError as exc:
                raise ProtocolError('install pyserial: python3 -m pip install -r requirements.txt') from exc
            stream = serial.Serial(args.port, 115200, timeout=args.timeout, write_timeout=3)
            time.sleep(2)  # Uno resets when serial DTR opens.
            stream.reset_input_buffer()
        client = Client(stream)
        client.hello()
        client.request(command)
        result = client.request('RUN')
        if expected is not None:
            assess_gate(result, expected)
        print(json.dumps(result, indent=2))
        return 1 if result.get('pass') is False else 0
    except (OSError, ProtocolError) as exc:
        print(f'tester: {exc}', file=sys.stderr)
        return 2
    finally:
        if process:
            process.stdin.close()
            try:
                process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
            process.stdout.close()
        elif stream:
            stream.close()


if __name__ == '__main__':
    sys.exit(main())
