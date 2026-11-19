#!/usr/bin/env python3
"""Generate tests/fixtures/usage_tracking_flow.jsonl for the ut port.

Each line: {"func": <live_name>, "in": {...inputs..., "io": {seam scripts}}}.
Script semantics (shared with the C harness dispatcher and the oracle Script):
  - absent key            -> fail-closed RuntimeError "no script for <name>"
  - non-list value        -> constant result reused every call
  - list value            -> per-call queue (exhausted -> fail-closed)
  - {"__raise__":K,"message":M,...} -> raised exception of kind K
  - {"__script__":true,"ret":X} -> list-valued result X (queue-ambiguity wrap)
  - null                  -> None result
>=5 cases per function; covers branches, seam order, raises, BaseException
passthrough and fail-closed paths. ASCII inputs; used_bp/resets_at_ms INT-typed;
datetime carriers are integer epoch-MICROSECONDS (UTC).
"""
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT = HERE / "fixtures" / "usage_tracking_flow.jsonl"

CASES = []


def add(func, inp, io=None):
    inp = dict(inp)
    inp["io"] = io or {}
    CASES.append({"func": func, "in": inp})


def R(kind, msg=""):
    return {"__raise__": kind, "message": msg}


def RH(code, msg="http err"):
    return {"__raise__": "HTTPStatusError", "message": msg, "status_code": code}


def L(ret):
    return {"__script__": True, "ret": ret}


# fixed datetime carriers (epoch-us / epoch-ms / epoch-s)
US = 1731974400000000          # 2024-11-19T00:00:00Z
MS = 1731974400000
SEC = 1731974400.0
US_FUTURE = US + 86400_000000  # +1 day
MS_FUTURE = MS + 86400_000

WIN = {"label": "Today", "used_percent": 25.0, "reset_at": US, "detail": None, "scope": "account"}
WIN2 = {"label": "This week", "used_percent": 80.0, "reset_at": None, "detail": "of $10", "scope": "account"}
SNAP = {"provider": "nous", "source": "portal-account", "fetched_at": US, "title": "Nous credits",
        "plan": None, "windows": [WIN], "details": ["Top-up credits: $5.00"],
        "unavailable_reason": None, "identity": None, "raw": None, "usage_window": True}
GAUGE = {"id": "day", "scope": "all", "label": "Today", "used_bp": 2500, "resets_at_ms": MS_FUTURE}
GAUGE_TT = {"id": "top_tier", "scope": "top_tier", "label": "Large models (monthly)",
            "used_bp": 10000, "resets_at_ms": MS_FUTURE}
CSTATE = {"from_header": True, "is_usage_window": True, "gauges": [GAUGE],
          "top_tier_gauge": GAUGE_TT, "keep_going_on_credits": None}

# 1. _is_num
F = "_is_num"
add(F, {"v": 5}); add(F, {"v": 0}); add(F, {"v": 3.5}); add(F, {"v": True})
add(F, {"v": False}); add(F, {"v": "5"}); add(F, {"v": None}); add(F, {"v": [1]}); add(F, {"v": {}})

# 2. _codex_banked_resets
F = "_codex_banked_resets"
add(F, {"payload": {"rate_limit_reset_credits": {"available_count": 3}}})
add(F, {"payload": {"rate_limit_reset_credits": {"available_count": 2.9}}})
add(F, {"payload": {"rate_limit_reset_credits": {"available_count": True}}})
add(F, {"payload": {"rate_limit_reset_credits": {"available_count": "x"}}})
add(F, {"payload": {"rate_limit_reset_credits": None}})
add(F, {"payload": {}})
add(F, {"payload": {"rate_limit_reset_credits": 5}})   # truthy non-dict -> AttributeError
add(F, {"payload": {"rate_limit_reset_credits": {"available_count": None}}})

# 3. _codex_window_labels
F = "_codex_window_labels"
add(F, {"rate_limit": {}})
add(F, {"rate_limit": {"primary_window": {"limit_window_seconds": 18000},
                       "secondary_window": {"limit_window_seconds": 604800}}})
