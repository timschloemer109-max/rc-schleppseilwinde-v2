#!/usr/bin/env python3
"""Loopback-only Lua/host bridge to the actual winch Application and EX Bus.

Input: S sequence ch1Us ch2Us ch3Us ch4Us endSwitch buttonsHex\n
       H nondecreasingHallTotal\n
Output: R nowMs state escUs reason rcFresh arm endLatched rest hall errors
          menu32Hex telemetryExHex id:raw:decimals,...\n
Only a strictly advancing sample sequence refreshes RC. The independent host
clock continues on stalled/disconnected Lua, including real core RC expiry.
No physical firmware/peripheral or motor control is linked into this process.
"""
from __future__ import annotations
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import queue
import selectors
import shutil
import socket
import subprocess
import threading
import time

from exbus_master import channels, crc16_documented, crc8_polynomial, jetibox_request, telemetry_request

ROOT = Path(__file__).resolve().parents[1]
MAX_LINE = 160
TICK_SECONDS = 0.010


class EvidenceLog:
    """Optional bounded evidence writer, never a clock/RC dependency."""
    def __init__(self, path: Path):
        path.parent.mkdir(parents=True, exist_ok=True)
        self.file = path.open('a', encoding='utf-8')
        self.pending: queue.Queue = queue.Queue(maxsize=256)
        self.records = 0; self.dropped = 0; self.error = None
        self.writer = threading.Thread(target=self._write, daemon=True)
        self.writer.start()
        self.record('session_start', schema='winch-jeti-host-evidence-1',
                    platform='host_only', baud=250000, max_records=10000)

    def record(self, event: str, **fields):
        if self.records >= 10000:
            self.dropped += 1
            return
        item = {'utc': datetime.now(timezone.utc).isoformat(timespec='milliseconds'),
                'event': event, **fields}
        line = json.dumps(item, sort_keys=True, separators=(',', ':')) + '\n'
        if len(line) > 2048:
            self.dropped += 1
            return
        try:
            self.pending.put_nowait(line)
            self.records += 1
        except queue.Full:
            self.dropped += 1

    def _write(self):
        try:
            while True:
                line = self.pending.get()
                if line is None:
                    break
                self.file.write(line); self.file.flush()
        except OSError as error:
            self.error = str(error)
        finally:
            self.file.close()

    def close(self):
        self.record('session_end', accepted_records=self.records, dropped_records=self.dropped)
        # At shutdown only, give pending evidence a bounded opportunity to flush.
        try:
            self.pending.put(None, timeout=2)
        except queue.Full:
            return
        self.writer.join(timeout=3)


def build_helper(compiler: str | None = None, sanitize: bool = True,
                 artifact_name: str = 'jeti_live_core') -> tuple[Path, dict]:
    compiler = compiler or os.environ.get('CXX') or shutil.which('clang++') or shutil.which('g++')
    if not compiler:
        candidates = sorted((ROOT / '.build' / 'tools').glob('llvm-mingw-*/bin/clang++.exe'))
        compiler = str(candidates[-1]) if candidates else None
    if not compiler:
        raise RuntimeError('C++ compiler missing: use --compiler')
    folder = ROOT / '.build' / ('jeti-bridge-sanitized' if sanitize else 'jeti-bridge')
    folder.mkdir(parents=True, exist_ok=True)
    if artifact_name not in {'jeti_live_core', 'jeti_live_core_test'}:
        raise ValueError('Invalid host build artifact name')
    helper = folder / (artifact_name + ('.exe' if os.name == 'nt' else ''))
    flags = ['-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Wconversion', '-Wsign-conversion',
             '-Wshadow', '-Werror', '-g', '-O1', '-I' + str(ROOT / 'core')]
    if sanitize:
        flags += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
    subprocess.run([compiler, *flags, *sorted(str(path) for path in (ROOT / 'core').glob('*.cpp')),
                    str(ROOT / 'simulator' / 'jeti_live_core.cpp'), '-o', str(helper)], check=True)
    environment = os.environ.copy()
    environment['PATH'] = str(Path(compiler).resolve().parent) + os.pathsep + environment.get('PATH', '')
    environment['ASAN_OPTIONS'] = ('detect_leaks=0' if os.name == 'nt' else 'detect_leaks=1') + ':halt_on_error=1'
    environment['UBSAN_OPTIONS'] = 'halt_on_error=1:print_stacktrace=1'
    return helper, environment


