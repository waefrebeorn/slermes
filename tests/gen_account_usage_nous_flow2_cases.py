#!/usr/bin/env python3
"""Generate tests/fixtures/account_usage_nous_flow2.jsonl for the aun2 port.

Each line: {"func": <live_name>, "in": {...inputs..., "io": {seam scripts}}}.
Script semantics (shared with the C harness dispatcher and the oracle Script):
  - absent key            -> fail-closed RuntimeError "no script for <name>"
  - non-list value        -> constant result reused every call
  - list value            -> per-call queue (exhausted -> fail-closed)
  - {"__raise__":K,"message":M} -> raised exception of kind K
  - {"__script__":true,"ret":X} -> list-valued result X (queue-ambiguity wrap)
  - null                  -> None result
>=5 cases per function (11 functions); covers branches, seam order, raises,
BaseException passthrough and fail-closed paths. ASCII inputs; carrier scripts
null or non-empty objects; snapshot/window carriers full-field; fetched_at /
reset_at integer epoch-us. str()-computed positions str-or-null. NaN/Inf ride
a {"__special__":...} marker (direct _fmt_usd / _is_finite_num inputs only).
"""
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT = HERE / "fixtures" / "account_usage_nous_flow2.jsonl"

CASES = []


def add(func, inp, io=None):
    inp = dict(inp)
    inp["io"] = io or {}
    CASES.append({"func": func, "in": inp})


def R(kind, msg=""):
    return {"__raise__": kind, "message": msg}


def L(ret):
    return {"__script__": True, "ret": ret}


US = 1731974400000000          # 2024-11-19T00:00:00Z
WIN = {"label": "Today", "used_percent": 25.0, "reset_at": US, "detail": None, "scope": "account"}
SNAP = {"provider": "nous", "source": "portal-account", "fetched_at": US, "title": "Nous credits",
        "plan": None, "windows": [WIN], "details": ["d"], "unavailable_reason": None,
        "identity": None, "raw": None, "usage_window": False}
