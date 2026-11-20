#!/usr/bin/env python3
"""Generate tests/fixtures/account_usage_flow2.jsonl for the au2 port.

Each line: {"func": <live_name>, "in": {...inputs..., "io": {seam scripts},
"state": {cache seed}}}. Script semantics (shared with the C harness dispatcher
and the oracle Script):
  - absent key            -> fail-closed RuntimeError "no script for <name>"
  - non-list value        -> constant result reused every call
  - list value            -> per-call queue (exhausted -> fail-closed)
  - {"__raise__":K,"message":M,...} -> raised exception of kind K
  - {"__script__":true,"ret":X} -> list-valued result X (queue-ambiguity wrap)
  - null                  -> None result
>=5 cases per function; covers branches, seam order, raises, BaseException
passthrough and fail-closed paths. ASCII inputs; carrier scripts null or
non-empty objects; snapshot carriers full-field; fetched_at integer epoch-us.
"""
import hashlib
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT = HERE / "fixtures" / "account_usage_flow2.jsonl"

CASES = []


def add(func, inp, io=None, state=None):
    inp = dict(inp)
    inp["io"] = io or {}
    if state is not None:
        inp["state"] = state
    CASES.append({"func": func, "in": inp})


def R(kind, msg=""):
    return {"__raise__": kind, "message": msg}


def RH(code, msg="http err"):
    return {"__raise__": "HTTPStatusError", "message": msg, "status_code": code}


def L(ret):
    return {"__script__": True, "ret": ret}


def keystr(key):
    return "".join(f"{len(s)}:{s}" for s in key)


def sha16(token):
    return hashlib.sha256(token.encode("utf-8")).hexdigest()


US = 1731974400000000          # 2024-11-19T00:00:00Z
NOW = US / 1e6                 # 1731974400.0
WIN = {"label": "Today", "used_percent": 25.0, "reset_at": US, "detail": None, "scope": "account"}
SNAP = {"provider": "nous", "source": "portal-account", "fetched_at": US, "title": "Nous credits",
        "plan": None, "windows": [WIN], "details": ["d"], "unavailable_reason": None,
        "identity": None, "raw": None, "usage_window": False}
KEY4 = ["/h", "nous", "", ""]
KS = keystr(KEY4)
KEY_ID = ["/h", "nous", "id1", ""]
KS_ID = keystr(KEY_ID)
KEY_ID2 = ["/h", "nous", "id2", ""]
KS_ID2 = keystr(KEY_ID2)
KEY_COD = ["/h", "nous", "codex:a:b", ""]
KS_COD = keystr(KEY_COD)
CM = {"id": "cm"}
CLIENT = {"id": "cl"}
RESP = {"id": "r"}
CTX = {"id": "ctx"}
POOL = {"id": "pool"}
FUT = {"id": "fut"}
THREAD = {"id": "t"}
READ = {"fn": "get_nous_portal_account_info", "force_fresh": True}
FETCHERS = {"openai-codex": 1, "anthropic": 1, "openrouter": 1}

# 1. _plural
F = "_plural"
for c in [1, 0, 2, True, False, 1.0, "1", None, 3.5, []]:
    add(F, {"count": c})

# 2. _get_json
F = "_get_json"
GJ = {"httpx.Client": CM, "client.__enter__": CLIENT, "client.get": RESP,
      "response.raise_for_status": None, "client.__exit__": None, "response.json": {"a": 1}}
