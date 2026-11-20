#!/usr/bin/env python3
"""Generate tests/fixtures/auth_flow2.jsonl — scripted-seam FLOW cases for the
auth_flow2 port (the still-UNCLAIMED remainder of hermes_cli/auth.py:
primary_failure_wording, OAuthProviderFlow.is_terminal_refresh_error,
_resolve_nous_access_token).

Every case: {"func": <harness name>, "in": {... inputs ..., "seams": {name: script}}}
A seam script is a single result, or a JSON array = one result PER CALL (last
repeats). {"__raise__": cls, "msg": m} raises that exception class in the live
drive and is delivered as a raise descriptor to the C universal seam. A MISSING
seam key fail-closes (RuntimeError) on BOTH sides; a present JSON null is a
valid None result.

Domains (documented in the port header):
- ASCII strings; JSON integral numbers only (no integral-float literals in
  type-name-sensitive positions).
- AuthError doubles: {"kind":"AuthError","message","provider","code",
  "relogin_required"}; the oracle materialises the REAL extracted AuthError
  (RuntimeError subclass), the C port reads the fields.
- txn_enter scripts a 3-array [auth_store, state, state_source_path]; the port
  works on copies so per-case mutations never leak.
- httpx_client_enter scripts the client VALUE passed to the refresh seams.
- apply_nous_refreshed_tokens scripts {"access_token": ...}; both the live
  stub and the C port write it into state (the only post-apply field read).
Run: python tests/gen_auth_flow2_cases.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "auth_flow2.jsonl")

CASES = []


def add(func, inp, seams):
    inp = dict(inp)
    inp["seams"] = seams
    CASES.append({"func": func, "in": inp})


def rs(cls, msg=""):
    return {"__raise__": cls, "msg": msg}


def aerr(message="boom", provider="", code=None, relogin=False):
    return {"kind": "AuthError", "message": message, "provider": provider,
            "code": code, "relogin_required": relogin}


# ══════════════ primary_failure_wording (no seams) ══════════════
add("primary_failure_wording", {"exc": aerr("429", code="codex_rate_limited")}, {})
add("primary_failure_wording", {"exc": aerr("429", provider="nous", code="codex_rate_limited")}, {})
add("primary_failure_wording", {"exc": aerr("429", code="codex_rate_limited", relogin=True)}, {})
add("primary_failure_wording", {"exc": aerr("bad", code="other_code")}, {})
add("primary_failure_wording", {"exc": aerr("no code")}, {})
add("primary_failure_wording", {"exc": {"kind": "ValueError", "message": "x"}}, {})
add("primary_failure_wording", {"exc": {"kind": "KeyboardInterrupt", "message": ""}}, {})
add("primary_failure_wording", {"exc": None}, {})
add("primary_failure_wording", {"exc": {"kind": "RuntimeError", "message": "boom"}}, {})
add("primary_failure_wording", {"exc": aerr("429", code="codex_rate_limited", relogin=False)}, {})
add("primary_failure_wording", {"exc": aerr("x", code="codex_rate_limited_2")}, {})
add("primary_failure_wording", {"exc": {"kind": "TypeError", "message": "t"}}, {})


# ══════════════ is_terminal_refresh_error (no seams; OAuthProviderFlow method) ══════════════
def itref(pid="nous", codes=None, exc=None):
    return {"self": {"provider_id": pid, "terminal_refresh_codes": codes or []},
            "exc": exc if exc is not None else aerr()}


add("is_terminal_refresh_error", itref(
    codes=["invalid_grant", "refresh_token_reused"],
    exc=aerr("dead", provider="nous", code="invalid_grant", relogin=True)), {})
add("is_terminal_refresh_error", itref(
    codes=["invalid_grant"],
    exc=aerr("not dead", provider="nous", code="transient_error", relogin=True)), {})
add("is_terminal_refresh_error", itref(
    codes=["invalid_grant"],
    exc=aerr("other provider", provider="xai-oauth", code="invalid_grant", relogin=True)), {})
add("is_terminal_refresh_error", itref(
    codes=["invalid_grant"],
    exc=aerr("no relogin", provider="nous", code="invalid_grant", relogin=False)), {})
add("is_terminal_refresh_error", itref(
    codes=["invalid_grant"],
    exc={"kind": "ValueError", "message": "not an AuthError"}), {})
add("is_terminal_refresh_error", itref(codes=[], exc=aerr(
    "empty", provider="nous", code="invalid_grant", relogin=True)), {})
add("is_terminal_refresh_error", itref(
    codes=["a", "b"],
    exc=aerr("truthy str", provider="nous", code="b", relogin="yes")), {})
add("is_terminal_refresh_error", itref(
    codes=["a"], exc=aerr("none code", provider="nous", code=None, relogin=True)), {})
add("is_terminal_refresh_error", itref(codes=["a"], exc=None), {})
add("is_terminal_refresh_error", itref(
    pid="", codes=[""], exc=aerr("empty ids", provider="", code="", relogin=True)), {})
add("is_terminal_refresh_error", itref(
    codes=["invalid_grant"],
    exc=aerr("429", provider="nous", code="codex_rate_limited", relogin=True)), {})
add("is_terminal_refresh_error", itref(
    codes=["invalid_grant"],
    exc={"kind": "KeyboardInterrupt", "message": ""}), {})


# ══════════════ _resolve_nous_access_token ══════════════
STATE = {
    "client_id": "cli-1",
    "access_token": "tok-valid",
    "refresh_token": "rt-1",
    "expires_at": "2099-01-01T00:00:00Z",
}
EMPTY_STATE = {"client_id": "cli-1"}  # non-empty (truthy) but no tokens


def base(**over):
    """Script EVERY seam the flow can fire; override per case."""
    s = {
        "hermes_home_key": "hk1",
        "cache_lock_enter": None,
        "cache_lock_exit": None,
        "cache_get": None,
        "monotonic": 100.0,
        "cache_set": None,
        "txn_enter": {"__arr__": [{"id": "store"}, dict(STATE), "/auth.json"]},
        "txn_exit": None,
        "nous_portal_base_url": "https://portal.example",
        "resolve_verify": {"verify": True},
        "shared_lock_enter": None,
        "shared_lock_exit": None,
        "import_anon_auth": None,
        "is_guest_state": False,
        "held_connectors_token": None,
        "httpx_client_enter": "client-1",
        "httpx_client_exit": None,
        "refresh_guest_state": "minted-tok",
        "write_shared_nous_state": None,
        "save_provider_state_to_source": None,
        "merge_shared_nous_oauth_state": False,
        "is_expiring": False,
        "refresh_nous_or_quarantine": {"access_token": "refreshed-tok"},
        "apply_nous_refreshed_tokens": {"access_token": "refreshed-tok"},
        "tls_state_from_verify": {"verify": "tls"},
    }
    s.update(over)
    return s


def rn(**over):
    return {"timeout_seconds": 15.0, "insecure": False, "ca_bundle": None,
            "refresh_skew_seconds": 120}


# memo hit (fresh) — only 5 seams fire, no transaction
add("resolve_nous_access_token", rn(), base(
    cache_get={"__arr__": [100.0, "cached-tok"]}, monotonic=101.0))
# memo hit with a 4.99s age (boundary still fresh)
add("resolve_nous_access_token", rn(), base(
    cache_get={"__arr__": [100.0, "cached-2"]}, monotonic=104.99))
# stale memo -> fall through to guest held-token fast path
add("resolve_nous_access_token", rn(), base(
    cache_get={"__arr__": [100.0, "old-tok"]}, monotonic=200.0,
    is_guest_state=True, held_connectors_token="held-tok"))
# guest with no held token -> mint connectors token
add("resolve_nous_access_token", rn(), base(
    is_guest_state=True, held_connectors_token=None))
# guest mint with timeout 0 -> httpx.Timeout(0 or 15) and lock_timeout clamp
add("resolve_nous_access_token", rn(timeout_seconds=0), base(
    is_guest_state=True, held_connectors_token=None))
# non-guest, non-expiring valid token, no shared merge -> no persist
add("resolve_nous_access_token", rn(), base(is_expiring=False,
    merge_shared_nous_oauth_state=False))
# non-guest, non-expiring valid token, merged shared -> persist fires once
add("resolve_nous_access_token", rn(), base(is_expiring=False,
    merge_shared_nous_oauth_state=True))
# non-guest, expiring, valid refresh -> full refresh + persist + memo
add("resolve_nous_access_token", rn(), base(is_expiring=True,
    refresh_nous_or_quarantine={"access_token": "refreshed-tok"},
    apply_nous_refreshed_tokens={"access_token": "refreshed-tok"}))
# non-memoable via insecure=True (no cache seams fire)
add("resolve_nous_access_token", rn(insecure=True), base(
    is_guest_state=True, held_connectors_token="held-insecure"))
# non-memoable via ca_bundle set
add("resolve_nous_access_token", rn(ca_bundle="ca.pem"), base(
    is_guest_state=True, held_connectors_token="held-cab"))
# missing state -> AuthError nous_auth_missing
add("resolve_nous_access_token", rn(), base(
    txn_enter={"__arr__": [{"id": "store"}, {}, "/auth.json"]}))
# missing access token -> AuthError nous_auth_missing_access_token
add("resolve_nous_access_token", rn(), base(
    txn_enter={"__arr__": [{"id": "store"}, dict(EMPTY_STATE), "/auth.json"]}))
# expiring + missing refresh token -> AuthError nous_auth_missing_refresh_token
add("resolve_nous_access_token", rn(), base(
    txn_enter={"__arr__": [{"id": "store"}, {"client_id": "c", "access_token": "tok",
                                        "expires_at": "2099-01-01T00:00:00Z"}, "/auth.json"]},
    is_expiring=True))
# txn enter raises -> propagates, no CM exited
add("resolve_nous_access_token", rn(), base(txn_enter=rs("RuntimeError", "tx boom")))
# shared store lock enter raises -> txn_exit fires on unwind
add("resolve_nous_access_token", rn(), base(shared_lock_enter=rs("ValueError", "lock boom")))
# import raises ImportError -> shared_lock_exit + txn_exit fire
add("resolve_nous_access_token", rn(), base(import_anon_auth=rs("ImportError", "no anon")))
# is_guest_state raises OSError -> exits fire
add("resolve_nous_access_token", rn(), base(is_guest_state=rs("OSError", "guest boom")))
# BaseException passthrough: KeyboardInterrupt is NOT an Exception
add("resolve_nous_access_token", rn(), base(is_guest_state=rs("KeyboardInterrupt", "")))
# httpx_client EXIT raise overrides a body raise (ValueError -> RuntimeError)
add("resolve_nous_access_token", rn(), base(
    is_guest_state=True, held_connectors_token=None,
    refresh_guest_state=rs("ValueError", "body boom"),
    httpx_client_exit=rs("RuntimeError", "exit boom")))
# refresh_nous_or_quarantine raises -> httpx_client_exit then outer exits fire
add("resolve_nous_access_token", rn(), base(
    is_expiring=True,
    refresh_nous_or_quarantine=rs("ConnectionError", "refresh boom")))
# verify null -> verify arg dropped from httpx_client_enter / tls_state_from_verify
add("resolve_nous_access_token", rn(), base(
    resolve_verify=None, is_expiring=True))
# per-call array script: held_connectors_token first None, then a token
add("resolve_nous_access_token", rn(), base(
    is_guest_state=True, held_connectors_token=[None, "held-2"]))


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print("wrote %d cases (%d funcs) to %s" % (len(CASES), len(counts), OUT))
    thin = {k: v for k, v in counts.items() if v < 5}
    if thin:
        print("WARNING <5 cases: %s" % thin)


if __name__ == "__main__":
    main()
