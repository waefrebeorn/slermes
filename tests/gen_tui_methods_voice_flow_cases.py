#!/usr/bin/env python3
"""Generate tests/fixtures/tui_methods_voice_flow.jsonl for the TUI voice-method
FLOW port (src/gateway/tui_methods_voice_flow.c).

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

OUT = Path(__file__).resolve().parent / "fixtures" / "tui_methods_voice_flow.jsonl"
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


def err_env(rid, code, message):
    return {"jsonrpc": "2.0", "id": rid, "error": {"code": code, "message": message}}


RID = "r1"
IMP_OK = "mod"


# ---------------- _caller_transport ----------------
case("_caller_transport", io={"current_transport": "t1"})
case("_caller_transport", io={"current_transport": None})
case("_caller_transport", io={"current_transport": ""})
case("_caller_transport", io={"current_transport": rs("RuntimeError", "transport down")})
case("_caller_transport", io={"current_transport": rs("SystemExit", "bye")})
case("_caller_transport", io={}, fail_closed=True)

# ---------------- _voice_emit ----------------
case("_voice_emit", {"event": "voice.transcript", "payload": {"text": "hi"}},
     {"emit": None}, state={"voice_event_sid": "s1", "voice_event_transport": None})
case("_voice_emit", {"event": "voice.status", "payload": {"state": "idle"}},
     {"bind_transport": "tok1", "emit": None, "reset_transport": None},
     state={"voice_event_sid": "s2", "voice_event_transport": "t1"})
case("_voice_emit", {"event": "x"},
     {"bind_transport": rs("RuntimeError", "bind fail")},
     state={"voice_event_transport": "t1"})
case("_voice_emit", {"event": "x"},
     {"bind_transport": "tok", "emit": rs("RuntimeError", "emit fail"),
      "reset_transport": None}, state={"voice_event_transport": "t1"})
case("_voice_emit", {"event": "x"},
     {"bind_transport": "tok", "emit": rs("RuntimeError", "emit fail"),
      "reset_transport": rs("SystemExit", "reset fail")},
     state={"voice_event_transport": "t1"})
case("_voice_emit", {"event": "x"}, {}, state={"voice_event_transport": None},
     fail_closed=True)

# ---------------- _resume_voice_wake ----------------
case("_resume_voice_wake", io={}, state={"voice_wake_owner": None})
case("_resume_voice_wake", io={"wake_resume_if_owner": True},
     state={"voice_wake_owner": "t1"})
case("_resume_voice_wake", io={"wake_resume_if_owner": rs("RuntimeError", "resume")},
     state={"voice_wake_owner": "t1"})
case("_resume_voice_wake", io={"wake_resume_if_owner": rs("SystemExit", "stop")},
     state={"voice_wake_owner": "t1"})
case("_resume_voice_wake", io={}, state={"voice_wake_owner": "t1"}, fail_closed=True)

# ---------------- _voice_mode_enabled ----------------
case("_voice_mode_enabled", io={"env_get": "1"})
case("_voice_mode_enabled", io={"env_get": " 1 "})
case("_voice_mode_enabled", io={"env_get": "0"})
case("_voice_mode_enabled", io={"env_get": rs("RuntimeError", "env fail")})
case("_voice_mode_enabled", io={"env_get": rs("SystemExit", "x")})
case("_voice_mode_enabled", io={}, fail_closed=True)

# ---------------- _voice_tts_enabled ----------------
case("_voice_tts_enabled", io={"env_get": "1"})
case("_voice_tts_enabled", io={"env_get": "0"})
case("_voice_tts_enabled", io={"env_get": " 1\t"})
case("_voice_tts_enabled", io={"env_get": rs("RuntimeError", "env fail")})
case("_voice_tts_enabled", io={}, fail_closed=True)
case("_voice_tts_enabled", io={"env_get": rs("KeyboardInterrupt", "")})

# ---------------- _session_voice_enabled ----------------
case("_session_voice_enabled", {"session_id": "s1"},
     {"import": IMP_OK, "local_voice_enabled": True})
case("_session_voice_enabled", {"session_id": "s1"},
     {"import": IMP_OK, "local_voice_enabled": None, "env_get": "1"})
case("_session_voice_enabled", {"session_id": "s1"},
     {"import": rs("ImportError", "no module"), "env_get": "0"})
case("_session_voice_enabled", {"session_id": "s1"},
     {"import": IMP_OK, "local_voice_enabled": rs("ImportError", "gone"), "env_get": "1"})
case("_session_voice_enabled", {"session_id": "s1"},
     {"import": IMP_OK, "local_voice_enabled": rs("RuntimeError", "boom")})
case("_session_voice_enabled", {"session_id": "s1"},
     {"import": IMP_OK, "local_voice_enabled": rs("SystemExit", "quit")})
case("_session_voice_enabled", {"session_id": "s1"}, {}, fail_closed=True)

# ---------------- _set_session_voice_off ----------------
case("_set_session_voice_off", {"session_id": "s1"},
     {"import": IMP_OK, "set_local_voice_mode": True})
case("_set_session_voice_off", {"session_id": "s1"},
     {"import": IMP_OK, "set_local_voice_mode": False})
case("_set_session_voice_off", {"session_id": "s1"},
     {"import": rs("ImportError", "no module")})
case("_set_session_voice_off", {"session_id": "s1"},
     {"import": IMP_OK, "set_local_voice_mode": rs("ImportError", "gone")})
case("_set_session_voice_off", {"session_id": "s1"},
     {"import": IMP_OK, "set_local_voice_mode": rs("RuntimeError", "boom")})
case("_set_session_voice_off", {"session_id": "s1"},
     {"import": IMP_OK, "set_local_voice_mode": rs("SystemExit", "x")})
case("_set_session_voice_off", {"session_id": "s1"}, {}, fail_closed=True)

# ---------------- _capture_live ----------------
case("_capture_live", io={"import": IMP_OK, "is_continuous_active": True})
case("_capture_live", io={"import": IMP_OK, "is_continuous_active": False})
case("_capture_live", io={"import": rs("ImportError", "old wrapper")})
case("_capture_live", io={"import": rs("RuntimeError", "broken")})
case("_capture_live", io={"import": IMP_OK, "is_continuous_active": rs("RuntimeError", "probe")})
case("_capture_live", io={"import": IMP_OK, "is_continuous_active": rs("SystemExit", "x")})
case("_capture_live", io={}, fail_closed=True)

# ---------------- _end_voice_chat ----------------
case("_end_voice_chat", {"stop_loop": False, "stop_tts": False},
     {"env_set": None})
case("_end_voice_chat", {"stop_loop": True, "stop_tts": False},
     {"env_set": None, "import": IMP_OK, "stop_continuous": None})
case("_end_voice_chat", {"stop_loop": False, "stop_tts": True},
     {"env_set": None, "tts_stream_stop": None})
case("_end_voice_chat", {"stop_loop": True, "stop_tts": True},
     {"env_set": None, "import": IMP_OK, "stop_continuous": None,
      "tts_stream_stop": None})
case("_end_voice_chat", {"stop_loop": True, "stop_tts": True},
     {"env_set": None, "import": IMP_OK,
      "stop_continuous": rs("RuntimeError", "no loop"),
      "tts_stream_stop": rs("RuntimeError", "no tts")})
case("_end_voice_chat", {"stop_loop": True},
     {"env_set": None, "import": IMP_OK, "stop_continuous": rs("SystemExit", "x")})
case("_end_voice_chat", {"stop_tts": True},
     {"env_set": rs("RuntimeError", "env write")})
case("_end_voice_chat", {"stop_loop": False, "stop_tts": False}, {}, fail_closed=True)

# ---------------- _end_session_voice_chat ----------------
case("_end_session_voice_chat", {"session_id": "s1", "stop_loop": False, "stop_tts": False},
     {"import": IMP_OK, "set_local_voice_mode": False, "env_set": None})
case("_end_session_voice_chat", {"session_id": "s1", "stop_loop": True, "stop_tts": True},
     {"import": IMP_OK, "set_local_voice_mode": False, "env_set": None,
      "tts_stream_stop": None})
case("_end_session_voice_chat", {"session_id": "s1", "stop_loop": False, "stop_tts": True},
     {"import": IMP_OK, "set_local_voice_mode": True, "tts_stream_stop": None})
case("_end_session_voice_chat", {"session_id": "s1", "stop_loop": False, "stop_tts": False},
     {"import": IMP_OK, "set_local_voice_mode": True})
case("_end_session_voice_chat", {"session_id": "s1"},
     {"import": rs("ImportError", "no config"), "env_set": None})
case("_end_session_voice_chat", {"session_id": "s1"},
     {"import": IMP_OK, "set_local_voice_mode": rs("RuntimeError", "boom")})
case("_end_session_voice_chat", {"session_id": "s1", "stop_tts": True},
     {"import": IMP_OK, "set_local_voice_mode": True,
      "tts_stream_stop": rs("SystemExit", "x")})
case("_end_session_voice_chat", {"session_id": "s1"}, {}, fail_closed=True)

# ---------------- _running_sessions ----------------
case("_running_sessions", state={"sessions": {}})
case("_running_sessions", state={"sessions": {"a": {"running": True}}})
case("_running_sessions", state={"sessions": {"a": {"running": False},
                                             "b": {"running": True}}})
case("_running_sessions", state={"sessions": {"a": {"running": 0},
                                             "b": {"running": 1}}})
case("_running_sessions", state={})
case("_running_sessions", state={"sessions": {"a": {"running": []},
                                             "b": {"running": "yes"}}})

# ---------------- _any_session_running ----------------
case("_any_session_running", state={"sessions": {}})
case("_any_session_running", state={"sessions": {"a": {"running": True}}})
case("_any_session_running", state={"sessions": {"a": {"running": False}}})
case("_any_session_running", state={"sessions": {"a": {"running": True},
                                                "b": {"running": False}}})
case("_any_session_running", state={})
case("_any_session_running", state={"sessions": {"a": {"running": 0}}})

# ---------------- _arm_barge_listener_if_enabled ----------------
case("_arm_barge_listener_if_enabled", {}, {"env_get": "0"})
case("_arm_barge_listener_if_enabled", {},
     {"env_get": "1", "load_cfg": {"voice": {"barge_in": True}},
      "arm_full_duplex_listener": None})
case("_arm_barge_listener_if_enabled", {},
     {"env_get": "1", "load_cfg": {"voice": {"barge_in": False}}})
case("_arm_barge_listener_if_enabled", {},
     {"env_get": "1", "load_cfg": {"voice": {}},
      "arm_full_duplex_listener": None})
case("_arm_barge_listener_if_enabled", {},
     {"env_get": "1", "load_cfg": {"voice": {"barge_in": None}}})
case("_arm_barge_listener_if_enabled", {}, {"env_get": rs("RuntimeError", "env")})
case("_arm_barge_listener_if_enabled", {},
     {"env_get": "1", "load_cfg": rs("RuntimeError", "cfg")})
case("_arm_barge_listener_if_enabled", {}, {"env_get": "1"}, fail_closed=True)

# ---------------- _fd_barge_params ----------------
case("_fd_barge_params", {"barge_in_threshold_multiplier": 0.5,
                          "barge_in_grace_seconds": 0.25})
case("_fd_barge_params", {})
case("_fd_barge_params", {"barge_in_threshold_multiplier": 2,
                          "barge_in_grace_seconds": 1})
case("_fd_barge_params", {"barge_in_threshold_multiplier": "bad",
                          "barge_in_grace_seconds": "bad"})
case("_fd_barge_params", {"barge_in_threshold_multiplier": 0,
                          "barge_in_grace_seconds": 0})
case("_fd_barge_params", {"barge_in_threshold_multiplier": "1.5",
                          "barge_in_grace_seconds": "0.75"})
case("_fd_barge_params", {"barge_in_grace_seconds": -1})
case("_fd_barge_params", {"barge_in_threshold_multiplier": True,
                          "barge_in_grace_seconds": False})

# ---------------- _voice_cfg_dict ----------------
case("_voice_cfg_dict", io={"load_cfg": {"voice": {"record_key": "cmd+r"}}})
case("_voice_cfg_dict", io={"load_cfg": {"voice": None}})
case("_voice_cfg_dict", io={"load_cfg": 5})
case("_voice_cfg_dict", io={"load_cfg": {"other": 1}})
case("_voice_cfg_dict", io={"load_cfg": [1, 2]})
case("_voice_cfg_dict", io={"load_cfg": rs("RuntimeError", "cfg")})
case("_voice_cfg_dict", io={}, fail_closed=True)

# ---------------- _voice_cfg_number ----------------
case("_voice_cfg_number", {"value": 120, "default": 200})
case("_voice_cfg_number", {"value": "120", "default": 200})
case("_voice_cfg_number", {"value": True, "default": 200})
case("_voice_cfg_number", {"value": None, "default": 3.0})
case("_voice_cfg_number", {"default": 3.0})
case("_voice_cfg_number", {"value": 10.5, "default": 200})

# ---------------- _voice_status_payload ----------------
case("_voice_status_payload", io={"load_cfg": {"voice": {"record_key": "cmd+r"}},
                                  "env_get": ["1", "0"]})
case("_voice_status_payload", {"extra": {"stop_hint": "say stop"}},
     {"load_cfg": {"voice": {"record_key": ""}}, "env_get": ["0", "1"]})
case("_voice_status_payload", io={"load_cfg": {"voice": {"record_key": 5}},
                                  "env_get": ["0", "0"]})
case("_voice_status_payload", io={"load_cfg": 7, "env_get": ["1", "1"]})
case("_voice_status_payload", io={"load_cfg": rs("RuntimeError", "cfg")})
case("_voice_status_payload", io={"load_cfg": {"voice": {}},
                                  "env_get": rs("RuntimeError", "env")})
case("_voice_status_payload", io={}, fail_closed=True)

# ---------------- _wake_owner_snapshot ----------------
case("_wake_owner_snapshot", state={"wake_owner_transport": "t1",
                                    "wake_owner_surface": "tui"})
case("_wake_owner_snapshot", state={"wake_owner_transport": None,
                                    "wake_owner_surface": ""})
case("_wake_owner_snapshot", state={"wake_owner_transport": "t2",
                                    "wake_owner_surface": "gui"})
case("_wake_owner_snapshot", state={})
case("_wake_owner_snapshot", state={"wake_owner_surface": 5})

# ---------------- _release_wake_for_transport ----------------
case("_release_wake_for_transport", {"transport": "t1"},
     {"import": IMP_OK, "stop_listening": None},
     state={"wake_owner_transport": "t1", "wake_owner_surface": "tui"})
case("_release_wake_for_transport", {"transport": "t1"}, {},
     state={"wake_owner_transport": "t2", "wake_owner_surface": "tui"})
case("_release_wake_for_transport", {"transport": "t1"}, {},
     state={"wake_owner_transport": None, "wake_owner_surface": ""})
case("_release_wake_for_transport", {"transport": "t1"},
     {"import": IMP_OK, "stop_listening": rs("RuntimeError", "stop")},
     state={"wake_owner_transport": "t1"})
case("_release_wake_for_transport", {"transport": "t1"},
     {"import": IMP_OK, "stop_listening": rs("SystemExit", "x")},
     state={"wake_owner_transport": "t1"})
case("_release_wake_for_transport", {"transport": "t1"},
     {"import": rs("ImportError", "no wake")},
     state={"wake_owner_transport": "t1"})
case("_release_wake_for_transport", {"transport": "t1"}, {},
     state={"wake_owner_transport": "t1"}, fail_closed=True)

# ---------------- _release_gateway_wake_owner ----------------
case("_release_gateway_wake_owner", io={"import": IMP_OK, "stop_listening": None},
     state={"wake_owner_transport": "t1"})
case("_release_gateway_wake_owner", io={},
     state={"wake_owner_transport": None})
case("_release_gateway_wake_owner", io={"import": IMP_OK,
                                       "stop_listening": rs("RuntimeError", "stop")},
     state={"wake_owner_transport": "t2"})
case("_release_gateway_wake_owner", io={"import": IMP_OK,
                                       "stop_listening": rs("SystemExit", "x")},
     state={"wake_owner_transport": "t3"})
case("_release_gateway_wake_owner", io={},
     state={"wake_owner_transport": "t1"}, fail_closed=True)

# ---------------- _persist_wake_enabled ----------------
case("_persist_wake_enabled", {"enabled": True},
     {"import": IMP_OK, "save_config_value": 1})
case("_persist_wake_enabled", {"enabled": False},
     {"import": IMP_OK, "save_config_value": 0})
case("_persist_wake_enabled", {"enabled": True},
     {"import": rs("ImportError", "no cli")})
case("_persist_wake_enabled", {"enabled": True},
     {"import": IMP_OK, "save_config_value": rs("RuntimeError", "write")})
case("_persist_wake_enabled", {"enabled": True},
     {"import": IMP_OK, "save_config_value": rs("SystemExit", "x")})
case("_persist_wake_enabled", {"enabled": True}, {}, fail_closed=True)

# ---------------- _owner_result ----------------
case("_owner_result", {"rid": RID, "field": "stopped", "ok": True},
     {"ok": ok_env(RID, {"stopped": True})})
case("_owner_result", {"rid": RID, "field": "paused", "ok": False},
     {"ok": ok_env(RID, {"paused": False})})
case("_owner_result", {"rid": RID, "field": "fed", "ok": True,
                       "extra": {"disabled_persisted": False}},
     {"ok": ok_env(RID, {"fed": True})})
case("_owner_result", {"rid": RID, "field": "resumed", "ok": True},
     {"ok": rs("RuntimeError", "ok fail")})
case("_owner_result", {"rid": RID, "field": "resumed", "ok": True},
     {"ok": rs("SystemExit", "x")})
case("_owner_result", {"rid": RID, "field": "stopped", "ok": True}, {}, fail_closed=True)

# ---------------- _deliver_fd_transcript ----------------
case("_deliver_fd_transcript", {"text": "hello"},
     {"import": IMP_OK, "is_voice_stop_phrase": False, "emit": None})
case("_deliver_fd_transcript", {"text": "stop"},
     {"import": IMP_OK, "is_voice_stop_phrase": True,
      "set_local_voice_mode": False, "env_set": None, "emit": None})
case("_deliver_fd_transcript", {"text": "hello"},
     {"import": rs("RuntimeError", "no transcript module"), "emit": None})
case("_deliver_fd_transcript", {"text": "hello"},
     {"import": IMP_OK, "is_voice_stop_phrase": rs("RuntimeError", "probe"), "emit": None})
case("_deliver_fd_transcript", {"text": "hello"},
     {"import": IMP_OK, "is_voice_stop_phrase": rs("SystemExit", "x")})
case("_deliver_fd_transcript", {"text": "hello"},
     {"import": IMP_OK, "is_voice_stop_phrase": False, "emit": rs("RuntimeError", "emit")})
case("_deliver_fd_transcript", {"text": "hello"}, {}, fail_closed=True)

# ---------------- _voice_toggle_status ----------------
case("_voice_toggle_status", {"rid": RID},
     {"load_cfg": {"voice": {"record_key": "ctrl+b"}}, "env_get": ["1", "1"],
      "import": IMP_OK,
      "check_voice_requirements": {"available": True, "audio_available": True,
                                   "stt_available": False, "details": "ok"},
      "ok": ok_env(RID, {})})
case("_voice_toggle_status", {"rid": RID},
     {"load_cfg": {"voice": {}}, "env_get": ["0", "0"], "import": IMP_OK,
      "check_voice_requirements": rs("RuntimeError", "deps"), "ok": ok_env(RID, {})})
case("_voice_toggle_status", {"rid": RID},
     {"load_cfg": {"voice": {}}, "env_get": ["1", "1"], "import": IMP_OK,
      "check_voice_requirements": rs("SystemExit", "x")})
case("_voice_toggle_status", {"rid": RID},
     {"load_cfg": rs("RuntimeError", "cfg")})
case("_voice_toggle_status", {"rid": RID},
     {"load_cfg": {"voice": {}}, "env_get": ["1", "1"], "import": IMP_OK,
      "check_voice_requirements": {}, "ok": rs("RuntimeError", "ok")})
case("_voice_toggle_status", {"rid": RID}, {}, fail_closed=True)

# ---------------- _set_voice_tts ----------------
case("_set_voice_tts", {"on": True},
     {"env_set": None, "tts_lease_async": None})
case("_set_voice_tts", {"on": False},
     {"env_set": None, "tts_stream_stop": None, "tts_lease_async": None})
case("_set_voice_tts", {"on": True}, {"env_set": rs("RuntimeError", "env")})
case("_set_voice_tts", {"on": False},
     {"env_set": None, "tts_stream_stop": rs("RuntimeError", "stop")})
case("_set_voice_tts", {"on": True},
     {"env_set": None, "tts_lease_async": rs("SystemExit", "x")})
case("_set_voice_tts", {"on": True}, {}, fail_closed=True)

# ---------------- _voice_toggle_tts ----------------
case("_voice_toggle_tts", {"rid": RID},
     {"env_get": ["1", "0", "1", "1"], "env_set": None,
      "tts_lease_async": None, "load_cfg": {"voice": {}}, "ok": ok_env(RID, {})})
case("_voice_toggle_tts", {"rid": RID},
     {"env_get": ["0"], "err": err_env(RID, 4014, "enable voice mode first: /voice on")})
case("_voice_toggle_tts", {"rid": RID},
     {"env_get": ["1", "1", "1", "0"], "env_set": None, "tts_stream_stop": None,
      "tts_lease_async": None, "load_cfg": {"voice": {}},
      "ok": ok_env(RID, {"record_key": "ctrl+b"})})
case("_voice_toggle_tts", {"rid": RID},
     {"env_get": rs("RuntimeError", "env")})
case("_voice_toggle_tts", {"rid": RID},
     {"env_get": ["1", "0", "1", "1"], "env_set": None,
      "tts_lease_async": None, "load_cfg": rs("RuntimeError", "cfg")})
case("_voice_toggle_tts", {"rid": RID},
     {"env_get": ["1", "0", "1", "1"], "env_set": None,
      "tts_lease_async": None, "load_cfg": {"voice": {}},
      "ok": rs("SystemExit", "x")})
case("_voice_toggle_tts", {"rid": RID}, {}, fail_closed=True)

# ---------------- _vr_transcript ----------------
case("_vr_transcript", {"payload": {"text": "hi"}},
     {"emit": None}, state={"voice_event_sid": "s1", "voice_wake_owner": None})
case("_vr_transcript", {"payload": {"no_speech_limit": True}},
     {"emit": None, "wake_resume_if_owner": True},
     state={"voice_event_sid": "s1", "voice_wake_owner": "t1"})
case("_vr_transcript", {"payload": {"text": "hi"}},
     {"emit": rs("RuntimeError", "emit")})
case("_vr_transcript", {"payload": {"text": "hi"}},
     {"emit": None, "wake_resume_if_owner": rs("RuntimeError", "resume")},
     state={"voice_wake_owner": "t1"})
case("_vr_transcript", {"payload": {"text": "hi"}}, {}, fail_closed=True)


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