U = {"url": "https://x.com/usage", "headers": {"A": "1"}, "timeout": 15.0}
add(F, U, GJ)
add(F, U, dict(GJ, **{"response.json": None}))
add(F, U, dict(GJ, **{"response.json": {}}))
add(F, U, dict(GJ, **{"response.json": L([1])}))
add(F, U, dict(GJ, **{"response.raise_for_status": RH(401)}))
add(F, U, dict(GJ, **{"response.raise_for_status": RH(500), "client.__exit__": True}))
add(F, U, dict(GJ, **{"client.get": R("RuntimeError", "g")}))
add(F, U, dict(GJ, **{"client.get": R("RuntimeError", "g"), "client.__exit__": True}))
add(F, U, dict(GJ, **{"httpx.Client": R("SystemExit", "bye")}))
add(F, U, dict(GJ, **{"client.__enter__": R("ValueError", "e")}))
add(F, U, dict(GJ, **{"response.json": R("KeyboardInterrupt", "ki")}))
add(F, U, {})
add(F, U, dict(GJ, **{"client.__enter__": None}))
add(F, U, dict(GJ, **{"client.get": None}))
add(F, U, dict(GJ, **{"response.raise_for_status": RH(401), "client.__exit__": R("RuntimeError", "exit")}))

# 3. _data
F = "_data"
DA = {"client.get": RESP, "response.raise_for_status": None, "response.json": {"data": {"x": 1}}}
D = {"client": CLIENT, "normalized": "https://x.com", "path": "credits", "headers": {"A": "1"}}
add(F, D, DA)
add(F, D, dict(DA, **{"response.json": {"data": None}}))
add(F, D, dict(DA, **{"response.json": {}}))
add(F, D, dict(DA, **{"response.json": None}))
add(F, D, dict(DA, **{"response.json": {"data": L([1])}}))
add(F, D, dict(DA, **{"response.json": L([1])}))
add(F, D, dict(DA, **{"response.json": "x"}))
add(F, D, dict(DA, **{"response.json": 5}))
add(F, D, dict(DA, **{"client.get": R("RuntimeError", "g")}))
add(F, D, dict(DA, **{"response.raise_for_status": RH(403)}))
add(F, D, dict(DA, **{"response.json": R("SystemExit", "bye")}))
add(F, dict(D, client=None), DA)
add(F, D, {})
add(F, D, dict(DA, **{"response.json": {"data": {}}}))
add(F, D, dict(DA, **{"response.json": {"data": 0}}))

# 4. _fetch_portal_account
F = "_fetch_portal_account"
FP = {"contextvars.copy_context": CTX, "DaemonThreadPoolExecutor": POOL, "partial": READ,
      "pool.submit": FUT, "future.result": {"ok": 1}, "pool.shutdown": None}
P = {"timeout": 10.0, "stored_token_only": False}
add(F, P, FP)
add(F, dict(P, stored_token_only=True), {k: v for k, v in FP.items() if k != "partial"})
add(F, P, dict(FP, **{"future.result": R("RuntimeError", "r"), "future.add_done_callback": None}))
add(F, P, dict(FP, **{"future.result": R("SystemExit", "bye"), "future.add_done_callback": None}))
add(F, P, dict(FP, **{"future.result": R("KeyboardInterrupt", "ki"), "future.add_done_callback": None}))
add(F, P, dict(FP, **{"future.result": R("GeneratorExit", "ge"), "future.add_done_callback": None}))
add(F, P, dict(FP, **{"pool.submit": R("ValueError", "s")}))
add(F, P, dict(FP, **{"contextvars.copy_context": R("SystemExit", "bye")}))
add(F, P, dict(FP, **{"DaemonThreadPoolExecutor": R("RuntimeError", "p")}))
add(F, P, dict(FP, **{"partial": R("RuntimeError", "pa")}))
add(F, P, dict(FP, **{"pool.shutdown": R("RuntimeError", "sd")}))
add(F, P, dict(FP, **{"future.result": R("ValueError", "r"),
                     "future.add_done_callback": R("RuntimeError", "cb")}))
add(F, P, {})
add(F, P, dict(FP, **{"contextvars.copy_context": None}))
add(F, P, dict(FP, **{"pool.submit": None, "pool.shutdown": None}))
add(F, P, dict(FP, **{"future.result": None}))

# 5. _key
F = "_key"
add(F, {"provider": "nous", "identity_id": None, "base_url": None}, {"hermes_home_key": "/h"})
add(F, {"provider": "  OpenAI-Codex  ", "identity_id": " codex:a:b ", "base_url": " https://x.com/ "},
    {"hermes_home_key": "/h"})
