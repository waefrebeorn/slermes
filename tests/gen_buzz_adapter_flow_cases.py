#!/usr/bin/env python3
"""Generate tests/fixtures/buzz_adapter_flow.jsonl for the Buzz adapter FLOW
port. Deterministic; >=5 cases per ported function. Honours the corpus
restrictions documented in src/gateway/platforms/buzz_adapter_flow.c (ASCII
scope, canonical json.dumps JSON text, scalar str()-positions, clean int()
strings, {path,name} path doubles with name == basename).

Run: python tests/gen_buzz_adapter_flow_cases.py
"""
import json
import os
import posixpath

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "buzz_adapter_flow.jsonl")

PK = "ab" * 32                      # 64-char lowercase hex pubkey
PK_UP = PK.upper()
OTHER = "cd" * 32
NPUB = "npub1" + "q" * 58
DN = "Chip"

CASES = []


def case(func, args, state=None, io=None):
    inp = {"args": args}
    if state is not None:
        inp["state"] = state
    if io is not None:
        inp["io"] = io
    CASES.append({"func": func, "in": inp})


def raise_spec(kind, msg=""):
    return {"__script__": True, "raise": {"type": kind, "message": msg}}


def od_map(n, prefix="e", val=None):
    return {f"{prefix}{i}": val for i in range(n)}


def meta_map(n, prefix="e"):
    return {f"{prefix}{i}": [f"pk{i}", f"snip{i}"] for i in range(n)}


def self_ids(**over):
    base = {"_self_pubkey": PK, "_self_npub": NPUB, "_display_name": DN}
    base.update(over)
    return base


def dm_meta(cid="c1"):
    return {cid: {"name": "DM", "description": ""}}


def dm_event(pubkey=OTHER, content="hello", kind=9, ptag=True):
    tags = [["p", PK]] if ptag else []
    return {"id": "ev1", "pubkey": pubkey, "content": content, "kind": kind, "tags": tags}


# ── _split_csv ──
for raw in ["a,b,c", "", "a,,b", ["x", "y"], None, 5, "single"]:
    case("_split_csv", {"raw": raw})

# ── _add_pubkey ──
case("_add_pubkey", {"bucket": [], "raw": "ABC"})
case("_add_pubkey", {"bucket": ["abc"], "raw": "abc"})
case("_add_pubkey", {"bucket": [], "raw": ""})
case("_add_pubkey", {"bucket": [], "raw": None})
case("_add_pubkey", {"bucket": [], "raw": 5})
case("_add_pubkey", {"bucket": ["x"], "raw": "X"})
case("_add_pubkey", {"bucket": ["a"], "raw": "B"})

# ── _normalize_user_ref ──
case("_normalize_user_ref", {"ref": PK})
case("_normalize_user_ref", {"ref": "  " + PK_UP + "  "})
case("_normalize_user_ref", {"ref": ""})
case("_normalize_user_ref", {"ref": None})
case("_normalize_user_ref", {"ref": "npub1xyz"}, io={"npub_to_hex": "de" * 32})
case("_normalize_user_ref", {"ref": "npub1xyz"}, io={"npub_to_hex": None})
case("_normalize_user_ref", {"ref": "short"})
case("_normalize_user_ref", {"ref": "zz" * 32})

# ── _pubkey_set ──
case("_pubkey_set", {"raw": f"{PK},{PK},npub1a"}, io={"npub_to_hex": "ff" * 32})
case("_pubkey_set", {"raw": [PK, "npub1a", 5, ""]}, io={"npub_to_hex": "ff" * 32})
case("_pubkey_set", {"raw": ""})
case("_pubkey_set", {"raw": None})
case("_pubkey_set", {"raw": "bad,short"})
case("_pubkey_set", {"raw": ["npub1a", "npub1a"]}, io={"npub_to_hex": "ff" * 32})

# ── _ttl_get ──
case("_ttl_get", {"cache": {"k": [100, "v"]}, "key": "k", "ttl": 60}, io={"time.monotonic": 150})
case("_ttl_get", {"cache": {"k": [100, "v"]}, "key": "k", "ttl": 60}, io={"time.monotonic": 200})
case("_ttl_get", {"cache": {}, "key": "k", "ttl": 60}, io={})
case("_ttl_get", {"cache": {"k": None}, "key": "k", "ttl": 60}, io={})
case("_ttl_get", {"cache": {"k": [100, {"a": 1}]}, "key": "k", "ttl": 60}, io={"time.monotonic": 100})
case("_ttl_get", {"cache": {"k": [100, "v"]}, "key": "k", "ttl": 60},
     io={"time.monotonic": raise_spec("SystemExit", "bye")})