UW = {"gauges": [{"id": "g1", "scope": "all", "label": "All", "used_bp": 5000, "resets_at_ms": US // 1000}],
      "paused_reason": None, "keep_going_on_credits": None}
TOPUP = "https://portal.nousresearch.com/topup"

# ---------------------------------------------------------------- _fmt_usd
F = "_fmt_usd"
for d in [0, 1234.5, -1234.5, 1234567.891, 0.005, -0.005, 100, True, False,
          0.1, 0.125, 2.675, 999999.995, -0.0, 1e15 - 1]:
    add(F, {"d": d})
for s in ["inf", "-inf", "nan"]:
    add(F, {"d": {"__special__": s}})
add(F, {"d": "x"})
add(F, {"d": None})
add(F, {"d": []})
add(F, {"d": {}})

# ---------------------------------------------------------- _is_finite_num
F = "_is_finite_num"
for v in [0, 1.5, -3, True, False, "x", None, [], {}, 100, 0.0]:
    add(F, {"v": v})
for s in ["nan", "inf", "-inf"]:
    add(F, {"v": {"__special__": s}})

# ------------------------------------------- build_nous_credits_snapshot
F = "build_nous_credits_snapshot"
BIO = {"nous_portal_topup_url": TOPUP, "nous_snapshot": SNAP}
add(F, {"account": None, "cta": True}, {})
add(F, {"account": {}, "cta": True}, {})
add(F, {"account": {"logged_in": False}, "cta": True}, {})
add(F, {"account": {"logged_in": True}, "cta": True}, BIO)
add(F, {"account": {"logged_in": True}, "cta": False}, {"nous_snapshot": SNAP})
add(F, {"account": {"logged_in": True, "subscription": {
    "monthly_credits": 100, "credits_remaining": 25, "plan": "pro",
    "current_period_end": "2026-12-01", "rollover_credits": 5}}, "cta": True}, BIO)
add(F, {"account": {"logged_in": True, "subscription": {
    "monthly_credits": 0, "credits_remaining": 0}}, "cta": True}, BIO)
add(F, {"account": {"logged_in": True, "subscription": {
    "monthly_credits": 10, "credits_remaining": 20}}, "cta": True}, BIO)
add(F, {"account": {"logged_in": True, "subscription": {"monthly_credits": True}}, "cta": True}, BIO)
add(F, {"account": {"logged_in": True, "paid_service_access_info": {
    "subscription_credits_remaining": 1.5, "purchased_credits_remaining": 2,
    "total_usable_credits": 3.5}}, "cta": True}, BIO)
add(F, {"account": {"logged_in": True, "paid_service_access": False}, "cta": True}, BIO)
add(F, {"account": {"logged_in": True, "usage_window": UW}, "cta": True},
    {"usage_window_rows": L([[WIN], ["wline"]]), "nous_snapshot": SNAP})
add(F, {"account": {"logged_in": True, "usage_window": UW}, "cta": False},
    {"usage_window_rows": L([[WIN], ["wline"]]), "nous_snapshot": SNAP})
add(F, {"account": {"logged_in": True, "usage_window": UW, "paid_service_access": False,
                    "paid_service_access_info": {"purchased_credits_remaining": 12.5},
                    "subscription": {"current_period_end": "2026-12-01", "plan": "pro"}}, "cta": True},
    {"usage_window_rows": L([[WIN], []]), "nous_snapshot": SNAP, "format_renews": "Nov 19"})
add(F, {"account": {"logged_in": True, "usage_window": UW,
                    "subscription": {"current_period_end": "2026-12-01"}}, "cta": True},
    {"usage_window_rows": L([[WIN], []]), "nous_snapshot": SNAP, "format_renews": None})
add(F, {"account": {"logged_in": True, "usage_window": UW,
                    "subscription": {"current_period_end": "2026-12-01"}}, "cta": True},
    {"usage_window_rows": L([[WIN], []]), "nous_snapshot": SNAP, "format_renews": ""})
add(F, {"account": {"logged_in": True, "usage_window": UW}, "cta": True},
    {"usage_window_rows": R("AttributeError", "a"), "nous_snapshot": SNAP})
add(F, {"account": {"logged_in": True, "usage_window": UW}, "cta": True},
    {"usage_window_rows": R("TypeError", "t"), "nous_snapshot": SNAP})
add(F, {"account": {"logged_in": True, "usage_window": UW}, "cta": True},
    {"usage_window_rows": R("ValueError", "v"), "nous_snapshot": SNAP})
add(F, {"account": {"logged_in": True, "usage_window": UW}, "cta": True},
    {"usage_window_rows": R("SystemExit", "bye"), "nous_snapshot": SNAP})
add(F, {"account": {"logged_in": True}, "cta": True},
    {"nous_portal_topup_url": TOPUP, "nous_snapshot": R("AttributeError", "a")})
add(F, {"account": {"logged_in": True}, "cta": True},
    {"nous_portal_topup_url": TOPUP, "nous_snapshot": R("RuntimeError", "r")})
add(F, {"account": {"logged_in": True}, "cta": True},
    {"nous_portal_topup_url": R("TypeError", "t"), "nous_snapshot": SNAP})
add(F, {"account": {"logged_in": True}, "cta": True},
    {"nous_portal_topup_url": R("SystemExit", "bye"), "nous_snapshot": SNAP})
add(F, {"account": {"logged_in": True, "usage_window": UW,
                    "subscription": {"current_period_end": "2026-12-01"}}, "cta": True},
    {"usage_window_rows": L([[WIN], []]), "nous_snapshot": SNAP, "format_renews": R("AttributeError", "a")})
add(F, {"account": {"logged_in": True, "usage_window": UW,
                    "subscription": {"current_period_end": "2026-12-01"}}, "cta": True},
    {"usage_window_rows": L([[WIN], []]), "nous_snapshot": SNAP, "format_renews": R("KeyboardInterrupt", "ki")})
add(F, {"account": {"logged_in": True, "usage_window": UW}, "cta": True}, {})
add(F, {"account": {"logged_in": True, "subscription": {
    "monthly_credits": 100, "credits_remaining": 100, "plan": None}}, "cta": True}, BIO)

# ------------------------------------------------------- nous_credits_lines
F = "nous_credits_lines"
NC = {"markdown": False, "timeout": 10.0, "credits_state": None, "nous_route": True}
add(F, NC, {"nous_credits_view": L([["l1", "l2"], False])})
add(F, NC, {"nous_credits_view": L([[], True])})
add(F, dict(NC, nous_route=False, credits_state={"from_header": True}),
    {"nous_credits_view": L([["a"], False])})
add(F, dict(NC, markdown=True), {"nous_credits_view": L([["m"], True])})
add(F, NC, {"nous_credits_view": R("RuntimeError", "x")})
add(F, NC, {"nous_credits_view": R("SystemExit", "bye")})
add(F, NC, {"nous_credits_view": R("KeyboardInterrupt", "ki")})
add(F, NC, {})

# ------------------------------------------- _snapshot_from_credits_state
F = "_snapshot_from_credits_state"
add(F, {"state": None}, {})
add(F, {"state": {}}, {"nous_snapshot": SNAP})
add(F, {"state": "x"}, {"nous_snapshot": SNAP})
add(F, {"state": {"is_usage_window": True, "gauges": UW["gauges"], "paused_reason": None}},
    {"usage_window_rows": L([[WIN], ["wl"]]), "nous_snapshot": SNAP})
add(F, {"state": {"is_usage_window": True}}, {})
add(F, {"state": {"is_usage_window": True, "gauges": UW["gauges"]}}, {})
add(F, {"state": {"is_usage_window": True, "gauges": UW["gauges"], "paused_reason": "x",
                  "purchased_usd": "5", "paid_access": False}},
    {"usage_window_rows": L([[WIN], []]), "nous_snapshot": SNAP})
add(F, {"state": {"used_fraction": 0.25, "subscription_usd": "10",
                  "subscription_limit_usd": "40", "purchased_usd": "5", "remaining_usd": "15"}},
    {"nous_snapshot": SNAP})
add(F, {"state": {"used_fraction": True}}, {"nous_snapshot": SNAP})
add(F, {"state": {"used_fraction": 0.0}}, {"nous_snapshot": SNAP})
add(F, {"state": {"used_fraction": 0.5, "paid_access": False}}, {"nous_snapshot": SNAP})
add(F, {"state": {"used_fraction": "x"}}, {"nous_snapshot": SNAP})
add(F, {"state": {"used_fraction": 0.5, "subscription_usd": "10", "subscription_limit_usd": "40"}},
    {"nous_snapshot": SNAP})
add(F, {"state": {"is_usage_window": True, "gauges": UW["gauges"], "paused_reason": None}},
    {"usage_window_rows": R("AttributeError", "a"), "nous_snapshot": SNAP})
add(F, {"state": {"is_usage_window": True, "gauges": UW["gauges"], "paused_reason": None}},
    {"usage_window_rows": R("ValueError", "v"), "nous_snapshot": SNAP})
add(F, {"state": {}}, {"nous_snapshot": R("TypeError", "t")})
add(F, {"state": {}}, {"nous_snapshot": R("SystemExit", "bye")})
add(F, {"state": {"is_usage_window": True, "gauges": UW["gauges"], "paused_reason": None}}, {})

# ------------------------------------------------------- build_credits_view
F = "build_credits_view"
ACCT = {"logged_in": True, "email": "a@b.com", "org_name": "Acme", "paid_service_access": True,
        "subscription": {"monthly_credits": 100, "credits_remaining": 50, "plan": "pro"}}
RENDER = L(["line1", "Top up: x", "Top up via y", "(or run z)", "line2"])
CVIO = {"nous_logged_in": True, "fetch_portal_account": ACCT,
        "render_account_usage_lines": RENDER, "nous_snapshot": SNAP, "nous_portal_topup_url": TOPUP}
add(F, {"markdown": False, "timeout": 10.0}, CVIO)
add(F, {"markdown": True, "timeout": 5.0}, CVIO)
add(F, {"markdown": False, "timeout": 10.0}, {"nous_logged_in": False})
add(F, {"markdown": False, "timeout": 10.0}, {"nous_logged_in": R("RuntimeError", "x")})
add(F, {"markdown": False, "timeout": 10.0}, {"nous_logged_in": R("SystemExit", "bye")})
add(F, {"markdown": False, "timeout": 10.0},
    {"nous_logged_in": True, "fetch_portal_account": R("RuntimeError", "f"), "logger.debug": None})
add(F, {"markdown": False, "timeout": 10.0},
    {"nous_logged_in": True, "fetch_portal_account": R("SystemExit", "bye")})
add(F, {"markdown": False, "timeout": 10.0},
    {"nous_logged_in": True, "fetch_portal_account": R("KeyboardInterrupt", "ki")})
add(F, {"markdown": False, "timeout": 10.0}, {"nous_logged_in": True, "fetch_portal_account": None})
add(F, {"markdown": False, "timeout": 10.0},
    {"nous_logged_in": True, "fetch_portal_account": {"logged_in": False}})
add(F, {"markdown": False, "timeout": 10.0},
    dict(CVIO, **{"render_account_usage_lines": R("RuntimeError", "r")}))
add(F, {"markdown": False, "timeout": 10.0},
    dict(CVIO, **{"render_account_usage_lines": R("SystemExit", "bye")}))
add(F, {"markdown": False, "timeout": 10.0},
    dict(CVIO, **{"nous_portal_topup_url": [TOPUP, R("ValueError", "v")]}))
add(F, {"markdown": False, "timeout": 10.0},
    {"nous_logged_in": True, "fetch_portal_account": R("RuntimeError", "f"),
     "logger.debug": R("RuntimeError", "log")})
add(F, {"markdown": False, "timeout": 10.0}, {})
add(F, {"markdown": False, "timeout": 10.0},
    dict(CVIO, **{"fetch_portal_account": {"logged_in": True}}))
add(F, {"markdown": False, "timeout": 10.0},
    dict(CVIO, **{"fetch_portal_account": dict(ACCT, paid_service_access=False)}))
add(F, {"markdown": False, "timeout": 10.0},
    dict(CVIO, **{"render_account_usage_lines": L([1])}))
add(F, {"markdown": False, "timeout": 10.0},
    {"nous_logged_in": True,
     "fetch_portal_account": {"logged_in": True, "usage_window": UW}, "nous_snapshot": SNAP})
add(F, {"markdown": False, "timeout": 10.0},
    dict(CVIO, **{"render_account_usage_lines": L(["  Top up: indented", "keep"])}))

# ------------------------------------------------------- CreditsView.__init__
F = "CreditsView.__init__"
add(F, {"kw": {"logged_in": True}})
add(F, {"kw": {"logged_in": False, "balance_lines": ["a", "b"], "identity_line": "x",
               "topup_url": "u", "depleted": True}})
add(F, {"kw": {"logged_in": True, "balance_lines": ["a"]}})
add(F, {"kw": {"logged_in": True, "identity_line": None}})
add(F, {"kw": {"logged_in": True, "depleted": False}})
add(F, {"kw": {"logged_in": 1}})
add(F, {"kw": {"logged_in": True, "balance_lines": []}})

# ------------------------------------------------------- CreditsView.__repr__
F = "CreditsView.__repr__"
add(F, {"self": {"logged_in": True, "balance_lines": [], "identity_line": None,
                 "topup_url": None, "depleted": False}})
add(F, {"self": {"logged_in": True, "balance_lines": ["a"], "identity_line": "x",
                 "topup_url": "u", "depleted": False}})
add(F, {"self": {"logged_in": False, "balance_lines": ["a", "b"], "identity_line": None,
                 "topup_url": None, "depleted": True}})
add(F, {"self": {"logged_in": True, "balance_lines": [], "identity_line": "it's",
                 "topup_url": None, "depleted": False}})
add(F, {"self": {"logged_in": True, "balance_lines": ["a'b"], "identity_line": None,
                 "topup_url": 'say "hi"', "depleted": False}})
add(F, {"self": {"logged_in": True, "balance_lines": [], "identity_line": None,
                 "topup_url": "a\nb", "depleted": False}})

# --------------------------------------------------------- CreditsView.__eq__
F = "CreditsView.__eq__"
CV0 = {"logged_in": True, "balance_lines": ["a"], "identity_line": "x",
       "topup_url": "u", "depleted": False}
add(F, {"self": CV0, "other": {"__credits_view__": dict(CV0)}})
add(F, {"self": CV0, "other": {"__credits_view__": dict(CV0, logged_in=False)}})
add(F, {"self": CV0, "other": {}})
add(F, {"self": CV0, "other": None})
add(F, {"self": CV0, "other": 5})
add(F, {"self": CV0, "other": "str"})
add(F, {"self": CV0, "other": {"__credits_view__": dict(CV0, logged_in=1)}})
add(F, {"self": CV0, "other": {"__credits_view__": dict(CV0, balance_lines=["b"])}})
add(F, {"self": CV0, "other": {"__credits_view__": dict(CV0, identity_line=None)}})
add(F, {"self": CV0, "other": {"__credits_view__": dict(CV0, balance_lines=["a"], depleted=0)}})

# ---------------------------------------------------- CreditsView.__setattr__
F = "CreditsView.__setattr__"
for nm in ["logged_in", "bogus", "balance_lines", "identity_line", "x'y", ""]:
    add(F, {"self": CV0, "name": nm, "value": 1})

# ---------------------------------------------------- CreditsView.__delattr__
F = "CreditsView.__delattr__"
for nm in ["logged_in", "bogus", "depleted", "topup_url", 'a"b']:
    add(F, {"self": CV0, "name": nm})


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    # per-function counts
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print("wrote %s: %d cases" % (OUT, len(CASES)))
    for fn, n in counts.items():
        flag = "" if n >= 5 else "  (<5!)"
        print("  %-32s %d%s" % (fn, n, flag))
    assert all(n >= 5 for n in counts.values()), "every function needs >=5 cases"
    assert len(counts) == 11, "expected 11 functions"


if __name__ == "__main__":
    main()