add(F, {"provider": None, "identity_id": None, "base_url": None}, {"hermes_home_key": "/h"})
add(F, {"provider": "", "identity_id": "", "base_url": ""}, {"hermes_home_key": "/h"})
add(F, {"provider": "nous", "identity_id": None, "base_url": "https://x.com///"}, {"hermes_home_key": "/h"})
add(F, {"provider": "nous", "identity_id": None, "base_url": None}, {"hermes_home_key": R("SystemExit", "bye")})
add(F, {"provider": "nous", "identity_id": None, "base_url": None}, {})
add(F, {"provider": "NOUS", "identity_id": None, "base_url": None}, {"hermes_home_key": "/h"})

# 6. has_account_usage
F = "has_account_usage"
add(F, {"provider": "anthropic"}, {"usage_fetchers": FETCHERS})
add(F, {"provider": "  NOUS  "}, {"usage_fetchers": FETCHERS, "get_provider_profile": {"hook": "custom"}})
add(F, {"provider": "nous"}, {"usage_fetchers": FETCHERS, "get_provider_profile": {"hook": "base"}})
add(F, {"provider": "nous"}, {"usage_fetchers": FETCHERS, "get_provider_profile": None})
add(F, {"provider": ""}, {"usage_fetchers": FETCHERS})
add(F, {"provider": None}, {"usage_fetchers": FETCHERS})
add(F, {"provider": "nous"}, {"usage_fetchers": None})
add(F, {"provider": "nous"}, {"usage_fetchers": FETCHERS, "get_provider_profile": R("SystemExit", "bye")})
add(F, {"provider": "nous"}, {})
add(F, {"provider": "openai-codex"}, {"usage_fetchers": FETCHERS})
add(F, {"provider": "nous"}, {"usage_fetchers": R("RuntimeError", "f")})

# 7. _identity_id_for
F = "_identity_id_for"
add(F, {"provider": "nous", "entry": {"runtime_api_key": "tok"}}, {"sha256_hexdigest": sha16("tok")})
add(F, {"provider": "openai-codex", "entry": {"runtime_api_key": "tok"}},
    {"codex_principal_identity": L(["a", "b"])})
add(F, {"provider": "openai-codex", "entry": {"runtime_api_key": "tok"}},
    {"codex_principal_identity": None, "sha256_hexdigest": sha16("tok")})
add(F, {"provider": "nous", "entry": {"id": "e1"}}, {})
add(F, {"provider": "nous", "entry": {"id": None}}, {})
add(F, {"provider": "nous", "entry": None}, {})
add(F, {"provider": "nous", "entry": {"runtime_api_key": "  "}}, {})
add(F, {"provider": "openai-codex", "entry": {"runtime_api_key": "tok"}},
    {"codex_principal_identity": L(["only"])})
add(F, {"provider": "openai-codex", "entry": {"runtime_api_key": "tok"}},
    {"codex_principal_identity": R("SystemExit", "bye")})
add(F, {"provider": "nous", "entry": {"runtime_api_key": "tok"}},
    {"sha256_hexdigest": R("RuntimeError", "h")})
add(F, {"provider": "nous", "entry": {"runtime_api_key": "tok"}}, {})
add(F, {"provider": "openai-codex", "entry": {"runtime_api_key": "tok"}},
    {"codex_principal_identity": L([]), "sha256_hexdigest": sha16("tok")})
add(F, {"provider": "nous", "entry": {"runtime_api_key": 5}}, {"sha256_hexdigest": sha16("5")})

# 8. remember_account_usage
F = "remember_account_usage"
SNAP_ID = dict(SNAP, identity="codex:a:b")
add(F, {"provider": "nous", "snapshot": SNAP, "identity_id": None, "base_url": None,
        "started_monotonic": 100.0}, {"hermes_home_key": "/h"}, {})