add(F, {"rate_limit": {"primary_window": {"limit_window_seconds": 604800}}})
add(F, {"rate_limit": {"primary_window": {"limit_window_seconds": 999}}})
add(F, {"rate_limit": {"primary_window": "x"}})
add(F, {"rate_limit": {"primary_window": {"limit_window_seconds": True}}})
add(F, {"rate_limit": {"primary_window": None, "secondary_window": {"limit_window_seconds": 18000}}})

# 4. _codex_headers
F = "_codex_headers"
add(F, {"token": "tok", "account_id": "acct"}, {"codex_account_headers": {"X-A": "1"}})
add(F, {"token": "tok", "account_id": None}, {"codex_account_headers": {}})
add(F, {"token": "tok", "account_id": ""}, {"codex_account_headers": {"Authorization": "override"}})
add(F, {"token": "tok", "account_id": "a"}, {"codex_account_headers": R("ValueError", "bad")})
add(F, {"token": "tok", "account_id": "a"}, {"codex_account_headers": R("SystemExit", "bye")})
add(F, {"token": "tok", "account_id": "a"}, {})   # fail-closed
add(F, {"token": "tok", "account_id": "a"}, {"codex_account_headers": None})  # **None TypeError

# 5. _codex_snapshot_identity
F = "_codex_snapshot_identity"
add(F, {"token": "tok"}, {"codex_principal_identity": L(["a", "b"])})
add(F, {"token": "tok"}, {"codex_principal_identity": None})
add(F, {"token": "tok"}, {"codex_principal_identity": R("ValueError", "x")})
add(F, {"token": "tok"}, {"codex_principal_identity": R("KeyboardInterrupt", "ki")})
add(F, {"token": "tok"}, {})
add(F, {"token": "tok"}, {"codex_principal_identity": L(["only"])})  # IndexError

# 6. _unavailable
F = "_unavailable"
add(F, {"message": "m1"}); add(F, {"message": ""}); add(F, {"message": "No Codex credentials available."})
add(F, {"message": "x" * 40}); add(F, {"message": "line\nbreak"})

# 7. _codex_reset_guard
F = "_codex_reset_guard"
PL = {"primary_window": {"used_percent": 100.0}, "secondary_window": {"used_percent": 40.0}}
add(F, {"payload": {}, "available": 0, "force": False}, {})
add(F, {"payload": {"rate_limit": PL}, "available": 2, "force": False}, {"plural": "s"})
add(F, {"payload": {"rate_limit": PL}, "available": 2, "force": True}, {})
add(F, {"payload": {"rate_limit": {"primary_window": {"used_percent": 50}}}, "available": 1, "force": False}, {"plural": ""})
add(F, {"payload": {}, "available": 3, "force": False}, {"plural": "s"})
add(F, {"payload": {"rate_limit": PL}, "available": 2, "force": False}, {"plural": R("SystemExit", "bye")})
add(F, {"payload": {"rate_limit": {"primary_window": 5}}, "available": 2, "force": False}, {})
add(F, {"payload": {"rate_limit": 5}, "available": 2, "force": False}, {})

# 8. _codex_reset_outcome
F = "_codex_reset_outcome"
add(F, {"body": {"code": "reset", "windows_reset": 2}, "available": 3},
    {"plural": "s", "clear_codex_pool_quota_cooldowns": None})
add(F, {"body": {"code": "nothing_to_reset"}, "available": 3}, {"plural": "s"})
add(F, {"body": {"code": "no_credit"}, "available": 3}, {"plural": "s"})
add(F, {"body": {"code": "already_redeemed"}, "available": 1}, {"plural": ""})
add(F, {"body": {"code": "weird"}, "available": 3}, {"plural": "s"})
add(F, {"body": {"code": " RESET "}, "available": 2},
    {"plural": "", "clear_codex_pool_quota_cooldowns": None})
add(F, {"body": {"code": "reset"}, "available": 3},
    {"plural": "s", "clear_codex_pool_quota_cooldowns": R("RuntimeError", "x")})
add(F, {"body": {"code": "reset"}, "available": 3},
    {"plural": "s", "clear_codex_pool_quota_cooldowns": R("SystemExit", "bye")})
add(F, {"body": {"code": "reset"}, "available": 3}, {"plural": R("GeneratorExit", "ge")})
add(F, {"body": {"code": None}, "available": 3}, {"plural": "s"})

