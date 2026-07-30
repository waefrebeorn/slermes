#!/usr/bin/env python3
"""Faithfulness oracle for port_learning_graph_render_helpers.c (v544).

Reads JSON lines emitted by t_port_learning_graph_render_helpers.c:
  {"fn":<name>, "in":<args>, "out":<c_result>}

Recomputes the SAME function from the LIVE agent/learning_graph_render.py
for the given args and compares the C string output exactly against the
Python string we produce with identical formatting.

Mapping of C formatting -> Python formatting:
  _clamp / _smoothstep : C emits %.10g of a double; we recompute the float
      and compare with tolerance, then format both with %.10g.
  format_date          : C emits a date string or "unknown"; Python returns a
      string -> compare exactly.
  _rgb_to_hsl          : C emits "%.2f,%.4f,%.4f"; recompute h,s,l via the
      live function and format identically.
  _hsl_to_rgb          : C emits "r,g,b" ints; recompute via live function,
      format as "%d,%d,%d".
  _complementary_ink   : same as _hsl_to_rgb (it returns an int triple).
"""
import json, sys, math

sys.path.insert(0, "/home/wubu/hermes-agent-dev")
from agent.learning_graph_render import (
    _clamp, _smoothstep, format_date,
    _rgb_to_hsl, _hsl_to_rgb, _complementary_ink,
    _to_ts, _period_key, _period_label, _node_score, _node_meta,
)

ok = True
n = 0
mismatch = 0

def fmt_float(x):
    return "%.10g" % x

for line in sys.stdin:
    line = line.strip()
    if not line or not line.startswith("{"):
        continue
    obj = json.loads(line)
    fn = obj["fn"]
    args = obj["in"]
    got = obj["out"]
    exp = None
    try:
        if fn == "_clamp":
            a = args if isinstance(args, list) else [args]
            exp = fmt_float(_clamp(*a))
            exp = float(exp); got = float(got)   # numeric compare
        elif fn == "_smoothstep":
            a = args if isinstance(args, list) else [args]
            exp = fmt_float(_smoothstep(*a))
            exp = float(exp); got = float(got)   # numeric compare
        elif fn == "format_date":
            if args == ["nan"]:
                ts = float("nan")
            elif args == ["inf"]:
                ts = float("inf")
            else:
                ts = float(args[0])
            exp = format_date(ts)                # string compare
        elif fn == "_rgb_to_hsl":
            r, g, b = args
            h, s, l = _rgb_to_hsl((r, g, b))
            exp = "%.2f,%.4f,%.4f" % (h, s, l)   # string compare
        elif fn == "_hsl_to_rgb":
            h, s, l = args
            rr, gg, bb = _hsl_to_rgb(h, s, l)
            exp = "%d,%d,%d" % (rr, gg, bb)       # string compare
        elif fn == "_complementary_ink":
            r, g, b = args
            rr, gg, bb = _complementary_ink((r, g, b))
            exp = "%d,%d,%d" % (rr, gg, bb)       # string compare
        elif fn == "_to_ts":
            ts = float(args[0])
            res = _to_ts(ts)
            ok_flag = 1 if res is not None else 0
            out_val = res if res is not None else 0.0
            exp = "%d,%.6g" % (ok_flag, out_val)  # string compare (matches C "ok,out")
        elif fn == "_period_key":
            ts, gran = args
            exp = ",".join(str(x) for x in _period_key(float(ts), gran))
        elif fn == "_period_label":
            ts, gran = args
            exp = _period_label(float(ts), gran)
        elif fn == "_node_score":
            node = args[0]; rec = float(args[1])   # args[0] already a parsed dict
            exp = float("%.10g" % _node_score(node, rec))
            got = float(got)   # numeric compare
        elif fn == "_node_meta":
            node = args[0]     # args[0] already a parsed dict
            exp = _node_meta(node)
        else:
            print("UNKNOWN FN", fn); continue
    except Exception as e:
        print("ORACLE ERROR", fn, repr(args), e)
        ok = False
        mismatch += 1
        continue
    n += 1
    if exp != got:
        ok = False
        mismatch += 1
        print("MISMATCH", fn, "in=", args, "PY=", repr(exp), "C=", repr(got))

print("PYCOMPARE", "OK" if ok else "BAD", f"({n} cases, {mismatch} mismatches)")
sys.exit(0 if ok else 1)
