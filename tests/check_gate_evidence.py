#!/usr/bin/env python3
"""check_gate_evidence.py — the workflow guard for gate EVIDENCE, not gate count.

Why this exists. The fleet can grow the "PORT" number without producing a single
piece of evidence: a gate that links the port but whose fixtures never move the
answer, whose Python twin never reads its input, whose Python function returns a
tuple the harness cannot compare, or whose C function ignores its argument (a
stub). Each of those reads as a link or a green cup and proves nothing. Measured
on this tree:

  40-gate random PORT sample -> 26 NON-DISCRIMINATING, 5 TWIN BLIND, 0 clean MATCH
  708 PORT gates call a Python fn that never reads its own input
  142 PORT gates wrap a function returning a tuple (twin mixes a different shape)
    5 PORT gates wrap a C function that ignores every parameter (a stub)
   41 COPY gates measure a hand transcription of the port

So the honest headline is not "PORT 8,326". It is:

    EVIDENCE = PORT - input-independent - tuple-shaped - stub-backed

This guard ratchets that number: it fails when evidence DROPS or when any debt
class GROWS. Lower the debt by fixing gates; nothing else moves the number.

Usage:
  python tests/check_gate_evidence.py              # ratchet check (used by make)
  python tests/check_gate_evidence.py --save       # adopt the current state
  python tests/check_gate_evidence.py --report     # print, never fail
"""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
BUILD = REPO / "tests" / "watercup" / "build"
PROV = BUILD / "gate_provenance.json"
BASELINE = REPO / "docs" / "dev" / "gate-evidence-baseline.json"
AUDIT = REPO / "scripts" / "audit_gate_provenance.py"
STUBS = REPO / "scripts" / "audit_stub_functions.py"

DEBT_CLASSES = ("COPY", "SHIM", "MISSING", "BROKEN", "PLACEHOLDER")


def run_audit() -> None:
    subprocess.run([sys.executable, str(AUDIT), "--json", str(PROV)],
                   cwd=REPO, check=True, stdout=subprocess.DEVNULL)


def stub_symbols() -> set[str]:
    sys.path.insert(0, str(REPO / "scripts"))
    from audit_stub_functions import functions_in
    out: set[str] = set()
    for root in ("src", "lib"):
        for c in (REPO / root).rglob("*.c"):
            for row in functions_in(c):
                out.add(row["symbol"])
    return out


def shallow_gates() -> set[str]:
    """Gates whose twin hands the fixture raw to a Python function that needs more.

    Measured by scripts/audit_shallow_twins.py: 17,083 of 22,604 gates pass the
    fixture straight through while the oracle takes keyword-only args, a config
    object, or several required arguments. Those calls raise or answer a different
    question, so no cup they produce is parity evidence.
    """
    cache = REPO / ".testbin" / "shallow_twins.json"
    if not cache.exists():
        subprocess.run([sys.executable, str(REPO / "scripts" / "audit_shallow_twins.py"),
                        "--json", str(cache)], cwd=REPO, check=True,
                       stdout=subprocess.DEVNULL)
    rows = json.loads(cache.read_text())
    return {r["gate"] for r in rows if r["class"] in ("SHALLOW", "NO-TWIN")}


def measure() -> dict:
    if not PROV.exists():
        run_audit()
    rows = json.loads(PROV.read_text())
    stubs = stub_symbols()
    shallow = shallow_gates()
    counts: dict[str, int] = {}
    for r in rows:
        counts[r["class"]] = counts.get(r["class"], 0) + 1
    port = [r for r in rows if r["class"] == "PORT"]
    const = [r for r in port if r.get("input_independent") is True]
    tuple_ = [r for r in port if r.get("returns_tuple") is True]
    stub = [r for r in port if r["symbol"] in stubs]
    shal = [r for r in port if r["gate"] in shallow]
    evidence = [r for r in port
                if r.get("input_independent") is not True
                and r.get("returns_tuple") is not True
                and r["symbol"] not in stubs
                and r["gate"] not in shallow]
    return {
        "evidence_gates": sorted(r["gate"] for r in evidence),
        "total": len(rows),
        "classes": counts,
        "port": len(port),
        "input_independent": len(const),
        "tuple_shaped": len(tuple_),
        "stub_backed": len(stub),
        "shallow_twin": len(shal),
        "evidence": len(evidence),
        "debt": {k: counts.get(k, 0) for k in DEBT_CLASSES},
        "stub_symbols_total": len(stubs),
    }


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--save", action="store_true")
    ap.add_argument("--report", action="store_true")
    ap.add_argument("--refresh", action="store_true", help="recompute provenance first")
    args = ap.parse_args()

    if args.refresh:
        run_audit()
    m = measure()

    print("== gate evidence")
    print("  gates total        %6d" % m["total"])
    for cls, n in sorted(m["classes"].items(), key=lambda kv: -kv[1]):
        print("  %-18s %6d" % (cls, n))
    print("  %-18s %6d" % ("PORT (links port)", m["port"]))
    print("    - no-input-read  %6d" % m["input_independent"])
    print("    - tuple-shaped   %6d" % m["tuple_shaped"])
    print("    - stub-backed    %6d" % m["stub_backed"])
    print("    - shallow-twin   %6d" % m["shallow_twin"])
    print("  %-18s %6d" % ("EVIDENCE (bound)", m["evidence"]))
    print("  ^ an UPPER BOUND: gates that CAN discriminate. The runtime share is")
    print("    lower -- a 40-gate random sample had 26 non-discriminating even after")
    print("    this filter, because a fn can read its input and still answer the same")
    print("    thing for every fixture (scripts/refixture_gates.py measures that).")
    print("  (stub-shaped functions in the tree: %d)" % m["stub_symbols_total"])

    if args.save:
        BASELINE.parent.mkdir(parents=True, exist_ok=True)
        BASELINE.write_text(json.dumps(
            {"evidence": m["evidence"], "evidence_gates": m["evidence_gates"],
             "debt": m["debt"], "port": m["port"], "total": m["total"]},
            indent=1) + "\n")
        print("saved baseline ->", BASELINE.relative_to(REPO))
        return 0

    if args.report or not BASELINE.exists():
        if not BASELINE.exists():
            print("\nno baseline yet: run `python tests/check_gate_evidence.py --save`")
        return 0

    base = json.loads(BASELINE.read_text())
    failures = []
    # Compare SETS, not counts. A count changes when gates are honestly
    # reclassified (an oracle that no longer exists stops being evidence); a SET
    # difference names the exact gates that regressed, which a count hides.
    was = set(base.get("evidence_gates") or [])
    now = set(m["evidence_gates"])
    lost = sorted(was - now)
    if lost:
        failures.append("%d gate(s) stopped being evidence, e.g. %s"
                        % (len(lost), ", ".join(lost[:5])))
    gained = sorted(now - was)
    if gained:
        print("\n%d gate(s) became evidence since the baseline: %s"
              % (len(gained), ", ".join(gained[:5])))
    for k, v in m["debt"].items():
        was = (base.get("debt") or {}).get(k, 0)
        if v > was:
            failures.append("%s grew %d -> %d (new gate in a debt class)" % (k, was, v))
    if failures:
        print("\n== WORKFLOW GATE FAILED")
        for f in failures:
            print("  - " + f)
        return 1
    print("\n== workflow gate passed (evidence %d, debt as baselined)" % m["evidence"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
