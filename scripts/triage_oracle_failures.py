#!/usr/bin/env python3
"""Classify oracle-suite failures into actionable buckets.

A red suite is only useful if you can tell a C-side mismatch from a broken
oracle. This reads the per-port suite logs and batch-oracle tracebacks and
buckets every non-passing port.

Buckets:
  C-MISMATCH   both sides produced output and they differ -- real parity work
  ORACLE-BROKEN the Python oracle itself failed (traceback in batch_<port>.err)
  COMPILE      the harness did not compile
  LINK         the harness did not link
  NO-OUTPUT    the C harness ran but produced nothing
  OTHER

Usage: triage_oracle_failures.py [rc-dir]
"""
import collections
import json
import re
import sys
from pathlib import Path

REPO = Path("C:/Users/eman5/slermes")
BUILD = REPO / "tests/oracle/build"


def classify(port, rc):
    log = BUILD / f"suite_{port}.log"
    text = log.read_text(encoding="utf-8", errors="replace") if log.exists() else ""
    if "LINK FAILED" in text:
        return "LINK"
    if "FATAL: harness did not compile" in text:
        return "COMPILE"
    err = BUILD / f"batch_{port}.err"
    tb = err.read_text(encoding="utf-8", errors="replace") if err.exists() else ""
    if "Traceback" in tb or "Error:" in tb or "Exception" in tb:
        last = tb.strip().splitlines()[-1] if tb.strip() else ""
        return "ORACLE-BROKEN: " + re.sub(r"'[^']*'", "'X'", last)[:80]
    if "oracle produced no output" in text:
        return "NO-OUTPUT"
    if re.search(r"^cases: MISMATCH", text, re.M):
        return "C-MISMATCH"
    return "OTHER"


def main():
    buckets = collections.defaultdict(list)
    for f in sorted(BUILD.glob("suite_*.rc")):
        port = f.stem[6:]
        rc = f.read_text(encoding="utf-8", errors="replace").strip()
        if rc == "0":
            buckets["PASS"].append(port)
            continue
        buckets[classify(port, rc)].append(port)

    print(f"PASS: {len(buckets['PASS'])}")
    for k in sorted(buckets, key=lambda x: -len(buckets[x])):
        if k == "PASS":
            continue
        print(f"{len(buckets[k]):4} {k}")
        for p in buckets[k][:6]:
            print(f"        {p}")
    out = Path("C:/Users/eman5/AppData/Local/hermes/cache/scratch/oracle_triage.json")
    out.write_text(json.dumps({k: v for k, v in buckets.items()}, indent=1),
                   encoding="utf-8")
    print("\nwritten:", out)


if __name__ == "__main__":
    main()