# ── _credentials_key ──
case("_credentials_key", {"data": {"nsec": " nsec1x "}})
case("_credentials_key", {"data": {"private_key_hex": " ab "}})
case("_credentials_key", {"data": {"private_key": "***"}})
case("_credentials_key", {"data": {"nsec": "", "private_key": "  "}})
case("_credentials_key", {"data": {}})
case("_credentials_key", {"data": {"nsec": 5}})
case("_credentials_key", {"data": {"nsec": "a", "private_key_hex": "b"}})

# ── _bounded_cli_message ──
case("_bounded_cli_message", {"message": "hello", "redact_path": None})
case("_bounded_cli_message", {"message": "x" * 900, "redact_path": None})
case("_bounded_cli_message", {"message": "y" * 901, "redact_path": None})
case("_bounded_cli_message", {"message": "fail /home/u/creds.json now",
                              "redact_path": {"path": "/home/u/creds.json", "name": "creds.json"}})
case("_bounded_cli_message", {"message": "no path here", "redact_path": None})
case("_bounded_cli_message", {"message": "/p/a /p/a", "redact_path": {"path": "/p/a", "name": "a"}})

# ── _json_or ──
case("_json_or", {"text": '{"a":1}', "default": None})
case("_json_or", {"text": "", "default": {"d": 1}})
case("_json_or", {"text": None, "default": [1, 2]})
case("_json_or", {"text": "not json", "default": "x"})
case("_json_or", {"text": "null", "default": None})
case("_json_or", {"text": "[1,2,3]", "default": []})
case("_json_or", {"text": '  {"a":1}  ', "default": None})
case("_json_or", {"text": "{", "default": 7})

# ── _cli_error_message ──
case("_cli_error_message", {"stderr": '{"message":"bad","error":"auth"}', "returncode": 2, "redact_path": None})
case("_cli_error_message", {"stderr": '{"message":"  bad  "}', "returncode": 1, "redact_path": None})
case("_cli_error_message", {"stderr": '{"message":""}', "returncode": 1, "redact_path": None})
case("_cli_error_message", {"stderr": "plain error", "returncode": 3, "redact_path": None})
case("_cli_error_message", {"stderr": "", "returncode": 4, "redact_path": None})
case("_cli_error_message", {"stderr": None, "returncode": 5, "redact_path": None})
case("_cli_error_message", {"stderr": '{"message":"x","error":"  "}', "returncode": 1, "redact_path": None})
case("_cli_error_message", {"stderr": "z" * 950, "returncode": 1, "redact_path": None})
case("_cli_error_message", {"stderr": "err /p/c.json", "returncode": 2,
                            "redact_path": {"path": "/p/c.json", "name": "c.json"}})

# ── _parse_send_receipt ──
case("_parse_send_receipt", {"stdout": '{"accepted":true,"event_id":" ev1 "}'})
case("_parse_send_receipt", {"stdout": '{"accepted":false,"message":" rejected "}'})
case("_parse_send_receipt", {"stdout": '{"accepted":false}'})
case("_parse_send_receipt", {"stdout": '{"accepted":true,"event_id":""}'})
case("_parse_send_receipt", {"stdout": '{"accepted":1,"event_id":"e"}'})
case("_parse_send_receipt", {"stdout": '{"event_id":"e"}'})
case("_parse_send_receipt", {"stdout": "not json"})
case("_parse_send_receipt", {"stdout": '{"accepted":false,"message":5}'})
case("_parse_send_receipt", {"stdout": '{"accepted":true,"event_id":"e"}'})

# ── _parse_json_list ──
case("_parse_json_list", {"stdout": '[{"a":1},{"b":2}]'})
case("_parse_json_list", {"stdout": '[{"a":1},5,"x",null]'})
case("_parse_json_list", {"stdout": '{"a":1}'})
case("_parse_json_list", {"stdout": ""})
case("_parse_json_list", {"stdout": "bad"})
case("_parse_json_list", {"stdout": "[]"})

# ── _attachment_rejection_note ──
for r in [0, 1, 999, 1000, 5]:
    case("_attachment_rejection_note", {"rejected": r})

