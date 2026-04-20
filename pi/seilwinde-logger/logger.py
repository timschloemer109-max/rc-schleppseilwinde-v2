#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import signal
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

import serial
import serial.tools.list_ports

ARDUINO_HEADER = [
    "record_type",
    "arduino_ms",
    "run_state",
    "stop_reason",
    "esc_arm_state",
    "rc_mode",
    "rc_speed_us",
    "rc_speed_valid",
    "rc_trigger_us",
    "rc_trigger_valid",
    "esc_output_us",
    "current_a",
    "hall_pulse_count",
    "delta_pulses",
    "pulses_per_s",
    "last_hall_age_ms",
    "rope_pulses_avg",
    "end_switch_active",
    "end_switch_latched",
    "fault_latched",
    "learning_active",
    "event_name",
    "event_value",
]
CSV_HEADER = ["host_utc", *ARDUINO_HEADER]
EXPECTED_FIELD_COUNT = len(ARDUINO_HEADER)
PORT_PREFIXES = ("/dev/ttyUSB", "/dev/ttyACM")

RUNNING = True


def handle_signal(signum, _frame) -> None:
    global RUNNING
    RUNNING = False
    print(f"[logger] signal {signum} received, shutting down", file=sys.stderr)


def utc_now_iso() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds")


def utc_now_stamp() -> str:
    return datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Log RC-Schleppseilwinde telemetry from Arduino serial to CSV.")
    parser.add_argument("--port", help="Explicit serial port, for example /dev/ttyUSB0")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baud rate")
    parser.add_argument("--log-dir", type=Path, default=Path("/var/log/seilwinde"), help="Directory for CSV logs")
    parser.add_argument(
        "--scan-delay",
        type=float,
        default=2.0,
        help="Delay in seconds between reconnect attempts while no port is available",
    )
    parser.add_argument(
        "--line-timeout",
        type=float,
        default=1.0,
        help="Serial line read timeout in seconds",
    )
    return parser.parse_args()


def find_serial_port(explicit_port: str | None) -> str | None:
    if explicit_port:
        return explicit_port

    candidates = []
    for port in serial.tools.list_ports.comports():
        if port.device.startswith(PORT_PREFIXES):
            candidates.append(port.device)

    if not candidates:
        return None

    candidates.sort()
    return candidates[0]


def open_serial(port_name: str, baud: int, timeout: float) -> serial.Serial:
    connection = serial.Serial(port=port_name, baudrate=baud, timeout=timeout, write_timeout=timeout)
    connection.dtr = False
    connection.rts = False
    return connection


def open_log_file(log_dir: Path) -> tuple[Path, object, csv.writer]:
    log_dir.mkdir(parents=True, exist_ok=True)
    file_path = log_dir / f"telemetry_{utc_now_stamp()}.csv"
    handle = file_path.open("w", newline="", encoding="utf-8")
    writer = csv.writer(handle)
    writer.writerow(CSV_HEADER)
    handle.flush()
    return file_path, handle, writer


def parse_arduino_line(raw_line: str) -> list[str] | None:
    if not raw_line:
        return None

    if raw_line.startswith("#"):
        return None

    parsed = next(csv.reader([raw_line]), None)
    if parsed is None:
        return None

    if parsed == ARDUINO_HEADER:
        return None

    if len(parsed) != EXPECTED_FIELD_COUNT:
        return None

    record_type = parsed[0]
    if record_type not in {"telemetry", "event"}:
        return None

    return parsed


def log_session(port_name: str, args: argparse.Namespace) -> None:
    connection = open_serial(port_name, args.baud, args.line_timeout)
    log_path, handle, writer = open_log_file(args.log_dir)
    print(f"[logger] logging {port_name} -> {log_path}", file=sys.stderr)

    try:
        while RUNNING:
            raw_bytes = connection.readline()
            if not raw_bytes:
                continue

            raw_line = raw_bytes.decode("utf-8", errors="replace").strip()
            parsed = parse_arduino_line(raw_line)
            if parsed is None:
                continue

            writer.writerow([utc_now_iso(), *parsed])
            handle.flush()
    finally:
        try:
            connection.close()
        finally:
            handle.close()


def main() -> int:
    args = parse_args()
    signal.signal(signal.SIGINT, handle_signal)
    signal.signal(signal.SIGTERM, handle_signal)

    while RUNNING:
      port_name = find_serial_port(args.port)
      if port_name is None:
          time.sleep(args.scan_delay)
          continue

      try:
          log_session(port_name, args)
      except serial.SerialException as exc:
          print(f"[logger] serial error on {port_name}: {exc}", file=sys.stderr)
          time.sleep(args.scan_delay)
      except OSError as exc:
          print(f"[logger] file or device error on {port_name}: {exc}", file=sys.stderr)
          time.sleep(args.scan_delay)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
