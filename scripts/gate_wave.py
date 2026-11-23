#!/usr/bin/env python3
"""Wire and verify a wave of differential gates.

Usage:
  python scripts/gate_wave.py wire  <map.json>   # DEPS + objects.mk (idempotent)
  python scripts/gate_wave.py plan  <map.json>   # print bash gate loop
  python scripts/gate_wave.py tally <out-dir>    # sum ORACLE case counts

map.json is {"gate_name": "python/module/path", ...}. Files expected:
  include/noc_<gate>.h
  src/<module>.c
  tests/t_port_<gate>.c
  tests/oracle_<gate>.py
  tests/fixtures/<gate>.jsonl
"""
import json, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEPS = os.path.join(ROOT, 'tests', 'run_all_oracles.sh')
OBJS = os.path.join(ROOT, 'build', 'objects.mk')


def load_map(path):
    d = json.load(open(path, encoding='utf-8'))
    assert isinstance(d, dict) and d
    return d


def check(m):
    missing = []
    for g, mod in m.items():
        for rel in ('include/noc_%s.h' % g, 'src/%s.c' % mod,
                    'tests/t_port_%s.c' % g, 'tests/oracle_%s.py' % g,
                    'tests/fixtures/%s.jsonl' % g):
            if not os.path.isfile(os.path.join(ROOT, rel)):
                missing.append(rel)
    return missing


def wire(m):
    miss = check(m)
    if miss:
        print('MISSING FILES:', *miss, sep='\n  ')
        return 1
    # DEPS: append after the last known DEPS entry (before the closing `)`).
    s = open(DEPS, encoding='utf-8', newline='').read()
    nl = '\r\n' if '\r\n' in s else '\n'
    anchor = '  [cronconst]="src/cron/constants.o lib/libjson/json.o"'
    if anchor not in s:
        # generic fallback: insert just before the DEPS closing paren
        anchor = None
    new = []
    for g, mod in m.items():
        line = '  [%s]="src/%s.o lib/libjson/json.o"' % (g, mod)
        if line not in s:
            new.append(line)
    if anchor and new:
        s = s.replace(anchor, anchor + nl + nl.join(new), 1)
    elif new:
        idx = s.find('\n)')
        s = s[:idx] + '\n' + '\n'.join(new) + s[idx:]
    open(DEPS, 'w', encoding='utf-8', newline='').write(s)

    # objects.mk: append to CUPBOARD_PORT_WIRED after the known anchor.
    s = open(OBJS, encoding='utf-8', newline='').read()
    anchor = 'src/tools/tool_output_truncate.o '
    add = []
    for mod in m.values():
        entry = 'src/%s.o ' % mod
        if entry not in s:
            add.append(entry)
    if add:
        assert anchor in s, 'objects.mk anchor missing'
        s = s.replace(anchor, anchor + ''.join(add), 1)
    open(OBJS, 'w', encoding='utf-8', newline='').write(s)
    print('wired %d gates, %d objects' % (len(new), len(add)))
    return 0


def plan(m):
    arr = ' '.join('D[%s]="src/%s.o lib/libjson/json.o"' % (g, mod) for g, mod in m.items())
    print('declare -A D; %s; for g in %s; do bash tests/run_one_oracle.sh $g oracle_$g.py "" "${D[$g]}" > .testbin/w_$g.out 2>&1; echo "$g rc=$? $(grep -a ORACLE .testbin/w_$g.out | tail -1)"; done'
          % (arr, ' '.join(m)))


def tally(d):
    total = mism = 0
    files = 0
    for fn in os.listdir(d):
        if not fn.endswith('.out'):
            continue
        s = open(os.path.join(d, fn), encoding='utf-8', errors='replace').read()
        hits = re.findall(r'ORACLE \S+: (\d+) cases, (\d+) mismatches', s)
        if hits:
            c, x = hits[-1]
            total += int(c); mism += int(x); files += 1
    print('gates=%d cases=%d mismatches=%d' % (files, total, mism))


if __name__ == '__main__':
    cmd = sys.argv[1]
    if cmd == 'tally':
        sys.exit(tally(sys.argv[2]))
    m = load_map(sys.argv[2])
    sys.exit(plan(m) if cmd == 'plan' else wire(m))
