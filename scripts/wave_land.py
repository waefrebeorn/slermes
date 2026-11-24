#!/usr/bin/env python3
"""Land a wave of differential gates end to end.

One command does what used to be five manual steps, in the order that keeps
failures cheap:

    python scripts/wave_land.py plan   docs/wave19-plan.json   # inventory only
    python scripts/wave_land.py wire   docs/wave19-plan.json   # objects.mk + DEPS
    python scripts/wave_land.py build  docs/wave19-plan.json   # .o for every gate
    python scripts/wave_land.py gates  docs/wave19-plan.json   # run the gates
    python scripts/wave_land.py land   docs/wave19-plan.json   # wire+build+gates

`land` stops at the first stage that fails and prints the per-gate state, so a
half-finished wave is obvious (files missing vs object missing vs gate red).

Wiring mirrors scripts/gate_wave.py (same two files, same idempotence) -- the
theme keys go into the DEPS map in tests/run_all_oracles.sh and the objects go
into build/objects.mk, which is then re-cupboarded by organize_objects.py.
"""
from __future__ import annotations

import json
import os
import re
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEPS = os.path.join(REPO, "tests", "run_all_oracles.sh")
OBJS = os.path.join(REPO, "build", "objects.mk")
PY = "C:/Users/eman5/hermes-agent/.venv/Scripts/python.exe"

FILES = ("include/noc_%s.h", "src/%s.c", "tests/t_port_%s.c",
         "tests/oracle_%s.py", "tests/fixtures/%s.jsonl")


def load(path):
    with open(path, encoding="utf-8") as fh:
        plan = json.load(fh)
    assert isinstance(plan, dict) and plan, "plan must be {gate: module}"
    return plan


def inventory(plan):
    """(gate, module, missing files, object built?) for every gate."""
    rows = []
    for gate, mod in plan.items():
        missing = []
        for pat in FILES:
            rel = pat % (mod if pat == "src/%s.c" else gate)
            if not os.path.isfile(os.path.join(REPO, rel)):
                missing.append(rel)
        obj = os.path.join(REPO, "src", mod + ".o")
        rows.append({"gate": gate, "module": mod, "missing": missing,
                     "built": os.path.isfile(obj)})
    return rows


def report(plan):
    rows = inventory(plan)
    bad = [r for r in rows if r["missing"]]
    unbuilt = [r for r in rows if not r["missing"] and not r["built"]]
    print("wave: %d gates, %d complete, %d incomplete, %d unbuilt"
          % (len(rows), len(rows) - len(bad) - len(unbuilt), len(bad), len(unbuilt)))
    for r in bad[:10]:
        print("  missing %s: %s" % (r["gate"], ", ".join(r["missing"][:3])))
    for r in unbuilt[:10]:
        print("  unbuilt %s (%s)" % (r["gate"], r["module"]))
    return rows


def wire(plan):
    """Add DEPS entries + objects, then regroup. Idempotent."""
    ready = [r for r in inventory(plan) if not r["missing"]]
    if not ready:
        print("wire: nothing complete yet")
        return 1
    text = open(OBJS, encoding="utf-8", newline="").read()
    lines = text.splitlines(keepends=True)
    new_objs = [r["module"] + ".o" for r in ready]
    have = set()
    for line in lines:
        if line.startswith("CUPBOARD_PORT_WIRED ="):
            have = set(line.split())
            break
    add = ["src/" + o for o in new_objs if "src/" + o not in have]
    if add:
        for i, line in enumerate(lines):
            if line.startswith("CUPBOARD_PORT_WIRED ="):
                core = line.rstrip("\r\n")
                lines[i] = core + " " + " ".join(add) + ("\r\n" if line.endswith("\r\n") else "\n")
                break
        open(OBJS, "w", encoding="utf-8", newline="").write("".join(lines))

    etext = open(DEPS, encoding="utf-8", newline="").read()
    nl = "\r\n" if "\r\n" in etext else "\n"
    elines = etext.splitlines()
    new = ['  [%s]="src/%s.o lib/libjson/json.o"' % (r["gate"], r["module"])
           for r in ready if "[%s]=" % r["gate"] not in etext]
    if new:
        anchor = None
        for i, line in enumerate(elines):
            if re.match(r"\s*\[w\d+_[A-Za-z0-9_]+\]=", line):
                anchor = i
        if anchor is None:
            raise SystemExit("no existing wave DEPS block to anchor on")
        elines[anchor + 1:anchor + 1] = new
        open(DEPS, "w", encoding="utf-8", newline="").write(nl.join(elines) + nl)

    subprocess.run([sys.executable, os.path.join(REPO, "scripts", "organize_objects.py")],
                   cwd=REPO, check=True, stdout=subprocess.DEVNULL)
    print("wire: %d gates (%d new objects, %d new DEPS)" % (len(ready), len(add), len(new)))
    return 0


