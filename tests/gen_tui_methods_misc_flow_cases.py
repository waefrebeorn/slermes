#!/usr/bin/env python3
"""Generate tests/fixtures/tui_methods_misc_flow.jsonl for the TUI slash-exec
misc method FLOW port (src/gateway/tui_methods_misc_flow.c).

The task named tui_gateway/methods_misc.py (nonexistent); the real defining
module is tui_gateway/methods_slash.py (fully unclaimed at port time). Each
case: {"func": <live def name>, "in": {"args": {...}, "io": {...},
["state": {...}]}, ["fail_closed": true]}.

io scripts mirror the C universal seam: a plain value is returned on every
call; a bare LIST is a per-call queue (exhaustion is fail-closed); a
{"__script__": true, "ret": v} wrapper returns v (use it for list-valued
results); {"__script__": true, "raise": {"type", "message"}} raises.

Every func gets >= 5 cases: happy paths, branch variants, error paths, a
fail-closed missing-script case, and (where a catch site exists) a
BaseException propagation case. ASCII corpus; POSIX paths; integral numbers.
history_lock present => the truthy marker "lock"; agent dicts materialize to
always-truthy SimpleNamespace doubles live. CompressionLockHeld raises carry
"holder" (str(exc) is derived from it by the live __init__).
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "tui_methods_misc_flow.jsonl"

CASES = []


def case(func, args=None, io=None, state=None, fail_closed=False):
    inp = {"args": args or {}, "io": io or {}}
    if state is not None:
        inp["state"] = state
    c = {"func": func, "in": inp}
    if fail_closed:
        c["fail_closed"] = True
    CASES.append(c)


def script(ret):
    return {"__script__": True, "ret": ret}


def raise_script(kind, message, **extra):
    return {"__script__": True, "raise": dict({"type": kind, "message": message}, **extra)}


# ── shared fixture records ──
HIST = [{"role": "user", "text": "hi"}, {"role": "assistant", "text": "hello"}]
AGENT = {
    "model": "m1", "provider": "nous", "base_url": "https://api.nous.example",
    "api_key": "k1", "_session_messages": [{"role": "user", "text": "old"}],
    "valid_tool_names": ["skill_manage", "terminal"],
    "_cached_system_prompt": "sys", "tools": [{"name": "t1"}],
    "context_compressor": {"kind": "c1"},
}
USAGE = {
    "input": 1200, "output": 340, "reasoning": 0, "prompt": 1500,
    "completion": 340, "total": 1840, "calls": 3, "context_max": 180000,
    "context_used": 37000, "context_percent": 20, "context_estimated": True,
    "compressions": 1, "model": "m1",
}
MIRROR = {"model": "m1", "provider": "nous"}
MSGS = [{"role": "user", "text": "hi"}, {"role": "assistant", "text": "hello"},
        {"role": "tool", "text": "out"}]


def sess(**over):
    s = {"session_key": "sk1", "history_lock": "lock", "history": HIST,
         "running": False, "profile_home": "/h/p1", "agent": None}
    s.update(over)
    return s


def fmt3_args(session, sid="s1", arg=""):
    return {"sid": sid, "session": session, "arg": arg}


def mirror_args(agent, session=None, sid="s1", arg=""):
    return {"sid": sid, "session": session if session is not None else sess(),
            "agent": agent, "arg": arg}


# ══ _format_live_review_output ══
case("_format_live_review_output", fmt3_args(None))
case("_format_live_review_output", fmt3_args(sess()),
     {"session_uses_compute_host": True})
case("_format_live_review_output", fmt3_args(sess()),
     {"session_uses_compute_host": False})
case("_format_live_review_output", fmt3_args(sess(running=True, agent=AGENT)),
     {"session_uses_compute_host": False})
REVIEW_IO = {
    "session_uses_compute_host": False,
    "set_session_context": "tok",
    "runtime_record_set": "rtok",
    "profile_runtime_scope": None,
    "start_review": {"note": "r1"},
    "runtime_record_reset": None,
    "clear_session_context": None,
    "format_dispatch_note": "dispatched r1",
}
case("_format_live_review_output", fmt3_args(sess(agent=AGENT)), dict(REVIEW_IO))
case("_format_live_review_output", fmt3_args(sess(agent=AGENT), arg="focus"),
     dict(REVIEW_IO, start_review=raise_script("ValueError", "bad review")))
case("_format_live_review_output", fmt3_args(sess(agent=AGENT)),
     dict(REVIEW_IO, start_review=raise_script("RuntimeError", "boom")))
case("_format_live_review_output", fmt3_args(sess(agent=AGENT)),
     dict(REVIEW_IO, start_review=raise_script("SystemExit", "bye")))
case("_format_live_review_output", fmt3_args(sess(agent=AGENT)),
     dict(REVIEW_IO, runtime_record_reset=raise_script("RuntimeError", "reset fail")))
case("_format_live_review_output", fmt3_args(sess(agent=AGENT, history=[])),
     dict(REVIEW_IO))
case("_format_live_review_output",
     fmt3_args({"history_lock": "lock", "history": HIST, "agent": AGENT}),
     {"session_uses_compute_host": False})
case("_format_live_review_output", fmt3_args(sess(agent=AGENT)), {}, fail_closed=True)

# ══ _format_live_refine_output ══
case("_format_live_refine_output", fmt3_args(None))
case("_format_live_refine_output", fmt3_args(sess(running=True)))
REFINE_HOST_IO = {
    "session_uses_compute_host": True,
    "send_compute_host_control": {"type": "ok", "output": "done"},
    "apply_compute_host_metadata_mirror": None,
}
case("_format_live_refine_output", fmt3_args(sess(), arg="x"), dict(REFINE_HOST_IO))
case("_format_live_refine_output", fmt3_args(sess()),
     dict(REFINE_HOST_IO, send_compute_host_control={"type": "control.error",
                                                     "message": "nope"}))
case("_format_live_refine_output", fmt3_args(sess()),
     dict(REFINE_HOST_IO, send_compute_host_control={"type": "error", "message": None}))
case("_format_live_refine_output", fmt3_args(sess()),
     dict(REFINE_HOST_IO, send_compute_host_control=raise_script("RuntimeError", "transport")))
case("_format_live_refine_output", fmt3_args(sess()),
     {"session_uses_compute_host": False})
case("_format_live_refine_output", fmt3_args(sess(agent=AGENT, session_key="", history=[])),
     {"session_uses_compute_host": False})
REFINE_LOCAL_IO = {
    "session_uses_compute_host": False,
    "session_db": "db",
    "persistence_for": {"messages": {}},
    "persistence.read_conversation": script([1, 2]),
    "message.as_openai_message": [{"role": "user", "text": "a"},
                                  {"role": "assistant", "text": "b"}],
    "agent.spawn_background_review": None,
}
case("_format_live_refine_output", fmt3_args(sess(agent=AGENT), arg="  memory  "),
     dict(REFINE_LOCAL_IO))
case("_format_live_refine_output", fmt3_args(sess(agent=AGENT)),
     dict(REFINE_LOCAL_IO, persistence_for=None))
case("_format_live_refine_output", fmt3_args(sess(agent=AGENT)),
     dict(REFINE_LOCAL_IO,
          **{"persistence.read_conversation": raise_script("ValueError", "db bad"),
             "logger.debug": None}))
case("_format_live_refine_output", fmt3_args(sess(agent=AGENT)),
     dict(REFINE_LOCAL_IO,
          **{"agent.spawn_background_review": raise_script("ValueError", "spawn bad")}))
case("_format_live_refine_output", fmt3_args(sess(agent=AGENT)),
     dict(REFINE_LOCAL_IO,
          **{"agent.spawn_background_review": raise_script("SystemExit", "bye")}))
case("_format_live_refine_output", fmt3_args(sess(agent=AGENT)), {}, fail_closed=True)

# ══ _usage_route ══
case("_usage_route", {"session": sess(agent=AGENT)})
case("_usage_route", {"session": sess(agent={})})
case("_usage_route", {"session": sess()},
     {"deferred_build_agent_kwargs": {"model_override": "m", "provider_override": "p"},
      "resolve_agent_model_runtime": script(["name", {"provider": "nous",
                                                       "base_url": "b", "api_key": "k"}])})
case("_usage_route", {"session": sess()},
     {"deferred_build_agent_kwargs": {},
      "resolve_agent_model_runtime": raise_script("RuntimeError", "signed out")})
case("_usage_route", {"session": sess()},
     {"deferred_build_agent_kwargs": {},
      "resolve_agent_model_runtime": raise_script("ValueError", "unconfigured")})
case("_usage_route", {"session": sess()},
     {"deferred_build_agent_kwargs": {},
      "resolve_agent_model_runtime": raise_script("AuthError", "auth gone")})
case("_usage_route", {"session": sess()},
     {"deferred_build_agent_kwargs": {},
      "resolve_agent_model_runtime": raise_script("TypeError", "bad")})
case("_usage_route", {"session": sess()},
     {"deferred_build_agent_kwargs": {},
      "resolve_agent_model_runtime": script(["only-one"])})
case("_usage_route", {"session": sess()}, {}, fail_closed=True)

# ══ _usage_on_nous_route ══
case("_usage_on_nous_route", {"route": {"provider": "nous", "base_url": "b"}},
     {"is_nous_inference_route": True})
case("_usage_on_nous_route", {"route": {}}, {"is_nous_inference_route": False})
case("_usage_on_nous_route", {"route": {"provider": None, "base_url": None}},
     {"is_nous_inference_route": False})
case("_usage_on_nous_route", {"route": {"provider": "x"}},
     {"is_nous_inference_route": raise_script("RuntimeError", "boom")})
case("_usage_on_nous_route", {"route": "x"})
case("_usage_on_nous_route", {"route": {}}, {}, fail_closed=True)

# ══ _usage_nous_fields ══
NOUS_IO = {
    "profile_runtime_scope": None,
    "usage_route": {"provider": "nous"},
    "usage_on_nous_route": True,
    "nous_credits_usage_fields": {"balance": "5"},
}
case("_usage_nous_fields", {"session": sess(agent=AGENT)}, dict(NOUS_IO))
case("_usage_nous_fields", {"session": sess()},
     dict(NOUS_IO, nous_credits_usage_fields={"nous_route": False, "x": 1}))
case("_usage_nous_fields", {"session": sess()},
     dict(NOUS_IO, usage_route=raise_script("RuntimeError", "route bad")))
case("_usage_nous_fields", {"session": sess()},
     dict(NOUS_IO, nous_credits_usage_fields="x"))
case("_usage_nous_fields", {"session": sess()},
     dict(NOUS_IO, nous_credits_usage_fields=raise_script("KeyboardInterrupt", "ki")))
case("_usage_nous_fields", {"session": sess()}, {}, fail_closed=True)

# ══ _usage_account_blocks ══
BLOCKS_IO = {
    "profile_runtime_scope": None,
    "usage_route": {"provider": "nous", "base_url": "b", "api_key": "k"},
    "account_limit_lines": script(["limit1"]),
    "usage_on_nous_route": True,
    "nous_usage_lines": script(["nous1"]),
}
case("_usage_account_blocks",
     {"session": sess(agent=dict(AGENT, _credits_state={"is_usage_window": True}))},
     dict(BLOCKS_IO))
case("_usage_account_blocks", {"session": sess(agent=AGENT)},
     dict(BLOCKS_IO, account_limit_lines=raise_script("RuntimeError", "limits bad")))
case("_usage_account_blocks", {"session": sess(agent=AGENT)},
     dict(BLOCKS_IO, nous_usage_lines=raise_script("ValueError", "nous bad")))
case("_usage_account_blocks",
     {"session": sess(agent=dict(AGENT, _credits_state={"is_usage_window": False}))},
     dict(BLOCKS_IO))
case("_usage_account_blocks", {"session": sess()},
     dict(BLOCKS_IO, account_limit_lines=raise_script("SystemExit", "bye")))
case("_usage_account_blocks", {"session": sess(agent=AGENT)},
     dict(BLOCKS_IO, account_limit_lines=5))
case("_usage_account_blocks", {"session": sess()}, {}, fail_closed=True)

# ══ _format_live_usage_output ══
case("_format_live_usage_output", fmt3_args(sess()),
     {"usage_account_blocks": script(["extra1"]), "format_live_usage_tokens": "TOKENS"})
case("_format_live_usage_output", fmt3_args(sess()),
     {"usage_account_blocks": script([]), "format_live_usage_tokens": ""})
case("_format_live_usage_output", fmt3_args(sess()),
     {"usage_account_blocks": script([]), "format_live_usage_tokens": "T"})
case("_format_live_usage_output", fmt3_args(sess()),
     {"usage_account_blocks": raise_script("RuntimeError", "blocks bad")})
case("_format_live_usage_output", fmt3_args(sess()),
     {"usage_account_blocks": 5, "format_live_usage_tokens": "T"})
case("_format_live_usage_output", fmt3_args(sess()),
     {"usage_account_blocks": script(["a", 1]), "format_live_usage_tokens": "T"})
case("_format_live_usage_output", fmt3_args(sess()), {}, fail_closed=True)

# ══ _format_live_usage_tokens ══
case("_format_live_usage_tokens", {"session": sess()}, {"session_usage_snapshot": {}})
case("_format_live_usage_tokens",
     {"session": sess(agent=AGENT, _metadata_message_count=5)},
     {"session_usage_snapshot": USAGE})
case("_format_live_usage_tokens", {"session": sess(agent=AGENT)},
     {"session_usage_snapshot": USAGE})
case("_format_live_usage_tokens", {"session": sess(agent=AGENT)},
     {"session_usage_snapshot": dict(USAGE, reasoning=0, context_max=0,
                                     compressions=0)})
case("_format_live_usage_tokens",
     {"session": sess(agent={"model": "am"}, _metadata_message_count=0)},
     {"session_usage_snapshot": dict(USAGE, model=None),
      "metadata_mirror": {"model": "m2"}})
case("_format_live_usage_tokens", {"session": sess(agent=AGENT)},
     {"session_usage_snapshot": dict(USAGE, model=None),
      "metadata_mirror": raise_script("RuntimeError", "mirror bad")})
case("_format_live_usage_tokens",
     {"session": {"history_lock": "lock", "history": HIST, "agent": AGENT}},
     {"session_usage_snapshot": USAGE})
case("_format_live_usage_tokens", {"session": sess(agent=AGENT)},
     {"session_usage_snapshot": dict(USAGE, reasoning=77, context_estimated=False)})
case("_format_live_usage_tokens", {"session": sess()}, {}, fail_closed=True)

# ══ _live_session_messages ══
LSM_IO = {
    "session_db": "db",
    "persistence_for": {"messages": {}},
    "persistence.read_conversation": script([1]),
    "message.as_openai_message": [{"role": "user", "text": "a"}],
}
case("_live_session_messages", {"session": sess()}, dict(LSM_IO))
case("_live_session_messages", {"session": sess()}, dict(LSM_IO, persistence_for=None))
case("_live_session_messages", {"session": sess(session_key="")}, dict(LSM_IO))
case("_live_session_messages", {"session": sess()},
     dict(LSM_IO, **{"persistence.read_conversation": raise_script("ValueError", "bad")}))
case("_live_session_messages", {"session": sess()},
     dict(LSM_IO, **{"message.as_openai_message": raise_script("RuntimeError", "m bad")}))
case("_live_session_messages", {"session": sess()},
     dict(LSM_IO, session_db=raise_script("RuntimeError", "db bad")))
case("_live_session_messages", {"session": sess()},
     dict(LSM_IO, **{"persistence.read_conversation": raise_script("SystemExit", "bye")}))
case("_live_session_messages", {"session": sess()}, {}, fail_closed=True)

# ══ _format_live_history_output ══
HIST_IO = {
    "live_session_messages": script([{"role": "user", "text": "dbmsg"}]),
    "history_to_messages": script(MSGS),
}
case("_format_live_history_output", fmt3_args(sess()), dict(HIST_IO))
case("_format_live_history_output", fmt3_args(sess()),
     dict(HIST_IO, live_session_messages=None))
case("_format_live_history_output", fmt3_args(sess()),
     dict(HIST_IO, history_to_messages=script([])))
case("_format_live_history_output", fmt3_args(sess()),
     dict(HIST_IO, history_to_messages=script([
         {"role": "user", "text": "x" * 450},
         {"role": "tool", "context": "ctx text"},
         {"role": "system", "text": ""}])))
case("_format_live_history_output", fmt3_args(sess()),
     dict(HIST_IO, history_to_messages=raise_script("RuntimeError", "h2m bad")))
case("_format_live_history_output",
     fmt3_args({"session_key": "sk1", "history": HIST, "agent": None}),
     {"live_session_messages": None})
case("_format_live_history_output", fmt3_args(sess()), {}, fail_closed=True)

# ══ _format_live_context_output ══
CTX_BASE = {
    "live_session_messages": None,
    "history_to_messages": script(MSGS),
    "session_usage_snapshot": USAGE,
    "metadata_mirror": MIRROR,
}
case("_format_live_context_output", fmt3_args(sess()), dict(CTX_BASE))
CTX_AGENT_IO = dict(
    CTX_BASE,
    session_cwd="/w",
    set_session_context="tok",
    compute_session_context_breakdown={"p": 1},
    render_context_breakdown_lines=script(["bd1"]),
    clear_session_context=None,
)
case("_format_live_context_output", fmt3_args(sess(agent=AGENT)), dict(CTX_AGENT_IO))
case("_format_live_context_output", fmt3_args(sess(agent=AGENT), arg=" ALL "),
     dict(CTX_AGENT_IO, compute_context_details={"d": 1}))
case("_format_live_context_output", fmt3_args(sess(agent=AGENT), arg="details"),
     dict(CTX_AGENT_IO, compute_context_details=raise_script("RuntimeError", "det bad")))
case("_format_live_context_output", fmt3_args(sess(agent=AGENT)),
     dict(CTX_AGENT_IO,
          compute_session_context_breakdown=raise_script("RuntimeError", "bd bad"),
          context_file_sources_for_agent={"s": 1},
          render_context_file_lines=script(["f1"])))
case("_format_live_context_output", fmt3_args(sess(agent=AGENT)),
     dict(CTX_AGENT_IO, set_session_context=raise_script("RuntimeError", "ctx bad")))
case("_format_live_context_output", fmt3_args(sess(agent=AGENT)),
     dict(CTX_AGENT_IO,
          compute_session_context_breakdown=raise_script("SystemExit", "bye")))
case("_format_live_context_output", fmt3_args(sess()),
     dict(CTX_BASE, live_session_messages=script([{"role": "user", "text": "db"}])))
CTX_FILE_IO = dict(
    CTX_BASE,
    session_cwd="/w",
    set_session_context="tok",
    compute_session_context_breakdown={"p": 1},
    render_context_breakdown_lines=script([]),
    clear_session_context=None,
    context_file_sources_for_agent={"s": 1},
    render_context_file_lines=script(["f1"]),
)
case("_format_live_context_output", fmt3_args(sess(agent=AGENT)), dict(CTX_FILE_IO))
case("_format_live_context_output", fmt3_args(sess(agent=AGENT)),
     dict(CTX_FILE_IO, render_context_file_lines=raise_script("RuntimeError", "file bad")))
case("_format_live_context_output", fmt3_args(sess()),
     dict(CTX_BASE, session_usage_snapshot=dict(USAGE, context_max=0, compressions=0)))
case("_format_live_context_output", fmt3_args(sess()), {}, fail_closed=True)

# ══ _format_live_tools_output ══
case("_format_live_tools_output", fmt3_args(sess()),
     {"session_info": {"tools": {"core": ["terminal", "read"],
                                 "extra": ["write", "terminal"]}}})
case("_format_live_tools_output", fmt3_args(sess()), {"session_info": "x"})
case("_format_live_tools_output", fmt3_args(sess()), {"session_info": {"tools": {}}})
case("_format_live_tools_output", fmt3_args(sess()),
     {"session_info": {"tools": {"core": "not-a-list", "n": 5}}})
case("_format_live_tools_output", fmt3_args(sess()),
     {"session_info": {"tools": {"core": [1, "a", 1]}}})
case("_format_live_tools_output", fmt3_args(sess()),
     {"session_info": raise_script("RuntimeError", "info bad")})
case("_format_live_tools_output", fmt3_args(sess()), {}, fail_closed=True)

# ══ _format_live_help_output ══
case("_format_live_help_output", fmt3_args(sess()),
     {"commands_by_category": {"General": {"help": "show help", "model": "pick model"},
                               "Session": {"clear": "clear screen"}}})
case("_format_live_help_output", fmt3_args(sess()),
     {"commands_by_category": raise_script("RuntimeError", "boom")})
case("_format_live_help_output", fmt3_args(sess()),
     {"commands_by_category": {"General": ["not", "a", "dict"]}})
case("_format_live_help_output", fmt3_args(sess()),
     {"commands_by_category": raise_script("SystemExit", "bye")})
case("_format_live_help_output", fmt3_args(sess()), {"commands_by_category": {}})
case("_format_live_help_output", fmt3_args(sess()),
     {"commands_by_category": {"G": {"averylongcommandname": "desc"}}})
case("_format_live_help_output", fmt3_args(sess()), {}, fail_closed=True)

# ══ _format_live_model_output ══
case("_format_live_model_output", {"session": sess(agent=AGENT)})
case("_format_live_model_output", {"session": sess(agent={"model": "m1"})})
case("_format_live_model_output", {"session": sess(agent={"provider": "p"})})
case("_format_live_model_output", {"session": sess()})
case("_format_live_model_output", {"session": {}})
case("_format_live_model_output", {"session": None})

# ══ _format_live_status_output ══
case("_format_live_status_output", fmt3_args(sess()),
     {"methods.session_status": {"result": {"output": "status text"}}})
case("_format_live_status_output", fmt3_args(sess()),
     {"methods.session_status": {"error": {"message": "bad"}}})
case("_format_live_status_output", fmt3_args(sess()),
     {"methods.session_status": {"error": {"message": None}}})
case("_format_live_status_output", fmt3_args(sess()), {"methods.session_status": {}})
case("_format_live_status_output", fmt3_args(sess()),
     {"methods.session_status": {"result": None}})
case("_format_live_status_output", fmt3_args(sess()),
     {"methods.session_status": raise_script("RuntimeError", "rpc bad")})
case("_format_live_status_output", fmt3_args(sess()), {}, fail_closed=True)

# ══ _live_slash_command_output ══
def slash_args(name, session, arg=""):
    return {"sid": "s1", "session": session, "name": name, "arg": arg}


case("_live_slash_command_output", slash_args("/MODEL", sess(agent=AGENT)),
     {"format_live_model_output": "Current model: m1"})
case("_live_slash_command_output", slash_args("model", sess(agent=AGENT), "m2"))
case("_live_slash_command_output", slash_args("context", sess()),
     {"session_uses_compute_host": False, "format_live_context_output": "CTX"})
case("_live_slash_command_output", slash_args("context", sess()),
     {"session_uses_compute_host": [True, True], "format_live_context_output": "CTX"})
case("_live_slash_command_output", slash_args("tools", None))
case("_live_slash_command_output", slash_args("tools", sess()),
     {"session_uses_compute_host": False})
case("_live_slash_command_output", slash_args("help", sess()),
     {"session_uses_compute_host": True, "format_live_help_output": "HELP"})
case("_live_slash_command_output", slash_args("usage", None))
case("_live_slash_command_output", slash_args("compress", sess(), "here"),
     {"mirror_slash_side_effects": "compressed"})
case("_live_slash_command_output", slash_args("clear", None))
case("_live_slash_command_output", slash_args("models", sess()))
case("_live_slash_command_output", slash_args("rename", None))
case("_live_slash_command_output", slash_args("effort", sess()))
case("_live_slash_command_output", slash_args("unknown", sess()))
case("_live_slash_command_output", slash_args("review", None),
     {"format_live_review_output": "REVIEW"})
case("_live_slash_command_output", slash_args("history", None))
case("_live_slash_command_output", slash_args("status", sess()),
     {"format_live_status_output": "STATUS"})
case("_live_slash_command_output", slash_args("refine", sess()),
     {"format_live_refine_output": "REFINE"})
case("_live_slash_command_output", slash_args("usage", sess()), {}, fail_closed=True)
case("_live_slash_command_output", slash_args("usage", sess()),
     {"format_live_usage_output": raise_script("SystemExit", "bye")})

# ══ _compress_live_with_feedback ══
def compress_args(agent, arg="", snapshot_kwargs=True, session=None):
    return {"sid": "s1", "session": session if session is not None else sess(),
            "agent": agent, "arg": arg, "snapshot_kwargs": snapshot_kwargs}


COMPRESS_FULL_IO = {
    "parse_compress_args": {"aggressive": False, "preview": False},
    "profile_runtime_scope": None,
    "manual_compress_turn": None,
    "estimate_request_tokens_rough": [100, 50],
    "compress_session_history": None,
    "sync_session_key_after_compress": None,
    "session_info": {"i": 1},
    "emit": None,
    "summarize_manual_compression": {"headline": "H", "token_line": "T", "note": "N"},
    "finalize_context_engine_notification": None,
}
case("_compress_live_with_feedback", compress_args(AGENT),
     {"parse_compress_args": {"aggressive": True, "preview": False}})
case("_compress_live_with_feedback", compress_args(AGENT, "here 3"),
     {"parse_compress_args": {"aggressive": False, "preview": True},
      "profile_runtime_scope": None,
      "compress_now": {"r": 1},
      "render_compress_result": script(["line1", "line2"])})
case("_compress_live_with_feedback", compress_args(AGENT, "here"),
     dict(COMPRESS_FULL_IO))
case("_compress_live_with_feedback", compress_args(AGENT, "  here  ", False),
     dict(COMPRESS_FULL_IO))
case("_compress_live_with_feedback", compress_args(AGENT),
     dict(COMPRESS_FULL_IO,
          compress_session_history=raise_script("CompressionLockHeld",
                                                "Compression lock held: w7",
                                                holder="w7"),
          describe_compression_lock_skip="skipped w7"))
case("_compress_live_with_feedback", compress_args(AGENT),
     dict(COMPRESS_FULL_IO,
          compress_session_history=raise_script("CompressionLockHeld",
                                                "Compression lock held: unknown",
                                                holder=None),
          describe_compression_lock_skip="skipped unknown"))
case("_compress_live_with_feedback", compress_args(AGENT),
     dict(COMPRESS_FULL_IO,
          compress_session_history=raise_script("RuntimeError", "compress bad")))
case("_compress_live_with_feedback", compress_args(AGENT),
     dict(COMPRESS_FULL_IO,
          summarize_manual_compression={"token_line": "T"}))
case("_compress_live_with_feedback", compress_args(AGENT),
     dict(COMPRESS_FULL_IO,
          compress_session_history=raise_script("SystemExit", "bye")))
case("_compress_live_with_feedback", compress_args(AGENT, session=sess(history=[])),
     dict(COMPRESS_FULL_IO, estimate_request_tokens_rough=[50]))
case("_compress_live_with_feedback", compress_args(AGENT),
     {"parse_compress_args": {}})
case("_compress_live_with_feedback", compress_args(AGENT),
     dict(COMPRESS_FULL_IO,
          summarize_manual_compression={"headline": "H", "token_line": "", "note": None}))
case("_compress_live_with_feedback", compress_args(AGENT), {}, fail_closed=True)

# ══ _mirror_approvals ══
case("_mirror_approvals", mirror_args(None, arg="x"), {"broadcast_session_info": None})
case("_mirror_approvals", mirror_args(None, arg=""))
case("_mirror_approvals", mirror_args(None, arg="x"),
     {"broadcast_session_info": raise_script("RuntimeError", "bc bad")})
case("_mirror_approvals", mirror_args(None, arg="x"),
     {"broadcast_session_info": raise_script("SystemExit", "bye")})
case("_mirror_approvals", mirror_args(None, arg="x"), {}, fail_closed=True)

# ══ _mirror_personality ══
PERS_IO = {
    "load_cfg": {"c": 1},
    "validate_personality": script(["calm", "prompt text"]),
    "persist_personality": None,
    "apply_personality_to_session": None,
}
case("_mirror_personality", mirror_args({}, arg="calm"), dict(PERS_IO))
case("_mirror_personality", mirror_args({}, arg=""))
case("_mirror_personality", mirror_args(None, arg="calm"))
case("_mirror_personality", mirror_args({}, arg="calm"),
     dict(PERS_IO, validate_personality=script([1])))
case("_mirror_personality", mirror_args({}, arg="calm"),
     dict(PERS_IO, validate_personality=5))
case("_mirror_personality", mirror_args({}, arg="calm"),
     dict(PERS_IO, validate_personality=script(["a", "b", "c"])))
case("_mirror_personality", mirror_args({}, arg="calm"),
     dict(PERS_IO, persist_personality=raise_script("RuntimeError", "persist bad")))
case("_mirror_personality", mirror_args({}, arg="calm"), {}, fail_closed=True)

# ══ _mirror_fast ══
FAST_IO = {"session_info": {}, "emit": None}
case("_mirror_fast", mirror_args({}, arg="fast"), dict(FAST_IO))
case("_mirror_fast", mirror_args({}, arg="normal"), dict(FAST_IO))
case("_mirror_fast", mirror_args({}, arg="bogus"), dict(FAST_IO))
case("_mirror_fast", mirror_args({}, arg="ULTRA"), dict(FAST_IO))
case("_mirror_fast", mirror_args(None, arg="fast"))
case("_mirror_fast", mirror_args({}, arg="fast"),
     dict(FAST_IO, session_info=raise_script("RuntimeError", "info bad")))
case("_mirror_fast", mirror_args({}, arg="fast"), {}, fail_closed=True)

# ══ _mirror_reload_mcp ══
case("_mirror_reload_mcp", mirror_args({"reload_mcp_tools": True}),
     {"agent.reload_mcp_tools": None})
case("_mirror_reload_mcp", mirror_args({}))
case("_mirror_reload_mcp", mirror_args(None))
case("_mirror_reload_mcp", mirror_args({"reload_mcp_tools": True}),
     {"agent.reload_mcp_tools": raise_script("RuntimeError", "mcp bad")})
case("_mirror_reload_mcp", mirror_args({"reload_mcp_tools": True}),
     {"agent.reload_mcp_tools": raise_script("SystemExit", "bye")})
case("_mirror_reload_mcp", mirror_args({"reload_mcp_tools": True}), {}, fail_closed=True)

# ══ _mirror_stop ══
case("_mirror_stop", mirror_args(None), {"process_registry.kill_all": None})
case("_mirror_stop", mirror_args(None), {"process_registry.kill_all": {"killed": 2}})
case("_mirror_stop", mirror_args(None),
     {"process_registry.kill_all": raise_script("RuntimeError", "kill bad")})
case("_mirror_stop", mirror_args(None),
     {"process_registry.kill_all": raise_script("SystemExit", "bye")})
case("_mirror_stop", mirror_args(None), {}, fail_closed=True)

# ══ _mirror_autoprune ══
case("_mirror_autoprune", mirror_args(None, arg="later"),
     {"prune_advisor.answered": True, "emit": None})
case("_mirror_autoprune", mirror_args(None, arg="later"),
     {"prune_advisor.answered": False})
case("_mirror_autoprune", mirror_args(None, arg=""))
case("_mirror_autoprune", mirror_args(None, session=sess(profile_home=None), arg="x"),
     {"prune_advisor.answered": True, "emit": None})
case("_mirror_autoprune", mirror_args(None, arg="x"),
     {"prune_advisor.answered": raise_script("RuntimeError", "prune bad")})
case("_mirror_autoprune", mirror_args(None, arg="x"),
     {"prune_advisor.answered": True, "emit": raise_script("SystemExit", "bye")})
case("_mirror_autoprune", mirror_args(None, arg="x"), {}, fail_closed=True)

# ══ _compute_host_slash ══
def ch_args(name, command=None):
    return {"sid": "s1", "session": sess(), "name": name,
            "command": command if command is not None else "/" + name}


case("_compute_host_slash", ch_args("personality", "/personality calm"),
     {"send_compute_host_control": {"type": "ok", "output": "done"},
      "apply_compute_host_metadata_mirror": None})
case("_compute_host_slash", ch_args("model", "/model m2"),
     {"send_claimed_compute_host_control": {"type": "ok", "output": "switched"},
      "apply_compute_host_metadata_mirror": None})
case("_compute_host_slash", ch_args("compress", "/compress here"),
     {"send_claimed_compute_host_control": raise_script("queue.Empty", "")})
case("_compute_host_slash", ch_args("model"),
     {"send_claimed_compute_host_control": raise_script("queue.Empty", "")})
case("_compute_host_slash", ch_args("model"),
     {"send_claimed_compute_host_control": raise_script("SessionNotOwned", "not mine")})
case("_compute_host_slash", ch_args("model"),
     {"send_claimed_compute_host_control": raise_script("RuntimeError", "boom")})
case("_compute_host_slash", ch_args("model"),
     {"send_claimed_compute_host_control": {"type": "control.error", "message": "nope"}})
case("_compute_host_slash", ch_args("compress"),
     {"send_claimed_compute_host_control": {"type": "error", "message": None}})
case("_compute_host_slash", ch_args("model"),
     {"send_claimed_compute_host_control": raise_script("SystemExit", "bye")})
case("_compute_host_slash", ch_args("model"),
     {"send_claimed_compute_host_control": {"type": "ok", "output": None},
      "apply_compute_host_metadata_mirror": None})
case("_compute_host_slash", ch_args("model"), {}, fail_closed=True)

# ══ _mirror_slash_side_effects ══
def ms_args(command, session=None, agent=None):
    if session is None:
        session = sess(agent=agent)
    return {"sid": "s1", "session": session, "command": command}


case("_mirror_slash_side_effects", ms_args("/model m2", agent=AGENT),
     {"session_uses_compute_host": False, "profile_runtime_scope": None,
      "apply_model_switch": {"warning": "w1"}})
case("_mirror_slash_side_effects", ms_args("/model m2", session=sess(running=True)),
     {"session_uses_compute_host": False, "busy_message": "busy!"})
case("_mirror_slash_side_effects", ms_args("/model m2"),
     {"session_uses_compute_host": True,
      "compute_host_slash": script(["ok", "host text"])})
case("_mirror_slash_side_effects", ms_args("/compress here", agent=AGENT),
     {"session_uses_compute_host": False, "profile_runtime_scope": None,
      "compress_live_with_feedback": "compressed"})
case("_mirror_slash_side_effects", ms_args("/compress"),
     {"session_uses_compute_host": False, "profile_runtime_scope": None})
case("_mirror_slash_side_effects", ms_args("/fast on", agent={}),
     {"profile_runtime_scope": None, "mirror_fast": None})
case("_mirror_slash_side_effects", ms_args("/unknown"))
case("_mirror_slash_side_effects", ms_args(""))
case("_mirror_slash_side_effects", ms_args("   "))
case("_mirror_slash_side_effects", ms_args("/compact x", agent=AGENT),
     {"session_uses_compute_host": False, "profile_runtime_scope": None,
      "compress_live_with_feedback": "compacted"})
case("_mirror_slash_side_effects", ms_args("/stop"),
     {"profile_runtime_scope": None,
      "mirror_stop": raise_script("CompressionBusy", "turn holds it")})
case("_mirror_slash_side_effects", ms_args("/stop"),
     {"profile_runtime_scope": None,
      "mirror_stop": raise_script("SessionNotOwned", "not mine")})
case("_mirror_slash_side_effects", ms_args("/compress here", agent=AGENT),
     {"session_uses_compute_host": False, "profile_runtime_scope": None,
      "compress_live_with_feedback": raise_script("RuntimeError", "boom"),
      "finalize_context_engine_notification": None})
case("_mirror_slash_side_effects", ms_args("/stop"),
     {"profile_runtime_scope": None,
      "mirror_stop": raise_script("SystemExit", "bye")})
case("_mirror_slash_side_effects", ms_args("/approvals mode"),
     {"profile_runtime_scope": None, "mirror_approvals": None})
case("_mirror_slash_side_effects", ms_args("/model m2", agent=AGENT),
     {"session_uses_compute_host": False, "profile_runtime_scope": None,
      "apply_model_switch": {"warning": None}})
case("_mirror_slash_side_effects", ms_args("/model", agent=AGENT),
     {"session_uses_compute_host": False, "profile_runtime_scope": None})
case("_mirror_slash_side_effects", ms_args("/personality calm", agent={}),
     {"session_uses_compute_host": False, "profile_runtime_scope": None,
      "mirror_personality": None})
case("_mirror_slash_side_effects", ms_args("/autoprune later"),
     {"profile_runtime_scope": None, "mirror_autoprune": ""})
case("_mirror_slash_side_effects", ms_args("/reload-mcp", agent={}),
     {"profile_runtime_scope": None, "mirror_reload_mcp": None})
case("_mirror_slash_side_effects", ms_args("/stop"), {}, fail_closed=True)


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    thin = [f for f, n in counts.items() if n < 5]
    print(f"wrote {len(CASES)} cases to {OUT}")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    if thin:
        raise SystemExit(f"THIN COVERAGE: {thin}")


if __name__ == "__main__":
    main()