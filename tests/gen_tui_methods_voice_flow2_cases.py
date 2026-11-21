#!/usr/bin/env python3
"""Generate tests/fixtures/tui_methods_voice_flow2.jsonl for the flow2
remainder port (src/gateway/tui_methods_voice_flow2.c).

Each case: {"func": <name>, "in": {"args": {...}, "state": {...}, "io": {...}},
["fail_closed": true]}. io scripts mirror the C universal seam: a plain value
is returned on every call; a bare LIST is a per-call queue (exhaustion is
fail-closed); {"__script__": true, "ret": v} returns v (use it for list-valued
results); {"__script__": true, "raise": {"type","message"}} raises.

Every func gets >= 5 cases: happy paths, branch variants, error paths, a
fail-closed missing-script case, and (where a catch site exists) a real
BaseException propagation case. ASCII corpus; POSIX paths; integral numbers.
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "tui_methods_voice_flow2.jsonl"
CASES = []


def case(func, args=None, io=None, state=None, fail_closed=False):
    c = {"func": func, "in": {"args": args or {}, "io": io or {}}}
    if state:
        c["in"]["state"] = state
    if fail_closed:
        c["fail_closed"] = True
    CASES.append(c)


def sc(ret):
    return {"__script__": True, "ret": ret}


def rs(kind, message=""):
    return {"__script__": True, "raise": {"type": kind, "message": message}}


def ok_env(rid, result):
    return {"jsonrpc": "2.0", "id": rid, "result": result}


RID = "r1"
IMP_OK = "mod"

# ---------------- _frame_fields ----------------
case("_frame_fields", {"frame": {"sample_rate": 44100, "frame_length": 2048}})
case("_frame_fields", {"frame": {}})
case("_frame_fields", {"frame": {"sample_rate": 16000}})
case("_frame_fields", {"frame": {"frame_length": 512, "extra": 1}})
case("_frame_fields", {"frame": {"sample_rate": None, "frame_length": None}})
case("_frame_fields", {"frame": {"sample_rate": 8000.0, "frame_length": 960}})

# ---------------- register ----------------
case("register", {"server": "srv1"}, {"bind_module": None})
case("register", {"server": "srv2"}, {"bind_module": sc({"bound": 2})})
case("register", {"server": "srv3"}, {"bind_module": rs("RuntimeError", "bind fail")})
case("register", {"server": "srv4"}, {"bind_module": rs("ImportError", "no module")})
case("register", {"server": "srv5"}, {"bind_module": rs("SystemExit", "bye")})
case("register", {"server": "srv6"}, {}, fail_closed=True)

# ---------------- _vr_on_status ----------------
case("_vr_on_status", {"state": "idle"}, {"voice_emit": None, "resume_voice_wake": None})
case("_vr_on_status", {"state": "active"}, {"voice_emit": None})
case("_vr_on_status", {"state": "processing"}, {"voice_emit": None})
case("_vr_on_status", {"state": "idle"}, {"voice_emit": rs("RuntimeError", "emit")})
case("_vr_on_status", {"state": "idle"},
     {"voice_emit": None, "resume_voice_wake": rs("SystemExit", "stop")})
case("_vr_on_status", {"state": "idle"}, {"voice_emit": None}, fail_closed=True)
case("_vr_on_status", {"state": ""}, {"voice_emit": None})

# ---------------- _vr_on_silent_limit ----------------
case("_vr_on_silent_limit", io={"set_session_voice_off": None, "vr_transcript": None},
     state={"voice_capture_sid": "s1"})
case("_vr_on_silent_limit", io={"set_session_voice_off": None, "vr_transcript": None},
     state={"voice_capture_sid": ""})
case("_vr_on_silent_limit", io={"set_session_voice_off": rs("RuntimeError", "off")},
     state={"voice_capture_sid": "s1"})
case("_vr_on_silent_limit",
     io={"set_session_voice_off": None, "vr_transcript": rs("SystemExit", "x")},
     state={"voice_capture_sid": "s1"})
case("_vr_on_silent_limit", io={}, state={"voice_capture_sid": "s1"}, fail_closed=True)
case("_vr_on_silent_limit", io={"set_session_voice_off": None},
     state={"voice_capture_sid": "s1"}, fail_closed=True)

# ---------------- _vr_on_stop_phrase ----------------
case("_vr_on_stop_phrase", {"t": "stop"},
     {"end_session_voice_chat": None, "vr_transcript": None},
     state={"voice_capture_sid": "s1"})
case("_vr_on_stop_phrase", {"t": ""},
     {"end_session_voice_chat": None, "vr_transcript": None},
     state={"voice_capture_sid": ""})
case("_vr_on_stop_phrase", {"t": "stop"},
     {"end_session_voice_chat": rs("RuntimeError", "end")},
     state={"voice_capture_sid": "s1"})
case("_vr_on_stop_phrase", {"t": "stop"},
     {"end_session_voice_chat": None, "vr_transcript": rs("KeyboardInterrupt", "")},
     state={"voice_capture_sid": "s1"})
case("_vr_on_stop_phrase", {"t": "stop"}, {}, state={"voice_capture_sid": "s1"},
     fail_closed=True)
case("_vr_on_stop_phrase", {"t": "stop"}, {"end_session_voice_chat": None},
     state={"voice_capture_sid": "s1"}, fail_closed=True)

# ---------------- _stop_session_voice_capture ----------------
case("_stop_session_voice_capture", {"session_id": "s1"},
     {"import": IMP_OK, "stop_continuous": None, "resume_voice_wake": None},
     state={"voice_capture_sid": "s1", "voice_capture_gen": 0})
case("_stop_session_voice_capture", {"session_id": "s2"}, {},
     state={"voice_capture_sid": "s1", "voice_capture_gen": 5})
case("_stop_session_voice_capture", {"session_id": ""}, {},
     state={"voice_capture_sid": "s1", "voice_capture_gen": 2})
case("_stop_session_voice_capture", {"session_id": "s1"},
     {"import": rs("ImportError", "no module"), "resume_voice_wake": None},
     state={"voice_capture_sid": "s1", "voice_capture_gen": 1})
case("_stop_session_voice_capture", {"session_id": "s1"},
     {"import": IMP_OK, "stop_continuous": rs("RuntimeError", "failed"),
      "resume_voice_wake": None},
     state={"voice_capture_sid": "s1", "voice_capture_gen": 3})
case("_stop_session_voice_capture", {"session_id": "s1"},
     {"import": IMP_OK, "stop_continuous": None,
      "resume_voice_wake": rs("SystemExit", "x")},
     state={"voice_capture_sid": "s1", "voice_capture_gen": 4})
case("_stop_session_voice_capture", {"session_id": "s1"}, {},
     state={"voice_capture_sid": "s1", "voice_capture_gen": 0}, fail_closed=True)
case("_stop_session_voice_capture", {"session_id": "s1"},
     {"import": rs("SystemExit", "x")},
     state={"voice_capture_sid": "s1", "voice_capture_gen": 7})

# ---------------- _arm_full_duplex_listener ----------------
case("_arm_full_duplex_listener", {},
     {"thread_start": None}, state={"fd_listener_active": False})
case("_arm_full_duplex_listener", {}, {}, state={"fd_listener_active": True})
case("_arm_full_duplex_listener", {}, {"thread_start": None}, state={})
case("_arm_full_duplex_listener", {},
     {"thread_start": rs("RuntimeError", "fail")}, state={"fd_listener_active": False})
case("_arm_full_duplex_listener", {},
     {"thread_start": rs("SystemExit", "x")}, state={"fd_listener_active": False})
case("_arm_full_duplex_listener", {}, {}, state={"fd_listener_active": False},
     fail_closed=True)

# ---------------- _wake_probe ----------------
case("_wake_probe", {"cfg": {}, "params": {}, "surface": "gui"},
     {"import": IMP_OK, "resolve_capture_mode": "client",
      "check_wake_word_requirements": {"available": True, "phrase": "hey hermes"}})
case("_wake_probe", {"cfg": {"surface": "tui"}, "params": {}, "surface": "desktop"},
     {"import": IMP_OK, "resolve_capture_mode": "client",
      "check_wake_word_requirements": {"available": True}})
case("_wake_probe", {"cfg": {}, "params": {"client_capture": True}, "surface": "tui"},
     {"import": IMP_OK, "resolve_capture_mode": "client",
      "check_wake_word_requirements": {"available": True}})
case("_wake_probe", {"cfg": {}, "params": {}, "surface": "cli"},
     {"import": IMP_OK, "resolve_capture_mode": "local",
      "check_wake_word_requirements": {"available": False, "hint": "no mic"}})
case("_wake_probe", {"cfg": {}, "params": {"client_capture": False}, "surface": "tui"},
     {"import": IMP_OK, "resolve_capture_mode": "local",
      "check_wake_word_requirements": {"available": True}})
case("_wake_probe", {"cfg": {}, "params": {}, "surface": "gui"},
     {"import": rs("RuntimeError", "no wake")})
case("_wake_probe", {"cfg": {}, "params": {}, "surface": "gui"},
     {"import": IMP_OK, "resolve_capture_mode": rs("SystemExit", "x")})
case("_wake_probe", {"cfg": {}, "params": {}, "surface": "gui"},
     {"import": IMP_OK, "resolve_capture_mode": "local"}, fail_closed=True)

# ---------------- _voice_toggle_mode ----------------
case("_voice_toggle_mode", {"rid": RID, "params": {"action": "on"}},
     {"env_set": None, "import": IMP_OK, "voice_stop_hint": "say stop",
      "voice_tts_enabled": True, "tts_lease_async": None,
      "voice_status_payload": {"enabled": True, "record_key": "ctrl+b"},
      "ok": ok_env(RID, {"enabled": True, "record_key": "ctrl+b"})})
case("_voice_toggle_mode", {"rid": RID, "params": {"action": "on"}},
     {"env_set": None, "import": IMP_OK, "voice_stop_hint": "stop",
      "voice_tts_enabled": False,
      "voice_status_payload": {"enabled": True},
      "ok": ok_env(RID, {"enabled": True})})
case("_voice_toggle_mode", {"rid": RID, "params": {"action": "on"}},
     {"env_set": None, "import": rs("ImportError", "no module"),
      "voice_tts_enabled": False,
      "voice_status_payload": {"enabled": True},
      "ok": ok_env(RID, {"enabled": True})})
case("_voice_toggle_mode", {"rid": RID, "params": {"action": "on"}},
     {"env_set": None, "import": IMP_OK, "voice_stop_hint": rs("RuntimeError", "hint"),
      "voice_tts_enabled": True, "tts_lease_async": None,
      "voice_status_payload": {"enabled": True},
      "ok": ok_env(RID, {"enabled": True})})
case("_voice_toggle_mode", {"rid": RID, "params": {"action": "on"}},
     {"env_set": None, "import": IMP_OK, "voice_stop_hint": "stop",
      "voice_tts_enabled": rs("SystemExit", "x")})
case("_voice_toggle_mode", {"rid": RID, "params": {"action": "off"}},
     {"env_set": None, "import": IMP_OK, "stop_continuous": None,
      "set_voice_tts": None,
      "voice_status_payload": {"enabled": False},
      "ok": ok_env(RID, {"enabled": False})})
case("_voice_toggle_mode", {"rid": RID, "params": {}},
     {"env_set": None, "import": rs("ImportError", "no module"),
      "set_voice_tts": None,
      "voice_status_payload": {"enabled": False},
      "ok": ok_env(RID, {"enabled": False})})
case("_voice_toggle_mode", {"rid": RID, "params": {"action": "off"}},
     {"env_set": None, "import": IMP_OK, "stop_continuous": rs("RuntimeError", "loop"),
      "set_voice_tts": None,
      "voice_status_payload": {"enabled": False},
      "ok": ok_env(RID, {"enabled": False})})
case("_voice_toggle_mode", {"rid": RID, "params": {"action": "off"}},
     {"env_set": None, "import": rs("SystemExit", "x")})
case("_voice_toggle_mode", {"rid": RID, "params": {"action": "on"}},
     {"env_set": rs("RuntimeError", "env")})
case("_voice_toggle_mode", {"rid": RID, "params": {"action": "on"}},
     {"env_set": None, "import": IMP_OK, "voice_stop_hint": "stop",
      "voice_tts_enabled": False,
      "voice_status_payload": rs("RuntimeError", "payload")})
case("_voice_toggle_mode", {"rid": RID, "params": {"action": "on"}}, {},
     fail_closed=True)


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"generated {len(CASES)} cases across {len(counts)} funcs -> {OUT}")
    for k in sorted(counts):
        print(f"  {k}: {counts[k]}")


if __name__ == "__main__":
    main()