def decode_reply(hex_text: str, packet_id: int, block_id: int) -> bytes:
    frame = bytes.fromhex(hex_text)
    if not (8 <= len(frame) <= 64 and frame[:2] == b'\x3b\x01' and frame[2] == len(frame)
            and frame[3] == packet_id and frame[4] == block_id and frame[5] == len(frame) - 8
            and crc16_documented(frame[:-2]) == int.from_bytes(frame[-2:], 'little')):
        raise ValueError('Invalid independent EX Bus reply validation')
    return frame[6:-2]


def decode_ex(payload: bytes) -> dict[int, tuple[int, int]]:
    if not (8 <= len(payload) <= 28 and payload[0] == 0x9f
            and payload[1] & 63 == len(payload) - 2
            and payload[2:7] == bytes.fromhex('00 a4 00 03 00')
            and crc8_polynomial(payload[1:-1]) == payload[-1]):
        raise ValueError('Invalid independent EX telemetry validation')
    kind = payload[1] >> 6
    if kind == 0:
        if len(payload) < 10 or payload[7] > 15 or 10 + (payload[8] >> 3) + (payload[8] & 7) != len(payload):
            raise ValueError('Invalid EX descriptor')
        return {}
    if kind != 1:
        raise ValueError('Unsupported EX type')
    result = {}
    position = 7
    while position < len(payload) - 1:
        identifier, value_type = payload[position] >> 4, payload[position] & 15
        width = {0: 1, 1: 2, 4: 3, 8: 4}.get(value_type, 0)
        position += 1
        if not (1 <= identifier <= 15 and width and position + width < len(payload)):
            raise ValueError('Invalid EX numeric field')
        data = bytearray(payload[position:position + width])
        last = data[-1]; data[-1] &= 31
        raw = int.from_bytes(data, 'little') * (-1 if last & 128 else 1)
        result[identifier] = (raw, (last >> 5) & 3)
        position += width
    if position != len(payload) - 1:
        raise ValueError('Invalid EX data tail')
    return result


class CoreSession:
    def __init__(self, helper: Path, environment: dict):
        self.process = subprocess.Popen([str(helper)], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                        stderr=subprocess.PIPE, text=True, bufsize=1, env=environment)
        self.responses: queue.Queue = queue.Queue(maxsize=4)
        self.reader = threading.Thread(target=self._read, daemon=True)
        self.reader.start()
        self.packet_id = 0
        self.channel_frames = 0; self.menu_replies = 0; self.telemetry_replies = 0
        self.readings: dict[int, tuple[int, int]] = {}
        self.status = {}

    def _read(self):
        for line in self.process.stdout:
            try:
                self.responses.put_nowait(line)
            except queue.Full:
                break
        try:
            self.responses.put_nowait(None)
        except queue.Full:
            pass

    def step(self, now_us: int, values: list[int] | None = None, hall: int = 0,
             end: int = 0, buttons: int = 0xf0) -> dict:
        self.packet_id = (self.packet_id + 1) & 255
        channel_hex = channels(self.packet_id, values).hex() if values is not None else '-'
        line = f'STEP {now_us} {hall} {end} {channel_hex} {jetibox_request(self.packet_id, buttons).hex()} {telemetry_request(self.packet_id).hex()}\n'
        self.process.stdin.write(line); self.process.stdin.flush()
        answer = self.responses.get(timeout=2)
        if answer is None:
            raise RuntimeError('Host core terminated: ' + self.process.stderr.read())
        status = json.loads(answer)
        menu = decode_reply(status['menu_reply'], self.packet_id, 0x3b)
        if len(menu) != 32 or any(byte < 32 or byte > 126 for byte in menu):
            raise ValueError('Invalid JETIBOX 32-character reply')
        ex = decode_reply(status['telemetry_reply'], self.packet_id, 0x3a)
        self.readings.update(decode_ex(ex))
        self.channel_frames += int(values is not None)
        self.menu_replies += 1; self.telemetry_replies += 1
        status['menu'] = menu.decode('ascii')
        status['menu_hex'] = menu.hex(); status['ex_hex'] = ex.hex()
        status['readings'] = dict(self.readings)
        status['channel_frames'] = self.channel_frames
        status['menu_replies'] = self.menu_replies
        status['telemetry_replies'] = self.telemetry_replies
        self.status = status
        return status

    def close(self):
        if self.process.stdin:
            self.process.stdin.close()
        try:
            code = self.process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            self.process.kill(); self.process.wait()
            raise RuntimeError('Host core did not stop')
        diagnostic = self.process.stderr.read()
        if code or diagnostic:
            raise RuntimeError(f'Host core exit={code}: {diagnostic}')


