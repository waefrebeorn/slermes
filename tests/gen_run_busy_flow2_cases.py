#!/usr/bin/env python3
"""Generate tests/fixtures/run_busy_flow2.jsonl for the run_busy FLOW2 oracle.

Each case is {"func": <run_busy.py def>, "in": {<args>, "io": {<seam scripts>}}}.
Seam scripts: a bare array is a per-call queue; {"__script__":true,"ret":v}
is a constant container return; {"__raise__":kind,"message":m} is a raise.
Modeling contract: src/gateway/run_busy_flow2.c.

Run: python tests/gen_run_busy_flow2_cases.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "run_busy_flow2.jsonl")

CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def raise_(kind, msg=""):
    return {"__raise__": kind, "message": msg}


def script(ret):
    return {"__script__": True, "ret": ret}


# ── approval_input_words ────────────────────────────────────────────────────
# key = "approval.inputs.<input_key>"; t(key, lang="en") then t(key).
add("approval_input_words", input_key="approve",
    io={"t": ["Yes, YES, tak , yes", "yEs,ja"]})
add("approval_input_words", input_key="deny",
    io={"t": ["no, Nie ,no", "NIE, nope"]})
add("approval_input_words", input_key="approve",
    io={"t": ["approval.inputs.approve", "yes"]})          # en catalog miss
add("approval_input_words", input_key="approve",
    io={"t": ["yes", "approval.inputs.approve"]})          # active catalog miss
add("approval_input_words", input_key="always",
    io={"t": ["always, ALWAYS", ""]})
add("approval_input_words", input_key="session",
    io={"t": [" , , session", "session"]})
add("approval_input_words", input_key="approve",
    io={"t": ["Tak, TAK, yes", "tak"]})

# ── _tail_has_slot ──────────────────────────────────────────────────────────
add("_tail_has_slot", tail="c1:t1", slot="c1")
add("_tail_has_slot", tail="c1", slot="c1")
add("_tail_has_slot", tail="", slot="c1")
add("_tail_has_slot", tail="c1x:t1", slot="c1")
add("_tail_has_slot", tail="c1extra", slot="c1")
add("_tail_has_slot", tail=":t1", slot="")
add("_tail_has_slot", tail="", slot="")

# ── _destructive_optout_note ────────────────────────────────────────────────
add("_destructive_optout_note", persisted=True, io={"t": "gateway.confirm.destructive_optout"})
add("_destructive_optout_note", persisted=False,
    io={"t": "gateway.confirm.destructive_optout_save_failed"})
add("_destructive_optout_note", persisted=1, io={"t": "note-true"})
add("_destructive_optout_note", persisted=0, io={"t": "note-false"})
add("_destructive_optout_note", persisted=None, io={"t": "note-none"})
add("_destructive_optout_note", persisted=True, io={"t": raise_("ValueError", "i18n down")})

# ── _busy_reply_to ──────────────────────────────────────────────────────────
def ev(platform="telegram", thread_id=None, chat_type="group", internal=False, mid="m1"):
    return {"source": {"platform": platform, "thread_id": thread_id,
                       "chat_type": chat_type},
            "internal": internal, "message_id": mid}

add("_busy_reply_to", event=ev(thread_id="t1", chat_type="dm"), reply_anchor="anchor-1")
add("_busy_reply_to", event=ev(thread_id="t1", chat_type="group"), reply_anchor="anchor-1")
add("_busy_reply_to", event=ev(thread_id=None), reply_anchor="anchor-1")
add("_busy_reply_to", event=ev(platform="discord", thread_id="t1", chat_type="dm"),
    reply_anchor="anchor-1")
add("_busy_reply_to", event=ev(internal=True), reply_anchor="anchor-1")
add("_busy_reply_to", event=ev(internal=True, thread_id="t1", chat_type="dm"),
    reply_anchor=None)
add("_busy_reply_to", event=ev(mid=42), reply_anchor="x")
add("_busy_reply_to", event=ev(thread_id=0, chat_type="dm"), reply_anchor="anchor-1")

# ── _demote_interrupt ───────────────────────────────────────────────────────
add("_demote_interrupt", session_key="k1", why="subagents active")
add("_demote_interrupt", session_key="k2", why="compression in flight")
add("_demote_interrupt", session_key="", why="")
add("_demote_interrupt", session_key="k3", why="reason with spaces(#123)")
add("_demote_interrupt", session_key="k4", why="unicode ünïcode ✓")
add("_demote_interrupt", session_key="k5", why="")

# ── _command_handler_table ──────────────────────────────────────────────────
ALIASES = {"bg": "_handle_background_command", "sethome": "_handle_set_home_command"}
add("_command_handler_table", names=["status", "bg", "codex-runtime"], aliases=ALIASES,
    io={"getattr_handler": ["h_status", "h_background", "h_codex"]})
add("_command_handler_table", names=["sethome"], aliases=ALIASES,
    io={"getattr_handler": ["h_set_home"]})
add("_command_handler_table", names=["a", "b"], aliases={},
    io={"getattr_handler": ["h_a", "h_b"]})
add("_command_handler_table", names=[], aliases=ALIASES, io={})
add("_command_handler_table", names=["model", "approve"], aliases=ALIASES,
    io={"getattr_handler": ["h_model", "h_approve"]})
add("_command_handler_table", names=["status"], aliases=ALIASES,
    io={"getattr_handler": [raise_("AttributeError", "no attribute")]})

# ── _gateway_idle_command_handlers ──────────────────────────────────────────
IDLE = ["topic", "sethome", "codex-runtime", "reload-mcp", "goal"]
add("_gateway_idle_command_handlers", idle_commands=IDLE, aliases=ALIASES,
    io={"getattr_handler": ["h_topic", "h_set_home", "h_codex", "h_reload", "h_goal"]})
add("_gateway_idle_command_handlers", idle_commands=["stop", "bg"], aliases=ALIASES,
    io={"getattr_handler": ["h_stop", "h_background"]})
add("_gateway_idle_command_handlers", idle_commands=[], aliases=ALIASES, io={})
add("_gateway_idle_command_handlers", idle_commands=["model", "voice"], aliases={},
    io={"getattr_handler": ["h_model", "h_voice"]})
add("_gateway_idle_command_handlers", idle_commands=["x"], aliases=ALIASES,
    io={"getattr_handler": [raise_("RuntimeError", "handler missing")]})

# ── _plaintext_approval_words ───────────────────────────────────────────────
# 4 input keys (approve, deny, always, session) x 2 t() calls = 8 scripts.
def t8(pairs):
    return {"t": [x for pair in pairs for x in pair]}

add("_plaintext_approval_words",
    io=t8([["yes, YES", "tak"], ["no, NO", "nie"], ["always", "always"], ["session", "session"]]))
add("_plaintext_approval_words",
    io=t8([["approval.inputs.approve", "yEs"], ["deny", "approval.inputs.deny"],
           ["", ""], [", ", "ok"]]))
add("_plaintext_approval_words",
    io=t8([["yeah, yes, YES", "tak"], ["nope, no", "NIE"], ["always, always", "stale"],
           ["session", "session"]]))
add("_plaintext_approval_words",
    io=t8([["yes", "yes"], ["no", "no"], ["always", "always"], ["session", "session"]]))
add("_plaintext_approval_words",
    io={"t": ["yes", raise_("RuntimeError", "i18n fail")]})
add("_plaintext_approval_words",
    io=t8([["YES , yes", "yes"], ["NO", "no"], ["always", "always"], ["session", "SESSION"]]))

# ── _get_max_concurrent_sessions ────────────────────────────────────────────
add("_get_max_concurrent_sessions", config={"max": 3},
    io={"resolve_max_concurrent_sessions": 5})
add("_get_max_concurrent_sessions", config=None,
    io={"resolve_max_concurrent_sessions": None})
add("_get_max_concurrent_sessions", config={"x": 1},
    io={"resolve_max_concurrent_sessions": raise_("ValueError", "bad config")})
add("_get_max_concurrent_sessions", config={"x": 1},
    io={"resolve_max_concurrent_sessions": raise_("FileNotFoundError", "no module")})
add("_get_max_concurrent_sessions", config=None,
    io={"resolve_max_concurrent_sessions": raise_("ModuleNotFoundError", "no module")})
add("_get_max_concurrent_sessions", config=None,
    io={"resolve_max_concurrent_sessions": raise_("SystemExit", "bye")})
add("_get_max_concurrent_sessions", config=None,
    io={"resolve_max_concurrent_sessions": raise_("CancelledError", "cancelled")})
add("_get_max_concurrent_sessions", config=None,
    io={"resolve_max_concurrent_sessions": raise_("BaseException", "raw")})

# ── _active_session_limit_message ───────────────────────────────────────────
def lim(maxv, running, count, reply="LIMIT", cfg=None, exc=None, rex=None):
    io = {"resolve_max_concurrent_sessions": (exc if exc is not None else maxv),
          "is_session_running": running,
          "running_agent_count": count,
          "active_session_limit_message": (rex if rex is not None else reply)}
    return io

add("_active_session_limit_message", session_key="k1", config={"max": 2},
    io=lim(None, False, 0))                                  # max is None
add("_active_session_limit_message", session_key="k1", config={"max": 2},
    io=lim(3, True, 9))                                      # running
add("_active_session_limit_message", session_key="k1", config={"max": 2},
    io=lim(3, False, 1))                                     # below cap
add("_active_session_limit_message", session_key="k1", config={"max": 2},
    io=lim(3, False, 3, reply="Session limit reached (3/3)"))
add("_active_session_limit_message", session_key="k1", config={"max": 2},
    io=lim(3, False, 9, rex=raise_("ValueError", "i18n fail")))
add("_active_session_limit_message", session_key="k1", config={"max": 2},
    io=lim(3, False, 9, exc=raise_("RuntimeError", "resolve failed")))
add("_active_session_limit_message", session_key="k1", config={"max": 2},
    io=lim(3, False, 9, exc=raise_("SystemExit", "bye")))

# ── _route_stop_epoch ───────────────────────────────────────────────────────
add("_route_stop_epoch", session_key="k1",
    io={"active_authority": None})
add("_route_stop_epoch", session_key="k1",
    io={"active_authority": {"id": "auth"}, "route_epoch": 7})
add("_route_stop_epoch", session_key="k2",
    io={"active_authority": {"id": "auth"}, "route_epoch": 0})
add("_route_stop_epoch", session_key="k1",
    io={"active_authority": {"id": "auth"}, "route_epoch": raise_("RuntimeError", "route")})
add("_route_stop_epoch", session_key="k1",
    io={"active_authority": raise_("ValueError", "no authority")})
add("_route_stop_epoch", session_key="k1",
    io={"active_authority": raise_("KeyboardInterrupt", "ki")})

# ── _send_busy_ack_reply ────────────────────────────────────────────────────
add("_send_busy_ack_reply", event_id="e1", adapter_id="ad1", message="ACK",
    io={"send_busy_reply": None})
add("_send_busy_ack_reply", event_id="e1", adapter_id=None, message="ACK",
    io={"send_busy_reply": None})
add("_send_busy_ack_reply", event_id="e1", adapter_id="ad1", message="",
    io={"send_busy_reply": raise_("RuntimeError", "send failed")})
add("_send_busy_ack_reply", event_id="e2", adapter_id="ad2", message="ACK",
    io={"send_busy_reply": raise_("OSError", "offline")})
add("_send_busy_ack_reply", event_id="e2", adapter_id="ad2", message="ACK",
    io={"send_busy_reply": raise_("GeneratorExit", "gen")})
add("_send_busy_ack_reply", event_id="e3", adapter_id="ad3", message="ACK",
    io={"send_busy_reply": raise_("BaseException", "raw")})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print("wrote %d cases to %s" % (len(CASES), OUT))
    for k in sorted(counts):
        print("  %-36s %d" % (k, counts[k]))
    thin = [k for k, v in counts.items() if v < 5]
    assert not thin, "funcs with <5 cases: %s" % thin


if __name__ == "__main__":
    main()