# 9. _snapshot
F = "_snapshot"
add(F, {"provider": "nous", "source": "portal", "windows": [WIN], "details": ["d"], "kw": {}}, {"utc_now": US})
add(F, {"provider": "p", "source": "s", "windows": [], "details": [], "kw": {"title": "T", "plan": "Pro"}}, {"utc_now": US})
add(F, {"provider": "p", "source": "s", "windows": [], "details": [], "kw": {"usage_window": True}}, {"utc_now": US})
add(F, {"provider": "p", "source": "s", "windows": [], "details": [], "kw": {}}, {"utc_now": R("SystemExit", "bye")})
add(F, {"provider": "p", "source": "s", "windows": [], "details": [], "kw": {}}, {})
add(F, {"provider": "p", "source": "s", "windows": [WIN, WIN2], "details": ["a", "b"],
        "kw": {"identity": "codex:a:b", "raw": {"k": 1}}}, {"utc_now": US})

# 10. _usage_windows
F = "_usage_windows"
MAP = [["five_hour", "Current session"], ["seven_day", "Current week"]]
SRC = {"five_hour": {"utilization": 0.5, "resets_at": MS}, "seven_day": {"utilization": 0.9, "resets_at": None}}
add(F, {"source": SRC, "mapping": MAP, "used_key": "utilization", "reset_key": "resets_at",
        "fraction": True, "model_scoped": []}, {"parse_dt": US})
add(F, {"source": SRC, "mapping": MAP, "used_key": "utilization", "reset_key": "resets_at",
        "fraction": False, "model_scoped": []}, {"parse_dt": US})
add(F, {"source": {"five_hour": {"utilization": None}}, "mapping": MAP, "used_key": "utilization",
        "reset_key": "resets_at", "fraction": True, "model_scoped": []}, {})
add(F, {"source": SRC, "mapping": MAP, "used_key": "utilization", "reset_key": "resets_at",
        "fraction": True, "model_scoped": ["seven_day"]}, {"parse_dt": US})
add(F, {"source": {"five_hour": 5}, "mapping": MAP, "used_key": "utilization",
        "reset_key": "resets_at", "fraction": True, "model_scoped": []}, {})
add(F, {"source": SRC, "mapping": MAP, "used_key": "utilization", "reset_key": "resets_at",
        "fraction": True, "model_scoped": []}, {"parse_dt": R("ValueError", "x")})
add(F, {"source": SRC, "mapping": MAP, "used_key": "utilization", "reset_key": "resets_at",
        "fraction": True, "model_scoped": []}, {"parse_dt": R("SystemExit", "bye")})

# 11. account_limit_lines
F = "account_limit_lines"
add(F, {"provider": None, "base_url": None, "api_key": None, "markdown": False}, {})
add(F, {"provider": "", "base_url": None, "api_key": None, "markdown": False}, {})
add(F, {"provider": "anthropic", "base_url": None, "api_key": None, "markdown": False},
    {"fetch_account_usage": SNAP, "render_account_usage_lines": L(["line1", "line2"])})
add(F, {"provider": "anthropic", "base_url": "u", "api_key": "k", "markdown": True},
    {"fetch_account_usage": None, "render_account_usage_lines": L([])})
add(F, {"provider": "x", "base_url": None, "api_key": None, "markdown": False},
    {"fetch_account_usage": R("SystemExit", "bye")})
add(F, {"provider": "x", "base_url": None, "api_key": None, "markdown": False},
    {"fetch_account_usage": SNAP, "render_account_usage_lines": R("RuntimeError", "x")})
add(F, {"provider": "x", "base_url": None, "api_key": None, "markdown": False}, {})

# 12. _reset_phrase
F = "_reset_phrase"
add(F, {"dt": None}, {})
add(F, {"dt": US}, {"format_resets_in": "resets in 2h"})
add(F, {"dt": US}, {"format_resets_in": R("ValueError", "x"), "format_reset": "in 3h (stamp)", "logger.debug": None})
add(F, {"dt": US}, {"format_resets_in": R("SystemExit", "bye")})
add(F, {"dt": US}, {"format_resets_in": R("KeyboardInterrupt", "ki")})
add(F, {"dt": US}, {})
add(F, {"dt": US}, {"format_resets_in": R("RuntimeError", "x"), "format_reset": R("GeneratorExit", "ge"), "logger.debug": None})

