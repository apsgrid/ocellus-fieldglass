#!/usr/bin/env python3
import argparse
import pathlib
import re
import sys
import time

import serial


def open_serial(port: str, baud: int) -> serial.Serial:
    # Set control lines before opening to avoid needlessly toggling EN/BOOT on
    # common USB-serial adapters.
    s = serial.Serial()
    s.port = port
    s.baudrate = baud
    s.timeout = 0.15
    s.dtr = False
    s.rts = False
    s.open()
    time.sleep(0.25)
    s.reset_input_buffer()
    return s


def collect_lines(s: serial.Serial, seconds: float):
    end = time.time() + seconds
    out = []
    while time.time() < end:
        raw = s.readline()
        if raw:
            out.append(raw.decode(errors="replace").rstrip())
    return out


def send_line(s: serial.Serial, command: str):
    s.write((command.rstrip("\r\n") + "\n").encode())


def command_simple(s: serial.Serial, command: str, seconds: float = 2.0):
    send_line(s, command)
    for line in collect_lines(s, seconds):
        print(line)


def command_dump(s: serial.Serial, remote: str, output: pathlib.Path):
    send_line(s, f"DUMP {remote}")
    started = False
    expected = None
    written = 0
    with output.open("wb") as f:
        deadline = time.time() + 30
        while time.time() < deadline:
            raw = s.readline()
            if not raw:
                continue
            if not started:
                text = raw.decode(errors="replace").rstrip()
                m = re.match(r"DUMP BEGIN\s+(\S+)\s+(\d+)", text)
                if m:
                    expected = int(m.group(2))
                    started = True
                else:
                    print(text, file=sys.stderr)
                continue
            if raw.startswith(b"DUMP END"):
                break
            f.write(raw)
            written += len(raw)
    if not started:
        output.unlink(missing_ok=True)
        raise SystemExit("Fieldglass did not begin the dump")
    print(f"wrote {written} bytes to {output}" + (f" (device reported {expected})" if expected is not None else ""))


def main():
    ap = argparse.ArgumentParser(description="Ocellus Fieldglass serial maintenance client")
    ap.add_argument("--port", required=True, help="serial port, e.g. COM3 or /dev/ttyUSB0")
    ap.add_argument("--baud", type=int, default=115200)
    sub = ap.add_subparsers(dest="cmd", required=True)

    sub.add_parser("status")
    sub.add_parser("files")
    sub.add_parser("start")
    sub.add_parser("stop")
    sub.add_parser("help")

    dump = sub.add_parser("dump")
    dump.add_argument("remote")
    dump.add_argument("output", type=pathlib.Path)

    args = ap.parse_args()
    s = open_serial(args.port, args.baud)
    try:
        if args.cmd == "dump":
            command_dump(s, args.remote, args.output)
        else:
            command_simple(s, args.cmd.upper())
    finally:
        s.close()


if __name__ == "__main__":
    main()