add(F, {"provider": "nous", "snapshot": SNAP_ID, "identity_id": None, "base_url": None,
        "started_monotonic": 100.0}, {"hermes_home_key": "/h"}, {})
add(F, {"provider": "nous", "snapshot": SNAP, "identity_id": "id1", "base_url": None,
        "started_monotonic": 100.0}, {"hermes_home_key": "/h"}, {})
add(F, {"provider": "nous", "snapshot": dict(SNAP, windows=[]), "identity_id": None,
        "base_url": None, "started_monotonic": 100.0}, {"hermes_home_key": "/h"}, {})
add(F, {"provider": "nous", "snapshot": None, "identity_id": None, "base_url": None,
        "started_monotonic": 100.0}, {}, {})
add(F, {"provider": "", "snapshot": SNAP, "identity_id": None, "base_url": None,
        "started_monotonic": 100.0}, {}, {})
add(F, {"provider": "nous", "snapshot": SNAP, "identity_id": None, "base_url": None,
        "started_monotonic": None}, {"time.monotonic": 50.0, "hermes_home_key": "/h"}, {})
add(F, {"provider": "nous", "snapshot": SNAP, "identity_id": None, "base_url": None,
        "started_monotonic": 100.0}, {"hermes_home_key": "/h"}, {"started": {KS: 200.0}})
add(F, {"provider": "nous", "snapshot": SNAP, "identity_id": None, "base_url": None,
        "started_monotonic": 100.0}, {"hermes_home_key": "/h"},
    {"started": {KS: 100.0}, "snapshots": {KS: SNAP}})
add(F, {"provider": "nous", "snapshot": SNAP, "identity_id": None, "base_url": None,
        "started_monotonic": 100.0}, {"hermes_home_key": R("SystemExit", "bye")}, {})
add(F, {"provider": "nous", "snapshot": SNAP, "identity_id": None, "base_url": None,
        "started_monotonic": None}, {"time.monotonic": R("RuntimeError", "m")}, {})
add(F, {"provider": "nous", "snapshot": SNAP, "identity_id": None, "base_url": None,
        "started_monotonic": None}, {}, {})
add(F, {"provider": "nous", "snapshot": dict(SNAP, identity="codex:x:y"), "identity_id": "id1",
        "base_url": None, "started_monotonic": 100.0}, {"hermes_home_key": "/h"}, {})
add(F, {"provider": "nous", "snapshot": SNAP, "identity_id": None, "base_url": "https://x.com/",
        "started_monotonic": 100.0}, {"hermes_home_key": "/h"}, {})

# 9. cached_account_usage
F = "cached_account_usage"
add(F, {"provider": "nous", "identity_id": None, "base_url": None},
    {"hermes_home_key": "/h"}, {"snapshots": {KS: SNAP}})
add(F, {"provider": "nous", "identity_id": None, "base_url": None}, {"hermes_home_key": "/h"}, {})
add(F, {"provider": "  NOUS ", "identity_id": None, "base_url": None},
    {"hermes_home_key": "/h"}, {"snapshots": {KS: SNAP}})
add(F, {"provider": "nous", "identity_id": "id1", "base_url": None},
    {"hermes_home_key": "/h"}, {"snapshots": {KS: SNAP}})
add(F, {"provider": "nous", "identity_id": None, "base_url": None},
    {"hermes_home_key": R("SystemExit", "bye")}, {})
add(F, {"provider": "nous", "identity_id": None, "base_url": None}, {}, {})

# 10. snapshot_is_stale
F = "snapshot_is_stale"


def stale_snap(fa):
    return dict(SNAP, fetched_at=fa)


