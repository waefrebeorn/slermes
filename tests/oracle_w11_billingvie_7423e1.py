#!/usr/bin/env python3
"""Differential oracle for tui_gateway/billing_view.py (gate w11_billingvie_7423e1)."""
import importlib.util
import json
import sys
import types
from decimal import Decimal

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)

# Parent-package bootstrap: billing_view uses `from .method_ctx import bind_module`.
pkg = types.ModuleType("tui_gateway")
pkg.__path__ = []
sys.modules["tui_gateway"] = pkg
method_ctx = types.ModuleType("tui_gateway.method_ctx")
method_ctx.bind_module = lambda *a, **k: None
sys.modules["tui_gateway.method_ctx"] = method_ctx

spec = importlib.util.spec_from_file_location("tui_gateway.billing_view", QUARRY + "/tui_gateway/billing_view.py")
live = importlib.util.module_from_spec(spec)
sys.modules["tui_gateway.billing_view"] = live
spec.loader.exec_module(live)

# Real money formatter used by _serialize_auto_reload in production.
bs = importlib.util.spec_from_file_location("w11_bill_money", QUARRY + "/agent/billing_view.py")
bview = importlib.util.module_from_spec(bs)
sys.modules["w11_bill_money"] = bview
bs.loader.exec_module(bview)
format_money = bview.format_money


def ns(obj):
    """Recursively turn fixture dicts into attribute-bearing objects."""
    if isinstance(obj, dict):
        return types.SimpleNamespace(**{k: ns(v) for k, v in obj.items()})
    return obj


def dec_fields(obj, keys):
    """Decimal-coerce only known money fields; other strings stay strings."""
    if not isinstance(obj, dict):
        return obj
    out = {}
    for k, v in obj.items():
        if k in keys and isinstance(v, str):
            try:
                out[k] = Decimal(v)
            except Exception:
                out[k] = v
        else:
            out[k] = v
    return out


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
    try:
        if func == "wire":
            want = live._wire_str(r.get("value"))
        elif func == "pick":
            want = live._billing_pick(r.get("result", {}), **r.get("fields", {}))
        elif func == "pm":
            p = r.get("pm")
            want = live._serialize_payment_method(ns(p) if p is not None else None)
        elif func == "preview":
            want = live._serialize_subscription_preview(
                ns(dec_fields(r.get("p", {}), ("monthly_credits_delta",))))
        elif func == "auto_reload":
            ar = r.get("ar")
            want = live._serialize_auto_reload(
                ns(dec_fields(ar, ("threshold_usd", "reload_to_usd"))) if ar is not None else None,
                format_money=format_money)
        else:
            continue
    except BaseException as ex:
        print("MISMATCH %s oracle raised %s: %s" % (func, type(ex).__name__, ex), file=sys.stderr)
        cases += 1
        mism += 1
        continue
    cases += 1
    if want != got:
        mism += 1
        print("MISMATCH in=%s py=%r c=%r" % (json.dumps(r, ensure_ascii=False), want, got), file=sys.stderr)
print("ORACLE w11_billingvie_7423e1: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or cases == 0 else 0)
