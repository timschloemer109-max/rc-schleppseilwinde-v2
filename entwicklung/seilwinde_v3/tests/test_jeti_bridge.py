#!/usr/bin/env python3
"""Independent host/TCP client checks against real Application, no motor path."""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import socket
import sys
import threading
import time
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'simulator'))
from jeti_bridge import BridgeServer, CoreSession, EvidenceLog, build_helper, decode_ex, decode_reply


def deterministic(helper, environment):
    core = CoreSession(helper, environment)
    try:
        # RUN on boot cannot start. Fresh neutral + new RUN edge can.
        for ms in range(0, 3300, 10):
            out = core.step(ms * 1000, [1500, 2000, 1000, 2000])
            assert out['esc_us'] == 1000 and out['state'] == 'idle'
        for ms in range(3300, 3650, 10):
            out = core.step(ms * 1000, [1500, 1500, 1000, 2000])
        for ms in range(3650, 3780, 10):
            out = core.step(ms * 1000, [1500, 2000, 1000, 2000])
        assert out['state'] == 'slow' and out['esc_us'] > 1000
        for ms in range(3780, 3900, 10):
            out = core.step(ms * 1000, [1500, 1500, 1000, 2000])
        assert out['state'] == 'slow' and out['esc_us'] > 1000, 'Autonomous RUN on neutral'
        out = core.step(3900000, [1500, 1500, 1000, 2000], end=1)
        assert out['esc_us'] == 1000 and out['reason'] == 'end_switch' and out['end_latched']
        out = core.step(3910000, [1500, 1500, 1000, 2000], end=1, buttons=0xe0)
        assert out['menu'][:16] == 'Rope metres     ', 'Actual RIGHT key must navigate real menu'
        # Independent decoder checked all real responses; numeric carousel
        # includes command PWM, Hall and explicit RC validity.
        assert set(out['readings']) == set(range(1, 16))
        assert out['bus_errors'] == 0
        print('PASS JETI host core: boot RUN inhibited, neutral/new edge, autonomous RUN, same-step end stop; 15 telemetry IDs, real menu navigation/CRC')
    finally:
        core.close()


def tcp_checks(helper, environment):
    core = CoreSession(helper, environment)
    log_dir = tempfile.TemporaryDirectory()
    log_path = Path(log_dir.name) / 'evidence.jsonl'
    evidence = EvidenceLog(log_path)
    server = BridgeServer(core, port=0, log=evidence)
    thread = threading.Thread(target=server.run)
    thread.start()
    client = socket.create_connection(('127.0.0.1', server.port), timeout=2)
    client.settimeout(2)
    sequence = 0
    def send_for(seconds, trigger, duplicate=False):
        nonlocal sequence
        until = time.monotonic() + seconds
        buffer = b''; rows = []
        while time.monotonic() < until:
            if not duplicate:
                sequence += 1
            client.sendall(f'S {sequence} 1500 {trigger} 1000 2000 0 F0\n'.encode('ascii'))
            time.sleep(0.02)
            buffer += client.recv(4096)
            while b'\n' in buffer:
                line, _, buffer = buffer.partition(b'\n'); words = line.decode('ascii').split()
                assert len(words) == 14 and words[0] == 'R'
                assert len(bytes.fromhex(words[11])) == 32
                decode_ex(bytes.fromhex(words[12]))
                rows.append(words)
        return rows
    try:
        send_for(3.4, 1500)
        rows = send_for(0.15, 2000)
        assert any(int(row[3]) > 1000 and row[2] == 'slow' for row in rows)
        rows = send_for(0.15, 1500)
        assert all(int(row[3]) > 1000 for row in rows)
        before = server.status['rc_sequence']
        rows = send_for(0.25, 1500, duplicate=True)
        assert server.status['rc_sequence'] == before
        assert rows[-1][2:6] == ['fault', '1000', 'rc_lost', '0'], 'Duplicate samples must expire while TCP connected'
        client.close(); client = None
        tick_before = server.status['now_ms']; time.sleep(0.15)
        assert server.status['now_ms'] > tick_before + 90, 'Core must tick without Lua client'
        client = socket.create_connection(('127.0.0.1', server.port), timeout=2); client.settimeout(2)
        sequence = 0
        rows = send_for(0.25, 2000)
        assert all(int(row[3]) == 1000 for row in rows)
        assert rows[-1][5] == '1', 'Fresh reconnect samples accepted without restarting motor'
        # A malformed client is isolated; it cannot stop the host clock.
        client.sendall(b'S ' + b'9' * 200 + b'\n'); time.sleep(0.15)
        assert server.client is None
        assert server.status['bus_errors'] == 0
        print('PASS JETI TCP bridge: real loopback samples/replies, 3-s arming, duplicate-sequence RC loss, disconnected clock, reconnect held RUN blocked, bounded invalid input')
    finally:
        if client is not None:
            client.close()
        server.stop_event.set(); thread.join(timeout=3)
        assert not thread.is_alive()
        core.close()
        evidence.close()
    rows = [json.loads(line) for line in log_path.read_text(encoding='utf-8').splitlines()]
    assert rows[0]['event'] == 'session_start' and rows[-1]['event'] == 'session_end'
    assert len([row for row in rows if row['event'] == 'connect']) == 2
    assert len([row for row in rows if row['event'] == 'disconnect']) == 2
    samples = [row for row in rows if row['event'] == 'sample_changed']
    assert len(samples) == 4, 'Same-value fresh samples must not flood evidence'
    changes = [row for row in rows if row['event'] == 'core_changed']
    assert any(row['esc_us'] > 1000 for row in changes)
    assert any(row['reason'] == 'rc_lost' and row['esc_us'] == 1000 for row in changes)
    assert max(row['crc_valid_menu_replies'] for row in changes) > 100
    assert evidence.error is None and evidence.dropped == 0
    assert len(rows) < 100, 'Only changes/lifecycle should be logged'
    log_dir.cleanup()
    print('PASS JETI evidence: bounded asynchronous JSONL, lifecycle/sample/core changes, actual accepted RC sequence and validated reply counts')
    if server.error:
        raise server.error


def decoder_rejects_corruption():
    # The master CRC implementation is independent from the C++ DUT. Do not
    # permit incorrect replies to appear as a successful virtual connection.
    try:
        decode_reply('3b0128013b20' + '20' * 32 + '0000', 1, 0x3b)
    except ValueError:
        pass
    else:
        raise AssertionError('Corrupted EX Bus CRC was accepted')
    try:
        decode_ex(bytes.fromhex('9f0600a400030000'))
    except ValueError:
        pass
    else:
        raise AssertionError('Corrupted EX payload was accepted')
    print('PASS JETI bridge independent decoder: invalid EX Bus CRC16 / EX CRC8 rejected')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler')
    parser.add_argument('--sanitize', action='store_true', default=True)
    parser.add_argument('--no-sanitize', dest='sanitize', action='store_false')
    args = parser.parse_args()
    helper, environment = build_helper(args.compiler, args.sanitize, 'jeti_live_core_test')
    deterministic(helper, environment)
    tcp_checks(helper, environment)
    decoder_rejects_corruption()
    print(f'PASS JETI bridge focused host/TCP checks; strict C++ warnings; ASAN/UBSAN={args.sanitize}')


if __name__ == '__main__':
    main()
