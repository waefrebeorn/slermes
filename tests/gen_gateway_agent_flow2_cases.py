#!/usr/bin/env python3
"""Generate tests/fixtures/gateway_agent_flow2.jsonl — the scripted-seam case
corpus for the gateway_agent_flow2 FLOW port (36 unclaimed methods of
hermes_cli/gateway_agent.py). >=5 cases per func: happy paths, every branch,
scripted Exception-family raises (swallowed where the live code catches),
BaseException passthrough (SystemExit), and fail-closed missing-script cases.

Deterministic, stdlib-only, ASCII. The oracle AST-extracts the LIVE defs and
replays each case against the same scripts; the C harness feeds them to
ga2_call. Callback attributes travel as seam-name markers (string) or null.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "gateway_agent_flow2.jsonl")
CASES = []


def case(func, args=None, state=None, io=None):
    CASES.append({"func": func, "in": {"args": args or {}, "state": state or {},
                                       "io": io or {}}})


def rs(ret=None, raise_=None):
    d = {"__script__": True}
    if raise_ is not None:
        d["raise"] = raise_
    else:
        d["ret"] = ret
    return d


def rais(kind, msg="boom", **kw):
    d = {"type": kind, "message": msg}
    d.update(kw)
    return d


def st(**kw):
    d = {"_session_id": "s1", "connection_epoch": 1,
         "stream_delta_callback": None, "interim_assistant_callback": None,
         "tool_gen_callback": None, "tool_progress_callback": None,
         "tool_complete_callback": None, "thinking_callback": None,
         "_reasoning_callback": None, "notice_callback": None,
         "notice_clear_callback": None, "reaction_callback": None,
         "control_update_callback": None, "connection_callback": None,
         "connection_update_callback": None, "modal_withdraw_callback": None,
         "clarify_callback": None, "_turn": None, "_usage_stale": False,
         "_generation": None, "_open_request": None,
         "session_api_calls": 0, "session_input_tokens": 0, "session_output_tokens": 0}
    d.update(kw)
    return d


def turn(**kw):
    d = {"session_id": "s1", "admission": "a1", "_shown": [], "_spoken": [],
         "seen": [], "answered": [], "gapped": False}
    d.update(kw)
    return d


def cb(name):
    """state marker + io script for a present callback sink returning null."""
    return {name: name}, {name: None}


GCE = rais("GatewayClientError", "rpc failed")
SE = rais("SystemExit", "1")
VE = rais("ValueError", "bad")
RE = rais("RuntimeError", "boom")

# ── tool_progress_mode setter ──
for mode in ("off", "all", "bogus", "verbose", "new"):
    case("tool_progress_mode", {"mode": mode}, st(_tool_progress_mode="all"),
         io={"connection.call": None})
case("tool_progress_mode", {"mode": "off"}, st(_tool_progress_mode="all"),
     io={"connection.call": rs(raise_=GCE)})
case("tool_progress_mode", {"mode": "new"}, st(_tool_progress_mode="all"),
     io={"connection.call": rs(raise_=SE)})
case("tool_progress_mode", {"mode": "off"}, st(_tool_progress_mode="all"), io={})

# ── pin_reasoning ──
for ret in (True, False, None):
    case("pin_reasoning", {"config": {"effort": "high"}}, st(),
         io={"self._set_reasoning": ret})
case("pin_reasoning", {"config": None}, st(), io={"self._set_reasoning": rs(raise_=VE)})
case("pin_reasoning", {"config": {"effort": "low"}}, st(), io={"self._set_reasoning": rs(raise_=SE)})
case("pin_reasoning", {"config": {"effort": "high"}}, st(), io={})

# ── stage_turn_reasoning ──
for ret in ("high", None, "none"):
    case("stage_turn_reasoning", {"config": {"effort": "high"}}, st(_turn_reasoning="old"),
         io={"_effort_word": ret})
case("stage_turn_reasoning", {"config": None}, st(), io={"_effort_word": rs(raise_=RE)})
case("stage_turn_reasoning", {"config": {"enabled": False}}, st(), io={"_effort_word": rs(raise_=SE)})
case("stage_turn_reasoning", {"config": {"effort": "high"}}, st(), io={})

# ── restore_reasoning (no seams) ──
for cfg in ({"effort": "high"}, None, {}, {"effort": "low"}, {"enabled": False}):
    case("restore_reasoning", {"config": cfg}, st(_turn_reasoning="high", _reasoning_config={"x": 1}))

# ── reasoning_callback setter ──
case("reasoning_callback", {"callback": "reasoning_callback"}, st(),
     io={"self._sync_show_reasoning": None})
case("reasoning_callback", {"callback": None}, st(_reasoning_callback="reasoning_callback"),
     io={"self._sync_show_reasoning": None})
case("reasoning_callback", {"callback": "reasoning_callback"}, st(),
     io={"self._sync_show_reasoning": rs(raise_=RE)})
case("reasoning_callback", {"callback": "reasoning_callback"}, st(),
     io={"self._sync_show_reasoning": rs(raise_=SE)})
case("reasoning_callback", {"callback": "reasoning_callback"}, st(), io={})

# ── _close_segment ──
T = turn(_shown=["hello"], _spoken=["hi"])
case("_close_segment", {"starts": {}, "separator": True},
     st(_turn=T, stream_delta_callback="stream_delta_callback"), io={"stream_delta_callback": None})
case("_close_segment", {"starts": {}, "separator": False},
     st(_turn=T, stream_delta_callback="stream_delta_callback"), io={"stream_delta_callback": None})
case("_close_segment", {"starts": {}, "separator": False},
     st(_turn=turn(), stream_delta_callback="stream_delta_callback"), io={"stream_delta_callback": None})
case("_close_segment", {"starts": {}, "separator": True}, st(_turn=T), io={})
case("_close_segment", {"starts": {}, "separator": False},
     st(_turn=None, stream_delta_callback="stream_delta_callback"), io={"stream_delta_callback": None})
case("_close_segment", {"starts": {}, "separator": True},
     st(_turn=T, stream_delta_callback="stream_delta_callback"), io={"stream_delta_callback": rs(raise_=SE)})

# ── _on_delta ──
case("_on_delta", {"payload": {"text": "chunk"}, "stream_callback": "stream_callback", "starts": {}},
     st(_turn=turn(), stream_delta_callback="stream_delta_callback"),
     io={"stream_delta_callback": None, "stream_callback": None})
case("_on_delta", {"payload": {"text": "chunk"}, "stream_callback": None, "starts": {}},
     st(_turn=turn(), stream_delta_callback="stream_delta_callback"), io={"stream_delta_callback": None})
case("_on_delta", {"payload": {"text": "chunk"}, "stream_callback": None, "starts": {}},
     st(_turn=turn(gapped=True), stream_delta_callback="stream_delta_callback"), io={"stream_delta_callback": None})
case("_on_delta", {"payload": {"text": None}, "stream_callback": None, "starts": {}},
     st(_turn=turn(_shown=["x"]), stream_delta_callback="stream_delta_callback"), io={"stream_delta_callback": None})
case("_on_delta", {"payload": {"text": 5}, "stream_callback": None, "starts": {}},
     st(_turn=turn(), stream_delta_callback="stream_delta_callback"), io={"stream_delta_callback": None})
case("_on_delta", {"payload": {"text": "chunk"}, "stream_callback": None, "starts": {}},
     st(_turn=None, stream_delta_callback="stream_delta_callback"), io={"stream_delta_callback": None})
case("_on_delta", {"payload": {"text": "chunk"}, "stream_callback": "stream_callback", "starts": {}},
     st(_turn=turn(), stream_delta_callback="stream_delta_callback"), io={"stream_delta_callback": rs(raise_=SE)})

# ── _on_interim ──
case("_on_interim", {"payload": {"text": "interim", "already_streamed": True}, "stream_callback": None, "starts": {}},
     st(_turn=turn(), interim_assistant_callback="interim_assistant_callback"),
     io={"interim_assistant_callback": None})
case("_on_interim", {"payload": {"text": "interim"}, "stream_callback": None, "starts": {}},
     st(_turn=turn(), interim_assistant_callback="interim_assistant_callback"),
     io={"interim_assistant_callback": None})
case("_on_interim", {"payload": {"text": "printed"}, "stream_callback": None, "starts": {}},
     st(_turn=turn()), io={"self._emit_print": None})
case("_on_interim", {"payload": {"text": ""}, "stream_callback": None, "starts": {}},
     st(_turn=turn()), io={})
case("_on_interim", {"payload": {"text": "  "}, "stream_callback": None, "starts": {}},
     st(_turn=turn()), io={})
case("_on_interim", {"payload": {"text": "x"}, "stream_callback": None, "starts": {}},
     st(_turn=turn(), interim_assistant_callback="interim_assistant_callback"),
     io={"interim_assistant_callback": rs(raise_=SE)})

# ── _on_tool_generating ──
case("_on_tool_generating", {"payload": {"name": "search"}, "stream_callback": None, "starts": {}},
     st(tool_gen_callback="tool_gen_callback"), io={"tool_gen_callback": None})
case("_on_tool_generating", {"payload": {"name": None}, "stream_callback": None, "starts": {}},
     st(tool_gen_callback="tool_gen_callback"), io={"tool_gen_callback": None})
case("_on_tool_generating", {"payload": {"name": "search"}, "stream_callback": None, "starts": {}}, st(), io={})
case("_on_tool_generating", {"payload": {"name": "x"}, "stream_callback": None, "starts": {}},
     st(tool_gen_callback="tool_gen_callback"), io={"tool_gen_callback": rs(raise_=RE)})
case("_on_tool_generating", {"payload": {"name": "x"}, "stream_callback": None, "starts": {}},
     st(tool_gen_callback="tool_gen_callback"), io={"tool_gen_callback": rs(raise_=SE)})
case("_on_tool_generating", {"payload": {"name": "x"}, "stream_callback": None, "starts": {}},
     st(tool_gen_callback="tool_gen_callback"), io={})

# ── _on_tool_progress ──
TP = {"name": "search", "message": "working", "tool_id": "t1", "progress": 2, "total": 5, "percent": 40}
case("_on_tool_progress", {"payload": TP, "stream_callback": None, "starts": {}},
     st(tool_progress_callback="tool_progress_callback"), io={"tool_progress_callback": None})
case("_on_tool_progress", {"payload": {"name": "x"}, "stream_callback": None, "starts": {}},
     st(tool_progress_callback="tool_progress_callback"), io={"tool_progress_callback": None})
case("_on_tool_progress", {"payload": TP, "stream_callback": None, "starts": {}}, st(), io={})
case("_on_tool_progress", {"payload": TP, "stream_callback": None, "starts": {}},
     st(tool_progress_callback="tool_progress_callback"), io={"tool_progress_callback": rs(raise_=RE)})
case("_on_tool_progress", {"payload": TP, "stream_callback": None, "starts": {}},
     st(tool_progress_callback="tool_progress_callback"), io={"tool_progress_callback": rs(raise_=SE)})
case("_on_tool_progress", {"payload": TP, "stream_callback": None, "starts": {}},
     st(tool_progress_callback="tool_progress_callback"), io={})

# ── _on_tool_start ──
TS_IO = {"time.monotonic": 100, "build_tool_preview": "preview", "tool_progress_callback": None}
case("_on_tool_start", {"payload": {"name": "search", "args": {"q": "x"}, "tool_id": "t1"}, "stream_callback": None, "starts": {}},
     st(_turn=turn(), tool_progress_callback="tool_progress_callback"), io=dict(TS_IO))
case("_on_tool_start", {"payload": {"tool_name": "bash", "args": "notdict", "tool_call_id": "c2"}, "stream_callback": None, "starts": {}},
     st(_turn=turn(), tool_progress_callback="tool_progress_callback"), io=dict(TS_IO))
case("_on_tool_start", {"payload": {}, "stream_callback": None, "starts": {}},
     st(_turn=turn(), tool_progress_callback="tool_progress_callback"), io=dict(TS_IO))
case("_on_tool_start", {"payload": {"name": "search", "tool_id": "t1"}, "stream_callback": None, "starts": {}},
     st(_turn=turn()), io={"time.monotonic": 100})
case("_on_tool_start", {"payload": {"name": "search", "tool_id": "t1"}, "stream_callback": None, "starts": {}},
     st(_turn=turn(), _usage_stale=True, tool_progress_callback="tool_progress_callback"),
     io=dict(TS_IO, **{"connection.call": {"usage": {"api_calls": 3}}, "state.apply_usage": None}))
case("_on_tool_start", {"payload": {"name": "search", "tool_id": "t1"}, "stream_callback": None, "starts": {}},
     st(_turn=turn(), tool_progress_callback="tool_progress_callback"), io=dict(TS_IO, **{"build_tool_preview": rs(raise_=RE)}))
case("_on_tool_start", {"payload": {"name": "search", "tool_id": "t1"}, "stream_callback": None, "starts": {}},
     st(_turn=turn(), tool_progress_callback="tool_progress_callback"), io={"time.monotonic": rs(raise_=SE)})
case("_on_tool_start", {"payload": {"name": "search", "tool_id": "t1"}, "stream_callback": None, "starts": {}},
     st(_turn=None, tool_progress_callback="tool_progress_callback"), io=dict(TS_IO))

# ── _on_tool_complete ──
TC = {"name": "search", "args": {"q": "x"}, "tool_id": "t1", "result": "ok", "duration_s": 3, "is_error": False}
case("_on_tool_complete", {"payload": TC, "stream_callback": None, "starts": {}},
     st(tool_complete_callback="tool_complete_callback"),
     io={"tool_progress_callback": None, "mark_dirty": None, "tool_complete_callback": None})
case("_on_tool_complete", {"payload": {"name": "x", "tool_id": "t1", "result": "r"}, "stream_callback": None, "starts": {"t1": 100}},
     st(tool_progress_callback="tool_progress_callback", tool_complete_callback="tool_complete_callback"),
     io={"time.monotonic": 105, "tool_progress_callback": None, "mark_dirty": None, "tool_complete_callback": None})
case("_on_tool_complete", {"payload": {"name": "x", "tool_id": "t9", "result": "r"}, "stream_callback": None, "starts": {}},
     st(tool_complete_callback="tool_complete_callback"),
     io={"tool_progress_callback": None, "mark_dirty": None, "tool_complete_callback": None})
case("_on_tool_complete", {"payload": {"name": "todo", "tool_id": "t1", "result": "{\"todos\": [], \"revision\": 2}"}, "stream_callback": None, "starts": {}},
     st(tool_complete_callback="tool_complete_callback"),
     io={"tool_progress_callback": None, "mark_dirty": None, "todo_store.restore": None, "tool_complete_callback": None})
case("_on_tool_complete", {"payload": {"name": "delegate_task", "tool_id": "t1", "result": "r", "inline_diff": "+a\n-b\n"}, "stream_callback": None, "starts": {}},
     st(tool_complete_callback="tool_complete_callback"),
     io={"tool_progress_callback": None, "mark_dirty": None, "diff_printer": "diff_emit", "diff_emit": None, "tool_complete_callback": None})
case("_on_tool_complete", {"payload": {"name": "x", "tool_id": "t1", "result": "r", "inline_diff": 5}, "stream_callback": None, "starts": {}},
     st(tool_complete_callback="tool_complete_callback"),
     io={"tool_progress_callback": None, "mark_dirty": None, "tool_complete_callback": None})
case("_on_tool_complete", {"payload": TC, "stream_callback": None, "starts": {}},
     st(), io={"mark_dirty": rs(raise_=RE)})
case("_on_tool_complete", {"payload": TC, "stream_callback": None, "starts": {}},
     st(), io={"mark_dirty": rs(raise_=SE)})

# ── _todo_from_result ──
case("_todo_from_result", {"result": "{\"todos\": [{\"id\": 1}], \"revision\": 3}"}, st(), io={"todo_store.restore": None})
case("_todo_from_result", {"result": "not json"}, st(), io={})
case("_todo_from_result", {"result": {"todos": [], "revision": 1}}, st(), io={"todo_store.restore": None})
case("_todo_from_result", {"result": [1, 2]}, st(), io={})
case("_todo_from_result", {"result": None}, st(), io={})
case("_todo_from_result", {"result": "[1, 2]"}, st(), io={})
case("_todo_from_result", {"result": {"todos": []}}, st(), io={"todo_store.restore": rs(raise_=VE)})
case("_todo_from_result", {"result": {"todos": []}}, st(), io={"todo_store.restore": rs(raise_=SE)})

# ── _set_todos ──
case("_set_todos", {"snapshot": {"todos": [{"id": 1}], "revision": 2}}, st(), io={"todo_store.restore": None})
case("_set_todos", {"snapshot": {"todos": "notlist"}}, st(), io={})
case("_set_todos", {"snapshot": {"todos": []}}, st(), io={"todo_store.restore": None})
case("_set_todos", {"snapshot": {"todos": []}}, st(), io={"todo_store.restore": rs(raise_=VE)})
case("_set_todos", {"snapshot": {"todos": []}}, st(), io={"todo_store.restore": rs(raise_=rais("TypeError"))})
case("_set_todos", {"snapshot": {"todos": []}}, st(), io={"todo_store.restore": rs(raise_=SE)})
case("_set_todos", {"snapshot": {"todos": []}}, st(), io={})

# ── _on_todo ──
case("_on_todo", {"payload": {"todos": [{"id": 1}], "revision": 2}, "stream_callback": None, "starts": {}}, st(), io={"todo_store.restore": None})
case("_on_todo", {"payload": {"todos": "x"}, "stream_callback": None, "starts": {}}, st(), io={})
case("_on_todo", {"payload": {"todos": []}, "stream_callback": None, "starts": {}}, st(), io={"todo_store.restore": rs(raise_=VE)})
case("_on_todo", {"payload": {"todos": []}, "stream_callback": None, "starts": {}}, st(), io={"todo_store.restore": rs(raise_=SE)})
case("_on_todo", {"payload": {"todos": []}, "stream_callback": None, "starts": {}}, st(), io={})

# ── simple callback forwarders ──
def fwd(func, cbkey, payload, extra_io=None):
    s, io = cb(cbkey)
    if extra_io:
        io.update(extra_io)
    case(func, {"payload": payload, "stream_callback": None, "starts": {}}, st(**s), io=io)
    case(func, {"payload": payload, "stream_callback": None, "starts": {}}, st(), io={})
    case(func, {"payload": payload, "stream_callback": None, "starts": {}}, st(**s),
         io={cbkey: rs(raise_=RE)})
    case(func, {"payload": payload, "stream_callback": None, "starts": {}}, st(**s),
         io={cbkey: rs(raise_=SE)})
    case(func, {"payload": payload, "stream_callback": None, "starts": {}}, st(**s), io={})


fwd("_on_control_update", "control_update_callback", {})
fwd("_on_thinking", "thinking_callback", {"text": "think"})
fwd("_on_thinking", "thinking_callback", {"text": None})
fwd("_on_reasoning", "_reasoning_callback", {"text": "reason"})
fwd("_on_reasoning_available", "tool_progress_callback", {"text": "x" * 600})
fwd("_on_notice_clear", "notice_clear_callback", {"key": "k1"})
fwd("_on_reaction", "reaction_callback", {"kind": "thumbsup"})
fwd("_on_connection_request", "connection_callback", {"op": "connect"})
fwd("_on_connection_update", "connection_update_callback", {"settled": False})

# ── _on_notice (AgentNotice) ──
NP = {"text": "low credits", "level": "warn", "kind": "ttl", "ttl_ms": 5000, "key": "k", "id": "i"}
case("_on_notice", {"payload": NP, "stream_callback": None, "starts": {}},
     st(notice_callback="notice_callback"), io={"notice_callback": None})
case("_on_notice", {"payload": {"text": "hi"}, "stream_callback": None, "starts": {}},
     st(notice_callback="notice_callback"), io={"notice_callback": None})
case("_on_notice", {"payload": {"text": None, "level": None, "kind": None}, "stream_callback": None, "starts": {}},
     st(notice_callback="notice_callback"), io={"notice_callback": None})
case("_on_notice", {"payload": NP, "stream_callback": None, "starts": {}}, st(), io={})
case("_on_notice", {"payload": NP, "stream_callback": None, "starts": {}},
     st(notice_callback="notice_callback"), io={"notice_callback": rs(raise_=SE)})
case("_on_notice", {"payload": NP, "stream_callback": None, "starts": {}},
     st(notice_callback="notice_callback"), io={})

# ── _restore_connection_card ──
case("_restore_connection_card", {"pending": {"op": "x"}}, st(connection_callback="connection_callback"),
     io={"connection_callback": None})
case("_restore_connection_card", {"pending": {"op": "x"}}, st(), io={})
case("_restore_connection_card", {"pending": None}, st(connection_update_callback="connection_update_callback"),
     io={"connection_update_callback": None})
case("_restore_connection_card", {"pending": {}}, st(connection_update_callback="connection_update_callback"),
     io={"connection_update_callback": None})
case("_restore_connection_card", {"pending": None}, st(), io={})
case("_restore_connection_card", {"pending": {"op": "x"}}, st(connection_callback="connection_callback"),
     io={"connection_callback": rs(raise_=SE)})

# ── connection_respond / connection_retry ──
case("connection_respond", {"op_id": "o1", "result": {"ok": True}}, st(), io={"connection.call": {"done": True}})
case("connection_respond", {"op_id": "o1", "result": None}, st(), io={"connection.call": None})
case("connection_respond", {"op_id": "o1", "result": "x"}, st(), io={"connection.call": rs(raise_=GCE)})
case("connection_respond", {"op_id": "o1", "result": "x"}, st(), io={"connection.call": rs(raise_=SE)})
case("connection_respond", {"op_id": "o1", "result": "x"}, st(), io={})
case("connection_retry", {"name": "gmail"}, st(), io={"connection.call": {"ok": True}})
case("connection_retry", {"name": ""}, st(), io={"connection.call": None})
case("connection_retry", {"name": "gmail"}, st(), io={"connection.call": rs(raise_=GCE)})
case("connection_retry", {"name": "gmail"}, st(), io={"connection.call": rs(raise_=SE)})
case("connection_retry", {"name": "gmail"}, st(), io={})

# ── _on_request_withdrawn ──
case("_on_request_withdrawn", {"payload": {"id": "r1"}}, st(_open_request=["r1", "sudo"], modal_withdraw_callback="modal_withdraw_callback"),
     io={"modal_withdraw_callback": None})
case("_on_request_withdrawn", {"payload": {"id": "r1"}}, st(_open_request=None, modal_withdraw_callback="modal_withdraw_callback"), io={})
case("_on_request_withdrawn", {"payload": {"id": "r2"}}, st(_open_request=["r1", "sudo"], modal_withdraw_callback="modal_withdraw_callback"), io={})
case("_on_request_withdrawn", {"payload": {"id": "r1"}}, st(_open_request=["r1", "sudo"]), io={})
case("_on_request_withdrawn", {"payload": {"id": "r1"}}, st(_open_request=["r1", "sudo"], modal_withdraw_callback="modal_withdraw_callback"),
     io={"modal_withdraw_callback": rs(raise_=RE)})
case("_on_request_withdrawn", {"payload": {"id": "r1"}}, st(_open_request=["r1", "sudo"], modal_withdraw_callback="modal_withdraw_callback"),
     io={"modal_withdraw_callback": rs(raise_=SE)})
case("_on_request_withdrawn", {"payload": {"id": "r1"}}, st(_open_request=["r1", "sudo"], modal_withdraw_callback="modal_withdraw_callback"), io={})

# ── _ask_clarify ──
case("_ask_clarify", {"params": {"questions": [{"qid": "q1"}]}}, st(clarify_callback="clarify_callback"),
     io={"clarify_reply": {"outcome": "submitted", "answers": {"q1": "a"}}})
case("_ask_clarify", {"params": {}}, st(), io={"clarify_reply": {"outcome": "cancelled"}})
case("_ask_clarify", {"params": {}}, st(clarify_callback="clarify_callback"), io={"clarify_reply": None})
case("_ask_clarify", {"params": {}}, st(), io={"clarify_reply": rs(raise_=RE)})
case("_ask_clarify", {"params": {}}, st(), io={"clarify_reply": rs(raise_=SE)})
case("_ask_clarify", {"params": {}}, st(), io={})

# ── _ask_sudo ──
case("_ask_sudo", {"params": {}}, st(), io={"_get_sudo_password_callback": None})
case("_ask_sudo", {"params": {}}, st(), io={"_get_sudo_password_callback": "sudo_password_callback", "sudo_password_callback": "pw"})
case("_ask_sudo", {"params": {}}, st(), io={"_get_sudo_password_callback": "sudo_password_callback", "sudo_password_callback": None})
case("_ask_sudo", {"params": {}}, st(), io={"_get_sudo_password_callback": "sudo_password_callback", "sudo_password_callback": ""})
case("_ask_sudo", {"params": {}}, st(), io={"_get_sudo_password_callback": rs(raise_=RE)})
case("_ask_sudo", {"params": {}}, st(), io={"_get_sudo_password_callback": rs(raise_=SE)})
case("_ask_sudo", {"params": {}}, st(), io={})

# ── _ask_vault_unlock ──
VU = {"backend": "bw", "display_name": "Bitwarden"}
case("_ask_vault_unlock", {"params": VU}, st(), io={"get_unlock_prompt_callback": None})
case("_ask_vault_unlock", {"params": VU}, st(), io={"get_unlock_prompt_callback": "vault_unlock_callback", "vault_unlock_callback": "pw"})
case("_ask_vault_unlock", {"params": VU}, st(), io={"get_unlock_prompt_callback": "vault_unlock_callback", "vault_unlock_callback": None})
case("_ask_vault_unlock", {"params": {}}, st(), io={"get_unlock_prompt_callback": "vault_unlock_callback", "vault_unlock_callback": ""})
case("_ask_vault_unlock", {"params": VU}, st(), io={"get_unlock_prompt_callback": rs(raise_=SE)})
case("_ask_vault_unlock", {"params": VU}, st(), io={})

# ── _ask_vault_save_login ──
VS = {"origin": "https://x", "site": "X"}
case("_ask_vault_save_login", {"params": VS}, st(), io={"get_save_login_prompt_callback": None})
case("_ask_vault_save_login", {"params": VS}, st(),
     io={"get_save_login_prompt_callback": "vault_save_login_callback",
         "vault_save_login_callback": {"identifier": "u@x", "password": "p\"w"}})
case("_ask_vault_save_login", {"params": VS}, st(),
     io={"get_save_login_prompt_callback": "vault_save_login_callback",
         "vault_save_login_callback": {"identifier": "u", "password": ""}})
case("_ask_vault_save_login", {"params": VS}, st(),
     io={"get_save_login_prompt_callback": "vault_save_login_callback",
         "vault_save_login_callback": "notdict"})
case("_ask_vault_save_login", {"params": VS}, st(),
     io={"get_save_login_prompt_callback": "vault_save_login_callback",
         "vault_save_login_callback": None})
case("_ask_vault_save_login", {"params": VS}, st(), io={"get_save_login_prompt_callback": rs(raise_=SE)})
case("_ask_vault_save_login", {"params": VS}, st(), io={})

# ── _ask_vault_code ──
VC = {"site": "X", "hint": "email"}
case("_ask_vault_code", {"params": VC}, st(), io={"get_code_prompt_callback": None})
case("_ask_vault_code", {"params": VC}, st(), io={"get_code_prompt_callback": "vault_code_callback", "vault_code_callback": "123456"})
case("_ask_vault_code", {"params": VC}, st(), io={"get_code_prompt_callback": "vault_code_callback", "vault_code_callback": None})
case("_ask_vault_code", {"params": {}}, st(), io={"get_code_prompt_callback": "vault_code_callback", "vault_code_callback": ""})
case("_ask_vault_code", {"params": VC}, st(), io={"get_code_prompt_callback": rs(raise_=SE)})
case("_ask_vault_code", {"params": VC}, st(), io={})

# ── _answer_request ──
case("_answer_request", {"frame": {"method": "sudo", "id": "r1", "params": {}}}, st(),
     io={"_get_sudo_password_callback": "sudo_password_callback", "sudo_password_callback": "pw", "connection.reply": None})
case("_answer_request", {"frame": {"method": "clarify", "id": "r2", "params": {"questions": []}}}, st(),
     io={"clarify_reply": {"outcome": "cancelled"}, "connection.reply": None})
case("_answer_request", {"frame": {"method": "vault.code", "id": "r3", "params": {"site": "X"}}}, st(),
     io={"get_code_prompt_callback": "vault_code_callback", "vault_code_callback": "999", "connection.reply": None})
case("_answer_request", {"frame": {"method": "secret", "id": "r4", "params": {"env_var": "K"}}}, st(),
     io={"self._ask_secret": "secretval", "connection.reply": None})
case("_answer_request", {"frame": {"method": "sudo", "id": "r5", "params": {}}}, st(),
     io={"_get_sudo_password_callback": rs(raise_=RE), "connection.reply": None})
case("_answer_request", {"frame": {"method": "clarify", "id": "r6", "params": {}}}, st(),
     io={"clarify_reply": rs(raise_=RE), "connection.reply": None})
case("_answer_request", {"frame": {"method": "sudo", "id": "r7", "params": {}}}, st(),
     io={"_get_sudo_password_callback": rs(raise_=SE)})
case("_answer_request", {"frame": {"id": "r8", "params": {}}}, st(), io={})
case("_answer_request", {"frame": {"method": "vault.unlock_prompt", "id": "r9", "params": {"backend": "bw"}}}, st(),
     io={"get_unlock_prompt_callback": None, "connection.reply": None})

# ── _voice_stopped ──
VS_IO = {"self._give_back_turn_route": None, "t": "voice stopped", "self._emit_print": None}
case("_voice_stopped", {"history": [{"role": "user", "content": "hi"}]}, st(), io=dict(VS_IO))
case("_voice_stopped", {"history": []}, st(), io=dict(VS_IO))
case("_voice_stopped", {"history": [{"role": "user", "content": "stop"}]}, st(_session_id="s2"), io=dict(VS_IO))
case("_voice_stopped", {"history": []}, st(), io=dict(VS_IO, **{"t": rs(raise_=RE)}))
case("_voice_stopped", {"history": []}, st(), io={"self._give_back_turn_route": None, "t": rs(raise_=SE)})
case("_voice_stopped", {"history": []}, st(), io={})

# ── _refresh_stats ──
case("_refresh_stats", {"turn": None}, st(), io={"connection.call": {"usage": {"api_calls": 3}}, "state.apply_usage": None})
case("_refresh_stats", {"turn": None}, st(), io={"connection.call": {"api_calls": 4}, "state.apply_usage": None})
case("_refresh_stats", {"turn": None}, st(), io={"connection.call": {"usage": {"api_calls": "x"}}})
case("_refresh_stats", {"turn": None}, st(), io={"connection.call": "notdict"})
case("_refresh_stats", {"turn": None}, st(), io={"connection.call": rs(raise_=GCE)})
case("_refresh_stats", {"turn": turn()}, st(),
     io={"connection.call": {"usage": {"api_calls": 2}}, "self._owns": True, "state.apply_usage": None})
case("_refresh_stats", {"turn": turn()}, st(),
     io={"connection.call": {"usage": {"api_calls": 2}}, "self._owns": False})
case("_refresh_stats", {"turn": None}, st(), io={"connection.call": rs(raise_=SE)})
case("_refresh_stats", {"turn": None}, st(), io={})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True, sort_keys=False) + "\n")
    # per-func count audit (>=5 required)
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    short = {k: v for k, v in counts.items() if v < 5}
    print(f"wrote {len(CASES)} cases across {len(counts)} funcs -> {OUT}")
    if short:
        print(f"WARNING funcs with <5 cases: {short}")
    missing = set(FUNCS_EXPECTED) - set(counts)
    if missing:
        print(f"WARNING funcs without cases: {sorted(missing)}")


FUNCS_EXPECTED = [
    "tool_progress_mode", "pin_reasoning", "stage_turn_reasoning", "restore_reasoning",
    "reasoning_callback", "_close_segment", "_on_delta", "_on_interim",
    "_on_tool_generating", "_on_tool_progress", "_on_tool_start", "_on_tool_complete",
    "_todo_from_result", "_set_todos", "_on_todo", "_on_control_update", "_on_thinking",
    "_on_reasoning", "_on_reasoning_available", "_on_notice", "_on_notice_clear",
    "_on_reaction", "_on_connection_request", "_on_connection_update",
    "_restore_connection_card", "connection_respond", "connection_retry",
    "_on_request_withdrawn", "_ask_clarify", "_ask_sudo", "_ask_vault_unlock",
    "_ask_vault_save_login", "_ask_vault_code", "_answer_request", "_voice_stopped",
    "_refresh_stats",
]

if __name__ == "__main__":
    main()