# 13. _render_usage_window_lines
F = "_render_usage_window_lines"
add(F, {"snapshot": SNAP}, {"render_bar": "####", "format_percent_used": "25%", "format_resets_in": "resets in 2h"})
add(F, {"snapshot": dict(SNAP, plan="Pro", windows=[WIN2])},
    {"render_bar": "##", "format_percent_used": "80%"})
add(F, {"snapshot": dict(SNAP, windows=[dict(WIN, used_percent=None)])}, {})
add(F, {"snapshot": dict(SNAP, windows=[WIN])},
    {"render_bar": R("ValueError", "x"), "logger.debug": None, "format_resets_in": "soon"})
add(F, {"snapshot": dict(SNAP, windows=[WIN])},
    {"render_bar": "##", "format_percent_used": R("RuntimeError", "x"), "logger.debug": None, "format_resets_in": "soon"})
add(F, {"snapshot": dict(SNAP, windows=[WIN])},
    {"render_bar": R("SystemExit", "bye")})
add(F, {"snapshot": dict(SNAP, windows=[], details=["d1"], unavailable_reason="down")}, {})
add(F, {"snapshot": dict(SNAP, windows=[dict(WIN, reset_at=None, detail="of $10")])},
    {"render_bar": "", "format_percent_used": "25%"})

# 14. _call_plugin_usage_hook
F = "_call_plugin_usage_hook"
add(F, {"profile": {"hook": "base"}, "base_url": None, "api_key": None}, {})
add(F, {"profile": {"hook": "custom"}, "base_url": "u", "api_key": "k"},
    {"run_bounded_sync": {"timed_out": False, "value": SNAP}})
add(F, {"profile": {"hook": "custom"}, "base_url": None, "api_key": None},
    {"run_bounded_sync": {"timed_out": True, "value": None}})
add(F, {"profile": None, "base_url": None, "api_key": None}, {})
add(F, {"profile": {"hook": "custom"}, "base_url": None, "api_key": None},
    {"run_bounded_sync": R("SystemExit", "bye")})
add(F, {"profile": {"hook": "custom"}, "base_url": None, "api_key": None},
    {"run_bounded_sync": None})
add(F, {"profile": {"hook": "custom"}, "base_url": None, "api_key": None}, {})

# 15. _fetch_codex_account_usage_impl
F = "_fetch_codex_account_usage_impl"
PAYLOAD = {"rate_limit": {"primary_window": {"used_percent": 50, "reset_at": MS, "limit_window_seconds": 18000},
                          "secondary_window": {"used_percent": 80, "reset_at": MS, "limit_window_seconds": 604800}},
           "rate_limit_reset_credits": {"available_count": 2},
           "credits": {"has_credits": True, "balance": 12.5}, "plan_type": "pro_plan"}
FULL_IO = {"resolve_codex_usage_credentials": L(["tok", "https://x.com", "acct"]),
           "codex_backend_urls": L(["https://x.com/usage", "https://x.com/c", "https://x.com/consume"]),
           "codex_account_headers": {"X-A": "1"}, "get_json": PAYLOAD,
           "parse_dt": US, "plural": "s", "title_case_slug": "Pro Plan",
           "codex_principal_identity": L(["a", "b"]), "utc_now": US}
add(F, {"base_url": None, "api_key": None, "read_only": False}, FULL_IO)
add(F, {"base_url": "u", "api_key": "k", "read_only": False},
    dict(FULL_IO, get_json={"rate_limit": {}, "credits": {"has_credits": True, "unlimited": True}}))
add(F, {"base_url": None, "api_key": None, "read_only": False},
    dict(FULL_IO, get_json=[RH(401), PAYLOAD],
         resolve_codex_usage_credentials=[L(["tok", "https://x.com", "acct"]), L(["tok2", "https://y.com", "acct2"])]))
add(F, {"base_url": None, "api_key": None, "read_only": True}, dict(FULL_IO, get_json=RH(401)))
add(F, {"base_url": None, "api_key": None, "read_only": False}, dict(FULL_IO, get_json=RH(500)))
add(F, {"base_url": None, "api_key": None, "read_only": False},
    dict(FULL_IO, resolve_codex_usage_credentials=R("SystemExit", "bye")))
