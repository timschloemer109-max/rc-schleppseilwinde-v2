#!/usr/bin/env python3
"""Replay independent Python master byte timelines through real Application/core."""
from __future__ import annotations
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('independent_master', ROOT / 'simulator' / 'exbus_master.py')
MASTER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MASTER)


def replay_stream(name: str, baud: int, endstop: bool) -> str:
    events = MASTER.scenario(name, baud, 0x5345494c)
    assert events == MASTER.scenario(name, baud, 0x5345494c), 'non-deterministic master'
    rows = []
    for event in events:
        for index, byte in enumerate(bytes.fromhex(event['hex'])):
            at = event['at_us'] + index * event['byte_interval_us']
            rows.append((at, 1, f'B {at} {byte}\n'))
    # Sensor values are independently injectable, with a physical stop after the
    # optional end-switch edge. High current is observation-only in safe defaults.
    hall = 0
    for at in range(0, 8_050_001, 50_000):
        if 4_100_000 <= at <= (4_300_000 if endstop else 6_000_000):
            hall += 1
        active = int(endstop and at >= 4_350_000)
        rows.append((at, 0, f'I {at} {hall} {active} 10000 1\n'))
    rows.append((8_050_000, 2, 'T 8050000\n'))
    rows.sort(key=lambda row: (row[0], row[1]))
    return ''.join(row[2] for row in rows)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler')
    parser.add_argument('--sanitize', action='store_true', default=True)
    parser.add_argument('--no-sanitize', dest='sanitize', action='store_false')
    args = parser.parse_args()
    compiler = args.compiler or os.environ.get('CXX') or shutil.which('clang++') or shutil.which('g++')
    if not compiler:
        candidates = sorted((ROOT / '.build' / 'tools').glob('llvm-mingw-*/bin/clang++.exe'))
        compiler = str(candidates[-1]) if candidates else None
    if not compiler:
        raise SystemExit('C++ compiler missing for real EX-Bus simulator replay')
    build = ROOT / '.build' / ('simulator-sanitized' if args.sanitize else 'simulator')
    build.mkdir(parents=True, exist_ok=True)
    executable = build / ('exbus_replay.exe' if os.name == 'nt' else 'exbus_replay')
    flags = ['-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Wconversion',
             '-Wsign-conversion', '-Wshadow', '-Werror', '-O1', '-g', '-I' + str(ROOT / 'core')]
    if args.sanitize:
        flags += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
    subprocess.run([compiler, *flags, *sorted(str(p) for p in (ROOT / 'core').glob('*.cpp')),
                    str(ROOT / 'simulator' / 'replay.cpp'), '-o', str(executable)], check=True)
    environment = os.environ.copy()
    environment['PATH'] = str(Path(compiler).resolve().parent) + os.pathsep + environment.get('PATH', '')
    environment['ASAN_OPTIONS'] = ('detect_leaks=0' if os.name == 'nt' else 'detect_leaks=1') + ':halt_on_error=1'
    environment['UBSAN_OPTIONS'] = 'halt_on_error=1:print_stacktrace=1'
    results = []
    for baud in [125000, 250000]:
        for name in ['normal', 'loss', 'noise', 'overload']:
            for endstop in ([False, True] if name != 'loss' else [False]):
                command = [str(executable)]
                if name == 'loss': command.append('--expect-loss')
                if endstop: command.append('--expect-endstop')
                run = subprocess.run(command, input=replay_stream(name, baud, endstop), env=environment,
                                     text=True, capture_output=True, check=False)
                if run.returncode:
                    raise AssertionError(f'{name} baud={baud} endstop={endstop}: {run.stdout}\n{run.stderr}')
                result = json.loads(run.stdout)
                assert result['passed'] and 4_000 <= result['started_ms'] < 4_200
                assert result['responses'] > 100 and result['logger_drops'] > 0
                assert not result['restart_after_loss']
                if endstop:
                    assert result['endstop_ms'] == 4350 and result['endstop_command_latency_ms'] == 0
                    assert result['final_pwm_us'] == 1000
                if name == 'loss':
                    assert 4_600 <= result['rc_loss_ms'] < 4_750 and result['final_pwm_us'] == 1000
                if name == 'noise': assert result['bus_errors'] > 0
                result.update({'scenario': name, 'baud': baud, 'endstop': endstop})
                results.append(result)
                print(f"PASS master replay: {name}, {baud} baud, endstop={endstop}, {result['bytes']} bytes, "
                      f"RUN at {result['started_ms']} ms, loss at {result['rc_loss_ms']} ms, stop latency {result['endstop_command_latency_ms']} ms")
    (build / 'results.json').write_text(json.dumps(results, indent=2) + '\n', encoding='utf-8')
    print(f'PASS: {len(results)} real Application/core replay cases, both baud rates, deterministic master; sanitizer={args.sanitize}')


if __name__ == '__main__':
    main()
