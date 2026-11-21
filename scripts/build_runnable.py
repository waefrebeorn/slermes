#!/usr/bin/env python3
"""Build a *runnable* Slermes image on Windows.

The normal `make slermes` links, but Smart App Control refuses freshly-linked
images containing the -O3 hash/regex/auxiliary-runtime objects (exit 127).
This script recompiles those three units at -O0 (the SAC-accepted shape),
replaces them in the link closure, links, and proves the result executes.

Usage: python3 scripts/build_runnable.py
Output: slermes_O0.exe  (verified with `--version`)
"""
import os
import re
import shlex
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASH = r'C:\msys64\usr\bin\bash.exe'
ENV = ('export PATH=/c/winlibs/mingw64/bin:$PATH; '
       'source /c/Users/eman5/slermes/build/winlibs-env.sh; '
       'cd /c/Users/eman5/slermes; ')

REPLACE = {
    'lib/libhash/hash.o': '.testbin/hash_O0.o',
    'lib/libregex/hermes_regex.o': '.testbin/hermes_regex_O0.o',
    'src/agent/auxiliary_client_runtime.o': '.testbin/auxiliary_client_runtime_O0.o',
}


def bash(cmd: str, timeout: int = 540, capture: bool = True):
    return subprocess.run([BASH, '-lc', ENV + cmd], cwd=ROOT, timeout=timeout,
                          capture_output=capture, text=True)


def ensure_o0(src: str, dst: str) -> None:
    p = ROOT / dst
    if p.exists():
        return
    out = bash(f'make -n -B {src} 2>/dev/null | grep -E "gcc.exe .* -c -o" | tail -1')
    cmd = out.stdout.strip()
    if not cmd:
        raise SystemExit(f'no compile command for {src}')
    cmd = cmd.removeprefix('ccache ').replace('-O3', '-O0')
    cmd = cmd.replace(f'-o {src}', f'-o {dst}')
    r = bash(cmd)
    if r.returncode or not p.exists():
        raise SystemExit(f'failed to build {dst}: {(r.stderr or "")[-400:]}')
    print(f'  built {dst}')


def link_command() -> str:
    out = bash('make -n -B slermes 2>/dev/null | grep -E "gcc.exe .* @slermes.link.rsp" | tail -1')
    lines = out.stdout.strip().splitlines()
    if not lines:
        raise SystemExit('no link command in make -n slermes')
    cmd = lines[0]
    # re-attach make's continuation lines (they end with a backslash)
    nxt = out.stdout.split(cmd, 1)[1].splitlines() if cmd in out.stdout else []
    for line in nxt:
        if not cmd.rstrip().endswith('\\'):
            break
        cmd = cmd.rstrip()[:-1] + ' ' + line.strip()
    cmd = cmd.replace(' -o slermes ', ' -o slermes_O0 ')
    cmd = cmd.replace('@slermes.link.rsp', '@slermes_O0.link.rsp')
    return cmd


def main() -> int:
    print('== ensure -O0 SAC-accepted objects ==')
    for src, dst in REPLACE.items():
        ensure_o0(src, dst)

    print('== build deduplicated object closure ==')
    out = bash('make -s print-objs 2>/dev/null')
    files = out.stdout.split()
    if len(files) < 100:
        raise SystemExit(f'bad object closure: {len(files)} objects')
    seen, closure = set(), []
    for f in files:
        f = REPLACE.get(f, f)
        if f not in seen:
            seen.add(f)
            closure.append(f)
    (ROOT / 'slermes_O0.link.rsp').write_text('\n'.join(closure) + '\n', encoding='utf-8')
    print(f'  {len(files)} raw -> {len(closure)} unique objects, {len(REPLACE)} substitutions')

    print('== link ==')
    r = bash(link_command())
    if r.returncode:
        print((r.stdout + r.stderr)[-2000:])
        return r.returncode
    exe = ROOT / 'slermes_O0.exe'
    if not exe.exists():
        raise SystemExit('link produced no executable')
    print(f'  {exe.name}: {exe.stat().st_size:,} bytes')

    print('== smoke test (DLL path included) ==')
    env = os.environ.copy()
    env['PATH'] = r'C:\msys64\mingw64\bin;C:\winlibs\mingw64\bin;' + env.get('PATH', '')
    run = subprocess.run([str(exe), '--version'], cwd=ROOT, env=env,
                         capture_output=True, text=True, timeout=60)
    print((run.stdout + run.stderr).strip()[:400])
    if run.returncode or 'Slermes' not in run.stdout:
        print(f'SMOKE FAILED rc={run.returncode}')
        return 1
    print('RUNNABLE BINARY VERIFIED')
    return 0


if __name__ == '__main__':
    sys.exit(main())