add(F, {"base_url": None, "api_key": None, "read_only": False}, {})
add(F, {"base_url": None, "api_key": None, "read_only": False}, dict(FULL_IO, get_json=None))
add(F, {"base_url": None, "api_key": None, "read_only": False},
    dict(FULL_IO, get_json={"rate_limit": {}, "credits": {"has_credits": False}, "plan_type": None}))

# 16. _fetch_codex_account_usage_read_only
F = "_fetch_codex_account_usage_read_only"
add(F, {"base_url": None, "api_key": None}, FULL_IO)
add(F, {"base_url": None, "api_key": None}, dict(FULL_IO, get_json=RH(401)))
add(F, {"base_url": None, "api_key": None}, dict(FULL_IO, get_json=RH(403)))
add(F, {"base_url": None, "api_key": None}, dict(FULL_IO, resolve_codex_usage_credentials=R("RuntimeError", "x")))
add(F, {"base_url": None, "api_key": None}, {})

# 17. _header_gauge_percent
F = "_header_gauge_percent"
add(F, {"bp": 0}); add(F, {"bp": 5000}); add(F, {"bp": 10000}); add(F, {"bp": 15000})
add(F, {"bp": True}); add(F, {"bp": 3.5}); add(F, {"bp": None}); add(F, {"bp": "5000"}); add(F, {"bp": -100})

# 18. _bp_window
F = "_bp_window"
add(F, {"label": "Today", "used_bp": 2500, "resets_at_ms": MS})
add(F, {"label": "Today", "used_bp": 2500, "resets_at_ms": None})
add(F, {"label": "Today", "used_bp": 0, "resets_at_ms": MS})
add(F, {"label": "Today", "used_bp": 10000, "resets_at_ms": MS})
add(F, {"label": "Today", "used_bp": 15000, "resets_at_ms": MS})
add(F, {"label": "Today", "used_bp": True, "resets_at_ms": MS})
add(F, {"label": "Today", "used_bp": 3.5, "resets_at_ms": MS})
add(F, {"label": "Today", "used_bp": 5000, "resets_at_ms": True})
add(F, {"label": "Today", "used_bp": 5000, "resets_at_ms": 3.5})

# 19. _window_top_tier_line
F = "_window_top_tier_line"
add(F, {"reset_at": US, "keep_going_on_credits": None})
add(F, {"reset_at": US, "keep_going_on_credits": False})
add(F, {"reset_at": US, "keep_going_on_credits": True})
add(F, {"reset_at": None, "keep_going_on_credits": False})
add(F, {"reset_at": None, "keep_going_on_credits": None})
add(F, {"reset_at": US_FUTURE, "keep_going_on_credits": False})
add(F, {"reset_at": US, "keep_going_on_credits": 1})   # identity: 1 is not False -> tail_other

# 20. _nous_snapshot
F = "_nous_snapshot"
add(F, {"windows": [WIN], "details": ["d"], "tail": ["Top up via /topup"], "source": "portal-account",
        "plan": None, "usage_window": True}, {"utc_now": US})
add(F, {"windows": [], "details": [], "tail": [], "source": "s", "plan": None, "usage_window": False}, {})
add(F, {"windows": [], "details": ["d"], "tail": [], "source": "dev-fixture", "plan": None, "usage_window": False}, {"utc_now": US})
add(F, {"windows": [WIN], "details": [], "tail": [], "source": "s", "plan": "Pro", "usage_window": True}, {"utc_now": US})
add(F, {"windows": [WIN], "details": [], "tail": [], "source": "s", "plan": None, "usage_window": False}, {"utc_now": R("SystemExit", "bye")})
add(F, {"windows": [WIN], "details": [], "tail": [], "source": "s", "plan": None, "usage_window": False}, {})

# 21. _usage_window_rows
F = "_usage_window_rows"
add(F, {"gauges": [GAUGE], "paused_reason": None, "keep_going_on_credits": None, "cta": True},
    {"parse_gauges": L([GAUGE]), "all_scope": L([GAUGE]), "gauge_title": "Today"})
add(F, {"gauges": [GAUGE, GAUGE_TT], "paused_reason": None, "keep_going_on_credits": None, "cta": True},
    {"parse_gauges": L([GAUGE, GAUGE_TT]), "all_scope": L([GAUGE]), "gauge_title": ["Today", "Large models (monthly)"]})
