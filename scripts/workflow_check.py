#!/usr/bin/env python3
"""Pre-dispatch / pre-commit workflow guard (make workflow-check).

Runs the reality scoreboard, the dead-credit guard, and build health in that
order. On Windows the ambient PATH can carry WindowsApps git-bash stubs whose
sh/head/grep deny execution, so prefer the real MSYS2 tools before spawning.
"""
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if Path(r'C:\msys64\usr\bin\sh.exe').exists():
    os.environ['PATH'] = r'C:\msys64\usr\bin;C:\winlibs\mingw64\bin;' + os.environ.get('PATH', '')
    os.environ.setdefault('WINLIBS', '/c/winlibs/mingw64')
    os.environ.setdefault('CFLAGS_EXTRA', '-I C:/winlibs/extra-inc')
    os.environ.setdefault('LIBRARY_PATH', 'C:/winlibs/extra-libs')
    os.environ.setdefault('ORACLE_LIBDIR', 'C:/winlibs/extra-libs')

STEPS = [
    [sys.executable, str(ROOT / 'tests' / 'parity_reality.py')],
    [sys.executable, str(ROOT / 'tests' / 'check_dead_ports.py'),
     '--baseline', str(ROOT / 'docs' / 'dev' / 'dead-ports-baseline.json'),
     '--fail-on-wired-dead'],
    [sys.executable, str(ROOT / 'tests' / 'verify_build_health.py'), '--no-link'],
]

for step in STEPS:
    print('== ' + Path(step[1]).name, flush=True)
    rc = subprocess.run(step, cwd=ROOT).returncode
    if rc:
        sys.exit(rc)
