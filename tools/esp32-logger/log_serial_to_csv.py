#!/usr/bin/env python3
"""Mitschneiden der ESP32-CSV-Ausgabe in eine Datei."""

from __future__ import annotations

import argparse
import datetime as dt
import pathlib
import sys

import serial


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="ESP32 PWM CSV seriell in Datei schreiben")
    parser.add_argument("--port", required=True, help="Serieller Port, z. B. COM7")
    parser.add_argument("--baud", type=int, default=115200, help="Baudrate, Standard: 115200")
    parser.add_argument("--out", required=True, help="Zieldatei fuer den CSV-Mitschnitt")
    parser.add_argument(
        "--host-time",
        action="store_true",
        help="Fuegt einen lokalen Host-Zeitstempel als erste CSV-Spalte hinzu",
    )
    return parser.parse_args()


def host_now_iso() -> str:
    return dt.datetime.now().astimezone().isoformat(timespec="milliseconds")


def main() -> int:
    args = parse_args()
    output_path = pathlib.Path(args.out)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    print(f"[logger] opening {args.port} @ {args.baud}", file=sys.stderr)
    print(f"[logger] writing to {output_path}", file=sys.stderr)

    with serial.Serial(args.port, args.baud, timeout=1) as ser, output_path.open(
        "w", encoding="utf-8", newline=""
    ) as out_file:
        header_written = False

        while True:
            try:
                raw = ser.readline()
            except serial.SerialException as exc:
                print(f"[logger] serial error: {exc}", file=sys.stderr)
                return 1

            if not raw:
                continue

            try:
                line = raw.decode("utf-8", errors="replace").strip()
            except UnicodeDecodeError:
                continue

            if not line:
                continue

            if not header_written and args.host_time:
                if line == "time_us,channel,pulse_us,period_us,state":
                    out_file.write("host_local_iso," + line + "\n")
                    out_file.flush()
                    header_written = True
                    continue

            if not header_written:
                out_file.write(line + "\n")
                out_file.flush()
                header_written = True
                continue

            if args.host_time:
                out_file.write(f"{host_now_iso()},{line}\n")
            else:
                out_file.write(line + "\n")

            out_file.flush()


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        print("\n[logger] interrupted", file=sys.stderr)
        raise SystemExit(0)
