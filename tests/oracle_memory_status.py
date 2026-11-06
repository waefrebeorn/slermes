#!/usr/bin/env python3
"""Oracle for gateway/memory_status.py pure read-side core.

Harness families (memory_status):
  nonneg   {family,case,in:<json>,ret:int,mb:int}  -> _nonneg_int(in), _mb(in)
             (ret/mb are -1 when the live returns None)
  classify {family,case,a:<json>,t:<json>,out:str} -> classify_pressure(a,t)
  parse    {family,case,in:<ts str>,epoch:float}   -> _parse_iso(in) epoch-UTC, -1 when absent
The `collect` family (col_*: ms_collect_memory_status) is NOT oracleable from
this harness: it emits {family,case,out} with no hb/sn input, so there is no
input to replay the live collect_memory_status on. That family is left to a
harness rewrite that emits the heartbeat+sentinel JSON.

Seeds: datetime.fromisoformat + UTC (from datetime.timezone.utc), and the
tier/threshold constants (_PRESSURE_TIERS, _CRITICAL/_ELEVATED kib+frac,
_HEARTBEAT_FRESH_TTL_S not needed here).
"""
import ast
import json
import sys
from datetime import datetime, timezone

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/gateway/memory_status.py"


def main() -> int:
    src = open(SRC, encoding="utf-8").read()
    ns = {"datetime": datetime, "UTC": timezone.utc, "Path": __import__("pathlib").Path}
    for n in ast.parse(src).body:
        if isinstance(n, ast.Assign) and any(
            isinstance(t, ast.Name) and t.id in (
                "_CRITICAL_AVAILABLE_KIB", "_CRITICAL_AVAILABLE_FRACTION",
                "_ELEVATED_AVAILABLE_KIB", "_ELEVATED_AVAILABLE_FRACTION",
                "_HEARTBEAT_FRESH_TTL_S")
            for t in n.targets):
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
        elif isinstance(n, ast.Assign) and any(
            isinstance(t, ast.Name) and t.id == "_PRESSURE_TIERS" for t in n.targets):
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
        elif isinstance(n, ast.FunctionDef) and n.name in (
                "_nonneg_int", "_mb", "_parse_iso", "classify_pressure"):
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
    nonneg = ns["_nonneg_int"]
    mb = ns["_mb"]
    parse_iso = ns["_parse_iso"]
    classify = ns["classify_pressure"]

    def epoch(value):
        d = parse_iso(value)
        if d is None:
            return -1.0
        return d.timestamp()

    cases = []
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            cases.append(json.loads(line))
        except Exception:
            pass
    if not cases:
        print("ORACLE memory_status: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    total = 0
    for c in cases:
        fam = c.get("family")
        if fam == "collect":
            continue  # no input emitted; not oracleable here
        total += 1
        if fam == "nonneg":
            v = c.get("in")
            want_r = nonneg(v)
            want_m = mb(v)
            have_r = c.get("ret")
            have_m = c.get("mb")
            if (want_r is None and have_r != -1) or \
               (want_r is not None and have_r != want_r) or \
               (want_m is None and have_m != -1) or \
               (want_m is not None and have_m != want_m):
                mism += 1
                print(f"MISMATCH nonneg {c.get('case')} in={v!r} "
                      f"C=({have_r},{have_m}) PY=({want_r},{want_m})")
        elif fam == "classify":
            want = classify(c.get("a"), c.get("t"))
            if c.get("out") != want:
                mism += 1
                print(f"MISMATCH classify {c.get('case')} a={c.get('a')!r} t={c.get('t')!r} "
                      f"C={c.get('out')!r} PY={want!r}")
        elif fam == "parse":
            want = epoch(c.get("in"))
            if abs(c.get("epoch") - want) > 1e-3:
                mism += 1
                print(f"MISMATCH parse {c.get('case')} in={c.get('in')!r} "
                      f"C={c.get('epoch'):.17g} PY={want:.17g}")
    print(f"ORACLE memory_status: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