class BridgeServer:
    def __init__(self, session: CoreSession, port: int = 9876, log: EvidenceLog | None = None):
        self.session = session
        self.log = log
        self.listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.listener.bind(('127.0.0.1', port))
        self.listener.listen(1); self.listener.setblocking(False)
        self.port = self.listener.getsockname()[1]
        self.selector = selectors.DefaultSelector()
        self.selector.register(self.listener, selectors.EVENT_READ)
        self.client = None
        self.incoming = bytearray(); self.outgoing = bytearray()
        self.last_sequence = -1
        self.fresh_sample = None
        self.end = 0; self.hall = 0; self.buttons = 0xf0
        self.stop_event = threading.Event()
        self.status = {}; self.error = None
        self.last_logged_sample = None; self.last_logged_status = None

    def _disconnect(self):
        if self.client is not None:
            if self.log:
                self.log.record('disconnect', now_ms=self.status.get('now_ms', 0))
            self.selector.unregister(self.client); self.client.close(); self.client = None
        self.incoming.clear(); self.outgoing.clear()
        self.last_sequence = -1; self.fresh_sample = None; self.buttons = 0xf0
        self.last_logged_sample = None

    def _line(self, line: bytes):
        words = line.decode('ascii').split()
        if len(words) == 2 and words[0] == 'H':
            hall = int(words[1]);
            if not self.hall <= hall <= 0xffffffff:
                raise ValueError('Hall total must be nondecreasing uint32')
            self.hall = hall
            return
        if len(words) != 8 or words[0] != 'S':
            raise ValueError('Expected bounded S sample or H hall line')
        sequence = int(words[1]); values = [int(value) for value in words[2:6]]
        end = int(words[6]); buttons = int(words[7], 16)
        if not (0 <= sequence <= 0x7fffffff and all(900 <= value <= 2100 for value in values)
                and end in (0, 1) and 0 <= buttons <= 255 and buttons & 15 == 0):
            raise ValueError('Implausible sample')
        # A replayed/stalled Lua sample cannot refresh RC, change the end stop,
        # or replay a menu edit. Invalid/out-of-order messages close the client.
        if sequence < self.last_sequence:
            raise ValueError('Sample sequence moved backwards')
        if sequence == self.last_sequence:
            return
        self.last_sequence = sequence
        self.fresh_sample = values; self.end = end; self.buttons = buttons
        sample = (tuple(values), end, buttons)
        if self.log and sample != self.last_logged_sample:
            self.log.record('sample_changed', now_ms=self.status.get('now_ms', 0),
                            sequence=sequence, channels_us=values, end_switch=end,
                            buttons_hex=f'{buttons:02x}')
            self.last_logged_sample = sample

    @staticmethod
    def _reply(status: dict) -> bytes:
        values = ','.join(f'{identifier}:{raw}:{decimal}' for identifier, (raw, decimal)
                          in sorted(status['readings'].items())) or '-'
        return (f"R {status['now_ms']} {status['state']} {status['esc_us']} {status['reason']} "
                f"{status['rc_fresh']} {status['arm']} {status['end_latched']} {status['remaining_pulses']} "
                f"{status['hall']} {status['bus_errors']} {status['menu_hex']} {status['ex_hex']} {values}\n").encode('ascii')

    def run(self):
        started = time.monotonic(); next_tick = started
        try:
            while not self.stop_event.is_set():
                now = time.monotonic()
                for key, mask in self.selector.select(max(0, min(TICK_SECONDS, next_tick - now))):
                    if key.fileobj is self.listener:
                        candidate, _ = self.listener.accept(); candidate.setblocking(False)
                        if self.client is None:
                            self.client = candidate; self.selector.register(candidate, selectors.EVENT_READ)
                            if self.log:
                                self.log.record('connect', now_ms=self.status.get('now_ms', 0))
                        else:
                            candidate.close()
                    else:
                        try:
                            data = self.client.recv(1024)
                            if not data:
                                self._disconnect(); continue
                            self.incoming.extend(data)
                            if len(self.incoming) > MAX_LINE * 32:
                                raise ValueError('Client input backlog exceeded bound')
                            count = 0
                            while b'\n' in self.incoming and count < 32:
                                line, _, rest = self.incoming.partition(b'\n')
                                self.incoming = bytearray(rest); count += 1
                                if len(line) > MAX_LINE:
                                    raise ValueError('Client line exceeded bound')
                                self._line(line)
                            if len(self.incoming) > MAX_LINE:
                                raise ValueError('Incomplete client line exceeded bound')
                        except (OSError, UnicodeError, ValueError):
                            self._disconnect()
                now = time.monotonic()
                if now >= next_tick:
                    target_us = int((now - started) * 1000000)
                    # Packet turnaround is modeled at 250 kBaud; it must not
                    # consume the next scheduled 10-ms host polling point.
                    target_us = max(target_us, self.status.get('now_us', 0))
                    self.status = self.session.step(target_us, self.fresh_sample, self.hall, self.end, self.buttons)
                    fields = {name: self.status[name] for name in
                              ('state', 'reason', 'esc_us', 'rc_fresh', 'arm', 'end_latched')}
                    if self.log and fields != self.last_logged_status:
                        self.log.record('core_changed', now_ms=self.status['now_ms'], **fields,
                                        rc_sequence=self.status['rc_sequence'],
                                        channel_frames=self.status['channel_frames'],
                                        crc_valid_menu_replies=self.status['menu_replies'],
                                        crc_valid_telemetry_replies=self.status['telemetry_replies'])
                        self.last_logged_status = fields
                    self.fresh_sample = None; self.buttons = 0xf0
                    next_tick = now + TICK_SECONDS
                    if self.client is not None:
                        # Queue at most one partial previous record + latest
                        # status; slow Lua never blocks the core's own clock.
                        if not self.outgoing:
                            self.outgoing.extend(self._reply(self.status))
                        try:
                            sent = self.client.send(self.outgoing)
                            del self.outgoing[:sent]
                        except BlockingIOError:
                            pass
                        except OSError:
                            self._disconnect()
        except Exception as error:
            self.error = error
        finally:
            self._disconnect(); self.selector.close(); self.listener.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', type=int, default=9876)
    parser.add_argument('--compiler')
    parser.add_argument('--no-sanitize', action='store_true')
    parser.add_argument('--log', type=Path, help='Optional bounded JSONL evidence file; host-only simulated inputs')
    args = parser.parse_args()
    helper, environment = build_helper(args.compiler, not args.no_sanitize)
    evidence = EvidenceLog(args.log) if args.log else None
    session = CoreSession(helper, environment)
    try:
        server = BridgeServer(session, args.port, evidence)
        print(f'Host-only JETI bridge listening on 127.0.0.1:{server.port}; Ctrl+C stops; no physical motor path', flush=True)
        server.run()
    except KeyboardInterrupt:
        pass
    finally:
        session.close()
        if evidence:
            evidence.close()
            if evidence.error:
                print('Evidence log write failed: ' + evidence.error, flush=True)
    if server.error:
        raise server.error


if __name__ == '__main__':
    main()
