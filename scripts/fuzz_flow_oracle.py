#!/usr/bin/env python3
"""Randomized differential fuzzing of flow oracles (recursive audit level 2).

Research grounding: WFDB-x conformance porting + DIFUZZI — hand-written
fixtures are a ceiling; random inputs within the documented domain find
divergences nobody wrote cases for. Every divergence becomes a new
conformance case (after triage: port bug / oracle modeling / out-of-domain).

Method: mutation-based corpus fuzzing. Existing fixture cases are templates;
leaf values are mutated type-preservingly (ASCII strings, integral numbers,
bools, nulls, list/dict recursion, seam-script rets). The C harness and the
live-Python oracle consume the SAME generated cases; mismatches are saved.

Timeout discipline (platforms_helpers_flow once hung the oracle 1200s on 540
cases): the corpus runs in BATCHES with a fast path (whole batch through
harness+oracle, bounded by --case-timeout * batch_size + startup). Any batch
that crashes, hangs, or mismatches falls back to a PER-CASE scan with a short
single-case budget, so one infinite-loop input costs seconds, not the whole
run. Findings: MISMATCH / PYERR (divergences), HARNESS_CRASH / HARNESS_HANG
(port-side faults), ORACLE_HANG (live-Python infinite loop = out-of-domain
input, not a port bug), ORACLE_ERR. Mismatches are attributed to exact inputs
by single-case harness+oracle re-runs (ground truth, never a shadow mirror).
Findings append incrementally to .testbin/fuzz_<name>_findings.jsonl.

Usage: python scripts/fuzz_flow_oracle.py <name> [--per-fn N] [--seed S]
       [--batch-size N] [--case-timeout SEC] [--max-cases N]
Requires: .testbin/harnesses/<name>.exe (falls back to newest
.testbin/tt_<name>_*.exe) and tests/fixtures/<name>.jsonl (or
tests/<name>_cases.jsonl). Run the oracle gate once to build the harness.
"""
import argparse
import json
import random
import string
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
PY = "C:/Users/eman5/hermes-agent/.venv/Scripts/python.exe"
ASCII = string.ascii_letters + string.digits + " _-./:+=@#"
STARTUP = 10  # seconds of slack per oracle/harness invocation


def rand_str(rng, like):
    n = max(0, len(like) + rng.randint(-3, 6))
    if like and all(ord(c) < 128 for c in like):
        return "".join(rng.choice(ASCII) for _ in range(n))
    return like  # non-ASCII domain: keep


def mutate(rng, v, depth=0):
    if depth > 6:
        return v
    if isinstance(v, str):
        return rand_str(rng, v)
    if isinstance(v, bool):
        return rng.choice([True, False, v])
    if isinstance(v, int):
        return v + rng.randint(-4, 8)
    if isinstance(v, float):
        return v  # integral-float identity is out of domain; keep
    if v is None:
        return rng.choice([None, None, "", 0])
    if isinstance(v, list):
        if not v:
            return v
        out = [mutate(rng, x, depth + 1) for x in v]
        if rng.random() < 0.15 and out:
            out.pop(rng.randrange(len(out)))
        return out
    if isinstance(v, dict):
        out = {}
        for k, x in v.items():
            if k in ("__raise__",):
                out[k] = x  # keep exception class names intact
            elif k == "msg":
                out[k] = rand_str(rng, x) if isinstance(x, str) else x
            else:
                out[k] = mutate(rng, x, depth + 1)
        return out
    return v


class Runner:
    def __init__(self, exe, oracle, case_timeout):
        self.exe = exe
        self.oracle = oracle
        self.ct = case_timeout

    def harness(self, lines):
        t = STARTUP + self.ct * max(1, len(lines))
        return subprocess.run([str(self.exe)],
                              input="\n".join(lines).encode("utf-8") + b"\n",
                              capture_output=True, timeout=t)

    def oracle_run(self, hout_bytes, n):
        t = STARTUP + self.ct * max(1, n)
        # input MUST be str in text mode: bytes kill the stdin writer thread
        # (TypeError) and the oracle then blocks on stdin forever.
        return subprocess.run([PY, str(self.oracle)],
                              input=hout_bytes.decode("utf-8", "replace"),
                              capture_output=True, timeout=t,
                              encoding="utf-8", errors="replace")


