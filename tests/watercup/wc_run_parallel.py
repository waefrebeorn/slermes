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

os.environ["WATERCUP_NO_BROWSER"] = "1"

# Install browser guard + comprehensive interactive-flow guard BEFORE any LIVE
# module imports. This patches all browser-spawn paths, hermes CLI entry
# points, and web-research tool flows so watercup can never launch a real
# browser, spawn a child process, or touch a live, interactive hermes CLI.
import browser_guard  # noqa: F401
try:
    import watercup_guard  # noqa: F401
    watercup_guard.install()
except Exception:
    pass

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
GATES = os.path.join(HERE, "gates")
BUILD = os.path.join(HERE, "build")
RESULT_FILE = os.path.join(BUILD, "results.jsonl")

# Operator-state isolation — see the long note in wc_run.py. Must happen here
# too: this runner imports wc_run inside ProcessPoolExecutor workers, and a
# worker inheriting the real HERMES_HOME would let an estop gate engage the
# operator's actual pause sentinel.
_WC_SANDBOX_HOME = os.path.join(BUILD, "home_watercup")
if not os.environ.get("WATERCUP_REAL_HERMES_HOME"):
    os.makedirs(_WC_SANDBOX_HOME, exist_ok=True)
    os.environ["SLERMES_HOME"] = _WC_SANDBOX_HOME
    os.environ["HERMES_HOME"] = _WC_SANDBOX_HOME


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
    #
    # INCIDENT 2026-10-20 19:44 (cron tick): an api_server/skills oracle
    # booted real gateway code with the INHERITED cwd (repo root!). Its
    # skills scan walked '.' and a refresh/purge flow DELETED 32 tracked
    # root files (Makefile, README, LICENSE, ...) and created fixture-named
    # dirs (hello/, world/, NULL/, state/) in the repo. Every oracle now
    # runs inside a fresh per-gate sandbox cwd, created and wiped here.
    sandbox = None
    try:
        import sys
        sys.path.insert(0, HERE)
        import wc_run
        import shutil
        import tempfile
        # Outside the repo (gate dirs are tracked): per-gate sandbox under
        # TMPDIR, namespaced by pid+gate, wiped before and after the run.
        sandbox = os.path.join(
            tempfile.gettempdir(),
            "watercup_sandbox_%d_%s" % (os.getpid(), gate_name),
        )
        shutil.rmtree(sandbox, ignore_errors=True)
        os.makedirs(sandbox, exist_ok=True)
        prev_cwd = os.getcwd()
        os.chdir(sandbox)
        try:
            kind = gate.get("kind", "cup")
            if kind == "unit":
                fail, lines = wc_run.run_unit_gate(gd, gate, verbose=False)
            else:
                fail, lines = wc_run.run_cup_gate(gd, gate, verbose=False)
            result["ok"] = (fail == 0)
            result["mismatches"] = fail
            result["lines"] = lines
        finally:
            os.chdir(prev_cwd)
            shutil.rmtree(sandbox, ignore_errors=True)
    except Exception as e:
        result["error"] = str(e)
    except SystemExit as e:
        # Same backstop as wc_run.main(): a SystemExit escaping anywhere in the
        # per-gate work would otherwise kill the worker and surface as an
        # unattributed pool error. SystemExit only — KeyboardInterrupt must
        # still propagate so Ctrl+C reaches the pool.
        result["error"] = "SystemExit(%s)" % (e.code,)

    return result


