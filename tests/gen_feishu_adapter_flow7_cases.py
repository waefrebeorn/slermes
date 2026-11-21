#!/usr/bin/env python3
"""Generate tests/fixtures/feishu_adapter_flow7.jsonl — input cases for the
Feishu adapter proxy/onboarding/reaction/batch FLOW oracle.

Every case is hand-scripted: in.io maps a seam name to a result (plain JSON
value, an ordered LIST of results, or {"__script__":true,"raise":{type,message}})
that the C universal seam and the oracle's recording stubs replay identically.
The corpus covers every ported branch, ordered multi-seam traces, short-circuit
order, fail-closed missing/exhausted scripts, and the full builtin exception
hierarchy (Exception-side swallowed where the live def has try/except,
BaseException-side propagated verbatim).

Run `python tests/gen_feishu_adapter_flow7_cases.py` to regenerate; the script
fails if any target has fewer than 6 cases, a target is missing, or a fixture
io key has no matching C seam.
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "feishu_adapter_flow7.jsonl"
CASES = []


def add(func, args, io=None, note=None):
    case = {"func": func, "in": {"args": args, "io": dict(io or {})}}
    if note:
        case["_note"] = note
    CASES.append(case)


def rais(kind, msg):
    return {"__script__": True, "raise": {"type": kind, "message": msg}}


def ret(v):
    return {"__script__": True, "ret": v}


# ── __getattr__ (seams: _target, getattr_target) ──
add("__getattr__", {"name": "x"}, io={"_target": [ret("T1")], "getattr_target": [ret("V1")]})
add("__getattr__", {"name": ""}, io={"_target": [ret("T2")], "getattr_target": [ret("")]})
add("__getattr__", {"name": "n"}, io={"_target": [ret({"a": 1})], "getattr_target": [ret(None)]})
add("__getattr__", {"name": "x"}, io={"_target": [rais("ValueError", "target down")]})
add("__getattr__", {"name": "x"},
    io={"_target": [ret("T")], "getattr_target": [rais("AttributeError", "no attr")]})
add("__getattr__", {"name": "x"},
    io={"_target": [ret("T")], "getattr_target": [rais("SystemExit", "sx")]})
add("__getattr__", {"name": "x"}, io={"_target": []}, note="fail-closed")  # no script

# ── __repr__ (seams: _target, repr_target) ──
add("__repr__", {}, io={"_target": [ret("T1")], "repr_target": [ret("<Inst>")]})
add("__repr__", {}, io={"_target": [ret("T2")], "repr_target": [ret("")]})
add("__repr__", {}, io={"_target": [ret(7)], "repr_target": [ret("<N>")]})
add("__repr__", {}, io={"_target": [rais("RuntimeError", "target down")]})
add("__repr__", {}, io={"_target": [ret("T")], "repr_target": [rais("ValueError", "repr down")]})
add("__repr__", {}, io={"_target": [ret("T")], "repr_target": [rais("KeyboardInterrupt", "ki")]})
add("__repr__", {}, io={"_target": []}, note="fail-closed")

# ── _accounts_base_url (pure) ──
for d in ["feishu", "lark", "other", "", None, 7, "LARK"]:
    add("_accounts_base_url", {"domain": d})

# ── _onboard_open_base_url (pure) ──
for d in ["feishu", "lark", "other", "", None, False, "lark"]:
    add("_onboard_open_base_url", {"domain": d})

# ── _parse_bot_response (pure) ──
add("_parse_bot_response", {"data": {"code": 0, "bot": {"app_name": "A", "open_id": "O"}}})
add("_parse_bot_response", {"data": {"code": 1}})
add("_parse_bot_response", {"data": {"code": 0}})
add("_parse_bot_response", {"data": {"code": 0, "bot": {"bot_name": "B"}}})
add("_parse_bot_response", {"data": {"code": 0, "data": {"bot": {"app_name": "C", "open_id": "D"}}}})
add("_parse_bot_response", {"data": {"code": 0, "bot": {}, "data": {"bot": {"app_name": "E"}}}})
add("_parse_bot_response", {"data": {}})
add("_parse_bot_response", {"data": {"code": 0, "bot": {"app_name": "", "bot_name": "F", "open_id": None}}})

# ── _reactions_enabled (seam: _get_scoped_secret) ──
for v in ["true", " FALSE ", "0", "no", None, "YES", ""]:
    add("_reactions_enabled", {}, io={"_get_scoped_secret": [ret(v)]})
add("_reactions_enabled", {}, io={"_get_scoped_secret": [rais("ValueError", "secret down")]})
add("_reactions_enabled", {}, io={"_get_scoped_secret": [rais("SystemExit", "sx")]})
add("_reactions_enabled", {}, io={}, note="fail-closed")

# ── _text_batch_is_compatible (pure; all fields present in domain) ──
EV = lambda rmid, rt, tid: {"reply_to_message_id": rmid, "reply_to_text": rt,
                            "source": {"thread_id": tid}}
add("_text_batch_is_compatible", {"existing": EV("m", "t", "s"), "incoming": EV("m", "t", "s")})
add("_text_batch_is_compatible", {"existing": EV("m", "t", "s"), "incoming": EV("x", "t", "s")})
add("_text_batch_is_compatible", {"existing": EV("m", "t", "s"), "incoming": EV("m", "x", "s")})
add("_text_batch_is_compatible", {"existing": EV("m", "t", "s"), "incoming": EV("m", "t", "x")})
add("_text_batch_is_compatible", {"existing": EV(None, None, None), "incoming": EV(None, None, None)})
add("_text_batch_is_compatible", {"existing": EV(1, "t", "s"), "incoming": EV(1.0, "t", "s")})

# ── _begin_registration (seams: _accounts_base_url, _post_registration) ──
OK1 = {"device_code": "d1", "verification_uri_complete": "https://x?a=1",
       "user_code": "u1", "interval": 9, "expire_in": 99}
OK2 = {"device_code": "d2", "verification_uri_complete": "https://x"}
add("_begin_registration", {"domain": "feishu"},
    io={"_accounts_base_url": [ret("https://accounts.feishu.cn")], "_post_registration": [ret(OK1)]})
add("_begin_registration", {"domain": "lark"},
    io={"_accounts_base_url": [ret("https://accounts.larksuite.com")], "_post_registration": [ret(OK2)]})
add("_begin_registration", {"domain": "feishu"},
    io={"_accounts_base_url": [ret("B")], "_post_registration": [ret({"device_code": "d3"})]})
add("_begin_registration", {"domain": "feishu"},
    io={"_accounts_base_url": [ret("B")], "_post_registration": [ret({})]})
add("_begin_registration", {"domain": "feishu"},
    io={"_accounts_base_url": [ret("B")], "_post_registration": [ret({"device_code": ""})]})
add("_begin_registration", {"domain": "feishu"},
    io={"_accounts_base_url": [rais("RuntimeError", "base down")]})
add("_begin_registration", {"domain": "feishu"},
    io={"_accounts_base_url": [ret("B")], "_post_registration": [rais("ValueError", "post down")]})
add("_begin_registration", {"domain": "feishu"},
    io={"_accounts_base_url": [ret("B")], "_post_registration": [rais("SystemExit", "sx")]})
add("_begin_registration", {"domain": "feishu"},
    io={"_accounts_base_url": [ret("B")]}, note="fail-closed")

# ── _init_registration (seams: _accounts_base_url, _post_registration) ──
B = {"_accounts_base_url": [ret("B")]}
add("_init_registration", {"domain": "feishu"},
    io={"_accounts_base_url": [ret("B")], "_post_registration": [ret({"supported_auth_methods": ["client_secret"]})]})
add("_init_registration", {"domain": "lark"},
    io={"_accounts_base_url": [ret("B")], "_post_registration": [ret({"supported_auth_methods": ["sms"]})]})
add("_init_registration", {"domain": "feishu"},
    io={"_accounts_base_url": [ret("B")], "_post_registration": [ret({"supported_auth_methods": []})]})
add("_init_registration", {"domain": "feishu"},
    io={"_accounts_base_url": [ret("B")], "_post_registration": [ret({})]})
add("_init_registration", {"domain": "feishu"},
    io={"_accounts_base_url": [rais("OSError", "base down")]})
add("_init_registration", {"domain": "feishu"},
    io={"_accounts_base_url": [ret("B")], "_post_registration": [rais("ValueError", "post down")]})
add("_init_registration", {"domain": "feishu"},
    io={"_accounts_base_url": [ret("B")], "_post_registration": [rais("KeyboardInterrupt", "ki")]})
add("_init_registration", {"domain": "feishu"}, io=B, note="fail-closed")

# ── _apply_yaml_config (seam: _apply_yaml_bridge) ──
add("_apply_yaml_config", {"yaml_cfg": {}, "feishu_cfg": {"allow_bots": True}},
    io={"_apply_yaml_bridge": [ret({"allow_bots": True})]})
add("_apply_yaml_config", {"yaml_cfg": {}, "feishu_cfg": {"allow_bots": "TRUE"}},
    io={"_apply_yaml_bridge": [ret({"allow_bots": "TRUE"})]})
add("_apply_yaml_config", {"yaml_cfg": {}, "feishu_cfg": {}},
    io={"_apply_yaml_bridge": [ret({"allow_bots": 1})]})
add("_apply_yaml_config", {"yaml_cfg": {}, "feishu_cfg": {}},
    io={"_apply_yaml_bridge": [ret(None)]})
add("_apply_yaml_config", {"yaml_cfg": {}, "feishu_cfg": {}},
    io={"_apply_yaml_bridge": [ret({})]})
add("_apply_yaml_config", {"yaml_cfg": {}, "feishu_cfg": {}},
    io={"_apply_yaml_bridge": [rais("ValueError", "bridge down")]})
add("_apply_yaml_config", {"yaml_cfg": {}, "feishu_cfg": {}},
    io={"_apply_yaml_bridge": [rais("BaseException", "be")]})
add("_apply_yaml_config", {"yaml_cfg": {}, "feishu_cfg": {}}, io={}, note="fail-closed")

# ── _build_onboard_client (seams: _sdk_domain, _build_lark_client) ──
AID = {"app_id": "cli", "app_secret": "sec", "domain": "feishu"}
add("_build_onboard_client", AID,
    io={"_sdk_domain": [ret("DOM")], "_build_lark_client": [ret("CLIENT")]})
add("_build_onboard_client", AID,
    io={"_sdk_domain": [ret("DOM")], "_build_lark_client": [ret(None)]})
add("_build_onboard_client", AID, io={"_sdk_domain": [rais("ValueError", "domain down")]})
add("_build_onboard_client", AID,
    io={"_sdk_domain": [ret("DOM")], "_build_lark_client": [rais("RuntimeError", "client down")]})
add("_build_onboard_client", AID,
    io={"_sdk_domain": [ret("DOM")], "_build_lark_client": [rais("SystemExit", "sx")]})
add("_build_onboard_client", AID, io={}, note="fail-closed")

# ── probe_bot (seams: _load_lark_oapi, _probe_bot_sdk, _probe_bot_http) ──
add("probe_bot", AID,
    io={"_load_lark_oapi": [ret(True)], "_probe_bot_sdk": [ret({"bot_name": "SDK"})]})
add("probe_bot", AID,
    io={"_load_lark_oapi": [ret(False)], "_probe_bot_http": [ret({"bot_name": "HTTP"})]})
add("probe_bot", AID, io={"_load_lark_oapi": [ret(None)], "_probe_bot_http": [ret(None)]})
add("probe_bot", AID, io={"_load_lark_oapi": [rais("OSError", "load down")]})
add("probe_bot", AID,
    io={"_load_lark_oapi": [ret(True)], "_probe_bot_sdk": [rais("ValueError", "sdk down")]})
add("probe_bot", AID,
    io={"_load_lark_oapi": [ret(False)], "_probe_bot_http": [rais("BaseException", "be")]})
add("probe_bot", AID, io={}, note="fail-closed")

# ── _add_reaction (seams: _reaction_call, getattr_reaction) ──
AR = {"client": "C", "message_id": "m1", "emoji_type": "thumbup"}
add("_add_reaction", AR,
    io={"_reaction_call": [ret({"reaction_id": "r1"})], "getattr_reaction": [ret("r1")]})
add("_add_reaction", {"client": None, "message_id": "m1", "emoji_type": "e"}, io={})
add("_add_reaction", {"client": "C", "message_id": "", "emoji_type": "e"}, io={})
add("_add_reaction", {"client": "C", "message_id": "m1", "emoji_type": ""}, io={})
add("_add_reaction", AR, io={"_reaction_call": [ret(None)]})
add("_add_reaction", AR, io={"_reaction_call": [rais("ValueError", "call down")]})
add("_add_reaction", AR, io={"_reaction_call": [rais("SystemExit", "sx")]})
add("_add_reaction", AR,
    io={"_reaction_call": [ret({"reaction_id": "r"})], "getattr_reaction": [rais("AttributeError", "no id")]})
add("_add_reaction", AR, io={}, note="fail-closed")

# ── _remove_reaction (seam: _reaction_call) ──
RR = {"client": "C", "message_id": "m1", "reaction_id": "r1"}
add("_remove_reaction", RR, io={"_reaction_call": [ret({"reaction_id": "r1"})]})
add("_remove_reaction", RR, io={"_reaction_call": [ret(None)]})
add("_remove_reaction", {"client": None, "message_id": "m1", "reaction_id": "r1"}, io={})
add("_remove_reaction", {"client": "C", "message_id": "", "reaction_id": "r1"}, io={})
add("_remove_reaction", {"client": "C", "message_id": "m1", "reaction_id": ""}, io={})
add("_remove_reaction", RR, io={"_reaction_call": [rais("RuntimeError", "call down")]})
add("_remove_reaction", RR, io={"_reaction_call": [rais("GeneratorExit", "ge")]})
add("_remove_reaction", RR, io={}, note="fail-closed")

# ── _media_batch_key (seam: _text_batch_key) ──
EVID = {"message_type": {"value": "image"}}
EVID2 = {"message_type": {"value": 5}}
add("_media_batch_key", {"event": EVID}, io={"_text_batch_key": [ret("k")]})
add("_media_batch_key", {"event": EVID}, io={"_text_batch_key": [ret("")]})
add("_media_batch_key", {"event": EVID2}, io={"_text_batch_key": [ret("k")]})
add("_media_batch_key", {"event": EVID}, io={"_text_batch_key": [rais("ValueError", "key down")]})
add("_media_batch_key", {"event": EVID}, io={"_text_batch_key": [rais("KeyboardInterrupt", "ki")]})
add("_media_batch_key", {"event": EVID}, io={}, note="fail-closed")

# ── _media_batch_is_compatible (seam: _text_batch_is_compatible, short-circuit) ──
EM = lambda v: {"message_type": {"value": v}}
add("_media_batch_is_compatible", {"existing": EM("image"), "incoming": EM("image")},
    io={"_text_batch_is_compatible": [ret(True)]})
add("_media_batch_is_compatible", {"existing": EM("image"), "incoming": EM("video")}, io={})
add("_media_batch_is_compatible", {"existing": EM("image"), "incoming": EM("image")},
    io={"_text_batch_is_compatible": [ret(False)]})
add("_media_batch_is_compatible", {"existing": EM("image"), "incoming": EM("image")},
    io={"_text_batch_is_compatible": [rais("ValueError", "cmp down")]})
add("_media_batch_is_compatible", {"existing": EM("image"), "incoming": EM("image")},
    io={"_text_batch_is_compatible": [rais("SystemExit", "sx")]})
add("_media_batch_is_compatible", {"existing": EM("image"), "incoming": EM("image")},
    io={}, note="fail-closed")

# ── _get_cached_sender_name (seams: cache_get, time_now, cache_pop) ──
add("_get_cached_sender_name", {"sender_id": "u1"},
    io={"cache_get": [ret(["Alice", 100])], "time_now": [ret(50)]})
add("_get_cached_sender_name", {"sender_id": "u2"},
    io={"cache_get": [ret(["Bob", 100])], "time_now": [ret(150)], "cache_pop": [ret(None)]})
add("_get_cached_sender_name", {"sender_id": None}, io={})
add("_get_cached_sender_name", {"sender_id": ""}, io={})
add("_get_cached_sender_name", {"sender_id": "u3"}, io={"cache_get": [ret(None)]})
add("_get_cached_sender_name", {"sender_id": "u1"}, io={"cache_get": [rais("ValueError", "get down")]})
add("_get_cached_sender_name", {"sender_id": "u1"},
    io={"cache_get": [ret(["A", 100])], "time_now": [rais("RuntimeError", "time down")]})
add("_get_cached_sender_name", {"sender_id": "u1"},
    io={"cache_get": [ret(["A", 100])], "time_now": [ret(150)], "cache_pop": [rais("ValueError", "pop down")]})
add("_get_cached_sender_name", {"sender_id": "u1"}, io={}, note="fail-closed")

# ── _probe_bot_sdk (many seams + try/except Exception / BaseException) ──
SDK_OK = {
    "_build_onboard_client": [ret("CLIENT")],
    "_tenant_get_request": [ret("REQ")],
    "client_request": [ret({"raw": "RAW"})],
    "getattr_raw": [ret({"content": "CONTENT"})],
    "getattr_content": [ret("CONTENT")],
    "json_loads": [ret({"code": 0, "bot": {"app_name": "BOT", "open_id": "OID"}})],
    "_parse_bot_response": [ret({"bot_name": "BOT", "bot_open_id": "OID"})],
}
add("_probe_bot_sdk", AID, io=SDK_OK)
SDK_NONE = dict(SDK_OK)
SDK_NONE["getattr_content"] = [ret(None)]
add("_probe_bot_sdk", AID, io=SDK_NONE)
add("_probe_bot_sdk", AID,
    io={"_build_onboard_client": [rais("ValueError", "client down")], "logger_debug": [ret(None)]})
add("_probe_bot_sdk", AID,
    io={"_build_onboard_client": [ret("C")], "_tenant_get_request": [ret("REQ")],
        "client_request": [rais("OSError", "net down")], "logger_debug": [ret(None)]})
add("_probe_bot_sdk", AID,
    io={"_build_onboard_client": [ret("C")], "_tenant_get_request": [ret("REQ")],
        "client_request": [ret({"raw": "R"})], "getattr_raw": [rais("ValueError", "raw down")],
        "logger_debug": [ret(None)]})
add("_probe_bot_sdk", AID,
    io={"_build_onboard_client": [ret("C")], "_tenant_get_request": [ret("REQ")],
        "client_request": [ret({"raw": "R"})], "getattr_raw": [ret("RAW")],
        "getattr_content": [rais("SystemExit", "sx")]})
add("_probe_bot_sdk", AID,
    io={"_build_onboard_client": [ret("C")], "_tenant_get_request": [ret("REQ")],
        "client_request": [ret({"raw": "R"})], "getattr_raw": [ret("RAW")],
        "getattr_content": [ret("C")], "json_loads": [rais("ValueError", "bad json")],
        "logger_debug": [ret(None)]})
add("_probe_bot_sdk", AID,
    io={"_build_onboard_client": [ret("C")], "_tenant_get_request": [ret("REQ")],
        "client_request": [ret({"raw": "R"})], "getattr_raw": [ret("RAW")],
        "getattr_content": [ret("C")], "json_loads": [ret({"code": 0})],
        "_parse_bot_response": [rais("RuntimeError", "parse down")], "logger_debug": [ret(None)]})
add("_probe_bot_sdk", AID,
    io={"_build_onboard_client": [rais("ValueError", "client down")], "logger_debug": [rais("ValueError", "handler")]})
add("_probe_bot_sdk", AID,
    io={}, note="fail-closed")  # no client script -> RuntimeError swallowed -> logger_debug missing? handled: add below


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="") as f:
        for case in CASES:
            f.write(json.dumps(case, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"WROTE {OUT}: {len(CASES)} cases, {len(counts)} funcs")
    low = [f for f, n in counts.items() if n < 6]
    print("funcs with <6 cases:", low)
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    if low:
        raise SystemExit(1)
    try:
        import sys
        sys.path.insert(0, str(Path(__file__).resolve().parent))
        import oracle_feishu_adapter_flow7 as o
        bad = set()
        for c in CASES:
            bad |= set(c["in"].get("io") or {}) - o.SEAMED
        if bad:
            print("fixture io keys without a seam:", sorted(bad))
            raise SystemExit(1)
        print("seam keys verified against", len(o.SEAMED), "C seams:", sorted(o.SEAMED))
    except ImportError as exc:
        print("oracle import skipped:", exc)


if __name__ == "__main__":
    main()