# ── _e_tags ──
case("_e_tags", {"event": {"tags": [["e", "t1", "", "reply"], ["e", "t2", "", "root"]]}})
case("_e_tags", {"event": {"tags": [["e", "t1"]]}})
case("_e_tags", {"event": {"tags": [["p", "x"], ["e", "t1", "", "root"]]}})
case("_e_tags", {"event": {"tags": "notlist"}})
case("_e_tags", {"event": {"tags": [["e", "t1", "", "reply"], ["x", "y"]]}})
case("_e_tags", {"event": {"tags": [["e", None, "", "root"]]}})
case("_e_tags", {"event": {"tags": [["e", "t1", "", 5]]}})
case("_e_tags", {"event": {}})

# ── _event_reply_parent_id ──
case("_event_reply_parent_id", {"event": {"tags": [["e", "r", "", "root"], ["e", "p", "", "reply"]]}})
case("_event_reply_parent_id", {"event": {"tags": [["e", "r", "", "root"]]}})
case("_event_reply_parent_id", {"event": {"tags": [["e", "p1"], ["e", "p2"]]}})
case("_event_reply_parent_id", {"event": {"tags": [["e", "r", "", "root"], ["e", "p", "", "reply"], ["e", "z"]]}})
case("_event_reply_parent_id", {"event": {"tags": [["e", "  ", "", "reply"], ["e", "real"]]}})
case("_event_reply_parent_id", {"event": {"tags": [["p", "x"]]}})
case("_event_reply_parent_id", {"event": {"tags": [["e", "p1", "", "reply"], ["e", "p2", "", "reply"]]}})

# ── _p_tagged ──
case("_p_tagged", {"event": {"tags": [["p", PK]]}, "pubkey": PK})
case("_p_tagged", {"event": {"tags": [["p", PK_UP]]}, "pubkey": PK})
case("_p_tagged", {"event": {"tags": [["p", "other"]]}, "pubkey": PK})
case("_p_tagged", {"event": {"tags": [["e", PK]]}, "pubkey": PK})
case("_p_tagged", {"event": {"tags": "x"}, "pubkey": PK})
case("_p_tagged", {"event": {"tags": [["p"]]}, "pubkey": PK})
case("_p_tagged", {"event": {"tags": [["p", None]]}, "pubkey": "none"})

# ── _scoped_platform_setting ──
case("_scoped_platform_setting", {"env_name": "BUZZ_X", "extra": {"k": "ex"}, "key": "k"},
     io={"_profile_scoped": False, "os.getenv": "envval"})
case("_scoped_platform_setting", {"env_name": "BUZZ_X", "extra": {"k": "ex"}, "key": "k"},
     io={"_profile_scoped": True, "_get_scoped_secret": "sec"})
case("_scoped_platform_setting", {"env_name": "BUZZ_X", "extra": {"k": "ex"}, "key": "k"},
     io={"_profile_scoped": True, "_get_scoped_secret": None})
case("_scoped_platform_setting", {"env_name": "BUZZ_X", "extra": {}, "key": "k"},
     io={"_profile_scoped": True, "_get_scoped_secret": None})
case("_scoped_platform_setting", {"env_name": "BUZZ_X", "extra": None, "key": "k"},
     io={"_profile_scoped": True, "_get_scoped_secret": None})
case("_scoped_platform_setting", {"env_name": "BUZZ_X", "extra": {"k": "ex"}, "key": "k"},
     io={"_profile_scoped": True, "_get_scoped_secret": ""})
case("_scoped_platform_setting", {"env_name": "BUZZ_X", "extra": {}, "key": "k"},
     io={"_profile_scoped": raise_spec("KeyboardInterrupt")})
case("_scoped_platform_setting", {"env_name": "BUZZ_X", "extra": {}, "key": "k"}, io={})

# ── _setting_or ──
case("_setting_or", {"env_name": "E", "extra": {"k": "ex"}, "key": "k", "default": "d"},
     io={"_profile_scoped": False, "os.getenv": None})
case("_setting_or", {"env_name": "E", "extra": {"k": "ex"}, "key": "k", "default": "d"},
     io={"_profile_scoped": False, "os.getenv": "raw"})
case("_setting_or", {"env_name": "E", "extra": {}, "key": "k", "default": "d"},
     io={"_profile_scoped": False, "os.getenv": None})
case("_setting_or", {"env_name": "E", "extra": {"k": None}, "key": "k", "default": "d"},
     io={"_profile_scoped": False, "os.getenv": None})
case("_setting_or", {"env_name": "E", "extra": None, "key": "k", "default": "d"},
     io={"_profile_scoped": False, "os.getenv": None})
case("_setting_or", {"env_name": "E", "extra": {"k": "ex"}, "key": "k", "default": "d"},
     io={"_profile_scoped": True, "_get_scoped_secret": "sec"})

