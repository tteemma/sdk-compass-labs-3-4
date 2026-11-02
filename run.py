#!/usr/bin/env python3
"""One-command client bootstrap and verified prebuilt firmware flashing."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import venv

ROOT = Path(__file__).resolve().parent
PROJECTS = {3: 'Compass_LR3_V2', 4: 'Compass_RTOS_LR4_V2'}

def environment():
    folder = ROOT / 'tools' / 'venv'
    python = folder / ('Scripts/python.exe' if os.name == 'nt' else 'bin/python')
    if not python.exists():
        venv.EnvBuilder(with_pip=True).create(folder)
    check = subprocess.run([str(python), '-c', 'import serial, curses'], capture_output=True)
    if check.returncode:
        subprocess.run([str(python), '-m', 'pip', 'install', '-r',
                        str(ROOT / 'ЛР3/client/requirements.txt')], check=True)
    return str(python)

def verify(lab):
    folder = ROOT / f'ЛР{lab}'
    hashes = json.loads((folder / 'artifacts/sha256.json').read_text())
    for filename, digest in hashes.items():
        p = folder / filename
        if hashlib.sha256(p.read_bytes()).hexdigest() != digest:
            raise RuntimeError(f'Firmware checksum mismatch: {p}')
    print(f'LR{lab}: all firmware checksums OK', flush=True)

def flash(lab, mcu, check_only):
    executable = os.environ.get('OPENOCD') or shutil.which('openocd')
    if not executable:
        raise RuntimeError('Install OpenOCD: macOS: brew install open-ocd; Ubuntu: sudo apt install openocd; Windows: add OpenOCD bin to PATH, or set OPENOCD to its executable.')
    project = ROOT / f'ЛР{lab}' / PROJECTS[lab]
    command = [executable, '-s', str(project / 'openocd'), '-f',
               str(project / f'openocd/SDK{mcu}.cfg')]
    if check_only:
        command += ['-c', 'shutdown']
    else:
        image = f'Debug/{PROJECTS[lab]}_HAL_STM32F{mcu}xx.elf'
        command += ['-c', f'program {image} verify reset exit']
    subprocess.run(command, cwd=project, check=True)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--lab', type=int, choices=(3,4), default=3)
    parser.add_argument('--demo', action='store_true')
    parser.add_argument('--port', help='COM5, /dev/cu.usbserial-..., or /dev/ttyUSB1; omitted: auto-select single port')
    parser.add_argument('--log', default='uart.jsonl')
    parser.add_argument('--flash', action='store_true', help='Flash verified prebuilt image, then start client')
    parser.add_argument('--flash-only', action='store_true')
    parser.add_argument('--mcu', choices=('407','427'), default='427')
    parser.add_argument('--check-config', action='store_true', help='Parse OpenOCD config without connecting to hardware')
    parser.add_argument('--verify', action='store_true', help='Verify all prebuilt firmware hashes and exit')
    parser.add_argument('--setup', action='store_true', help='Install client dependencies and exit')
    parser.add_argument('--list-ports', action='store_true')
    args = parser.parse_args()
    if args.demo and (args.flash or args.flash_only or args.check_config):
        parser.error('--demo cannot be combined with flashing/config checks')
    try:
        if args.verify:
            for lab in (3,4): verify(lab)
            return
        if args.flash or args.flash_only or args.check_config:
            verify(args.lab)
            flash(args.lab, args.mcu, args.check_config)
            if args.flash_only or args.check_config:
                return
        python = environment()
        if args.setup:
            print('Client environment ready:', python)
            return
        if args.list_ports:
            subprocess.run([python, '-m', 'serial.tools.list_ports', '-v'], check=True)
            return
        command = [python, str(ROOT / f'ЛР{args.lab}/client/client.py'), '--log', args.log]
        if args.demo:
            command.append('--demo')
        else:
            port = args.port
            if not port:
                found = json.loads(subprocess.check_output([python, '-c',
                    'import json; from serial.tools.list_ports import comports; print(json.dumps([p.device for p in comports()]))'], text=True))
                if len(found) != 1:
                    raise RuntimeError('Specify --port; available ports: ' + ', '.join(found) + '. Use --list-ports. Select the FTDI UART channel, not JTAG.')
                port = found[0]
            command.append(port)
        raise SystemExit(subprocess.call(command))
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        parser.exit(1, str(error) + '\n')

if __name__ == '__main__':
    main()
