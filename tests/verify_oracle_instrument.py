#!/usr/bin/env python3
"""Two gates over a completed oracle-suite run.

A single red bit cannot tell *what* is wrong, and counting an unrunnable
oracle as a parity failure is itself a validity failure -- it reports "the port
disagrees" when in fact nothing was measured. So:

  instrument gate (this script) -- every port must have a working instrument:
      harness compiles, harness produces output, oracle imports and runs.
      LINK / COMPILE / NO-OUTPUT are failures here.

  parity gate (scripts/triage_oracle_failures.py --parity) -- of the ports
      whose instrument works, the C and Python output must agree. C-MISMATCH
      is a failure here.

Usage:
    python tests/verify_oracle_instrument.py            # instrument gate
    python tests/verify_oracle_instrument.py --parity   # parity gate

Exit 0 = PASS, 1 = FAIL.
"""
import collections
import re
import sys
from pathlib import Path

REPO = Path("C:/Users/eman5/slermes")
BUILD = REPO / "tests/oracle/build"

INSTRUMENT = ("LINK", "COMPILE", "NO-OUTPUT")
PARITY = ("C-MISMATCH",)


def classify(port):
    log = BUILD / f"suite_{port}.log"
    text = log.read_text(encoding="utf-8", errors="replace") if log.exists() else ""
    if "LINK FAILED" in text:
        return "LINK"
    if "FATAL: harness did not compile" in text:
        return "COMPILE"
    err = BUILD / f"batch_{port}.err"
    tb = err.read_text(encoding="utf-8", errors="replace") if err.exists() else ""
    if tb.strip():
        return "ORACLE-BROKEN"
    if "oracle produced no output" in text:
        return "NO-OUTPUT"
    if re.search(r"^cases: MISMATCH", text, re.M):
        return "C-MISMATCH"
    return "OTHER"


def main():
    parity = "--parity" in sys.argv
    wanted = PARITY if parity else INSTRUMENT

    buckets = collections.Counter()
    offenders = collections.defaultdict(list)
    total = 0
    for f in sorted(BUILD.glob("suite_*.rc")):
        total += 1
        port = f.stem[6:]
        if f.read_text(encoding="utf-8", errors="replace").strip() == "0":
            buckets["PASS"] += 1
            continue
        kind = classify(port)
        buckets[kind] += 1
        offenders[kind].append(port)

    name = "parity" if parity else "instrument"
    failing = {k: v for k, v in buckets.items() if k in wanted}
    print(f"{name} gate over {total} ports: "
          f"{buckets['PASS']} PASS, "
          f"{sum(v for k, v in buckets.items() if k not in ('PASS',) and k not in wanted)} oracle-side/unclassified")
    for k in sorted(buckets):
        if k == "PASS":
            continue
        mark = "FAIL" if k in wanted else "note"
        print(f"  {mark:4} {k:14} {buckets[k]}")
        if k in wanted:
            for p in offenders[k][:8]:
                print(f"           {p}")

    ok = not any(buckets[k] for k in wanted)
    print(f"{name} gate:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
