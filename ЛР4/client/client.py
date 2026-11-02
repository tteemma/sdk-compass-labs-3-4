#!/usr/bin/env python3
"""SDK compass TUI: python client.py PORT [--log uart.jsonl], or --demo.
No command-line entry UI: use arrows, X/Y/Z, +/- and Enter.
"""
import argparse
import curses
import datetime as dt
import json
import math
from pathlib import Path
import time
from dataclasses import dataclass

@dataclass
class Sample:
    tick: int
    field: tuple
    heading: int
    scale: tuple
    sensor_ok: bool
    display_ok: bool
    sensor_id: int
    frames: int
    frame_ms: int

def parse_sample(line):
    tokens = line.split()
    if len(tokens) != 14 or tokens[0] != 'DATA':
        return None
    try:
        v = [int(s) for s in tokens[1:]]
    except ValueError:
        return None
    if not (-1 <= v[4] < 36000) or any(s < 0 or s > 200 for s in v[5:8]):
        return None
    if v[8] not in (0, 1) or v[9] not in (0, 1) or min(v[0], v[11], v[12]) < 0:
        return None
    return Sample(v[0], tuple(v[1:4]), v[4], tuple(v[5:8]), bool(v[8]), bool(v[9]), v[10], v[11], v[12])

class Link:
    def __init__(self, port, logfile, demo=False):
        self.log = Path(logfile).open('a', encoding='utf-8')
        self.demo = demo
        self.start = time.monotonic()
        self.scale = [100, 100, 100]
        self.buffer = bytearray()
        self.last_demo = 0
        self.messages = []
        self.serial = None
        try:
            if not demo:
                import serial
                self.serial = serial.Serial(port, 115200, timeout=0, write_timeout=0.2,
                                            xonxoff=False, rtscts=False, dsrdtr=False)
            self.send('PING')  # immediately after opening the port
        except BaseException:
            self.close()
            raise

    def record(self, direction, data):
        entry = {'time': dt.datetime.now(dt.timezone.utc).isoformat(), 'direction': direction,
                 'hex': data.hex(), 'text': data.decode('ascii', errors='backslashreplace'),
                 'demo': self.demo}
        self.log.write(json.dumps(entry, ensure_ascii=False) + '\n')
        self.log.flush()

    def send(self, command):
        data = (command + '\n').encode('ascii')
        if self.demo:
            self.record('TX', data)
            if command == 'PING':
                self.messages.append(b'PONG COMPASS 1\r\n')
            elif command.startswith('SCALE '):
                self.scale = [int(s) for s in command.split()[1:]]
                self.messages.append(b'OK SCALE\r\n')
        else:
            count = self.serial.write(data)
            self.record('TX', data[:count])
            if count != len(data):
                raise OSError('Incomplete UART write')

    def receive(self):
        if self.demo:
            now = time.monotonic()
            if now - self.last_demo >= 0.1:
                self.last_demo = now
                seconds = now - self.start
                raw = [int(30000 * math.cos(seconds / 3)), int(30000 * math.sin(seconds / 3)), 15000]
                field = [raw[i] * self.scale[i] // 100 for i in range(3)]
                heading = round(math.degrees(math.atan2(field[1], field[0])) * 100) % 36000 if any(field[:2]) else -1
                vals = [int(seconds * 1000), *field, heading, *self.scale, 1, 1, 0x3d,
                        int(seconds * 25), 40]
                self.messages.append(('DATA ' + ' '.join(map(str, vals)) + '\r\n').encode('ascii'))
            data = b''.join(self.messages)
            self.messages.clear()
        else:
            data = self.serial.read(min(self.serial.in_waiting, 4096))
        if data:
            self.record('RX', data)
        self.buffer.extend(data)
        lines = []
        while b'\n' in self.buffer:
            raw, _, rest = self.buffer.partition(b'\n')
            self.buffer = bytearray(rest)
            lines.append(raw.rstrip(b'\r').decode('ascii', errors='replace'))
        if len(self.buffer) > 4096:
            self.buffer.clear()
            lines.append('ERROR oversized incoming line')
        return lines

    def close(self):
        if self.serial is not None:
            self.serial.close()
        self.log.close()

def run(screen, link):
    curses.curs_set(0)
    screen.timeout(20)
    edits = [100, 100, 100]
    selected = 0
    sample = None
    pong = False
    last_receive = 0
    started = time.monotonic()
    last_ping = started
    message = 'Waiting for PONG...'
    while True:
        now = time.monotonic()
        for line in link.receive():
            if line == 'PONG COMPASS 1':
                pong = True
                message = 'Connected; PONG received'
            elif line.startswith(('OK ', 'ERR ', 'ERROR', 'READY')):
                message = line
            candidate = parse_sample(line)
            if candidate is not None:
                sample = candidate
                last_receive = now
        if not pong and now - last_ping >= 1:
            if now - started > 5:
                message = 'No PONG: check firmware, port, 115200 8N1; q exits'
            link.send('PING')
            last_ping = now
        screen.erase()
        height, width = screen.getmaxyx()
        def put(y, x, text):
            if 0 <= y < height and 0 <= x < width - 1:
                try:
                    screen.addstr(y, x, str(text)[:width - x - 1])
                except curses.error:
                    pass
        put(0, 0, 'SDK-1.1M | Compass | ' + ('DEMO: synthetic data' if link.demo else '115200 8N1'))
        put(2, 0, message)
        if sample:
            status = 'LIVE' if now - last_receive < 1 else 'STALE: no data >1s'
            put(4, 0, f'{status}  sensor={sample.sensor_ok} OLED={sample.display_ok} ID=0x{sample.sensor_id:02x}')
            for i in range(3):
                put(6 + i, 0, f'{"XYZ"[i]}: {sample.field[i] / 1000:+.3f} uT  applied={sample.scale[i]}%')
            put(10, 0, 'Heading: ' + (f'{sample.heading/100:.2f} deg' if sample.heading >= 0 and sample.sensor_ok else 'undefined'))
            put(11, 0, f'Frames: {sample.frames} (OLED {sample.frames%100:02})  interval={sample.frame_ms} ms')
            # Top is X+, right is Y+; the same reference is used on the OLED.
            if sample.heading >= 0 and sample.sensor_ok:
                a = math.radians(sample.heading / 100)
                for step in range(1, 7):
                    x = 57 + round(math.sin(a) * step * 1.8)
                    y = 10 - round(math.cos(a) * step)
                    put(y, x, '*' if step < 6 else '^')
                put(10, 57, '+')
                put(3, 57, 'N')
        else:
            put(4, 0, 'Waiting for DATA...')
        put(14, 0, 'Scale 0..200%: ' + '  '.join(f'{">" if selected==i else " "}{"XYZ"[i]}={edits[i]}' for i in range(3)))
        put(16, 0, 'X/Y/Z or Left/Right: select | +/- or Up/Down: 1% | PgUp/PgDn: 10%')
        put(17, 0, 'Enter: apply all axes | R: restore 100% and apply | Q: quit')
        if height < 19 or width < 76:
            put(height - 1, 0, 'Enlarge terminal to at least 76x19')
        screen.refresh()
        key = screen.getch()
        if key in (ord('q'), ord('Q'), 27):
            return
        if key in (ord('x'), ord('X'), ord('y'), ord('Y'), ord('z'), ord('Z')):
            selected = 'xyz'.index(chr(key).lower())
        elif key == curses.KEY_LEFT:
            selected = (selected - 1) % 3
        elif key in (curses.KEY_RIGHT, 9):
            selected = (selected + 1) % 3
        elif key in (ord('+'), ord('='), curses.KEY_UP, curses.KEY_PPAGE):
            edits[selected] = min(200, edits[selected] + (10 if key == curses.KEY_PPAGE else 1))
        elif key in (ord('-'), curses.KEY_DOWN, curses.KEY_NPAGE):
            edits[selected] = max(0, edits[selected] - (10 if key == curses.KEY_NPAGE else 1))
        elif key in (10, 13, curses.KEY_ENTER, ord('r'), ord('R')):
            if key in (ord('r'), ord('R')):
                edits = [100, 100, 100]
            if pong:
                link.send('SCALE ' + ' '.join(map(str, edits)))
                message = 'Scale sent; applied values appear in next DATA'
            else:
                message = 'Waiting for handshake; settings not sent'

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('port', nargs='?', help='/dev/cu.usbserial... or COM5')
    parser.add_argument('--demo', action='store_true')
    parser.add_argument('--log', default='uart.jsonl')
    args = parser.parse_args()
    if not args.demo and not args.port:
        parser.error('Specify PORT or --demo')
    link = None
    try:
        link = Link(args.port, args.log, args.demo)
        curses.wrapper(run, link)
    except (ImportError, OSError, curses.error) as error:
        parser.exit(1, f'{error}\nInstall requirements.txt and run in a terminal.\n')
    except KeyboardInterrupt:
        pass
    finally:
        if link is not None:
            link.close()

if __name__ == '__main__':
    main()
