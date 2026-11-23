#!/usr/bin/env python3
"""Differential oracle for agent/usage_gauges.py (gate w6_usagegauges_1dd126)."""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)
SRC = QUARRY + "/agent/usage_gauges.py"
NAME = "w6_usagegauges_live"
spec = importlib.util.spec_from_file_location(NAME, SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live          # dataclass resolution needs this
spec.loader.exec_module(live)


def gd(g):
    return {"id": g.id, "scope": g.scope, "label": g.label, "used_bp": g.used_bp,
            "resets_at_ms": g.resets_at_ms, "full": g.full,
            "used_fraction": g.used_fraction}


def mk(r):
    return live.WindowGauge(id=r.get("id", ""), scope="all", label=r.get("label", ""), used_bp=0)


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    got = r.get("out")
    if func == "paused_for_payment":
        want = live.paused_for_payment(r.get("reason"))
    elif func == "parse_gauges":
        pg = live.parse_gauges(r.get("value"))
        want = [gd(g) for g in pg] if pg else None
    elif func in ("gauge_title", "gauge_limit_name", "gauge_short"):
        g = mk(r)
        want = {"gauge_title": live.gauge_title,
                "gauge_limit_name": live.gauge_limit_name,
                "gauge_short": live.gauge_short}[func](g)
    elif func in ("all_scope", "top_tier_gauge", "binding_gauge"):
        pg = live.parse_gauges(r.get("value")) or ()
        if func == "all_scope":
            want = [gd(g) for g in live.all_scope(pg)]
        elif func == "top_tier_gauge":
            g = live.top_tier_gauge(pg)
            want = gd(g) if g is not None else None
        else:
            g = live.binding_gauge(pg)
            want = gd(g) if g is not None else None
    else:
        continue
    cases += 1
    if want != got:
        mism += 1
        print(f"MISMATCH {func} in={r} py={want!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w6_usagegauges_1dd126: {cases} cases, {mism} mismatches")
sys.exit(1 if mism or cases == 0 else 0)
