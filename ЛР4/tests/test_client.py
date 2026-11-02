import importlib.util
import json
import os
from pathlib import Path
import pty
import sys
import tempfile
import time
import unittest
spec = importlib.util.spec_from_file_location('compass_client', Path(__file__).parents[1] / 'client' / 'client.py')
client = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = client
spec.loader.exec_module(client)

class ClientTests(unittest.TestCase):
    def test_protocol(self):
        sample = client.parse_sample('DATA 123 -1000 2000 0 9000 0 100 200 1 1 61 77 40')
        self.assertEqual(sample.field, (-1000, 2000, 0))
        self.assertEqual(sample.scale, (0, 100, 200))
        for line in ('DATA junk', 'PONG COMPASS 1', 'DATA 0 0 0 0 36000 100 100 100 1 1 61 0 40',
                     'DATA 0 0 0 0 0 201 100 100 1 1 61 0 40'):
            self.assertIsNone(client.parse_sample(line))

    def test_demo_handshake_scales_and_log(self):
        with tempfile.TemporaryDirectory() as folder:
            logfile = Path(folder) / 'demo.jsonl'
            link = client.Link(None, logfile, demo=True)
            self.assertIn('PONG COMPASS 1', link.receive())
            link.send('SCALE 0 200 100')
            link.last_demo = 0
            lines = link.receive()
            self.assertTrue(any(s.startswith('DATA ') for s in lines))
            sample = next(client.parse_sample(s) for s in lines if s.startswith('DATA '))
            self.assertEqual(sample.field[0], 0)
            self.assertEqual(sample.scale, (0, 200, 100))
            link.close()
            entries = [json.loads(s) for s in logfile.read_text().splitlines()]
            self.assertEqual(entries[0]['text'], 'PING\n')
            self.assertTrue(all(e['demo'] and e['time'].endswith('+00:00') for e in entries))

    @unittest.skipIf(os.name == 'nt', 'PTY is POSIX-only')
    def test_real_serial_pty_fragmented_messages(self):
        master, slave = pty.openpty()
        with tempfile.TemporaryDirectory() as folder:
            link = client.Link(os.ttyname(slave), Path(folder) / 'uart.jsonl')
            try:
                self.assertEqual(os.read(master, 100), b'PING\n')
                os.write(master, b'PONG COM')
                time.sleep(.02)
                self.assertEqual(link.receive(), [])
                os.write(master, b'PASS 1\r\nDATA 1 0 0 0 -1 0 0 0 1 1 61 0 40\r\n')
                time.sleep(.02)
                lines = link.receive()
                self.assertEqual(lines[0], 'PONG COMPASS 1')
                self.assertEqual(client.parse_sample(lines[1]).heading, -1)
                link.send('SCALE 100 0 200')
                self.assertEqual(os.read(master, 100), b'SCALE 100 0 200\n')
                link.buffer = bytearray(b'x' * 4096)
                os.write(master, b'xxxx')
                time.sleep(.02)
                link.receive()
                self.assertLessEqual(len(link.buffer), 4096)
            finally:
                link.close()
                os.close(master)
                os.close(slave)

if __name__ == '__main__':
    unittest.main()