# ── _configured_relay ──
case("_configured_relay", {"extra": {"relay_url": "ex"}},
     io={"_profile_scoped": False, "os.getenv": " wss://r "})
case("_configured_relay", {"extra": {"relay_url": " wss://x "}},
     io={"_profile_scoped": False, "os.getenv": None})
case("_configured_relay", {"extra": {}}, io={"_profile_scoped": False, "os.getenv": None})
case("_configured_relay", {"extra": {"relay_url": None}}, io={"_profile_scoped": False, "os.getenv": None})
case("_configured_relay", {"extra": {"relay_url": "ex"}}, io={"_profile_scoped": False, "os.getenv": "env"})
case("_configured_relay", {"extra": {"relay_url": "ex"}}, io={"_profile_scoped": True, "_get_scoped_secret": None})

# ── _configured_home_channel ──
case("_configured_home_channel", {"extra": {}}, io={"_profile_scoped": False, "os.getenv": " ch "})
case("_configured_home_channel", {"extra": {"home_channel": " ch2 "}}, io={"_profile_scoped": False, "os.getenv": None})
case("_configured_home_channel", {"extra": {}}, io={"_profile_scoped": False, "os.getenv": None})
case("_configured_home_channel", {"extra": {"home_channel": None}}, io={"_profile_scoped": False, "os.getenv": None})
case("_configured_home_channel", {"extra": {"home_channel": 5}}, io={"_profile_scoped": False, "os.getenv": None})
case("_configured_home_channel", {"extra": {"home_channel": "ex"}}, io={"_profile_scoped": False, "os.getenv": "env"})

# ── _configured_credentials_file ──
case("_configured_credentials_file", {"extra": {}}, io={"_get_scoped_secret": " /path/c.json "})
case("_configured_credentials_file", {"extra": {"credentials_file": " /x "}}, io={"_get_scoped_secret": None})
case("_configured_credentials_file", {"extra": {}}, io={"_get_scoped_secret": None})
case("_configured_credentials_file", {"extra": None}, io={"_get_scoped_secret": None})
case("_configured_credentials_file", {"extra": {"credentials_file": "y"}}, io={"_get_scoped_secret": ""})
case("_configured_credentials_file", {"extra": {"credentials_file": None}}, io={"_get_scoped_secret": None})
case("_configured_credentials_file", {"extra": {}}, io={"_get_scoped_secret": raise_spec("SystemExit", "x")})

# ── _reply_to_mode ──
case("_reply_to_mode", {"config": {"reply_to_mode": "first"}, "extra": {}},
     io={"_profile_scoped": [False, False], "os.getenv": ["all", None]})
case("_reply_to_mode", {"config": {"reply_to_mode": "ALL"}, "extra": {"reply_in_thread": False}},
     io={"_profile_scoped": [False, False], "os.getenv": [None, None]})
case("_reply_to_mode", {"config": None, "extra": {"reply_in_thread": "0"}},
     io={"_profile_scoped": [False, False], "os.getenv": [None, None]})
case("_reply_to_mode", {"config": {}, "extra": {}},
     io={"_profile_scoped": [True, True], "_get_scoped_secret": ["thread", None]})
case("_reply_to_mode", {"config": {}, "extra": {"reply_in_thread": "off"}},
     io={"_profile_scoped": [False, False], "os.getenv": [None, None]})
case("_reply_to_mode", {"config": {}, "extra": {"reply_in_thread": "yes"}},
     io={"_profile_scoped": [False, False], "os.getenv": [None, None]})
case("_reply_to_mode", {"config": {"reply_to_mode": "  FIRST  "}, "extra": {}},
     io={"_profile_scoped": [False, False], "os.getenv": [None, None]})

# ── _resolve_credentials_data ──
# _credentials_candidates returns a LIST -> script one level deeper (sequence of list results).
case("_resolve_credentials_data", {"extra": None},
     io={"_credentials_candidates": [[{"path": "/a.json"}]], "read_text": '{"nsec":"k"}'})
case("_resolve_credentials_data", {"extra": None},
     io={"_credentials_candidates": [[{"path": "/a.json"}, {"path": "/b.json"}]],
         "read_text": ['{"nokey":1}', '{"private_key": "***"}']})
case("_resolve_credentials_data", {"extra": None},
     io={"_credentials_candidates": [[{"path": "/a.json"}]], "read_text": raise_spec("OSError", "no file")})
