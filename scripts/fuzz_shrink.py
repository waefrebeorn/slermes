#!/usr/bin/env python3
"""Shrink a held-out mismatch to the smallest input that still disagrees.

Hypothesis does this for property tests: once an input fails, it *reduces* it to a
simpler input that still fails, so the reported case is a short list or a single
small number instead of a large random object. A differential fuzzer needs the same
thing for a different reason: the minimal case is the specification. "the mapping
had three keys and one was 'None'" is a fix; a 300-byte random object is homework.

Mismatch is the differential predicate: run the C harness on the case, feed the
result to the live-module oracle, and ask whether the oracle reports a mismatch. A
candidate that still mismatches is smaller and just as good.

    python scripts/fuzz_shrink.py --plan docs/wave27-plan.json <gate> [--case fuzz-123-0]

With no --case it shrinks every mismatching case in tests/fixtures/<gate>.jsonl.
"""
from __future__ import annotations

import argparse
import copy
import json
import os
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gate_fuzz as gf  # noqa: E402  (build_harness, case loading, tuning)

REPO = Path(__file__).resolve().parent.parent
KEEP = {"func", "case", "out", "seams"}


def simplify(value):
    """Smaller candidates for a JSON value, most aggressive first."""
    out = []
    if isinstance(value, bool):
        out.append(False)
    elif isinstance(value, (int, float)):
        if value != 0:
            out.append(0)
        if isinstance(value, float) and value != 1.0:
            out.append(1)
        if value < 0:
            out.append(-value)
    elif isinstance(value, str):
        if value:
            out.append("")
        stripped = value.strip()
        if stripped != value:
            out.append(stripped)
        if len(value) > 1:
            out.append(value[: len(value) // 2])
            out.append(value[:1])
        if value != value.lower():
            out.append(value.lower())
        if value != value.upper():
            out.append(value.upper())
        for probe in ("0", "None", "false", "[]", "{}"):
            if value != probe:
                out.append(probe)
    elif isinstance(value, list):
        if value:
            out.append([])
        if len(value) > 1:
            out.append(value[: len(value) // 2])
            out.append(value[:1])
            out.append(value[:1] * len(value))  # a duplicate of the first element
            out.append(list(reversed(value)))
        for i in range(len(value)):
            inner = simplify(value[i])
            if inner:
                clone = list(value)
                clone[i] = inner[0]
                out.append(clone)
    elif isinstance(value, dict):
        if value:
            out.append({})
            keys = list(value)
            if len(keys) > 1:
                out.append({keys[0]: value[keys[0]]})
                rest = {k: value[k] for k in keys[1:]}
                out.append(rest)
            for k in keys:
                clone = dict(value)
                clone.pop(k)
                out.append(clone)
            for k in keys:
                for inner in simplify(value[k])[:3]:
                    clone = dict(value)
                    clone[k] = inner
                    out.append(clone)
    return out


class Runner:
    def __init__(self, gate: str, module: str):
        exe, err = gf.build_harness(gate, module)
        if not exe:
            raise SystemExit("harness build failed: %s" % err)
        self.gate = gate
        self.exe = exe
        self.oracle = str(REPO / "tests" / ("oracle_%s.py" % gate))

    def c_out(self, case: dict):
        line = json.dumps({k: v for k, v in case.items() if k != "out"}) + "\n"
        p = subprocess.run([self.exe], input=line, capture_output=True, text=True,
                           encoding="utf-8", errors="replace", timeout=60)
        for raw in p.stdout.splitlines():
            try:
                obj = json.loads(raw)
            except ValueError:
                continue
            if obj.get("case") == case.get("case"):
                return obj.get("out")
        return None

    def mismatches(self, case: dict) -> bool:
        out = self.c_out(case)
        probe = dict(case)
        probe["out"] = out
        p = subprocess.run([sys.executable, self.oracle],
                           input=json.dumps(probe) + "\n", capture_output=True,
                           text=True, encoding="utf-8", errors="replace", timeout=120)
        text = (p.stdout or "") + (p.stderr or "")
        if "live-error" in text:      # upstream raised: no value to compare
            return False
        return "MISMATCH" in text


def mismatching_ids(runner: Runner, cases: list[dict]) -> set[str]:
    """One oracle run over the whole set: cheaper than probing case by case."""
    payload = "".join(json.dumps(c) + "\n" for c in cases)
    p = subprocess.run([sys.executable, runner.oracle], input=payload,
                       capture_output=True, text=True, encoding="utf-8",
                       errors="replace", timeout=900)
    ids = set()
    for line in ((p.stdout or "") + (p.stderr or "")).splitlines():
        if line.startswith("MISMATCH") and "live-error" not in line:
            marker = "case="
            at = line.find(marker)
            if at >= 0:
                ids.add(line[at + len(marker):].split()[0].strip("'\""))
    return ids


def batch_mismatch(runner: Runner, trials: list[dict]) -> set[str]:
    """Evaluate many candidates with two process starts: one harness run, one oracle run.

    Shrinking is O(candidates) probes; paying Python startup per probe is what made
    the first version slow enough to time out on a 200-case gate.
    """
    if not trials:
        return set()
    payload = "".join(json.dumps(t) + "\n" for t in trials)
    h = subprocess.run([runner.exe], input=payload, capture_output=True, text=True,
                       encoding="utf-8", errors="replace", timeout=300)
    filled = h.stdout or ""
    p = subprocess.run([sys.executable, runner.oracle], input=filled,
                       capture_output=True, text=True, encoding="utf-8",
                       errors="replace", timeout=600)
    bad = set()
    for line in ((p.stdout or "") + (p.stderr or "")).splitlines():
        if line.startswith("MISMATCH") and "live-error" not in line:
            at = line.find("case=")
            if at >= 0:
                bad.add(line[at + 5:].split()[0].strip("'\""))
    return bad


def shrink(runner: Runner, case: dict, verbose=False) -> dict:
    best = copy.deepcopy(case)
    keys = [k for k in best if k not in KEEP]
    for _round in range(6):
        improved = False
        for key in keys:
            trials, tags = [], []
            for i, cand in enumerate(simplify(best[key])):
                trial = copy.deepcopy(best)
                trial[key] = cand
                trial["case"] = "shrink-%s-%d" % (key, i)
                trials.append(trial)
                tags.append((trial["case"], cand))
            bad = batch_mismatch(runner, trials)
            chosen = next((cand for tag, cand in tags if tag in bad), None)
            if chosen is None:
                continue
            best[key] = chosen
            best["case"] = case.get("case")
            improved = True
            if verbose:
                print("   - %s = %s" % (key, json.dumps(chosen)[:70]), flush=True)
        if not improved:
            break
    return best


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("gate")
    ap.add_argument("--plan")
    ap.add_argument("--module")
    ap.add_argument("--case")
    ap.add_argument("--verbose", action="store_true")
    ap.add_argument("--source", help="jsonl of cases (default: the held-out set, else the authored fixture)")
    args = ap.parse_args(argv)

    module = args.module
    if not module and args.plan:
        module = json.loads(Path(args.plan).read_text(encoding="utf-8")).get(args.gate)
    if not module:
        print("no module for %s (pass --plan or --module)" % args.gate, file=sys.stderr)
        return 2

    # Held-out cases first: the authored suite is expected to be green, and it is
    # the perturbed set that carries the findings worth shrinking.
    fixture = Path(args.source) if args.source else None
    if fixture is None:
        held = REPO / ".testbin" / "fuzz" / ("%s.filled.jsonl" % args.gate)
        fixture = held if held.is_file() else REPO / "tests" / "fixtures" / ("%s.jsonl" % args.gate)
    cases = []
    for line in fixture.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if line:
            cases.append(json.loads(line))
    if args.case:
        cases = [c for c in cases if c.get("case") == args.case]

    runner = Runner(args.gate, module)
    shrunk = 0
    if not args.case:
        ids = mismatching_ids(runner, cases)
        cases = [c for c in cases if c.get("case") in ids]
        print("%d mismatching case(s) to shrink" % len(cases), flush=True)
    for case in cases:
        if not runner.mismatches(case):
            continue
        small = shrink(runner, case, args.verbose)
        # Verify the reduction on its OWN, not inside a batch: a harness that keeps
        # state between lines would otherwise let a batch-only mismatch pass for a
        # minimal reproduction (seen on w29_sessions_2114c9_p4).
        if not runner.mismatches(small):
            print("\n%s %s: batch-only mismatch, does not reproduce alone" % (args.gate, case.get("case")))
            continue
        shrunk += 1
        print("\n%s %s" % (args.gate, case.get("case")))
        print("  was:  %s" % json.dumps({k: v for k, v in case.items() if k != "out"}, ensure_ascii=False)[:200])
        print("  min:   %s" % json.dumps({k: v for k, v in small.items() if k != "out"}, ensure_ascii=False)[:200])
    print("\nshrunk %d mismatching case(s)" % shrunk)
    return 0


if __name__ == "__main__":
    sys.exit(main())