add(F, {"gauges": [GAUGE_TT], "paused_reason": "payment_past_due", "keep_going_on_credits": None, "cta": True},
    {"parse_gauges": L([GAUGE_TT]), "all_scope": L([]), "gauge_title": "Large models (monthly)"})
add(F, {"gauges": [GAUGE], "paused_reason": "other", "keep_going_on_credits": None, "cta": True},
    {"parse_gauges": L([GAUGE]), "all_scope": L([GAUGE]), "gauge_title": "Today", "paused_for_payment": False})
add(F, {"gauges": [GAUGE], "paused_reason": None, "keep_going_on_credits": False, "cta": True},
    {"parse_gauges": L([GAUGE]), "all_scope": L([GAUGE]), "gauge_title": "Today"})
add(F, {"gauges": [GAUGE], "paused_reason": None, "keep_going_on_credits": False, "cta": False},
    {"parse_gauges": L([GAUGE]), "all_scope": L([GAUGE]), "gauge_title": "Today"})
add(F, {"gauges": [GAUGE], "paused_reason": None, "keep_going_on_credits": True, "cta": True},
    {"parse_gauges": L([GAUGE]), "all_scope": L([GAUGE]), "gauge_title": "Today"})
add(F, {"gauges": None, "paused_reason": None, "keep_going_on_credits": None, "cta": True},
    {"parse_gauges": None})
add(F, {"gauges": [GAUGE], "paused_reason": None, "keep_going_on_credits": None, "cta": True},
    {"parse_gauges": L([GAUGE]), "all_scope": L([]), "gauge_title": "Today"})
add(F, {"gauges": [GAUGE_TT], "paused_reason": None, "keep_going_on_credits": False, "cta": True},
    {"parse_gauges": L([GAUGE_TT]), "all_scope": L([GAUGE_TT]), "gauge_title": "Large models (monthly)"})
# identity (not truthiness): keep_going=1 is neither False nor True
add(F, {"gauges": [GAUGE], "paused_reason": None, "keep_going_on_credits": 1, "cta": True},
    {"parse_gauges": L([GAUGE]), "all_scope": L([GAUGE]), "gauge_title": "Today"})

# 22. apply_header_gauge
F = "apply_header_gauge"
add(F, {"snapshot": SNAP, "credits_state": CSTATE}, {"gauge_title": "Today", "time.time": SEC})
add(F, {"snapshot": None, "credits_state": CSTATE}, {})
add(F, {"snapshot": dict(SNAP, usage_window=False), "credits_state": CSTATE}, {})
add(F, {"snapshot": SNAP, "credits_state": None}, {})
add(F, {"snapshot": SNAP, "credits_state": dict(CSTATE, from_header=False)}, {})
add(F, {"snapshot": SNAP, "credits_state": dict(CSTATE, gauges=[dict(GAUGE, used_bp=9000)])},
    {"gauge_title": "Today", "time.time": SEC})
add(F, {"snapshot": SNAP, "credits_state": dict(CSTATE, gauges=[dict(GAUGE, resets_at_ms=MS)])},
    {"gauge_title": "Today", "time.time": SEC})
add(F, {"snapshot": SNAP, "credits_state": dict(CSTATE, gauges=[dict(GAUGE, used_bp=10000)],
                                                 top_tier_gauge=dict(GAUGE_TT, used_bp=10000))},
    {"gauge_title": ["Today", "Large models (monthly)"], "time.time": SEC})
add(F, {"snapshot": SNAP, "credits_state": CSTATE}, {"gauge_title": R("SystemExit", "bye")})
add(F, {"snapshot": SNAP, "credits_state": CSTATE}, {"gauge_title": "Today", "time.time": R("KeyboardInterrupt", "ki")})
# top-tier line IS added: window label matches top_gauge title, raised to 100
TT_WIN = {"label": "Large models (monthly)", "used_percent": 25.0, "reset_at": US, "detail": None, "scope": "account"}
TT_G = {"id": "top_tier", "scope": "top_tier", "label": "Large models (monthly)", "used_bp": 10000, "resets_at_ms": MS_FUTURE}
add(F, {"snapshot": dict(SNAP, windows=[TT_WIN]),
        "credits_state": {"from_header": True, "is_usage_window": True, "gauges": [TT_G],
                          "top_tier_gauge": TT_G, "keep_going_on_credits": None}},
    {"gauge_title": "Large models (monthly)", "time.time": SEC})
