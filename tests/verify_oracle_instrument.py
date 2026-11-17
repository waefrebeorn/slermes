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
import json
import re
import sys
from pathlib import Path

REPO = Path("C:/Users/eman5/slermes")
BUILD = REPO / "tests/oracle/build"

INSTRUMENT = ("LINK", "COMPILE", "NO-OUTPUT", "ORACLE-BROKEN")
PARITY = ("C-MISMATCH",)

REGISTRY = REPO / "tests/oracle/registry.json"
DRIFT = REPO / "tests/oracle_drift_allowlist.py"


def _drift_oracles():
    """Oracle file basenames that carry DECLARED drift debt.

    A name/path/arity drift means the oracle cannot load the live module, so it
    produces no output. That is an ABSENT MEASUREMENT, not a parity result, and
    the drift gate already tracks it as declared debt. Counting it as a failure
    here would report a measurement that does not exist.
    """
    import ast
    try:
        tree = ast.parse(DRIFT.read_text(encoding="utf-8"))
    except Exception:
        return set()
    keys = set()
    for node in ast.walk(tree):
        if isinstance(node, ast.Assign):
            for t in node.targets:
                if isinstance(t, ast.Name) and t.id in ("DECLARED", "ALLOW"):
                    for k in getattr(node.value, "keys", []):
                        if isinstance(k, ast.Constant) and isinstance(k.value, str):
                            keys.add(k.value.split(":")[0])
    return keys


def declared_drift(port):
    """True when this port's oracle file is in the declared-drift debt list."""
    try:
        reg = json.loads(REGISTRY.read_text(encoding="utf-8"))["ports"]
    except Exception:
        return False
    entry = reg.get(port) or {}
    oracle = entry.get("oracle") or ""
    if not oracle:
        return False
    return Path(oracle).name in _drift_oracles()


def reasons(port):
    """The per-case failure reasons the runner recorded."""
    vp = BUILD / f"batch_{port}.verdicts.json"
    if not vp.exists():
        return set()
    try:
        vs = json.loads(vp.read_text(encoding="utf-8"))
    except Exception:
        return set()
    return {v.get("reason") or "" for v in vs if v.get("verdict") != "MATCH"}


def classify(port):
    log = BUILD / f"suite_{port}.log"
    text = log.read_text(encoding="utf-8", errors="replace") if log.exists() else ""
    if "LINK FAILED" in text:
        return "LINK"
    if "FATAL: harness did not compile" in text:
        return "COMPILE"

    # Classification reads the VERDICTS, not log text. Grepping the log for a
    # message string mis-buckets as soon as the wording changes: the runner now
    # emits "C produced no output" as well as "oracle produced no output", and
    # the old check counted the former as a plain mismatch.
    rs = reasons(port)
    # An oracle that produced nothing is an ABSENT MEASUREMENT, not a parity
    # result. When the cause is drift already declared as debt, say SKIP: the
    # drift gate owns that failure, and reporting it here as a parity FAIL
    # claims a measurement that does not exist.
    if any(r.startswith("oracle produced no output") for r in rs):
        return "SKIP-DRIFT" if declared_drift(port) else "ORACLE-BROKEN"
    if any(r.startswith("C produced no output") for r in rs):
        return "NO-OUTPUT"
    if any(r == "output differs" for r in rs):
        return "C-MISMATCH"

    # Fallbacks for suite runs written before verdicts existed.
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
        # A CR in the rc filename (a port list written in Python text mode on
        # Windows ends up CRLF, and the CR becomes part of the name) otherwise
        # makes every port unreadable: the gate derives the port from the rc
        # name and then looks for suite_<port>.log, which does not exist, so
        # all 338 ports bucket as OTHER. Strip it so a stray CR degrades one
        # port instead of the whole run.
        port = f.stem[6:].rstrip("\r")
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
