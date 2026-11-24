#!/usr/bin/env python3
"""Differential oracle for agent/prompt_cache_lifecycle/economics.py (w20_economics_90c7dd).

The live module is imported straight from the quarry and every harness line is
replayed through ``evaluate_economics`` / ``_per_token``. Floats cross the wire
as "%.17g" strings on BOTH sides: Python's json.dumps would print repr() while
libjson prints the C format, and the C format is the stricter of the two.
"""
import importlib.util
import json
import math
import os
import sys
import types
from decimal import Decimal
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

NAME = "w20_economics_90c7dd_live"
sp = importlib.util.spec_from_file_location(
    NAME, os.path.join(Q, "agent", "prompt_cache_lifecycle", "economics.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)

RATE_KEYS = ("input_cost_per_million", "output_cost_per_million",
             "cache_read_cost_per_million", "cache_write_cost_per_million")


def fmt(v):
    """The harness's wire format for one double."""
    return "%.17g" % float(v)


def pricing_view(spec):
    """PricingEntry-shaped object: Decimal rates, optional per-request fee."""
    if not isinstance(spec, dict):
        return None
    attrs = {}
    for key in RATE_KEYS:
        val = spec.get(key)
        attrs[key] = None if val is None else Decimal(str(float(val)))
    if "request_cost" in spec:
        raw = spec["request_cost"]
        attrs["request_cost"] = None if raw is None else Decimal(str(float(raw)))
    return types.SimpleNamespace(**attrs)


def expected(r):
    f = r.get("func")
    if f == "per_token":
        raw = r.get("rate")
        rate = None if raw is None else Decimal(str(float(raw)))
        return fmt(live._per_token(rate))
    if f == "evaluate":
        d = live.evaluate_economics(
            pricing_view(r.get("pricing")),
            int(r.get("cached_tokens", 0) or 0),
            int(r.get("uncached_tokens", 0) or 0),
            spent_usd=float(r.get("spent_usd") or 0.0),
            observed_warm_usd=float(r.get("observed_warm_usd") or 0.0),
            output_tokens=int(r.get("output_tokens", 1) if r.get("output_tokens") is not None else 1),
        )
        return {
            "warm_cost": fmt(d.warm_cost),
            "miss_cost": fmt(d.miss_cost),
            "expected_savings": fmt(d.expected_savings),
            "economics_available": bool(d.economics_available),
            "action": d.action,
            "within_miss": bool(d.within_miss),
            "stop_reason": d.stop_reason,
        }
    if f == "stop_reason":
        d = live.CacheWarmingDecision(
            warm_cost=0.0, miss_cost=0.0, expected_savings=0.0,
            economics_available=bool(r.get("economics_available", False)),
            action="stop", within_miss=bool(r.get("within_miss", True)))
        return d.stop_reason
    raise AssertionError("unknown func %r" % (f,))


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if not isinstance(r, dict) or r.get("func") is None:
        continue
    cases += 1
    try:
        exp = expected(r)
    except Exception as exc:  # noqa: BLE001 - a live crash is a fixture bug, report it
        mism += 1
        print("MISMATCH %s case=%s live-error=%r" % (r.get("func"), r.get("case"), exc),
              file=sys.stderr)
        continue
    got = r.get("out")
    if isinstance(exp, float) and isinstance(got, float):
        same = math.isclose(exp, got, rel_tol=0, abs_tol=0)
    else:
        same = exp == got
    if not same:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), exp, got),
              file=sys.stderr)
print("ORACLE w20_economics_90c7dd: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
