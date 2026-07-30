#!/usr/bin/env python3
"""
sta_oracle_billing_usage.py — Python oracle for agent/billing_usage.py,
ported in src/agent/port_billing_usage.c.

Imports the REAL agent.billing_usage module. The oracle drives the pure
surface: format_renews(str) and usage_model_from_account(account_info).
The network functions (build_usage_model, _dev_fixture_usage_model) are NOT
ported and are not exercised here.

For usage_model_from_account we build a lightweight stub object exposing the
attributes the port reads (logged_in, paid_service_access_info, subscription,
paid_service_access) so the genuine Python function runs unmodified.

Output contract: one JSON object per line (compact, ensure_ascii=False).
"""
import json
import os
import sys

_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
if _ROOT not in sys.path:
    sys.path.insert(0, _ROOT)

import agent.billing_usage as bu


class _Attr:
    """Object with arbitrary attributes (mimics the portal dataclass)."""
    def __init__(self, **kw):
        for k, v in kw.items():
            setattr(self, k, v)
    def __repr__(self):
        return f"_Attr({self.__dict__})"


def model_to_dict(m):
    plan = m.plan_bar
    topup = m.topup_bar
    return {
        "available": m.available,
        "status": m.status,
        "plan_name": m.plan_name,
        "renews_at": m.renews_at,
        "renews_display": m.renews_display,
        "subscription_remaining_usd": m.subscription_remaining_usd,
        "topup_remaining_usd": m.topup_remaining_usd,
        "total_spendable_usd": m.total_spendable_usd,
        "has_topup": m.has_topup,
        "plan_bar": (
            {"kind": plan.kind, "remaining_usd": plan.remaining_usd,
             "total_usd": plan.total_usd, "spent_usd": plan.spent_usd,
             "pct_used": plan.pct_used, "fill_fraction": plan.fill_fraction}
            if plan else None),
        "topup_bar": (
            {"kind": topup.kind, "remaining_usd": topup.remaining_usd,
             "total_usd": topup.total_usd, "spent_usd": topup.spent_usd,
             "pct_used": topup.pct_used, "fill_fraction": topup.fill_fraction}
            if topup else None),
    }


def emit(obj):
    sys.stdout.write(json.dumps(obj, separators=(",", ":"), ensure_ascii=False) + "\n")


def main():
    if len(sys.argv) < 2:
        sys.stderr.write("usage: sta_oracle_billing_usage.py <cases.in>\n")
        return 2
    with open(sys.argv[1], "r", encoding="utf-8") as fp:
        for raw in fp:
            line = raw.rstrip("\n")
            if not line.strip() or line.startswith("#"):
                continue
            op, _, rest = line.partition(" ")
            rest = rest.strip()
            if op == "renews":
                emit({"op": "renews", "in": rest, "out": bu.format_renews(rest)})
            elif op == "model":
                # model <json>  — JSON-encoded account_info
                info = json.loads(rest)
                # reconstruct nested Attr objects
                access = info.get("paid_service_access_info")
                access = _Attr(**access) if isinstance(access, dict) else None
                sub = info.get("subscription")
                sub = _Attr(**sub) if isinstance(sub, dict) else None
                acc = _Attr(
                    logged_in=info.get("logged_in", False),
                    paid_service_access=info.get("paid_service_access"),
                    paid_service_access_info=access,
                    subscription=sub,
                )
                m = bu.usage_model_from_account(acc)
                emit({"op": "model", "out": model_to_dict(m)})
            else:
                emit({"op": "unknown", "raw": line})
    return 0


if __name__ == "__main__":
    sys.exit(main())
