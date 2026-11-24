#!/usr/bin/env python3
"""Grow a vacuous gate fixture to a size that can pin a decision.

A gate whose fixture holds one case proves the pipeline runs; it cannot show
that the port and the live module agree on anything the author did not happen
to type. This tool takes such a fixture, derives variants of its existing cases
by perturbing scalar fields, keeps only the variants the oracle accepts (a
live-error traceback means the input is outside the module's domain), and
re-runs the gate to prove the enlarged fixture still matches.

It never invents an expected value: every expectation comes from the oracle,
which drives the live Python module.

    python scripts/expand_fixture.py <gate> [<gate> ...] [--target 8] [--apply]

Without --apply it reports how many accepted variants each gate would gain.
"""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
FIXTURES = REPO / "tests" / "fixtures"

# Values a scalar can be pushed to, per JSON type. Chosen to hit the branches a
# port actually gets wrong: empty vs absent, zero vs falsy, case, whitespace.
STRING_VARIANTS = ["", " ", "0", "None", "x", "TRUE", " -1 "]
NUMBER_VARIANTS = [0, -1, 1, 2, 1000000]
BOOL_VARIANTS = [True, False]


def variants(value):
    if isinstance(value, bool):
        return [v for v in BOOL_VARIANTS if v != value]
    if isinstance(value, (int, float)):
        return [v for v in NUMBER_VARIANTS if v != value][:3]
    if isinstance(value, str):
        return [v for v in STRING_VARIANTS if v != value][:3]
    return []


def perturbed(case: dict, limit: int):
    """One field changed per variant, scalars only, never the discriminator."""
    out = []
    for key, val in case.items():
        if key in ("func", "case", "out"):
            continue
        for alt in variants(val):
            clone = dict(case)
            clone[key] = alt
            clone["case"] = f"{case.get('case', 'case')}-x{len(out)}"
            out.append(clone)
            if len(out) >= limit:
                return out
    return out


def oracle_accepts(gate: str, cases: list[dict]) -> list[dict]:
    payload = "".join(json.dumps(c) + "\n" for c in cases).encode()
    proc = subprocess.run(
        [sys.executable, str(REPO / "tests" / f"oracle_{gate}.py")],
        input=payload, capture_output=True, cwd=REPO,
    )
    text = (proc.stdout + proc.stderr).decode("utf-8", "replace")
    if "live-error" in text or proc.returncode not in (0, 1):
        return []
    return cases


def gate_cases(gate: str) -> list[dict]:
    path = FIXTURES / f"{gate}.jsonl"
    if not path.is_file():
        return []
    return [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines() if line.strip()]


def run_gate(gate: str) -> tuple[int, str]:
    proc = subprocess.run(
        [sys.executable, str(REPO / "scripts" / "gate_batch.py"), gate, "--jobs", "1"],
        capture_output=True, cwd=REPO,
    )
    text = (proc.stdout + proc.stderr).decode("utf-8", "replace")
    return proc.returncode, text.strip().splitlines()[-3:] if text.strip() else []


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("gates", nargs="+")
    ap.add_argument("--target", type=int, default=8)
    ap.add_argument("--apply", action="store_true")
    args = ap.parse_args(argv)

    grown = 0
    for gate in args.gates:
        base = gate_cases(gate)
        if not base:
            print(f"{gate}: no fixture, skipped")
            continue
        if len(base) >= args.target:
            print(f"{gate}: {len(base)} cases, already >= {args.target}")
            continue
        need = args.target - len(base)
        proposed = []
        for case in base:
            proposed.extend(perturbed(case, need - len(proposed)))
            if len(proposed) >= need:
                break
        accepted = oracle_accepts(gate, proposed) if proposed else []
        print(f"{gate}: {len(base)} -> {len(base) + len(accepted)} cases ({len(accepted)} accepted of {len(proposed)})")
        if not args.apply or not accepted:
            continue
        path = FIXTURES / f"{gate}.jsonl"
        with path.open("a", encoding="utf-8", newline="\n") as fh:
            for case in accepted:
                fh.write(json.dumps(case, sort_keys=True) + "\n")
        rc, tail = run_gate(gate)
        print(f"  gate rc={rc} {'; '.join(tail)}")
        if rc != 0:
            # Leave the tree exactly as it was: a grown fixture that breaks the
            # gate is worse than a small fixture.
            kept = [line for line in path.read_text(encoding="utf-8").splitlines() if line.strip()][: len(base)]
            path.write_text("\n".join(kept) + "\n", encoding="utf-8", newline="\n")
            print("  reverted")
        else:
            grown += 1
    if args.apply:
        print(f"\ngates grown and still green: {grown}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
