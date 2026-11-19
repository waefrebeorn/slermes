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

Usage: python scripts/fuzz_flow_oracle.py <name> [--per-fn N] [--seed S]
Requires: .testbin/harnesses/<name>.exe (run the oracle once via
run_one_oracle.sh to build it) and tests/fixtures/<name>.jsonl (or
tests/<name>_cases.jsonl).
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


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("name")
    ap.add_argument("--per-fn", type=int, default=40)
    ap.add_argument("--seed", type=int, default=1234)
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
    fuzz_fx = REPO / f".testbin/fuzz_{a.name}.jsonl"
    fuzz_fx.write_text("\n".join(json.dumps(c, ensure_ascii=False) for c in cases) + "\n",
                       encoding="utf-8")

    exe = REPO / f".testbin/harnesses/{a.name}.exe"
    if not exe.exists():
        print(f"no cached harness {exe}; run the oracle gate once first")
        return 2
    h = subprocess.run([str(exe)], stdin=fuzz_fx.open("rb"),
                       capture_output=True, timeout=600)
    if h.returncode != 0:
        print(f"HARNESS CRASH rc={h.returncode} on fuzz corpus — itself a finding")
        (REPO / f".testbin/fuzz_{a.name}_harness_err.txt").write_bytes(h.stderr)
        # bisect: find first crashing prefix
        lines = fuzz_fx.read_text(encoding="utf-8").splitlines()
        lo, hi = 1, len(lines)
        while lo < hi:
            mid = (lo + hi) // 2
            p = subprocess.run([str(exe)], input="\n".join(lines[:mid]).encode(),
                               capture_output=True, timeout=300)
            if p.returncode == 0:
                lo = mid + 1
            else:
                hi = mid
        print(f"first crashing prefix: {lo} cases; case {lo}: {lines[lo-1][:200]}")
        return 1
    hout = REPO / f".testbin/fuzz_{a.name}_out.jsonl"
    hout.write_bytes(h.stdout)

    o = subprocess.run([PY, str(REPO / f"tests/oracle_{a.name}.py")],
                       stdin=hout.open("rb"), capture_output=True, text=True,
                       timeout=1200)
    tail = o.stdout.strip().splitlines()[-1] if o.stdout.strip() else "?"
    print("oracle:", tail)
    mism = [l for l in o.stdout.splitlines() if l.startswith("MISMATCH")]
    per = defaultdict(int)
    for l in mism:
        per[l.split()[1]] += 1
    print(f"fuzz cases: {len(cases)} | divergences: {len(mism)}")
    for fn, n in sorted(per.items(), key=lambda kv: -kv[1]):
        print(f"  {n:4d} {fn}")
    if mism:
        div = REPO / f".testbin/fuzz_{a.name}_divergences.txt"
        div.write_text(o.stdout, encoding="utf-8")
        print("divergence detail:", div)
    return 0


if __name__ == "__main__":
    sys.exit(main())
