#!/usr/bin/env python3
"""Reproducible local/CI native tests and Nano R4 staging/build. No flashing."""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(os.path.abspath(__file__)).parent  # Preserve optional Windows SUBST alias.

def run(command: list[str], **kwargs) -> None:
    print('+ ' + subprocess.list2cmdline(command), flush=True)
    subprocess.run(command, check=True, **kwargs)

def native(args) -> None:
    compiler = args.compiler or shutil.which('clang++') or shutil.which('g++')
    if not compiler:
        raise SystemExit('C++ compiler missing: pass --compiler /path/to/clang++ or g++')
    dest = ROOT / '.build' / ('native-sanitized' if args.sanitize else 'native')
    dest.mkdir(parents=True, exist_ok=True)
    run([compiler, '--version'])
    common = ['-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
              '-Wconversion', '-Wsign-conversion', '-Wshadow', '-g', '-O1',
              '-I' + str(ROOT / 'core')]
    if args.sanitize:
        common += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
    sources = sorted(str(p) for p in (ROOT / 'core').glob('*.cpp'))
    tests = sorted((ROOT / 'tests').glob('test_*.cpp'))
    if not tests:
        raise SystemExit('No native test suites found')
    for test in tests:
        executable = dest / (test.stem + ('.exe' if os.name == 'nt' else ''))
        run([compiler, *common, *sources, str(test), '-o', str(executable)])
        environment = os.environ.copy()
        environment['ASAN_OPTIONS'] = ('detect_leaks=0' if os.name == 'nt' else 'detect_leaks=1') + ':halt_on_error=1'
        environment['UBSAN_OPTIONS'] = 'halt_on_error=1:print_stacktrace=1'
        run([str(executable)], env=environment)
    run([sys.executable, str(ROOT / 'simulator' / 'exbus_master.py'), '--self-test'])
    for test in sorted((ROOT / 'tests').glob('test_*.py')):
        extra = ['--sanitize'] if args.sanitize and test.name in {'test_r4_adapters.py', 'test_simulator.py'} else []
        run([sys.executable, str(test), *extra])
    print(f'PASS: {len(tests)} native suites and independent EX-Bus fixtures')

def stage() -> Path:
    target = ROOT / '.build' / 'sketch' / 'Schleppseilwinde_V3_0'
    # Copy into a fresh, verified build directory. Never delete the source tree.
    resolved = target.resolve()
    if ROOT.resolve() not in resolved.parents or '.build' not in resolved.parts:
        raise SystemExit('Unexpected staging path')
    if target.exists():
        shutil.rmtree(target)
    shutil.copytree(ROOT / 'firmware' / 'Schleppseilwinde_V3_0', target)
    shutil.copytree(ROOT / 'core', target / 'src' / 'winch')
    return target

def arduino(args) -> None:
    lock = json.loads((ROOT / 'toolchain.json').read_text(encoding='utf-8'))
    cli = args.arduino_cli or shutil.which('arduino-cli')
    if not cli:
        raise SystemExit('Arduino CLI missing: use --arduino-cli')
    prefix = [cli]
    if args.config_file:
        prefix += ['--config-file', str(Path(args.config_file).resolve())]
    version = subprocess.check_output([*prefix, 'version', '--format', 'json'], text=True)
    if json.loads(version)['VersionString'] != lock['arduino_cli']:
        raise SystemExit(f"Arduino CLI must be {lock['arduino_cli']}: {version}")
    if args.install:
        run([*prefix, 'core', 'update-index'])
        run([*prefix, 'core', 'install', lock['core'] + '@' + lock['core_version']])
    boards = json.loads(subprocess.check_output([*prefix, 'core', 'list', '--format', 'json'], text=True))
    installed = boards.get('platforms', []) if isinstance(boards, dict) else boards
    if not any(p.get('id') == lock['core'] and p.get('installed_version') == lock['core_version'] for p in installed):
        raise SystemExit('Pinned board core missing; rerun with --install')
    sketch = stage()
    dest = ROOT / '.build' / ('nano-r4-' + str(args.bus_baud))
    run([*prefix, 'compile', '--fqbn', lock['fqbn'], '--warnings', 'all',
         '--build-property', 'compiler.cpp.extra_flags=-DWINCH_BUS_BAUD=' + str(args.bus_baud),
         '--build-path', str(dest), '--export-binaries', str(sketch)])

def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('target', choices=['native', 'arduino', 'stage'])
    p.add_argument('--compiler')
    p.add_argument('--sanitize', action='store_true')
    p.add_argument('--arduino-cli')
    p.add_argument('--config-file')
    p.add_argument('--install', action='store_true')
    p.add_argument('--bus-baud', type=int, choices=[125000, 250000], default=125000)
    args = p.parse_args()
    if args.target == 'native': native(args)
    elif args.target == 'arduino': arduino(args)
    else: print(stage())

if __name__ == '__main__':
    main()
