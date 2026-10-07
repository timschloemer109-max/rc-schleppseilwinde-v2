#!/usr/bin/env python3
"""Run actual UART/PWM adapter code against API spies. No hardware emulation."""
from pathlib import Path
import argparse
import os
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler')
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    compiler = args.compiler or os.environ.get('CXX') or shutil.which('clang++') or shutil.which('g++')
    if not compiler:
        candidates = sorted((ROOT / '.build' / 'tools').glob('llvm-mingw-*/bin/clang++.exe'))
        compiler = str(candidates[-1]) if candidates else None
    if not compiler:
        raise SystemExit('C++ compiler missing for actual R4 adapter tests')
    build = ROOT / '.build' / ('platform-sanitized' if args.sanitize else 'platform')
    build.mkdir(parents=True, exist_ok=True)
    firmware = ROOT / 'firmware' / 'Schleppseilwinde_V3_0'
    executable = build / ('test_r4_adapters.exe' if os.name == 'nt' else 'test_r4_adapters')
    flags = ['-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Wconversion',
             '-Wsign-conversion', '-Wshadow', '-Werror', '-O1', '-g', '-DARDUINO_NANO_R4',
             '-I' + str(ROOT / 'tests' / 'platform_fakes'), '-I' + str(firmware)]
    if args.sanitize:
        flags += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
    subprocess.run([compiler, *flags, str(firmware / 'r4_exbus_uart.cpp'),
                    str(firmware / 'r4_esc_pwm.cpp'), str(ROOT / 'tests' / 'platform' / 'test_r4_adapters.cpp'),
                    '-o', str(executable)], check=True)
    environment = os.environ.copy()
    environment['PATH'] = str(Path(compiler).resolve().parent) + os.pathsep + environment.get('PATH', '')
    environment['ASAN_OPTIONS'] = ('detect_leaks=0' if os.name == 'nt' else 'detect_leaks=1') + ':halt_on_error=1'
    environment['UBSAN_OPTIONS'] = 'halt_on_error=1:print_stacktrace=1'
    subprocess.run([str(executable)], env=environment, check=True)

if __name__ == '__main__':
    main()