case("_resolve_credentials_data", {"extra": None},
     io={"_credentials_candidates": [[{"path": "/a.json"}]], "read_text": "not json"})
case("_resolve_credentials_data", {"extra": None}, io={"_credentials_candidates": [[]]})
case("_resolve_credentials_data", {"extra": None},
     io={"_credentials_candidates": [[{"path": "/a.json"}]], "read_text": raise_spec("SystemExit", "x")})
case("_resolve_credentials_data", {"extra": None},
     io={"_credentials_candidates": [[{"path": "/a.json"}]], "read_text": "[1,2]"})
case("_resolve_credentials_data", {"extra": None},
     io={"_credentials_candidates": [[{"path": "/a.json"}]], "read_text": '{"nsec":"  "}'})

# ── _resolve_private_key ──
case("_resolve_private_key", {"extra": None}, io={"_get_scoped_secret": " nsec1x "})
case("_resolve_private_key", {"extra": None},
     io={"_get_scoped_secret": None, "_credentials_candidates": [[{"path": "/a"}]], "read_text": '{"nsec":"k"}'})
case("_resolve_private_key", {"extra": None}, io={"_get_scoped_secret": "", "_credentials_candidates": [[]]})
case("_resolve_private_key", {"extra": None},
     io={"_get_scoped_secret": None, "_credentials_candidates": [[{"path": "/a"}]], "read_text": '{"nokey":1}'})
case("_resolve_private_key", {"extra": None},
     io={"_get_scoped_secret": None, "_credentials_candidates": raise_spec("OSError", "glob fail")})
case("_resolve_private_key", {"extra": None}, io={"_get_scoped_secret": raise_spec("KeyboardInterrupt")})

# ── normalize_user_id ──
case("normalize_user_id", {"user_id": PK})
case("normalize_user_id", {"user_id": "npub1xyz"}, io={"npub_to_hex": "de" * 32})
case("normalize_user_id", {"user_id": ""})
case("normalize_user_id", {"user_id": None})
case("normalize_user_id", {"user_id": "short"})

# ── _extract_thread_root ──
case("_extract_thread_root", {"event": {"tags": [["e", "r", "", "root"], ["e", "p", "", "reply"]]}})
case("_extract_thread_root", {"event": {"tags": [["e", "p", "", "reply"]]}})
case("_extract_thread_root", {"event": {"tags": [["e", "p1"]]}})
case("_extract_thread_root", {"event": {"tags": [["e", "r", "", "root"], ["e", "p1"]]}})
case("_extract_thread_root", {"event": {"tags": [["p", "x"]]}})
case("_extract_thread_root", {"event": {"tags": [["e", "r", "", "ROOT"]]}})
case("_extract_thread_root", {"event": {"tags": [["e", "r1", "", "root"], ["e", "r2", "", "root"]]}})

# ── _store_event_meta ──
case("_store_event_meta", {"state": {"seen": {}}, "event_id": "e1", "pubkey": "p", "snippet": "s"})
case("_store_event_meta", {"state": {"seen": {}, "event_meta": {"e0": ["a", "b"]}},
                           "event_id": "e1", "pubkey": "p", "snippet": "s"})
case("_store_event_meta", {"state": {"seen": {}, "event_meta": {"e0": ["a", "b"], "e1": ["c", "d"]}},
                           "event_id": "e0", "pubkey": "z", "snippet": "w"})
case("_store_event_meta", {"state": {"seen": {}, "event_meta": meta_map(500)},
                           "event_id": "new", "pubkey": "p", "snippet": "s"})
case("_store_event_meta", {"state": {"seen": {}}, "event_id": "e1", "pubkey": "p", "snippet": "x" * 600})
case("_store_event_meta", {"state": {"seen": {}, "event_meta": {"e0": ["a", "b"]}},
                           "event_id": "e0", "pubkey": "p", "snippet": "s"})

# ── _lookup_event_meta ──
case("_lookup_event_meta", {"state": {"event_meta": {"e1": ["p", "s"]}}, "event_id": "e1"})
case("_lookup_event_meta", {"state": {"event_meta": {"e1": ["p", "s"]}}, "event_id": "missing"})
case("_lookup_event_meta", {"state": {"event_meta": {"e1": ["p", "s"]}}, "event_id": ""})
case("_lookup_event_meta", {"state": {"event_meta": {"e1": ["p", "s"]}}, "event_id": None})
case("_lookup_event_meta", {"state": {"event_meta": {"e1": ["p"]}}, "event_id": "e1"})
case("_lookup_event_meta", {"state": {"event_meta": {"e1": "notarray"}}, "event_id": "e1"})
case("_lookup_event_meta", {"state": {"event_meta": {"e1": [None, None]}}, "event_id": "e1"})
case("_lookup_event_meta", {"state": {}, "event_id": "e1"})

