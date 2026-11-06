#!/usr/bin/env python3
"""Oracle for agent/learning_graph_render.py pure render helpers.

The harness (t_port_learning_graph_render_helpers.c) drives the C port and emits
{fn, in, out} for families that still EXIST in the live module (this oracle's
scope is deliberately those 6; the harness also emits _period_key/_period_label/
_to_ts/_complementary_ink/_node_meta which the live module refactored away, so
they are SKIPPED — see the stale-harness note in the gating commit):

  _clamp      {in:[v,lo,hi], out:num %.10g}
  _smoothstep {in:[p], out:num %.10g}
  format_date {in:[ts], out:"<d> <Mon> <YYYY>" or "unknown"}
  _rgb_to_hsl {in:[r,g,b], out:"%.2f,%.4f,%.4f"}
  _hsl_to_rgb {in:[h,s,l], out:"%d,%d,%d"}
  _node_score {in:[{...}, rec], out:num %.10g}

Ast-extract the LIVE defs + their transitive deps (_is_memory, _HUE_SEXTANTS,
_utc from datetime, UTC) and seed math/safe_strftime so the pure path runs.
"""
import ast
import json
import math
import sys
from datetime import datetime, timezone, UTC

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/agent/learning_graph_render.py"


def safe_strftime(dt, fmt):
    return dt.strftime(fmt)


def main() -> int:
    src = open(SRC, encoding="utf-8").read()
    ns = {"math": math, "safe_strftime": safe_strftime,
          "datetime": datetime, "UTC": UTC}
    for n in ast.parse(src).body:
        if isinstance(n, ast.Assign) and any(
            isinstance(t, ast.Name) and t.id in ("_HUE_SEXTANTS",)
            for t in n.targets):
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
        elif isinstance(n, ast.FunctionDef) and n.name in (
                "_clamp", "_smoothstep", "format_date", "_utc",
                "_rgb_to_hsl", "_hsl_to_rgb", "_is_memory", "_node_score"):
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
    clamp = ns["_clamp"]
    smooth = ns["_smoothstep"]
    fmt_date = ns["format_date"]
    rgb_to_hsl = ns["_rgb_to_hsl"]
    hsl_to_rgb = ns["_hsl_to_rgb"]
    node_score = ns["_node_score"]
    SKIP = {"_period_key", "_period_label", "_to_ts", "_complementary_ink",
            "_node_meta"}  # live module refactored these away -> stale harness

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
        print("ORACLE learning_graph_render_helpers: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    total = 0
    skipped = 0
    for c in cases:
        fn = c.get("fn")
        a = c.get("in")
        got = c.get("out")
        if fn in SKIP:
            skipped += 1
            continue
        total += 1
        if fn == "_clamp":
            want = clamp(a[0], a[1], a[2])
            want = float("%.10g" % want)
            if abs(want - float(got)) > 1e-4:
                mism += 1
                print(f"MISMATCH _clamp in={a} C={got} PY={want}")
        elif fn == "_smoothstep":
            want = float("%.10g" % smooth(a[0]))
            if abs(want - float(got)) > 1e-4:
                mism += 1
                print(f"MISMATCH _smoothstep in={a} C={got} PY={want}")
        elif fn == "format_date":
            want = fmt_date(a[0])
            if (got or "unknown") != want:
                mism += 1
                print(f"MISMATCH format_date in={a} C={got!r} PY={want!r}")
        elif fn == "_rgb_to_hsl":
            h, s, l = rgb_to_hsl((a[0], a[1], a[2]))
            want = "%.2f,%.4f,%.4f" % (h, s, l)
            if got != want:
                mism += 1
                print(f"MISMATCH _rgb_to_hsl in={a} C={got!r} PY={want!r}")
        elif fn == "_hsl_to_rgb":
            rr, gg, bb = hsl_to_rgb(a[0], a[1], a[2])
            want = "%d,%d,%d" % (rr, gg, bb)
            if got != want:
                mism += 1
                print(f"MISMATCH _hsl_to_rgb in={a} C={got!r} PY={want!r}")
        elif fn == "_node_score":
            want = float("%.10g" % node_score(a[0], a[1]))
            if abs(want - float(got)) > 1e-4:
                mism += 1
                print(f"MISMATCH _node_score in={a} C={got} PY={want}")
    print(f"ORACLE learning_graph_render_helpers: {total} cases, {mism} mismatches"
          f" (skipped {skipped} stale-harness families)")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