def bash(cmd, timeout=None):
    return subprocess.run(["C:/msys64/usr/bin/bash.exe", "-lc", cmd],
                          capture_output=True, text=True, errors="replace",
                          timeout=timeout)


def build(plan):
    ready = [r for r in inventory(plan) if not r["missing"]]
    objs = " ".join("src/%s.o" % r["module"] for r in ready)
    cmd = ("cd /c/Users/eman5/slermes && source build/winlibs-env.sh && "
           "make -j%s %s 2>&1 | grep -aE 'error:|warning: .*uninitialized' || true" % (JOBS, objs))
    res = bash(cmd)
    print(res.stdout[-4000:] or "build: clean")
    rows = inventory(plan)
    missing = [r["gate"] for r in rows if not r["missing"] and not r["built"]]
    print("build: %d/%d objects present" % (len(rows) - len(missing), len(rows)))
    for g in missing[:10]:
        print("  no object: %s" % g)
    return 1 if missing else 0


JOBS = str(min(12, (os.cpu_count() or 4)))


def deps_for(gates):
    """(gate, objs) pairs from the DEPS map in tests/run_all_oracles.sh."""
    text = open(DEPS, encoding="utf-8", errors="replace").read()
    out = []
    for gate in gates:
        m = re.search(r"\[%s\]=\"([^\"]*)\"" % re.escape(gate), text)
        out.append((gate, m.group(1) if m else ""))
    return out


def prebuild(ready):
    """One make -j batch for the subset's harnesses instead of N serial compiles.

    Without this every gate pays a gcc link (~4 s) plus a freshness find; with
    it the whole subset links in one parallel make and the run path only copies
    binaries. ORACLE_PREBUILT_OK tells run_one_oracle.sh the cache is trusted.
    """
    specs = os.path.join(REPO, ".testbin", "wave_specs.tsv")
    os.makedirs(os.path.dirname(specs), exist_ok=True)
    with open(specs, "w", encoding="utf-8", newline="\n") as fh:
        for gate, objs in deps_for([r["gate"] for r in ready]):
            fh.write("%s\t%s\t%s\t\n" % (gate, gate, objs))
    res = bash("cd /c/Users/eman5/slermes && source build/winlibs-env.sh && "
               "bash tests/build_all_harnesses.sh .testbin/wave_specs.tsv 2>&1 | tail -5")
    print("prebuild: %s" % (res.stdout.strip().splitlines()[-1] if res.stdout.strip() else "clean"))
    return res.returncode


def gates(plan):
    """Run the gates through the fast driver; ~2.5x the shell runner per measured
    round on this box (10 gates: 15.6 s vs 38-42 s, identical case counts)."""
    ready = [r for r in inventory(plan) if not r["missing"] and r["built"]]
    names = " ".join(r["gate"] for r in ready)
    cmd = ("cd /c/Users/eman5/slermes && source build/winlibs-env.sh && "
           "PYTHON_BIN=C:/Users/eman5/hermes-agent/.venv/Scripts/python.exe "
           "python scripts/fast_gates.py --jobs %s %s" % (JOBS, names))
    res = bash(cmd)
    out = res.stdout + res.stderr
    print(out[-6000:])
    return 0 if re.search(r"oracles run: \d+  \(exit 0", out) else 1


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    stage, path = sys.argv[1], sys.argv[2]
    plan = load(path)
    if stage == "plan":
        report(plan)
        return 0
    if stage == "wire":
        return wire(plan)
    if stage == "build":
        return build(plan)
    if stage == "gates":
        return gates(plan)
    if stage == "land":
        report(plan)
        for fn in (wire, build, gates):
            rc = fn(plan)
            if rc:
                print("land: %s failed" % fn.__name__)
                return rc
        print("land: wave green")
        return 0
    print("unknown stage: %s" % stage)
    return 2


if __name__ == "__main__":
    sys.exit(main())