add(F, {"snapshot": stale_snap(US)}, {"time.time": NOW})
add(F, {"snapshot": stale_snap(US)}, {"time.time": NOW + 1000})
add(F, {"snapshot": stale_snap(US)}, {"time.time": NOW - 10})
add(F, {"snapshot": stale_snap(US)}, {"time.time": NOW + 900})
add(F, {"snapshot": None}, {})
add(F, {"snapshot": "x"}, {})
add(F, {"snapshot": stale_snap(None)}, {"time.time": NOW})
add(F, {"snapshot": stale_snap(R("ValueError", "v"))}, {"time.time": NOW})
add(F, {"snapshot": stale_snap(R("FileNotFoundError", "f"))}, {"time.time": NOW})
add(F, {"snapshot": stale_snap(R("RuntimeError", "r"))}, {"time.time": NOW})
add(F, {"snapshot": stale_snap(R("SystemExit", "bye"))}, {"time.time": NOW})
add(F, {"snapshot": stale_snap(R("OverflowError", "o"))}, {"time.time": NOW})
add(F, {"snapshot": stale_snap(US)}, {"time.time": R("KeyboardInterrupt", "ki")})
add(F, {"snapshot": stale_snap(US)}, {})
add(F, {"snapshot": stale_snap(R("UnicodeDecodeError", "u"))}, {"time.time": NOW})
add(F, {"snapshot": stale_snap(R("NotImplementedError", "n"))}, {"time.time": NOW})

# 11. _claim_refresh
F = "_claim_refresh"
add(F, {"key": KEY4, "now": 100.0}, {}, {})
add(F, {"key": KEY4, "now": 100.0}, {}, {"inflight": [KS]})
add(F, {"key": KEY4, "now": 100.0}, {}, {"last_try": {KS: 50.0}})
add(F, {"key": KEY4, "now": 200.0}, {}, {"last_try": {KS: 0.0}})
add(F, {"key": KEY4, "now": 220.0}, {}, {"last_try": {KS: 100.0}})
add(F, {"key": KEY4, "now": 219.9}, {}, {"last_try": {KS: 100.0}})

# 12. _release_refresh
F = "_release_refresh"
add(F, {"key": KEY4}, {}, {"inflight": [KS]})
add(F, {"key": KEY4}, {}, {"inflight": []})
add(F, {"key": KEY4}, {}, {"inflight": [KS, KS_ID]})
add(F, {"key": KEY4}, {}, {})
add(F, {"key": KEY4}, {}, {"inflight": [KS_ID]})

# 13. refresh_account_usage_async
F = "refresh_account_usage_async"
RA = {"time.monotonic": 100.0, "hermes_home_key": "/h", "contextvars.copy_context": CTX,
      "threading.Thread": THREAD, "thread.start": None}
add(F, {"providers": ["nous"]}, RA, {})
add(F, {"providers": ["nous", "nous"]}, RA, {})
add(F, {"providers": ["nous", "anthropic"]}, RA, {})
add(F, {"providers": []}, RA, {})
add(F, {"providers": ["nous"]}, RA, {"inflight": [KS]})
add(F, {"providers": ["nous"]}, RA, {"last_try": {KS: 50.0}})
add(F, {"providers": ["nous"]}, dict(RA, **{"time.monotonic": R("SystemExit", "bye")}), {})
add(F, {"providers": ["nous"]}, dict(RA, **{"hermes_home_key": R("RuntimeError", "h")}), {})
add(F, {"providers": ["nous"]}, dict(RA, **{"threading.Thread": R("RuntimeError", "t")}), {})
add(F, {"providers": ["nous"]}, dict(RA, **{"thread.start": R("KeyboardInterrupt", "ki")}), {})
add(F, {"providers": ["nous"]}, dict(RA, **{"contextvars.copy_context": R("RuntimeError", "c")}), {})
add(F, {"providers": ["nous"]}, {}, {})
add(F, {"providers": ["nous"]}, dict(RA, **{"threading.Thread": None}), {})
add(F, {"providers": ["nous"]}, dict(RA, **{"time.monotonic": 200.0}), {"last_try": {KS: 0.0}})