def run_parallel(selected, max_workers=None):
    """Run gates in parallel using ProcessPoolExecutor.

    Per-gate timeout (WATERCUP_GATE_TIMEOUT, default 120s) bounds the REPORT:
    a gate that hangs is recorded as a timeout instead of blocking the run
    forever. Generous enough for a cold compile of the largest harnesses
    (~350KB of C), tight enough that a wedged gate surfaces in minutes.

    LIMITATION, stated plainly: a worker blocked inside a C call cannot be
    killed from Python. `future.cancel()` is a no-op once the worker has
    picked the gate up, so a truly wedged worker keeps running until the
    process exits — it just stops blocking the report. A hard guarantee needs
    per-gate subprocess isolation (like run_unit_gate already does for its
    twin), which costs a process spawn per gate and is not worth it for a
    fleet that is ~0.1s/gate when healthy.
    """
    if max_workers is None:
        max_workers = min(os.cpu_count() or 4, 16)

    try:
        per_gate_timeout = float(os.environ.get("WATERCUP_GATE_TIMEOUT", "120"))
    except ValueError:
        per_gate_timeout = 120.0

    results = []
    total = len(selected)
    done = 0
    start = time.time()

    # Managed manually rather than via `with`: the context manager calls
    # shutdown(wait=True) on exit, which blocks until every in-flight gate
    # finishes. A wedged worker would then hold the run for its full duration
    # (measured: 66s for 4 gates on 2 workers with a 3s timeout) even though
    # the report is already complete. Shutdown explicitly instead.
    executor = concurrent.futures.ProcessPoolExecutor(max_workers=max_workers)
    abandoned = False
    try:
        # Bounded submission: submitting all 22k futures up front queues them
        # all in memory. Keep a window of ~4x workers in flight instead.
        window = max_workers * 4
        pending = {}
        it = iter(selected)

        def _submit_more():
            while len(pending) < window:
                try:
                    name = next(it)
                except StopIteration:
                    return
                pending[executor.submit(run_single_gate, name)] = name

        _submit_more()
        while pending:
            # Wait for the earliest-finishing future, bounded per gate.
            finished, _ = concurrent.futures.wait(
                pending, timeout=per_gate_timeout,
                return_when=concurrent.futures.FIRST_COMPLETED)

            if not finished:
                # Nothing completed inside the window: every in-flight gate is
                # wedged. Record them, drop the queued ones, stop waiting.
                wedged = len(pending)
                for fut, name in list(pending.items()):
                    results.append({
                        "gate": name, "ok": False,
                        "error": "TIMEOUT after %.0fs (worker wedged)" % per_gate_timeout,
                    })
                    fut.cancel()
                    done += 1
                pending.clear()
                abandoned = True
                print("  WARNING: %d gate(s) timed out; abandoning them" % wedged,
                      file=sys.stderr)
                break

            for fut in finished:
                name = pending.pop(fut)
                try:
                    results.append(fut.result())
                except Exception as e:
                    results.append({"gate": name, "ok": False, "error": str(e)})
                done += 1
                if done % 1000 == 0 or done == total:
                    elapsed = time.time() - start
                    rate = done / elapsed if elapsed > 0 else 0
                    print(f"  progress: {done}/{total} gates ({100*done/total:.0f}%, {rate:.0f} gates/s)", file=sys.stderr)
            _submit_more()
    finally:
        # wait=False: do not block on wedged workers. cancel_futures drops
        # anything still queued. Workers blocked in a C call cannot be killed
        # from Python, so they die with the process.
        executor.shutdown(wait=not abandoned, cancel_futures=abandoned)

    elapsed = time.time() - start
    rate = (len(results) / elapsed) if elapsed > 0 else 0
    print(f"  completed {len(results)} gates in {elapsed:.1f}s ({rate:.0f} gates/s)", file=sys.stderr)
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

    # Reject unknown gate names BEFORE the empty-selection guard. Otherwise a
    # typo'd name survives into `selected`, the guard never fires, and the bogus
    # gate is executed and reported as an ordinary failing gate (exit 1) —
    # indistinguishable from a real divergence.
    if args.gates:
        unknown = [g for g in args.gates if g not in set(all_gates)]
        if unknown:
            print("ERROR: unknown gate name(s): %s" % ", ".join(unknown[:10]),
                  file=sys.stderr)
            print("  run --list via wc_run.py to see valid names", file=sys.stderr)
            return 2
        selected = [g for g in selected if g in set(all_gates)]

    if args.gap_only:
        selected = [g for g in selected if g.startswith("real__")]
    elif args.port_only:
        selected = [g for g in selected if g.startswith("port__")]
    elif args.legacy_only:
        selected = [g for g in selected if not g.startswith(("port__", "real__", "part__"))]

    if not selected:
        # FAIL-OPEN BUG (fixed): this used to `return 0` — success. A typo'd gate
        # name, or --port-only over a list with no port__ gates, silently
        # "passed" the parity gate and CI went green having verified nothing.
        # An empty selection is an error, not a pass.
        print("ERROR: no gates selected.", file=sys.stderr)
        if args.gates:
            unknown = [g for g in args.gates if g not in all_gates]
            if unknown:
                print("  unknown gate name(s): %s" % ", ".join(unknown[:10]),
                      file=sys.stderr)
        if args.gap_only or args.port_only or args.legacy_only:
            print("  the %s filter matched none of the %d candidate gate(s)"
                  % (("--gap-only" if args.gap_only else
                      "--port-only" if args.port_only else "--legacy-only"),
                     len(args.gates) if args.gates else len(all_gates)),
                  file=sys.stderr)
        print("  check the name/filter combination, or run --list via wc_run.py",
              file=sys.stderr)
        return 2

    if args.dry_run:
        print(f"Would run {len(selected)} gates")
        for g in selected[:10]:
            print(f"  {g}")
        return 0

    # Same interpreter check wc_run.main() does. Without it this runner emits
    # the cryptic per-gate "No module named 'yaml'" instead of one clear error.
    try:
        import wc_run as _wc_run
        _wc_run._check_interpreter_deps()
    except RuntimeError as e:
        print(str(e), file=sys.stderr)
        return 2
    except ImportError:
        pass  # wc_run not importable yet; run_single_gate will report it

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
