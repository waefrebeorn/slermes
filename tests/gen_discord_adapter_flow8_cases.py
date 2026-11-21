#!/usr/bin/env python3
"""Generate tests/fixtures/discord_adapter_flow8.jsonl.

Each case: {"func": <name>, "in": {"args": {...}, "state": {...}, "io": {...}}}.
io keys are a subset of the C/oracle seam registry. At least 6 cases per
ported function. `fc: true` marks a deliberate fail-closed case.
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "discord_adapter_flow8.jsonl"
OUT.parent.mkdir(parents=True, exist_ok=True)

cases = []


def add(func, args=None, io=None, state=None, fc=False, tag=""):
    rec = {"func": func, "in": {"args": args or {}, "io": io or {}}}
    if state is not None:
        rec["in"]["state"] = state
    if fc:
        rec["fc"] = True
    if tag:
        rec["tag"] = tag
    cases.append(rec)


rv = lambda t, m: {"__script__": True, "raise": {"type": t, "message": m}}
sl = lambda v: {"__script__": True, "ret": v}

# ---- 1 __contains__ (no seams) ----
F = "__contains__"
IDS = {"a": None, "b": None}
add(F, {"message_id": "a"}, state={"_ids": IDS})
add(F, {"message_id": "z"}, state={"_ids": IDS})
add(F, {"message_id": ""}, state={"_ids": {"": None}})
add(F, {"message_id": None}, state={"_ids": {"": None}})
add(F, {"message_id": 5}, state={"_ids": {"5": None}})
add(F, {"message_id": "a"}, state={})
add(F, {"message_id": True}, state={"_ids": {"True": None}})

# ---- 2 _snapshot (no seams, state mutator) ----
F = "_snapshot"
add(F, state={"_ids": {"a": None, "b": None, "c": None}, "_max_tracked": 2})
add(F, state={"_ids": {"a": None, "b": None, "c": None}, "_max_tracked": 5})
add(F, state={"_ids": {"a": 1, "b": 2}, "_max_tracked": 0})
add(F, state={"_ids": {"a": 1, "b": 2, "c": 3}, "_max_tracked": 1})
add(F, state={"_ids": {"1": None, "2": None}, "_max_tracked": 1})
add(F, state={})
add(F, state={"_ids": {"a": 1, "b": 2}, "_max_tracked": -1})

# ---- 3 VoiceReceiver creds accessors (no seams) ----
CREDS = ["k", "sess", 2, False]
for F in ("_secret_key", "_dave_session", "_dave_protocol_version", "_dave_downgraded"):
    add(F, state={"_creds": CREDS})
    add(F, state={"_creds": [10, 20, 30, 40]})
    add(F, state={"_creds": ["only"]})
    add(F, state={"_creds": []})
    add(F, state={})
    add(F, state={"_creds": [None, None, None, None]})
    add(F, state={"_creds": [{"x": 1}, [1, 2], "v", True]})

# ---- 7 _cancel_voice_timeout (task.cancel) ----
F = "_cancel_voice_timeout"
T = {"cancel": True}
add(F, {"guild_id": "1"}, {"task.cancel": None}, state={"_voice_timeout_tasks": {"1": T}})
add(F, {"guild_id": "2"}, {}, state={"_voice_timeout_tasks": {"1": T}})
add(F, {"guild_id": "1"}, {}, state={"_voice_timeout_tasks": {"1": None}})
add(F, {"guild_id": "1"}, {}, state={})
add(F, {"guild_id": "1"}, {"task.cancel": "cancelled"}, state={"_voice_timeout_tasks": {"1": T}})
add(F, {"guild_id": "1"}, {"task.cancel": False}, state={"_voice_timeout_tasks": {"1": T}})
add(F, {"guild_id": "1"}, {"task.cancel": rv("RuntimeError", "boom")},
    state={"_voice_timeout_tasks": {"1": T}}, fc=True)
add(F, {"guild_id": "1"}, {"task.cancel": rv("SystemExit", "bye")},
    state={"_voice_timeout_tasks": {"1": T}}, fc=True)
add(F, {"guild_id": "1"}, {}, state={"_voice_timeout_tasks": {"1": T}}, fc=True,
    tag="fail-closed: no cancel script")

# ---- 8 voice_mixer_active (no seams) ----
F = "voice_mixer_active"
add(F, {"guild_id": "1"}, state={"_voice_mixers": {"1": {"x": 1}}})
add(F, {"guild_id": "1"}, state={"_voice_mixers": {"2": {"x": 1}}})
add(F, {"guild_id": "1"}, state={"_voice_mixers": {"1": None}})
add(F, {"guild_id": "1"}, state={"_voice_mixers": {}})
add(F, {"guild_id": "1"}, state={})
add(F, {"guild_id": "1"}, state={"_voice_mixers": {"1": {}}})
add(F, {"guild_id": "1"}, state={"_voice_mixers": {"1": {"y": 2}}})

# ---- 9 is_in_voice_channel (vc.is_connected) ----
F = "is_in_voice_channel"
VC = {"is_connected": True}
add(F, {"guild_id": "1"}, {"vc.is_connected": True}, state={"_voice_clients": {"1": VC}})
add(F, {"guild_id": "1"}, {"vc.is_connected": False}, state={"_voice_clients": {"1": VC}})
add(F, {"guild_id": "1"}, {}, state={"_voice_clients": {"1": None}})
add(F, {"guild_id": "1"}, {}, state={})
add(F, {"guild_id": "1"}, {"vc.is_connected": rv("ValueError", "boom")},
    state={"_voice_clients": {"1": VC}}, fc=True)
add(F, {"guild_id": "1"}, {"vc.is_connected": rv("SystemExit", "bye")},
    state={"_voice_clients": {"1": VC}}, fc=True)
add(F, {"guild_id": "1"}, {"vc.is_connected": True}, state={"_voice_clients": {"1": {"is_connected": True}}})

# ---- 10 discard_pending_voice_input (receiver.discard_pending) ----
F = "discard_pending_voice_input"
RCV = {"discard_pending": True}
add(F, {"guild_id": "1"}, {"receiver.discard_pending": None},
    state={"_voice_receivers": {"1": RCV}})
add(F, {"guild_id": "1"}, {}, state={"_voice_receivers": {"1": None}})
add(F, {"guild_id": "1"}, {}, state={})
add(F, {"guild_id": "1"}, {"receiver.discard_pending": "ok"}, state={"_voice_receivers": {"1": RCV}})
add(F, {"guild_id": "1"}, {"receiver.discard_pending": rv("RuntimeError", "boom")},
    state={"_voice_receivers": {"1": RCV}}, fc=True)
add(F, {"guild_id": "1"}, {"receiver.discard_pending": rv("SystemExit", "bye")},
    state={"_voice_receivers": {"1": RCV}}, fc=True)
add(F, {"guild_id": "1"}, {}, state={"_voice_receivers": {"1": RCV}}, fc=True,
    tag="fail-closed: no discard script")

# ---- 11 _disable_all (no seams, state mutator) ----
F = "_disable_all"
add(F, state={"children": [{"disabled": False, "name": "a"}]})
add(F, state={"children": [{"disabled": False}, {"disabled": True}, {"x": 1}]})
add(F, state={"children": []})
add(F, state={"children": [{"disabled": True, "name": "z"}]})
add(F, state={})
add(F, state={"children": [{}]})
add(F, state={"children": [{"nested": {"k": 1}, "disabled": False}]})

# ---- 12 _write_command_sync_state (_command_sync_state_path, atomic_json_write) ----
F = "_write_command_sync_state"
AW = {"atomic_json_write": None}
add(F, {"state": {"x": 1}}, {"_command_sync_state_path": "/tmp/s.json", **AW})
add(F, {"state": {"x": 1}}, {"_command_sync_state_path": None, **AW})
add(F, {"state": {"x": 1}}, {"_command_sync_state_path": rv("RuntimeError", "boom")}, fc=True)
add(F, {"state": {"x": 1}}, {"_command_sync_state_path": "p", "atomic_json_write": rv("ValueError", "boom")}, fc=True)
add(F, {"state": {"x": 1}}, {"_command_sync_state_path": "p", "atomic_json_write": rv("SystemExit", "bye")}, fc=True)
add(F, {}, {"_command_sync_state_path": "p", **AW})
add(F, {"state": {"x": 1}}, {"_command_sync_state_path": {"path": 1}, **AW})

# ---- 13 _record_command_sync_attempt (_update_command_sync_entry) ----
F = "_record_command_sync_attempt"
add(F, {"app_id": 1, "fingerprint": "fp"}, {"_update_command_sync_entry": None})
add(F, {"app_id": "abc", "fingerprint": ""}, {"_update_command_sync_entry": None})
add(F, {"app_id": None, "fingerprint": "x"}, {"_update_command_sync_entry": "ok"})
add(F, {"app_id": 1, "fingerprint": "fp"}, {"_update_command_sync_entry": rv("RuntimeError", "boom")}, fc=True)
add(F, {"app_id": 1, "fingerprint": "fp"}, {"_update_command_sync_entry": rv("SystemExit", "bye")}, fc=True)
add(F, {"app_id": 1, "fingerprint": "fp"}, {}, fc=True, tag="fail-closed: no entry script")

# ---- 14 _in_bot_thread (_discord_thread_require_mention) ----
F = "_in_bot_thread"
THR = {"__thread__": True, "id": "123"}
add(F, {"message": {"channel": THR}}, {"_discord_thread_require_mention": False}, state={"_threads": ["123"]})
add(F, {"message": {"channel": THR}}, {"_discord_thread_require_mention": True}, state={"_threads": ["123"]})
add(F, {"message": {"channel": {"__thread__": True, "id": "999"}}}, {}, state={"_threads": ["123"]})
add(F, {"message": {"channel": {"__thread__": False, "id": "123"}}}, {}, state={"_threads": ["123"]})
add(F, {"message": {"channel": {"__thread__": True, "id": 123}}},
    {"_discord_thread_require_mention": False}, state={"_threads": ["123"]})
add(F, {"message": {"channel": THR}}, {}, state={"_threads": ["123"]}, fc=True,
    tag="fail-closed: no require_mention script")
add(F, {"message": {"channel": THR}}, {"_discord_thread_require_mention": rv("SystemExit", "bye")},
    state={"_threads": ["123"]}, fc=True)
add(F, {"message": {"channel": THR}}, {}, state={})

# ---- 15 _check_auth (current_gates, _component_check_auth) ----
F = "_check_auth"
IA = {"user": {"id": "1"}}
BASE = {"allowed_user_ids": ["u"], "allowed_role_ids": ["r"], "live_auth": True}
add(F, {"interaction": IA}, {"_component_check_auth": True}, state=BASE)
add(F, {"interaction": IA}, {"_component_check_auth": False}, state=BASE)
add(F, {"interaction": IA},
    {"current_gates": sl([["u2"], ["r2"]]), "_component_check_auth": True},
    state={**BASE, "current_gates_callable": True})
add(F, {"interaction": IA}, {"_component_check_auth": rv("ValueError", "boom")}, state=BASE, fc=True)
add(F, {"interaction": IA}, {"current_gates": rv("RuntimeError", "boom")},
    state={**BASE, "current_gates_callable": True}, fc=True)
add(F, {"interaction": IA}, {"_component_check_auth": rv("SystemExit", "bye")}, state=BASE, fc=True)
add(F, {"interaction": IA}, {}, state=BASE, fc=True, tag="fail-closed: no component script")
add(F, {"interaction": IA}, {"_component_check_auth": True}, state={**BASE, "live_auth": False})

# ---- 16 _abort_discord_websocket_transport (abort) ----
F = "_abort_discord_websocket_transport"
WSW = {"socket": {"_writer": {"transport": {"abort": True}}}}
WSR = {"socket": {"_response": {"connection": {"protocol": {"transport": {"abort": True}}}}}}
WSC = {"socket": {"_conn": {"protocol": {"transport": {"abort": True}}},
                   "_response": {"connection": None}}}
add(F, {"websocket": WSW}, {"abort": None})
add(F, {"websocket": WSR}, {"abort": None})
add(F, {"websocket": WSC}, {"abort": None})
add(F, {"websocket": {"socket": {}}}, {})
add(F, {"websocket": {"socket": {"_writer": {"transport": {"abort": False}}}}}, {})
add(F, {"websocket": WSW}, {"abort": rv("ValueError", "boom")}, fc=True)
add(F, {"websocket": WSW}, {"abort": rv("SystemExit", "bye")}, fc=True)
add(F, {"websocket": WSW}, {}, fc=True, tag="fail-closed: no abort script")

# ---- 17 _check_slash_authorization (_evaluate_slash_authorization, _reject_slash) ----
F = "_check_slash_authorization"
IA = {"user": {"id": "1"}}
add(F, {"interaction": IA, "command_text": "/x"},
    {"_evaluate_slash_authorization": sl([True, ""])})
add(F, {"interaction": IA, "command_text": "/x"},
    {"_evaluate_slash_authorization": sl([False, "nope"]), "_reject_slash": False})
add(F, {"interaction": IA, "command_text": "/x"},
    {"_evaluate_slash_authorization": sl([False, ""]), "_reject_slash": True})
add(F, {"interaction": IA, "command_text": "/x"},
    {"_evaluate_slash_authorization": sl([False, None]), "_reject_slash": None})
add(F, {"interaction": IA, "command_text": "/x"},
    {"_evaluate_slash_authorization": rv("RuntimeError", "boom")}, fc=True)
add(F, {"interaction": IA, "command_text": "/x"},
    {"_evaluate_slash_authorization": sl([False, "x"]), "_reject_slash": rv("SystemExit", "bye")}, fc=True)
add(F, {"interaction": IA, "command_text": "/x"},
    {"_evaluate_slash_authorization": sl([False, "x"])}, fc=True, tag="fail-closed: no reject script")
add(F, {"interaction": IA, "command_text": "/x"},
    {"_evaluate_slash_authorization": sl([False, 0]), "_reject_slash": False})

with open(OUT, "w", encoding="utf-8", newline="\n") as f:
    for c in cases:
        f.write(json.dumps(c, ensure_ascii=False, separators=(",", ":")) + "\n")

counts = {}
for c in cases:
    counts[c["func"]] = counts.get(c["func"], 0) + 1
print(f"WROTE {OUT}: {len(cases)} cases, {len(counts)} funcs")
for k in sorted(counts):
    print(f"  {k}: {counts[k]}")