# details already carry the top-tier line -> not duplicated (window raised 25->100)
add(F, {"snapshot": dict(SNAP, windows=[TT_WIN],
                         details=["Monthly large-model usage is used up. Resets Nov 18."]),
        "credits_state": {"from_header": True, "is_usage_window": True, "gauges": [TT_G],
                          "top_tier_gauge": TT_G, "keep_going_on_credits": False}},
    {"gauge_title": "Large models (monthly)", "time.time": SEC})

# 23. _nous_logged_in
F = "_nous_logged_in"
add(F, {}, {"get_provider_auth_state": {"access_token": "tok"}})
add(F, {}, {"get_provider_auth_state": {"access_token": "  "}})
add(F, {}, {"get_provider_auth_state": {"access_token": 5}})
add(F, {}, {"get_provider_auth_state": None})
add(F, {}, {"get_provider_auth_state": 5})
add(F, {}, {"get_provider_auth_state": R("RuntimeError", "x")})
add(F, {}, {"get_provider_auth_state": R("SystemExit", "bye")})
add(F, {}, {})

# 24. _nous_account_view
F = "_nous_account_view"
add(F, {"markdown": False, "timeout": 10.0, "credits_state": None, "nous_route": True},
    {"dev_fixture_credits_state": None, "get_provider_auth_state": None})
add(F, {"markdown": False, "timeout": 10.0, "credits_state": None, "nous_route": True},
    {"dev_fixture_credits_state": {"used_fraction": 0.5}, "snapshot_from_credits_state": SNAP,
     "render_account_usage_lines": L(["l1"])})
add(F, {"markdown": True, "timeout": 10.0, "credits_state": None, "nous_route": True},
    {"dev_fixture_credits_state": R("RuntimeError", "x"), "logger.debug": None,
     "get_provider_auth_state": {"access_token": "tok"},
     "fetch_portal_account": {"logged_in": True}, "build_nous_credits_snapshot": SNAP,
     "gauge_title": "Today", "time.time": SEC, "render_account_usage_lines": L(["l1"])})
add(F, {"markdown": False, "timeout": 10.0, "credits_state": None, "nous_route": True},
    {"dev_fixture_credits_state": None, "get_provider_auth_state": {"access_token": "tok"},
     "fetch_portal_account": R("RuntimeError", "x"), "logger.debug": None})
add(F, {"markdown": False, "timeout": 10.0, "credits_state": None, "nous_route": True},
    {"dev_fixture_credits_state": None, "get_provider_auth_state": {"access_token": "tok"},
     "fetch_portal_account": R("SystemExit", "bye")})
add(F, {"markdown": False, "timeout": 10.0, "credits_state": None, "nous_route": False},
    {"dev_fixture_credits_state": None, "get_provider_auth_state": {"access_token": "tok"},
     "fetch_portal_account": {"logged_in": True}, "build_nous_credits_snapshot": SNAP,
     "render_account_usage_lines": L(["l1"])})
add(F, {"markdown": False, "timeout": 10.0, "credits_state": None, "nous_route": True}, {})

# 25. nous_credits_view
F = "nous_credits_view"
add(F, {"markdown": False, "timeout": 10.0, "credits_state": None, "nous_route": True},
    {"dev_fixture_credits_state": None, "get_provider_auth_state": None})
add(F, {"markdown": False, "timeout": 10.0, "credits_state": None, "nous_route": True},
    {"dev_fixture_credits_state": {"used_fraction": 0.5}, "snapshot_from_credits_state": SNAP,
     "render_account_usage_lines": L(["l1"])})
add(F, {"markdown": False, "timeout": 10.0, "credits_state": None, "nous_route": True},
    {"dev_fixture_credits_state": None, "get_provider_auth_state": {"access_token": "tok"},
     "fetch_portal_account": R("RuntimeError", "x"), "logger.debug": None})
