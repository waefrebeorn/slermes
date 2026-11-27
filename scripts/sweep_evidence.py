"""Run a gate list through water-cup and record what each one actually proves.

The evidence ratchet counts gates that COULD discriminate. This sweep answers the
only question that matters: of the gates the ratchet admits, how many run, match, and
vary with their fixtures?

Per gate the raw output is parsed into one of:

    MATCH           every cup matched live Python
    NON-DISC        one cup for every input -- the fixtures do not vary the answer
    MISMATCH        at least one cup differed (a real divergence, the useful kind)
    TWIN-BLIND      C varies, the twin is constant (or the reverse)
    ERROR           the harness or twin failed to run

Usage:
    python scripts/sweep_evidence.py --list .testbin/evidence_candidates.txt \
        --log .testbin/ev_sweep.log --out .testbin/ev_sweep.json [--chunk 60]
"""
from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
HEAD = re.compile(r"^-- (\S+) \(subsystem: [^)]*\) --")


def classify(block: str) -> str:
    if "NON-DISCRIMINATING" in block:
        return "NON-DISC"
    if "TWIN BLIND" in block:
        return "TWIN-BLIND"
    if "all " in block and "MATCH LIVE Python" in block:
        return "MATCH"
    if "cup mismatches" in block or "CUP MISMATCH" in block:
        return "MISMATCH"
    if "ERROR" in block:
        return "ERROR"
    return "UNKNOWN"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--list", required=True)
    ap.add_argument("--log", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--chunk", type=int, default=60)
    ap.add_argument("--timeout", type=int, default=90, help="seconds per gate")
    ap.add_argument("--link", default="full")
    a = ap.parse_args()

    gates = [ln.strip() for ln in Path(a.list).read_text().splitlines() if ln.strip()]
    env = dict(os.environ)
    env["WATERCUP_LINK"] = a.link
    log = Path(a.log)
    results: dict[str, str] = {}
    if Path(a.out).exists():
        results = json.loads(Path(a.out).read_text())
    todo = [g for g in gates if g not in results]
    print(f"{len(gates)} gates, {len(todo)} still to run", flush=True)

    with log.open("a", encoding="utf-8") as fh:
        # One gate per process: a single gate that blocks on a wait (a gateway
        # response loop, a stdin prompt) hung a whole 50-gate chunk with no output.
        # Batch first (one interpreter per 25 gates is ~30x faster), then fall back to
        # one gate per process for any batch that hangs, so a blocking gate costs only
        # itself. Text decoding is utf-8 with replacement: cp1252 choked on the cp
        # output and killed the previous sweep after two gates.
        def run_once(names: list[str], timeout: int) -> tuple[str, bool]:
            try:
                proc = subprocess.run(
                    [sys.executable, "-u", "tests/watercup/wc_run.py", *names],
                    cwd=str(REPO), env=env, capture_output=True,
                    encoding="utf-8", errors="replace", timeout=timeout,
                )
                return (proc.stdout or "") + (proc.stderr or ""), True
            except subprocess.TimeoutExpired:
                return "", False

        for i in range(0, len(todo), a.chunk):
            batch = todo[i:i + a.chunk]
            out, ok = run_once(batch, a.timeout * len(batch))
            if ok:
                fh.write(out)
                for b in re.split(r"(?m)^(?=-- \S+ \(subsystem: )", out):
                    m = HEAD.match(b)
                    if m:
                        results[m.group(1)] = classify(b)
            else:
                for gate in batch:
                    one, ok1 = run_once([gate], a.timeout)
                    fh.write(one)
                    results[gate] = classify(one) if ok1 else "TIMEOUT"
            fh.flush()
            Path(a.out).write_text(json.dumps(results, indent=1, sort_keys=True))
            print(f"  {len(results)}/{len(gates)} done", flush=True)

    counts: dict[str, int] = {}
    for v in results.values():
        counts[v] = counts.get(v, 0) + 1
    for k, v in sorted(counts.items(), key=lambda kv: -kv[1]):
        print(f"{v:6d}  {k}")
    print(f"{len(results):6d}  recorded")
    return 0


if __name__ == "__main__":
    sys.exit(main())
