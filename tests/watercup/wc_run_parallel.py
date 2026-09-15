#!/usr/bin/env python3
"""
wc_run_parallel.py — parallel watercup runner using all CPU cores.

Parallelizes watercup gate compilation+execution across multiple workers.
Uses subprocess calls to wc_run's build_harness/run_harness/run_cup_gate
to avoid import issues with wc_run's dependencies.

Usage:
  python3 tests/watercup/wc_run_parallel.py              # all gates
  python3 tests/watercup/wc_run_parallel.py --max-workers 16
  python3 tests/watercup/wc_run_parallel.py --gap-only    # only gap sentinels
  python3 tests/watercup/wc_run_parallel.py --port-only   # only ported gates
  python3 tests/watercup/wc_run_parallel.py --legacy-only
  python3 tests/watercup/wc_run_parallel.py --json
  python3 tests/watercup/wc_run_parallel.py --dry-run     # just list
"""
import argparse
import concurrent.futures
import json
import os
import subprocess
import sys
import time
from pathlib import Path

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
GATES = os.path.join(HERE, "gates")
BUILD = os.path.join(HERE, "build")
RESULT_FILE = os.path.join(BUILD, "results.jsonl")


def discover_gates():
    return sorted(
        d for d in os.listdir(GATES)
        if os.path.isfile(os.path.join(GATES, d, "gate.json"))
    )


def run_single_gate(gate_name):
    """Run a single gate by calling wc_run as a subprocess. Returns dict."""
    gd = os.path.join(GATES, gate_name)
    gj = os.path.join(gd, "gate.json")

    result = {"gate": gate_name, "ok": False, "mismatches": 0}

    try:
        gate = json.load(open(gj))
        result["subsystem"] = gate.get("subsystem", "?")
        result["classification"] = gate.get("classification", "legacy")
    except Exception:
        result["error"] = "malformed gate.json"
        return result

    # Use wc_run's logic by calling it on a single gate
    # But wc_run doesn't support single gate via subprocess, so we replicate
    try:
        import sys
        sys.path.insert(0, HERE)
        import wc_run
        kind = gate.get("kind", "cup")
        if kind == "unit":
            fail, lines = wc_run.run_unit_gate(gd, gate, verbose=False)
        else:
            fail, lines = wc_run.run_cup_gate(gd, gate, verbose=False)
        result["ok"] = (fail == 0)
        result["mismatches"] = fail
        result["lines"] = lines
    except Exception as e:
        result["error"] = str(e)

    return result


def run_parallel(selected, max_workers=None):
    """Run gates in parallel using ProcessPoolExecutor."""
    if max_workers is None:
        max_workers = min(os.cpu_count() or 4, 16)

    results = []
    total = len(selected)
    done = 0
    start = time.time()

    with concurrent.futures.ProcessPoolExecutor(max_workers=max_workers) as executor:
        future_to_gate = {executor.submit(run_single_gate, name): name for name in selected}

        for future in concurrent.futures.as_completed(future_to_gate):
            name = future_to_gate[future]
            try:
                entry = future.result()
                results.append(entry)
            except Exception as e:
                results.append({"gate": name, "ok": False, "error": str(e)})

            done += 1
            if done % 1000 == 0 or done == total:
                elapsed = time.time() - start
                rate = done / elapsed if elapsed > 0 else 0
                print(f"  progress: {done}/{total} gates ({100*done/total:.0f}%, {rate:.0f} gates/s)", file=sys.stderr)

    elapsed = time.time() - start
    print(f"  completed {total} gates in {elapsed:.1f}s ({total/elapsed:.0f} gates/s)", file=sys.stderr)
    return results


def main():
    parser = argparse.ArgumentParser(description="Parallel watercup runner")
    parser.add_argument("gates", nargs="*", help="gate names (default: all)")
    parser.add_argument("--max-workers", type=int, default=None)
    parser.add_argument("--gap-only", action="store_true")
    parser.add_argument("--port-only", action="store_true")
    parser.add_argument("--legacy-only", action="store_true")
    parser.add_argument("--json", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--no-write", action="store_true", help="don't write results.jsonl")
    args = parser.parse_args()

    all_gates = discover_gates()
    selected = args.gates if args.gates else all_gates

    if args.gap_only:
        selected = [g for g in selected if g.startswith("real__")]
    elif args.port_only:
        selected = [g for g in selected if g.startswith("port__")]
    elif args.legacy_only:
        selected = [g for g in selected if not g.startswith(("port__", "real__", "part__"))]

    if not selected:
        print("No gates selected.", file=sys.stderr)
        return 0

    if args.dry_run:
        print(f"Would run {len(selected)} gates")
        for g in selected[:10]:
            print(f"  {g}")
        return 0

    if not args.json:
        print(f"=== Parallel Water-Cup Runner ===")
        print(f"    gates: {len(selected)}")
        print(f"    workers: {args.max_workers or min(os.cpu_count() or 4, 16)}")
        print()

    results = run_parallel(selected, max_workers=args.max_workers)

    # Write results
    if not args.no_write:
        os.makedirs(os.path.dirname(RESULT_FILE), exist_ok=True)
        with open(RESULT_FILE, "a") as f:
            for r in results:
                f.write(json.dumps({
                    "ok": r["ok"],
                    "gate": r["gate"],
                    "mismatches": r.get("mismatches", 0),
                    "classification": r.get("classification", "legacy"),
                }) + "\n")

    # Summary
    total = len(results)
    passing = sum(1 for r in results if r["ok"])
    failing = total - passing

    # By classification
    by_class = {}
    for r in results:
        cls = r.get("classification", "legacy")
        if cls not in by_class:
            by_class[cls] = {"total": 0, "pass": 0}
        by_class[cls]["total"] += 1
        if r["ok"]:
            by_class[cls]["pass"] += 1

    if args.json:
        print(json.dumps({
            "total": total,
            "passing": passing,
            "failing": failing,
            "by_class": by_class,
            "results": results,
        }, indent=1))
    else:
        print("=" * 60)
        print(f"TOTAL: {total}  PASS: {passing}  FAIL: {failing}")
        print("By classification:")
        for cls in sorted(by_class):
            b = by_class[cls]
            print(f"  {cls}: {b['pass']}/{b['total']} pass")
        if failing == 0:
            print("VERDICT: all cups match LIVE Python")
        else:
            print("VERDICT: water-cup caught divergence(s)")

    return 1 if failing else 0


if __name__ == "__main__":
    sys.exit(main())
