#!/usr/bin/env python3
"""Differential oracle for agent/cache_bounds.py (gate w6_cachebounds_876475).

Loads the LIVE module by path (it imports only typing). Each case replays the
mutating dict helpers on fresh dict copies and compares the resulting tables
to the C port's output.
"""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

SRC = Q + "/agent/cache_bounds.py"
spec = importlib.util.spec_from_file_location("w6_cachebounds_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w6_cachebounds_live"] = live
spec.loader.exec_module(live)


def _canon(x):
    if x is None: return ("null",)
    if isinstance(x, bool): return ("bool", x)
    if isinstance(x, (int, float)): return ("num", float(x))
    if isinstance(x, str): return ("str", x)
    if isinstance(x, (list, tuple)): return ("list", [_canon(v) for v in x])
    if isinstance(x, dict): return ("obj", sorted((k, _canon(v)) for k, v in x.items()))
    return ("other", repr(x))


def _stamp(v):
    return v.get("stamp", 0) if isinstance(v, dict) else v


def compute(r):
    f = r.get("func")
    if f == "drop_expired":
        values = dict(r.get("values") or {})
        stamps = dict(r.get("stamps") or {})
        live.drop_expired(values, stamps, r.get("ttl", 0), r.get("now", 0))
        return {"values": values, "stamps": stamps}
    if f == "evict_oldest":
        cache = dict(r.get("cache") or {})
        sib = [dict(s) for s in (r.get("siblings") or [])]
        live.evict_oldest(cache, r.get("key", ""), _stamp,
                          int(r.get("limit", 0)), *sib)
        return {"cache": cache, "siblings": sib}
    if f == "publish_lru":
        table = dict(r.get("table") or {})
        sib = [dict(s) for s in (r.get("siblings") or [])]
        evicted = live.publish_lru(table, r.get("key", ""), r.get("value"),
                                   int(r.get("limit", 0)), *sib)
        return {"evicted": evicted, "table": table, "siblings": sib}
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
print("ORACLE w6_cachebounds_876475: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
