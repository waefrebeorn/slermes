#!/usr/bin/env python3
"""
god_header_eliminate.py — safe, build-verified removal of `#include "hermes.h"`
from src/**/port_*.c files.

Strategy (per AUDIT-FIRST + transitive-dependency pitfall):
  For each port_*.c that includes hermes.h:
    1. Remove the include line.
    2. `make <obj>` (incremental). If it compiles -> keep the removal.
    3. If it fails -> restore hermes.h (file genuinely needs core types; skip).
Prints a report. Does NOT git-commit. Idempotent-ish (safe to re-run: only
touches files still containing the include).
"""
import subprocess, os, re, sys

ROOT = "/home/wubu/hermes-agent-dev/slermes"
os.chdir(ROOT)

def find_ports():
    out = []
    for dp, _, fns in os.walk("src"):
        for fn in fns:
            if fn.startswith("port_") and fn.endswith(".c"):
                p = os.path.join(dp, fn)
                with open(p) as f:
                    if '#include "hermes.h"' in f.read():
                        out.append(p)
    return out

def make_obj(src):
    obj = src[:-2] + ".o"
    r = subprocess.run(["make", obj], capture_output=True, text=True)
    return r.returncode == 0, r.stdout + r.stderr

def has_include(path):
    with open(path) as f:
        return '#include "hermes.h"' in f.read()

removed, kept = [], []
ports = find_ports()
print(f"found {len(ports)} port_*.c with hermes.h")
for p in ports:
    if not has_include(p):
        continue
    # read & remove the include line
    with open(p) as f:
        lines = f.readlines()
    new = [l for l in lines if '#include "hermes.h"' not in l]
    with open(p, "w") as f:
        f.writelines(new)
    ok, log = make_obj(p)
    if ok:
        removed.append(p)
        print(f"  REMOVED  {p}")
    else:
        # restore
        with open(p, "w") as f:
            f.writelines(lines)
        kept.append(p)
        # surface the first error so we can triage
        errs = [l for l in log.splitlines() if "error:" in l][:2]
        print(f"  KEPT     {p}  -> {'; '.join(errs)}")

print(f"\nSUMMARY removed={len(removed)} kept={len(kept)}")
with open("/tmp/god_header_report.txt", "w") as f:
    f.write("REMOVED:\n" + "\n".join(removed) + "\n\nKEPT (need hermes.h):\n" + "\n".join(kept))
