#!/usr/bin/env python3
"""Differential oracle for tools/feishu_lark.py (gate w6_feishulark_1501ff).

Loads the LIVE module by path (it imports only contextvars/json at load time).
The fixture injects the response shape: `raw` (object with optional `content`)
stands in for response.raw, `data` for response.data, and `data_vars` for the
__dict__ of a non-dict response.data object.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

SRC = Q + "/tools/feishu_lark.py"
spec = importlib.util.spec_from_file_location("w6_feishulark_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w6_feishulark_live"] = live
spec.loader.exec_module(live)


def _canon(x):
    if x is None: return ("null",)
    if isinstance(x, bool): return ("bool", x)
    if isinstance(x, (int, float)): return ("num", float(x))
    if isinstance(x, str): return ("str", x)
    if isinstance(x, (list, tuple)): return ("list", [_canon(v) for v in x])
    if isinstance(x, dict): return ("obj", sorted((k, _canon(v)) for k, v in x.items()))
    return ("other", repr(x))


def make_response(r):
    resp = types.SimpleNamespace()
    if isinstance(r.get("raw"), dict):
        raw = types.SimpleNamespace()
        if "content" in r["raw"]:
            raw.content = r["raw"]["content"]
        resp.raw = raw
    typed = r.get("data")
    dv = r.get("data_vars")
    if isinstance(typed, dict):
        resp.data = typed
    elif typed and isinstance(dv, dict):
        model = types.SimpleNamespace()
        model.__dict__.update(dv)
        resp.data = model
    else:
        resp.data = typed
    return resp


def compute(r):
    resp = make_response(r)
    if r.get("func") == "raw_body":
        return live.raw_body(resp)
    if r.get("func") == "response_data":
        return live.response_data(resp)
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        want = compute(r)
    except Exception as exc:
        mism += 1
        print("LIVE ERROR %s case=%s: %r" % (r.get("func"), r.get("case"), exc), file=sys.stderr)
        continue
    got = r.get("out")
    if _canon(want) != _canon(got):
        mism += 1
        print("MISMATCH %s case=%s C=%r PY=%r" % (r.get("func"), r.get("case"), got, want),
              file=sys.stderr)
print("ORACLE w6_feishulark_1501ff: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
