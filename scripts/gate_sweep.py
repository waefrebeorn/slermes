#!/usr/bin/env python3
"""Run every gate in one sweep and report one table.

The gates exist to be run; a gate nobody runs is decoration. Running them one at
a time means the set that gets run depends on who remembers what, and a green
build stops meaning "all gates pass".

Order is cheapest-first so a broken tree is reported fast:
  static/text gates  ->  registry/fixture/drift  ->  build health  ->  oracle

Usage:
    python scripts/gate_sweep.py              # everything
    python scripts/gate_sweep.py --quick      # skip the oracle instrument gate
    python scripts/gate_sweep.py --only naming,fixtures

Exit 0 only if every gate that ran passed.
"""
from __future__ import annotations

import argparse
import subprocess
import sys
import time
from pathlib import Path

REPO = Path("C:/Users/eman5/slermes")

# (label, argv, quick?)
GATES = [
    ("c-standards",   ["tests/verify_c_standards.py"],          True),
    ("naming",        ["tests/verify_naming.py"],               True),
    ("header-guards", ["tests/verify_header_guards.py"],        True),
    ("build-vars",    ["tests/verify_build_vars.py"],           True),
    ("port-objects",  ["tests/verify_port_objects.py"],         True),
    ("oracle-registry", ["tests/verify_oracle_registry.py"],    True),
    ("harness-reads",  ["tests/verify_harness_reads.py"],        True),
    ("oracle-fixtures", ["tests/verify_oracle_fixtures.py"],    True),
    ("oracle-drift",  ["tests/verify_oracle_drift.py"],         True),
    ("build-health",  ["tests/verify_build_health.py", "--no-link"], True),
    ("oracle-instrument", ["tests/verify_oracle_instrument.py"], False),
    ("oracle-parity", ["tests/verify_oracle_instrument.py", "--parity"], False),
]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--quick", action="store_true")
    ap.add_argument("--only", default="")
    a = ap.parse_args()
    want = {s.strip() for s in a.only.split(",") if s.strip()}

    rows = []
    for label, argv, quick in GATES:
        if want and label not in want:
            continue
        if a.quick and not quick:
            rows.append((label, "SKIP", 0.0, "skipped (--quick)"))
            continue
        t0 = time.time()
        try:
            r = subprocess.run([sys.executable, *argv], cwd=REPO,
                               capture_output=True, text=True, timeout=900)
            dt = time.time() - t0
            tail = [l for l in (r.stdout or "").strip().split("\n") if l.strip()]
            last = tail[-1] if tail else f"rc={r.returncode}"
            ok = r.returncode == 0
            rows.append((label, "PASS" if ok else "FAIL", dt, last[:70]))
        except subprocess.TimeoutExpired:
            rows.append((label, "FAIL", time.time() - t0, "timed out"))

    print("gate sweep")
    print("-" * 78)
    for label, verdict, dt, note in rows:
        print(f"  {verdict:4} {label:18} {dt:6.1f}s  {note}")
    print("-" * 78)
    failed = [r[0] for r in rows if r[1] == "FAIL"]
    print(f"{len(rows) - len(failed)}/{len(rows)} gates passed"
          + (f"; FAILED: {', '.join(failed)}" if failed else ""))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
