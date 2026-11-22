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
    if cand.startswith("/c/"):
        cand = "C:/" + cand[3:]
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

OUT_KEYS = ("trace", "out", "ret", "result", "value", "py", "c", "ok", "err")

def flip(v):
    if isinstance(v, bool): return not v
    if isinstance(v, str): return v + "__MUTANT__"
    if isinstance(v, (int, float)) and not isinstance(v, bool): return v + 1
    return "__MUTANT__"

def flip_first(obj):
    if isinstance(obj, dict):
        for k, v in obj.items():
            if k == "func": continue
            if not isinstance(v, (dict, list)):
                obj[k] = flip(v); return True
            if flip_first(v): return True
    elif isinstance(obj, list):
        for i, v in enumerate(obj):
            if not isinstance(v, (dict, list)):
                obj[i] = flip(v); return True
            if flip_first(v): return True
    return False

def flip_output(obj):
    """Change one scalar inside the first output field, preserving container type."""
    if isinstance(obj, dict):
        for k in OUT_KEYS:
            if k in obj:
                v = obj[k]
                if not isinstance(v, (dict, list)):
                    obj[k] = flip(v); return True
                if flip_output(v): return True
                if flip_first(v): return True
                # Empty/uniform container: append a scalar rather than replacing
                # the whole output, so oracles that call .get() still work.
                if isinstance(v, dict):
                    v["__MUTANT__"] = 1; return True
                v.append("__MUTANT__"); return True
        for v in obj.values():
            if isinstance(v, (dict, list)) and flip_output(v): return True
    elif isinstance(obj, list):
        for v in obj:
            if isinstance(v, (dict, list)) and flip_output(v): return True
    return False

base = run(rows)
rc0, out0 = base
if rc0 != 0:
    print(f"PASS step failed: rc={rc0}\n{out0[-800:]}")
    sys.exit(1)

m = json.loads(rows[0])
if not flip_output(m):
    print("no output field to mutate")
    sys.exit(2)
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