def classify_single(runner, line):
    """Run one case alone; return (kind, detail) — kind in
    OK / MISMATCH / PYERR / HARNESS_CRASH / HARNESS_HANG / ORACLE_HANG / ORACLE_ERR."""
    try:
        h = runner.harness([line])
    except subprocess.TimeoutExpired:
        return "HARNESS_HANG", ""
    if h.returncode != 0:
        return "HARNESS_CRASH", f"rc={h.returncode} {h.stderr.decode('utf-8', 'replace')[:300]}"
    try:
        o = runner.oracle_run(h.stdout, 1)
    except subprocess.TimeoutExpired:
        return "ORACLE_HANG", ""
    if o.returncode not in (0, 1):
        return "ORACLE_ERR", f"rc={o.returncode} {o.stdout[-300:]} {o.stderr[-300:]}"
    blocks = []
    cur = None
    for l in o.stdout.splitlines():
        if l.startswith("MISMATCH") or l.startswith("PYERR"):
            cur = [l]
            blocks.append(cur)
        elif cur is not None and l.startswith("  "):
            cur.append(l)
    if blocks:
        kind = "MISMATCH" if blocks[0][0].startswith("MISMATCH") else "PYERR"
        return kind, "\n".join("\n".join(b) for b in blocks)
    return "OK", ""


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("name")
    ap.add_argument("--per-fn", type=int, default=40)
    ap.add_argument("--seed", type=int, default=1234)
    ap.add_argument("--batch-size", type=int, default=25)
    ap.add_argument("--case-timeout", type=float, default=2.0,
                    help="per-case seconds budget for each side (default 2)")
    ap.add_argument("--max-cases", type=int, default=0,
                    help="cap total corpus (0 = no cap); for slow oracles")
    a = ap.parse_args()
    rng = random.Random(a.seed)

    fx = REPO / f"tests/fixtures/{a.name}.jsonl"
    if not fx.exists():
        fx = REPO / f"tests/{a.name}_cases.jsonl"
    templates = defaultdict(list)
    for line in fx.read_text(encoding="utf-8").splitlines():
        if line.strip():
            r = json.loads(line)
            templates[r.get("func") or r.get("fn")].append(r)

    cases = []
    for fn, ts in templates.items():
        for i in range(a.per_fn):
            t = ts[i % len(ts)]
            c = json.loads(json.dumps(t))
            key = "in" if "in" in c else None
            if key:
                c[key] = mutate(rng, c[key])
            cases.append(c)
    if a.max_cases and len(cases) > a.max_cases:
        cases = cases[:a.max_cases]
    fuzz_fx = REPO / f".testbin/fuzz_{a.name}.jsonl"
    fuzz_fx.write_text("\n".join(json.dumps(c, ensure_ascii=False) for c in cases) + "\n",
                       encoding="utf-8")

    exe = REPO / f".testbin/harnesses/{a.name}.exe"
    if not exe.exists():
        tts = sorted(REPO.glob(f".testbin/tt_{a.name}_*.exe"),
                     key=lambda p: p.stat().st_mtime)
        if tts:
            exe = tts[-1]
        else:
            print(f"no cached harness for {a.name}; run the oracle gate once first")
            return 2
    runner = Runner(exe, REPO / f"tests/oracle_{a.name}.py", a.case_timeout)

    findings = []   # {kind, case, detail}
    n_ok = 0
    inc = REPO / f".testbin/fuzz_{a.name}_findings.jsonl"
    inc.write_text("", encoding="utf-8")

    def record(kind, case, detail):
        nonlocal n_ok
        if kind == "OK":
            n_ok += 1
            return
        f = {"kind": kind, "case": case, "detail": detail}
        findings.append(f)
        with inc.open("a", encoding="utf-8") as fh:
            fh.write(json.dumps(f, ensure_ascii=False) + "\n")

    def scan(lines):
        for line in lines:
            kind, detail = classify_single(runner, line)
            record(kind, json.loads(line), detail)

    def process(lines):
        nonlocal n_ok
        if not lines:
            return
        try:
            h = runner.harness(lines)
            if h.returncode == 0:
                o = runner.oracle_run(h.stdout, len(lines))
                if o.returncode in (0, 1):
                    if "MISMATCH" in o.stdout or "PYERR" in o.stdout:
                        scan(lines)  # attribute to exact inputs
                    else:
                        n_ok += len(lines)
                    return
        except subprocess.TimeoutExpired:
            pass
        scan(lines)  # crash / hang / oracle error: per-case isolation

    lines = fuzz_fx.read_text(encoding="utf-8").splitlines()
    nb = (len(lines) + a.batch_size - 1) // a.batch_size
    for bi, s in enumerate(range(0, len(lines), a.batch_size), 1):
        process(lines[s:s + a.batch_size])
        print(f"[{a.name} batch {bi}/{nb}] clean={n_ok} findings={len(findings)}",
              file=sys.stderr, flush=True)

    mism = [f for f in findings if f["kind"] in ("MISMATCH", "PYERR")]
    per = defaultdict(int)
    for f in mism:
        per[(f["case"] or {}).get("func", "?")] += 1
    print(f"fuzz cases: {len(cases)} | clean: {n_ok} | divergences: {len(mism)}")
    for fn, n in sorted(per.items(), key=lambda kv: -kv[1]):
        print(f"  {n:4d} {fn}")
    other = [f for f in findings if f["kind"] not in ("MISMATCH", "PYERR")]
    if other:
        print(f"anomalies: {len(other)}")
        for f in other:
            c = f["case"]
            print(f"  {f['kind']}: {json.dumps(c, ensure_ascii=False)[:160] if c else f['detail']}")
    if findings:
        div = REPO / f".testbin/fuzz_{a.name}_divergences.txt"
        with div.open("w", encoding="utf-8") as fh:
            for f in findings:
                fh.write(f"=== {f['kind']} ===\n")
                fh.write(json.dumps(f["case"], ensure_ascii=False) + "\n")
                fh.write(f["detail"] + "\n\n")
        dij = REPO / f".testbin/fuzz_{a.name}_div_inputs.json"
        dij.write_text(json.dumps(findings, ensure_ascii=False, indent=1), encoding="utf-8")
        print("divergence detail:", div)
        print("divergent inputs: ", dij)
    return 0


if __name__ == "__main__":
    sys.exit(main())