add(F, {"markdown": False, "timeout": 10.0, "credits_state": None, "nous_route": True},
    {"dev_fixture_credits_state": None, "get_provider_auth_state": {"access_token": "tok"},
     "fetch_portal_account": R("GeneratorExit", "ge")})
add(F, {"markdown": False, "timeout": 10.0, "credits_state": None, "nous_route": True}, {})

# 26. nous_credits_usage_fields
F = "nous_credits_usage_fields"
add(F, {"agent": None, "nous_route": True},
    {"dev_fixture_credits_state": None, "get_provider_auth_state": None})
add(F, {"agent": {"_credits_state": None}, "nous_route": True},
    {"dev_fixture_credits_state": {"used_fraction": 0.5}, "snapshot_from_credits_state": SNAP,
     "render_account_usage_lines": L(["l1"])})
add(F, {"agent": None, "nous_route": False},
    {"dev_fixture_credits_state": {"used_fraction": 0.5}, "snapshot_from_credits_state": SNAP,
     "render_account_usage_lines": L(["l1"])})
add(F, {"agent": None, "nous_route": True},
    {"dev_fixture_credits_state": None, "get_provider_auth_state": {"access_token": "tok"},
     "fetch_portal_account": R("RuntimeError", "x"), "logger.debug": None})
add(F, {"agent": None, "nous_route": True},
    {"dev_fixture_credits_state": None, "get_provider_auth_state": {"access_token": "tok"},
     "fetch_portal_account": R("SystemExit", "bye")})
add(F, {"agent": None, "nous_route": True}, {})

# 27. nous_usage_lines
F = "nous_usage_lines"
add(F, {"nous_route": True, "markdown": False, "credits_state": None}, {"nous_credits_lines": L(["l1"])})
add(F, {"nous_route": False, "markdown": False, "credits_state": None}, {"nous_credits_lines": L(["l1"])})
add(F, {"nous_route": False, "markdown": True, "credits_state": None}, {"nous_credits_lines": L(["l1"])})
add(F, {"nous_route": False, "markdown": False, "credits_state": None}, {"nous_credits_lines": L([])})
add(F, {"nous_route": True, "markdown": False, "credits_state": None}, {"nous_credits_lines": R("SystemExit", "bye")})
add(F, {"nous_route": True, "markdown": False, "credits_state": None}, {})

# 28. nous_usage_view
F = "nous_usage_view"
add(F, {"nous_route": True, "credits_state": None},
    {"dev_fixture_credits_state": None, "get_provider_auth_state": None,
     "dev_fixture_usage_model": None, "usage_model_from_account": None})
add(F, {"nous_route": True, "credits_state": None},
    {"dev_fixture_credits_state": {"used_fraction": 0.5}, "snapshot_from_credits_state": SNAP,
     "render_account_usage_lines": L(["l1"]), "dev_fixture_usage_model": {"model": "m"}})
add(F, {"nous_route": True, "credits_state": None},
    {"dev_fixture_credits_state": None, "get_provider_auth_state": {"access_token": "tok"},
     "fetch_portal_account": {"logged_in": True}, "build_nous_credits_snapshot": SNAP,
     "render_account_usage_lines": L(["l1"]), "dev_fixture_usage_model": None,
     "usage_model_from_account": {"model": "m"}})
add(F, {"nous_route": False, "credits_state": None},
    {"dev_fixture_credits_state": {"used_fraction": 0.5}, "snapshot_from_credits_state": SNAP,
     "render_account_usage_lines": L(["l1"])})
add(F, {"nous_route": False, "credits_state": None},
    {"dev_fixture_credits_state": None, "get_provider_auth_state": None})
add(F, {"nous_route": True, "credits_state": None},
    {"dev_fixture_credits_state": None, "get_provider_auth_state": {"access_token": "tok"},
     "fetch_portal_account": R("SystemExit", "bye")})
add(F, {"nous_route": True, "credits_state": None}, {})


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    # per-function counts
    from collections import Counter
    cnt = Counter(c["func"] for c in CASES)
    print("wrote %d cases to %s" % (len(CASES), OUT))
    for fn in sorted(cnt):
        print("  %-38s %d" % (fn, cnt[fn]))
    low = [fn for fn, n in cnt.items() if n < 5]
    if low:
        raise SystemExit("FUNCS WITH <5 CASES: %s" % low)


if __name__ == "__main__":
    main()