# ── _trim_seen ──
case("_trim_seen", {"state": {"seen": od_map(501)}})
case("_trim_seen", {"state": {"seen": od_map(3)}})
case("_trim_seen", {"state": {"seen": od_map(3), "event_meta": meta_map(501)}})
case("_trim_seen", {"state": {}})
case("_trim_seen", {"state": {"seen": od_map(500)}})
case("_trim_seen", {"state": {"seen": od_map(501), "event_meta": meta_map(2)}})

# ── _remember_event ──
case("_remember_event", {"state": {"seen": {}}, "event": {"id": "e1", "pubkey": PK_UP, "content": "hello"}})
case("_remember_event", {"state": {"seen": {}}, "event": {"id": "e1", "pubkey": "p", "content": 5}})
case("_remember_event", {"state": {"seen": {}}, "event": {"id": "", "pubkey": "p", "content": "x"}})
case("_remember_event", {"state": {"seen": {}}, "event": {"id": "e1", "pubkey": "p", "content": "x" * 600}})
case("_remember_event", {"state": {"seen": {}}, "event": {"id": "e1", "pubkey": None, "content": "x"}})
case("_remember_event", {"state": {"seen": {}}, "event": {"id": None, "pubkey": "p", "content": "x"}})

# ── _remember_event_meta ──
case("_remember_event_meta", {"channel_id": "c1", "event_id": "e1", "pubkey": "P", "content": "hi"},
     state={"_channel_state": {"c1": {"seen": {}}}})
case("_remember_event_meta", {"channel_id": "missing", "event_id": "e1", "pubkey": "P", "content": "hi"},
     state={"_channel_state": {"c1": {"seen": {}}}})
case("_remember_event_meta", {"channel_id": "c1", "event_id": "e1", "pubkey": "P", "content": None},
     state={"_channel_state": {"c1": {"seen": {}}}})
case("_remember_event_meta", {"channel_id": "c1", "event_id": "e1", "pubkey": "P", "content": "hi"},
     state={})
case("_remember_event_meta", {"channel_id": "c1", "event_id": "e1", "pubkey": "P", "content": 5},
     state={"_channel_state": {"c1": {"seen": {}}}})

# ── _new_channel_state ──
for ct in ["group", "dm", "", "forum", "private"]:
    case("_new_channel_state", {"chat_type": ct})

# ── _restore_channel_state ──
case("_restore_channel_state", {"channel_id": "c1", "chat_type": "group"},
     state={"_restored_cursors": {"c1": {"chat_type": "dm", "last_ts": 100, "seen": ["e1", "e2"]}},
            "_channel_state": {}})
case("_restore_channel_state", {"channel_id": "c1", "chat_type": "group"},
     state={"_restored_cursors": {}, "_channel_state": {}})
case("_restore_channel_state", {"channel_id": "c1", "chat_type": "group"},
     state={"_restored_cursors": {"c1": {"chat_type": "", "last_ts": 5, "seen": []}}, "_channel_state": {}})
case("_restore_channel_state", {"channel_id": "c1", "chat_type": "group"},
     state={"_channel_state": {}})
case("_restore_channel_state", {"channel_id": "c1", "chat_type": "dm"},
     state={"_restored_cursors": {"c1": {"chat_type": "dm", "last_ts": 0, "seen": []}}, "_channel_state": {}})

# ── _mark_seen ──
case("_mark_seen", {"channel_id": "c1", "event_id": "e1"}, state={"_channel_state": {"c1": {"seen": {}}}})
case("_mark_seen", {"channel_id": "missing", "event_id": "e1"}, state={"_channel_state": {"c1": {"seen": {}}}})
case("_mark_seen", {"channel_id": "c1", "event_id": "e1"}, state={"_channel_state": {"c1": {"seen": {"e1": None}}}})
case("_mark_seen", {"channel_id": "c1", "event_id": "e1"}, state={})
case("_mark_seen", {"channel_id": "c1", "event_id": "new"}, state={"_channel_state": {"c1": {"seen": od_map(500)}}})

# ── _record_thread_root ──
case("_record_thread_root", {"event_id": "e1", "event": {"tags": [["e", "root1", "", "root"]]}},
     state={"_thread_roots": {}})