# 14. _refresh_legacy
F = "_refresh_legacy"
add(F, {"provider": "nous", "key": KEY4}, {"fetch_account_usage": None}, {"inflight": [KS]})
add(F, {"provider": "nous", "key": KEY4}, {"fetch_account_usage": R("RuntimeError", "f")}, {"inflight": [KS]})
add(F, {"provider": "nous", "key": KEY4}, {"fetch_account_usage": R("SystemExit", "bye")}, {"inflight": [KS]})
add(F, {"provider": "nous", "key": KEY4}, {"fetch_account_usage": None}, {})
add(F, {"provider": "nous", "key": KEY4}, {}, {"inflight": [KS]})
add(F, {"provider": "nous", "key": KEY4}, {"fetch_account_usage": SNAP}, {"inflight": [KS]})

# 15. refresh_account_usage_entries_async
F = "refresh_account_usage_entries_async"
REQ = {"provider": "nous", "identity_id": "id1", "api_key": "k", "base_url": "https://x.com"}
REQ2 = {"provider": "nous", "identity_id": "id2", "api_key": "k2", "base_url": None}
EA = {"time.monotonic": 100.0, "hermes_home_key": "/h", "contextvars.copy_context": CTX,
      "threading.Thread": THREAD, "thread.start": None}
add(F, {"requests": [REQ]}, EA, {})
add(F, {"requests": [REQ, REQ]}, EA, {})
add(F, {"requests": [REQ, REQ2]}, EA, {})
add(F, {"requests": []}, EA, {})
add(F, {"requests": [{"provider": "", "identity_id": "id", "api_key": "k"}]}, EA, {})
add(F, {"requests": [{"provider": "nous", "identity_id": "", "api_key": "k"}]}, EA, {})
add(F, {"requests": [{"provider": "nous", "identity_id": "id", "api_key": ""}]}, EA, {})
add(F, {"requests": [REQ]}, EA, {"inflight": [KS_ID]})
add(F, {"requests": [REQ]}, dict(EA, **{"time.monotonic": R("SystemExit", "bye")}), {})
add(F, {"requests": [REQ]}, dict(EA, **{"hermes_home_key": R("RuntimeError", "h")}), {})
add(F, {"requests": [REQ]}, dict(EA, **{"threading.Thread": R("RuntimeError", "t")}), {})
add(F, {"requests": [REQ]}, {}, {})
add(F, {"requests": [REQ]}, dict(EA, **{"time.monotonic": 100.0}), {"last_try": {KS_ID: 50.0}})
add(F, {"requests": [REQ]}, dict(EA, **{"thread.start": R("GeneratorExit", "ge")}), {})

# 16. _refresh_entries
F = "_refresh_entries"
RE = {"fetch_account_usage": None, "hermes_home_key": "/h"}
add(F, {"requests": [REQ]}, RE, {"inflight": [KS_ID]})
add(F, {"requests": [REQ]}, {"fetch_account_usage": R("RuntimeError", "f"), "hermes_home_key": "/h"},
    {"inflight": [KS_ID]})
add(F, {"requests": [REQ]}, {"fetch_account_usage": R("SystemExit", "bye"), "hermes_home_key": "/h"},
    {"inflight": [KS_ID]})
add(F, {"requests": [REQ, REQ2]}, RE, {"inflight": [KS_ID, KS_ID2]})
add(F, {"requests": [{}]}, RE, {"inflight": [KS_ID]})
add(F, {"requests": [{"provider": "nous"}]}, RE, {"inflight": [KS_ID]})
add(F, {"requests": [REQ, REQ2]},
    {"fetch_account_usage": [R("ValueError", "v"), None], "hermes_home_key": "/h"},
    {"inflight": [KS_ID, KS_ID2]})
add(F, {"requests": [REQ]}, {"fetch_account_usage": None, "hermes_home_key": R("RuntimeError", "h")},
    {"inflight": [KS_ID]})
add(F, {"requests": [REQ]}, {}, {"inflight": [KS_ID]})
add(F, {"requests": []}, RE, {"inflight": [KS_ID]})
add(F, {"requests": [dict(REQ, base_url=None)]}, RE, {"inflight": [KS_ID]})


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
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