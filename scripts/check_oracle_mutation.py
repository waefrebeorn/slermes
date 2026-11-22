#!/usr/bin/env python3
"""Pass -> fail -> pass mutation smoke for a fixture-driven oracle.

A green gate proves nothing if the oracle cannot see a wrong answer. This runs
the oracle on the untouched fixture, then on one mutated expected output, then
again on the untouched fixture. All three outcomes must be as expected.

Usage: check_oracle_mutation.py <oracle.py> <fixtures.jsonl>
"""
import json
import os
import subprocess
import sys

if len(sys.argv) != 3:
    print("usage: check_oracle_mutation.py <oracle.py> <fixtures.jsonl>")
    sys.exit(2)
oracle, fixture = sys.argv[1:3]
py = sys.executable
cache = ".testbin/pytestbin.cache"
if os.path.isfile(cache):
    cand = open(cache).read().strip()
    if cand and os.path.isfile(cand):
        py = cand

rows = [l for l in open(fixture, encoding="utf-8") if l.strip()]
if not rows:
    print("empty fixture")
    sys.exit(2)
# The oracle consumes the C harness's emitted line (which contains `out`),
# not the bare input fixture. Produce that line exactly as the gate does.
gname = os.path.basename(oracle)[7:-3]  # oracle_<name>.py
harness = os.path.join(".testbin", "harnesses", gname + ".exe")
if os.path.isfile(harness):
    import tempfile
    with tempfile.TemporaryDirectory() as td:
        env = dict(os.environ, SLERMES_HOME=td, HOME=td)
        pr = subprocess.run([harness], input="\n".join(rows) + "\n",
                            capture_output=True, text=True, encoding="utf-8",
                            errors="replace", env=env)
        if pr.returncode == 0 and pr.stdout.strip():
            rows = [l for l in pr.stdout.splitlines() if l.strip()]

def run(lines):
    env = dict(os.environ, PYTHONIOENCODING="utf-8")
    p = subprocess.run([py, oracle], input="\n".join(lines) + "\n",
                       capture_output=True, text=True, encoding="utf-8",
                       errors="replace", env=env)
    return p.returncode, p.stdout + p.stderr

def mutate(obj):
    if isinstance(obj, dict):
        for k, v in obj.items():
            if k == "out":
                obj[k] = mutate(v)
                return obj
            if isinstance(v, (dict, list)):
                r = mutate(v)
                if r is not None:
                    obj[k] = r
                    return obj
    elif isinstance(obj, list):
        for i, v in enumerate(obj):
            if isinstance(v, (dict, list)):
                r = mutate(v)
                if r is not None:
                    obj[i] = r
                    return obj
    return None

def flip(v):
    if isinstance(v, bool): return not v
    if isinstance(v, str): return v + "__MUTANT__"
    if isinstance(v, (int, float)) and not isinstance(v, bool): return v + 1
    return "__MUTANT__"

def flip_first(obj):
    if isinstance(obj, dict):
        for k, v in obj.items():
            if not isinstance(v, (dict, list)):
                obj[k] = flip(v); return True
            if flip_first(v): return True
    elif isinstance(obj, list):
        for i, v in enumerate(obj):
            if not isinstance(v, (dict, list)):
                obj[i] = flip(v); return True
            if flip_first(v): return True
    return False

base = run(rows)
rc0, out0 = base
if rc0 != 0:
    print(f"PASS step failed: rc={rc0}\n{out0[-800:]}")
    sys.exit(1)

m = json.loads(rows[0])
target = mutate(m)
if target is None:
    flip_first(m)
    target = m
else:
    flip_first(target["out"])
mutated = [json.dumps(m, ensure_ascii=False)] + rows[1:]
rc1, out1 = run(mutated)
if rc1 == 0 or "mismatches" not in out1:
    print(f"FAIL step did not detect the mutation: rc={rc1}\n{out1[-800:]}")
    sys.exit(1)

rc2, out2 = run(rows)
if rc2 != 0:
    print(f"RESTORE step failed: rc={rc2}\n{out2[-800:]}")
    sys.exit(1)
print(f"MUTATION SMOKE {oracle}: pass -> fail -> pass OK")