case("_record_thread_root", {"event_id": "", "event": {}}, state={"_thread_roots": {}})
case("_record_thread_root", {"event_id": "e1", "event": {"tags": [["e", "r2", "", "root"]]}},
     state={"_thread_roots": {"e0": "r0", "e1": "r1"}})
case("_record_thread_root", {"event_id": "e1", "event": {}}, state={})
case("_record_thread_root", {"event_id": "new", "event": {}},
     state={"_thread_roots": {f"e{i}": None for i in range(512)}})
case("_record_thread_root", {"event_id": "e1", "event": {"tags": [["p", "x"]]}}, state={"_thread_roots": {}})

# ── _resolve_reply_anchor ──
case("_resolve_reply_anchor", {"anchor": "a"}, state={"_thread_roots": {"a": "root"}})
case("_resolve_reply_anchor", {"anchor": "a"}, state={"_thread_roots": {"a": None}})
case("_resolve_reply_anchor", {"anchor": ""}, state={"_thread_roots": {"a": "root"}})
case("_resolve_reply_anchor", {"anchor": None}, state={"_thread_roots": {}})
case("_resolve_reply_anchor", {"anchor": "x"}, state={"_thread_roots": {}})
case("_resolve_reply_anchor", {"anchor": "x"}, state={})
case("_resolve_reply_anchor", {"anchor": "a"}, state={"_thread_roots": {"a": ""}})

# ── _reply_args ──
case("_reply_args", {"anchor": "a"}, state={"_thread_roots": {"a": "root"}, "_reply_to_mode": "first"})
case("_reply_args", {"anchor": "a"}, state={"_thread_roots": {"a": "root"}, "_reply_to_mode": "off"})
case("_reply_args", {"anchor": ""}, state={"_thread_roots": {}, "_reply_to_mode": "first"})
case("_reply_args", {"anchor": "x"}, state={"_thread_roots": {}, "_reply_to_mode": "all"})
case("_reply_args", {"anchor": "x"}, state={"_thread_roots": {}})
case("_reply_args", {"anchor": ""}, state={"_thread_roots": {}})

# ── _may_reclassify_as_dm ──
case("_may_reclassify_as_dm", {"channel_id": "c1"}, state={"_channel_meta": dm_meta()})
case("_may_reclassify_as_dm", {"channel_id": "c1"}, state={"_channel_meta": {"c1": {"name": "DM", "description": "x"}}})
case("_may_reclassify_as_dm", {"channel_id": "c1"}, state={"_channel_meta": {"c1": {"name": "General"}}})
case("_may_reclassify_as_dm", {"channel_id": "missing"}, state={"_channel_meta": dm_meta()})
case("_may_reclassify_as_dm", {"channel_id": "c1"}, state={"_channel_meta": {"c1": {"name": " DM ", "description": "  "}}})
case("_may_reclassify_as_dm", {"channel_id": "c1"}, state={})
case("_may_reclassify_as_dm", {"channel_id": "c1"}, state={"_channel_meta": {"c1": {"name": None, "description": None}}})

# ── _p_tagged_to_self ──
case("_p_tagged_to_self", {"event": {"tags": [["p", PK]]}}, state={"_self_pubkey": PK})
case("_p_tagged_to_self", {"event": {"tags": [["p", PK]]}}, state={"_self_pubkey": ""})
case("_p_tagged_to_self", {"event": {"tags": [["p", "other"]]}}, state={"_self_pubkey": PK})
case("_p_tagged_to_self", {"event": {"tags": [["p", PK]]}}, state={})
case("_p_tagged_to_self", {"event": {"tags": [["p", PK_UP]]}}, state={"_self_pubkey": PK})

# ── _is_mentioned ──
case("_is_mentioned", {"content": "hey " + PK}, state=self_ids())
case("_is_mentioned", {"content": "@Chip hello"}, state=self_ids())
case("_is_mentioned", {"content": NPUB}, state=self_ids())
case("_is_mentioned", {"content": "nothing"}, state=self_ids())
case("_is_mentioned", {"content": "x@Chip"}, state=self_ids())
case("_is_mentioned", {"content": "@Chip2"}, state=self_ids())
case("_is_mentioned", {"content": "@Chip"}, state=self_ids(_self_pubkey="", _self_npub="", _display_name=""))
case("_is_mentioned", {"content": "@Chip"}, state={"_self_npub": NPUB, "_display_name": DN})
case("_is_mentioned", {"content": "@chip!"}, state=self_ids())

# ── _is_addressed ──
case("_is_addressed", {"event": {"content": "@Chip hi", "tags": []}}, state=self_ids())
case("_is_addressed", {"event": {"content": "plain", "tags": [["p", PK]]}}, state=self_ids())
case("_is_addressed", {"event": {"content": "plain", "tags": []}}, state=self_ids())
case("_is_addressed", {"event": {"content": 5, "tags": [["p", PK]]}}, state=self_ids())
case("_is_addressed", {"event": {"content": "x@Chip", "tags": []}}, state=self_ids())

# ── _is_direct_message_event ──
case("_is_direct_message_event", {"channel_id": "c1", "event": dm_event()},
     state=self_ids(_channel_meta=dm_meta()))
case("_is_direct_message_event", {"channel_id": "c1", "event": dm_event(kind=10)},
     state=self_ids(_channel_meta=dm_meta()))
case("_is_direct_message_event", {"channel_id": "c1", "event": dm_event(pubkey=PK)},
     state=self_ids(_channel_meta=dm_meta()))
case("_is_direct_message_event", {"channel_id": "c1", "event": dm_event(content="@Chip")},
     state=self_ids(_channel_meta=dm_meta()))
case("_is_direct_message_event", {"channel_id": "c1", "event": dm_event()},
     state=self_ids(_channel_meta={"c1": {"name": "General", "description": ""}}))
case("_is_direct_message_event", {"channel_id": "c1", "event": dm_event()},
     state=self_ids(_self_pubkey="", _channel_meta=dm_meta()))
case("_is_direct_message_event", {"channel_id": "c1", "event": dm_event(kind="9")},
     state=self_ids(_channel_meta=dm_meta()))
case("_is_direct_message_event", {"channel_id": "c1", "event": dm_event(kind="bad")},
     state=self_ids(_channel_meta=dm_meta()))
case("_is_direct_message_event", {"channel_id": "c1", "event": dm_event(ptag=False)},
     state=self_ids(_channel_meta=dm_meta()))

# ── _maybe_latch_dm ──
case("_maybe_latch_dm", {"channel_id": "c1", "state": {"chat_type": "group", "seen": {}}, "event": dm_event()},
     state=self_ids(_channel_meta=dm_meta(), _channel_names={}))
case("_maybe_latch_dm", {"channel_id": "c1", "state": {"chat_type": "dm", "seen": {}}, "event": dm_event()},
     state=self_ids(_channel_meta=dm_meta(), _channel_names={}))
case("_maybe_latch_dm", {"channel_id": "c1", "state": {"chat_type": "group", "seen": {}}, "event": dm_event(ptag=False)},
     state=self_ids(_channel_meta=dm_meta(), _channel_names={}))
case("_maybe_latch_dm", {"channel_id": "c1", "state": {"seen": {}}, "event": dm_event()},
     state=self_ids(_channel_meta=dm_meta(), _channel_names={}))
case("_maybe_latch_dm", {"channel_id": "c1", "state": {"chat_type": "group", "seen": {}}, "event": dm_event()},
     state=self_ids(_channel_meta=dm_meta()))
case("_maybe_latch_dm", {"channel_id": "c1", "state": {"chat_type": "group", "seen": {}}, "event": dm_event()},
     state=self_ids(_channel_meta=dm_meta(), _channel_names={"c1": "Existing"}))

# ── _strip_mention ──
case("_strip_mention", {"content": "@Chip /whoami"}, state=self_ids())
case("_strip_mention", {"content": "@chip: hello"}, state=self_ids())
case("_strip_mention", {"content": NPUB + " rest"}, state=self_ids())
case("_strip_mention", {"content": "@" + NPUB + " rest"}, state=self_ids())
case("_strip_mention", {"content": PK + " rest"}, state=self_ids())
case("_strip_mention", {"content": "  @Chip  "}, state=self_ids())
case("_strip_mention", {"content": "@ChipX hi"}, state=self_ids())
case("_strip_mention", {"content": "mid @Chip sentence"}, state=self_ids())
case("_strip_mention", {"content": "  plain  "}, state=self_ids(_self_pubkey="", _self_npub="", _display_name=""))
case("_strip_mention", {"content": "@Chip hi"}, state={"_self_npub": NPUB, "_self_pubkey": PK})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"wrote {len(CASES)} cases to {OUT}")
    low = [k for k, v in counts.items() if v < 5]
    print(f"funcs: {len(counts)}; under-5: {low}")
    for k in sorted(counts):
        print(f"  {k}: {counts[k]}")


if __name__ == "__main__":
    main()
