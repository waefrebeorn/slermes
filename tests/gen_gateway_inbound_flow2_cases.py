#!/usr/bin/env python3
"""Generate tests/fixtures/gateway_inbound_flow2.jsonl — the scripted-seam
case corpus for the gateway_inbound_flow2 FLOW port (21 unclaimed methods of
gateway/run_inbound.py). >=5 cases per func: happy paths, every branch,
scripted Exception-family raises (swallowed where the live code catches),
BaseException passthrough (SystemExit), and fail-closed missing-script cases.

Deterministic, stdlib-only. The oracle AST-extracts the LIVE defs and replays
each case against the same scripts; the C harness feeds them to gi2_call.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "gateway_inbound_flow2.jsonl")

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


def ev(**kw):
    d = {"text": "hello", "message_type": "text", "allow_gateway_control": True}
    d.update(kw)
    return d


def src(**kw):
    d = {"platform": "telegram", "chat_id": "c1", "user_id": "u1",
         "user_name": "Uma", "chat_type": "dm"}
    d.update(kw)
    return d


EV = ev()
SRC = src()
KEY = "telegram:c1"

# ── _hm_cmd_start: logger-only, returns (True, "") ──
for i, k in enumerate(["k1", "k2", "", "telegram:c9", "slack:C2"]):
    case("_hm_cmd_start", {"event": ev(text=f"ping{i}"), "source": SRC, "key": k})

# ── _hm_cmd_egress ──
case("_hm_cmd_egress", {"event": EV, "source": SRC, "key": KEY},
     io={"format_status_text": "proxy up"})
case("_hm_cmd_egress", {"event": EV, "source": SRC, "key": KEY},
     io={"format_status_text": ""})
case("_hm_cmd_egress", {"event": EV, "source": SRC, "key": KEY},
     io={"format_status_text": None})
case("_hm_cmd_egress", {"event": EV, "source": SRC, "key": KEY},
     io={"format_status_text": rs(raise_=rais("RuntimeError"))})
case("_hm_cmd_egress", {"event": EV, "source": SRC, "key": KEY},
     io={"format_status_text": rs(raise_=rais("SystemExit", "1"))})

# ── _hm_cmd_queue / _hm_cmd_steer ──
for fn, tkey in (("_hm_cmd_queue", "gateway.queue.usage"),
                 ("_hm_cmd_steer", "gateway.steer.usage_idle")):
    base = {"t": "usage"}
    case(fn, {"event": EV, "source": SRC, "key": KEY},
         io=dict(base, **{"self._hm_send_payload_as_turn": [[False, None]]}))
    case(fn, {"event": EV, "source": SRC, "key": KEY},
         io=dict(base, **{"self._hm_send_payload_as_turn": [[True, "usage"]]}))
    case(fn, {"event": EV, "source": SRC, "key": KEY},
         io=dict(base, **{"self._hm_send_payload_as_turn": rs(raise_=rais("ValueError"))}))
    case(fn, {"event": EV, "source": SRC, "key": KEY},
         io=dict(base, **{"self._hm_send_payload_as_turn": rs(raise_=rais("SystemExit", "2"))}))
    case(fn, {"event": EV, "source": SRC, "key": KEY}, io={})   # t fail-closed

# ── _hm_confirm_destructive ──
CD_ARGS = {"event": EV, "command": "new", "detail": "wipe it",
           "handler": "self._handle_reset_command"}
case("_hm_confirm_destructive", CD_ARGS,
     io={"self._maybe_confirm_destructive_slash": "confirmed"})
case("_hm_confirm_destructive", CD_ARGS,
     io={"self._maybe_confirm_destructive_slash": None})
case("_hm_confirm_destructive", CD_ARGS,
     io={"self._maybe_confirm_destructive_slash": rs(raise_=rais("ValueError"))})
case("_hm_confirm_destructive", CD_ARGS,
     io={"self._maybe_confirm_destructive_slash": rs(raise_=rais("SystemExit", "3"))})
case("_hm_confirm_destructive", CD_ARGS, io={})   # fail-closed
case("_hm_confirm_destructive", dict(CD_ARGS, command="undo", detail="d2",
                                     handler="self._handle_undo_command"),
     io={"self._maybe_confirm_destructive_slash": [[True, "x"]]})

# ── _hm_cmd_new ──
NEW_IO = {"self._is_telegram_topic_root_lobby": False,
          "person_input_joined_turn": None,
          "t": "detail-new",
          "self._maybe_confirm_destructive_slash": "ok"}
case("_hm_cmd_new", {"event": EV, "source": SRC, "key": KEY},
     io={"self._is_telegram_topic_root_lobby": True,
         "self._telegram_topic_root_new_message": "topic-new"})
case("_hm_cmd_new", {"event": EV, "source": SRC, "key": KEY}, io=NEW_IO)
case("_hm_cmd_new", {"event": EV, "source": SRC, "key": KEY},
     io=dict(NEW_IO, **{"self._maybe_confirm_destructive_slash": rs(raise_=rais("RuntimeError"))}))
case("_hm_cmd_new", {"event": EV, "source": SRC, "key": KEY},
     io={"self._is_telegram_topic_root_lobby": rs(raise_=rais("SystemExit", "4"))})
case("_hm_cmd_new", {"event": EV, "source": SRC, "key": KEY},
     io={"self._is_telegram_topic_root_lobby": False,
         "person_input_joined_turn": None})   # t fail-closed
case("_hm_cmd_new", {"event": EV, "source": SRC, "key": KEY},
     io={"self._is_telegram_topic_root_lobby": False,
         "person_input_joined_turn": rs(raise_=rais("OSError", "lock"))})

# ── _hm_rewrite_turn_to_prompt ──
RW_ARGS = {"event": ev(), "source": SRC, "name": "learn", "ack": "ack!",
           "build": {"fn": "build_learn_prompt", "args": {"req": "x"}}}
case("_hm_rewrite_turn_to_prompt", RW_ARGS,
     io={"self._send_command_ack": None, "build_learn_prompt": "PROMPT"})
case("_hm_rewrite_turn_to_prompt", RW_ARGS,
     io={"self._send_command_ack": None,
         "build_learn_prompt": rs(raise_=rais("ValueError")),
         "t": "start-failed"})
case("_hm_rewrite_turn_to_prompt", RW_ARGS,
     io={"self._send_command_ack": None,
         "build_learn_prompt": rs(raise_=rais("SystemExit", "5"))})
case("_hm_rewrite_turn_to_prompt", RW_ARGS,
     io={"self._send_command_ack": rs(raise_=rais("RuntimeError"))})
case("_hm_rewrite_turn_to_prompt", RW_ARGS,
     io={"self._send_command_ack": None, "t": "start-failed"})   # build fail-closed -> caught
case("_hm_rewrite_turn_to_prompt", RW_ARGS,
     io={"self._send_command_ack": None,
         "build_learn_prompt": rs(raise_=rais("ValueError"))})   # t fail-closed after catch

# ── _hm_cmd_learn ──
LN_IO = {"event.get_command_args": "  describe cats  ",
         "t": "ack", "self._send_command_ack": None,
         "build_learn_prompt": "LEARN"}
case("_hm_cmd_learn", {"event": EV, "source": SRC, "key": KEY}, io=LN_IO)
case("_hm_cmd_learn", {"event": EV, "source": SRC, "key": KEY},
     io=dict(LN_IO, **{"event.get_command_args": ""}))
case("_hm_cmd_learn", {"event": EV, "source": SRC, "key": KEY},
     io=dict(LN_IO, **{"event.get_command_args": "   "}))
case("_hm_cmd_learn", {"event": EV, "source": SRC, "key": KEY},
     io={"event.get_command_args": None})   # .strip AttributeError propagates
case("_hm_cmd_learn", {"event": EV, "source": SRC, "key": KEY},
     io=dict(LN_IO, **{"build_learn_prompt": rs(raise_=rais("ValueError")),
                       "t": "start-failed"}))
case("_hm_cmd_learn", {"event": EV, "source": SRC, "key": KEY},
     io={"event.get_command_args": "x"})   # t fail-closed

# ── _hm_cmd_plan ──
PL_IO = {"event.get_command_args": "build a thing", "t": "ack",
         "self._send_command_ack": None, "build_plan_prompt": "PLAN"}
case("_hm_cmd_plan", {"event": EV, "source": SRC, "key": KEY}, io=PL_IO)
case("_hm_cmd_plan", {"event": EV, "source": SRC, "key": KEY},
     io=dict(PL_IO, **{"event.get_command_args": ""}))
case("_hm_cmd_plan", {"event": EV, "source": SRC, "key": KEY},
     io=dict(PL_IO, **{"event.get_command_args": "t" * 85}))   # ellipsis suffix
case("_hm_cmd_plan", {"event": EV, "source": SRC, "key": KEY},
     io=dict(PL_IO, **{"event.get_command_args": "t" * 80}))   # exact boundary
case("_hm_cmd_plan", {"event": EV, "source": SRC, "key": KEY},
     io=dict(PL_IO, **{"build_plan_prompt": rs(raise_=rais("RuntimeError")),
                       "t": "start-failed"}))
case("_hm_cmd_plan", {"event": EV, "source": SRC, "key": KEY},
     io={"event.get_command_args": rs(raise_=rais("SystemExit", "6"))})

# ── _hm_cmd_init ──
IN_IO = {"event.get_command_args": " extra ",
         "build_init_prompt_for_cwd": "Generate AGENTS.md please",
         "t": "ack-gen", "self._send_command_ack": None}
case("_hm_cmd_init", {"event": EV, "source": SRC, "key": KEY}, io=IN_IO)
case("_hm_cmd_init", {"event": EV, "source": SRC, "key": KEY},
     io=dict(IN_IO, **{"build_init_prompt_for_cwd": "Please UPDATE the existing AGENTS.md now",
                       "t": "ack-upd"}))
case("_hm_cmd_init", {"event": EV, "source": SRC, "key": KEY},
     io=dict(IN_IO, **{"build_init_prompt_for_cwd": rs(raise_=rais("ValueError")),
                       "t": "init-failed"}))
case("_hm_cmd_init", {"event": EV, "source": SRC, "key": KEY},
     io=dict(IN_IO, **{"build_init_prompt_for_cwd": rs(raise_=rais("SystemExit", "7"))}))
case("_hm_cmd_init", {"event": EV, "source": SRC, "key": ""},
     io=dict(IN_IO, **{"self._session_key_for_source": "derived-key"}))
case("_hm_cmd_init", {"event": EV, "source": SRC, "key": KEY},
     io={"event.get_command_args": rs(raise_=rais("RuntimeError")),
         "t": "init-failed"})   # inside try -> caught
case("_hm_cmd_init", {"event": EV, "source": SRC, "key": KEY},
     io=dict(IN_IO, **{"build_init_prompt_for_cwd": None}))   # `in None` TypeError

# ── _hm_cmd_blueprint ──
BP_ARGS = {"event": ev(), "source": SRC, "key": KEY}
case("_hm_cmd_blueprint", BP_ARGS,
     io={"self._handle_blueprint_command": {"text": "matched", "agent_seed": None}})
case("_hm_cmd_blueprint", BP_ARGS,
     io={"self._handle_blueprint_command": {"text": "", "agent_seed": None}})
case("_hm_cmd_blueprint", BP_ARGS, io={"self._handle_blueprint_command": None})
case("_hm_cmd_blueprint", BP_ARGS,
     io={"self._handle_blueprint_command": {"text": "seeded", "agent_seed": "SEED"},
         "self._send_command_ack": None})
case("_hm_cmd_blueprint", BP_ARGS,
     io={"self._handle_blueprint_command": {"text": "", "agent_seed": "SEED"}})
case("_hm_cmd_blueprint", BP_ARGS,
     io={"self._handle_blueprint_command": rs(raise_=rais("SystemExit", "8"))})

# ── _hm_cmd_undo ──
UD_IO = {"t": "detail", "self._maybe_confirm_destructive_slash": "ok"}
for raw in ["", "3", "0", "-2", "abc", "2 extra words", "+4", "  7  "]:
    case("_hm_cmd_undo", {"event": EV, "source": SRC, "key": KEY},
         io=dict(UD_IO, **{"event.get_command_args": raw}))

# ── _hm_resolve_command ──
RC_ARGS = {"event": EV, "source": SRC, "key": KEY}
case("_hm_resolve_command", RC_ARGS, io={"event.get_command": None})
case("_hm_resolve_command", RC_ARGS,
     io={"event.get_command": "new", "resolve_command": {"name": "new"},
         "is_gateway_known_command": True, "self._check_slash_access": None,
         "self._hm_command_hooks": [[False, None, None]],
         "is_interrupt_then_dispatch": False})
case("_hm_resolve_command", RC_ARGS,
     io={"event.get_command": "n", "resolve_command": [None, {"name": "new"}],
         "self._hm_quick_commands": {"n": {"type": "alias", "target": "new"}},
         "self._hm_expand_alias_quick_command": "new",
         "is_gateway_known_command": True, "self._check_slash_access": None,
         "self._hm_command_hooks": [[False, None, None]],
         "is_interrupt_then_dispatch": False})
case("_hm_resolve_command", RC_ARGS,
     io={"event.get_command": "wat", "resolve_command": None,
         "self._hm_quick_commands": {}, "is_gateway_known_command": False})
case("_hm_resolve_command", RC_ARGS,
     io={"event.get_command": "new", "resolve_command": {"name": "new"},
         "is_gateway_known_command": True, "self._check_slash_access": "denied"})
case("_hm_resolve_command", RC_ARGS,
     io={"event.get_command": "new", "resolve_command": {"name": "new"},
         "is_gateway_known_command": True, "self._check_slash_access": None,
         "self._hm_command_hooks": [[True, "hooked", None]]})
case("_hm_resolve_command", RC_ARGS,
     io={"event.get_command": "new", "resolve_command": {"name": "new"},
         "is_gateway_known_command": True, "self._check_slash_access": None,
         "self._hm_command_hooks": [[False, None, "stop"]],
         "is_interrupt_then_dispatch": True, "self._hm_drop_held_input": None})
case("_hm_resolve_command", RC_ARGS,
     io={"event.get_command": rs(raise_=rais("SystemExit", "9"))})
case("_hm_resolve_command", RC_ARGS,
     io={"event.get_command": "new"})   # resolve_command fail-closed

# ── _hm_dispatch_canonical_command ──
DC_ARGS = {"event": EV, "source": SRC, "key": KEY}
case("_hm_dispatch_canonical_command", dict(DC_ARGS, canonical="start"),
     io={"self._gateway_plain_command_handlers": {},
         "self._gateway_idle_command_handlers": {}})
case("_hm_dispatch_canonical_command", dict(DC_ARGS, canonical="egress"),
     io={"self._gateway_plain_command_handlers": {},
         "self._gateway_idle_command_handlers": {},
         "format_status_text": "egress-ok"})
case("_hm_dispatch_canonical_command", dict(DC_ARGS, canonical="unknown"),
     io={"self._gateway_plain_command_handlers": {},
         "self._gateway_idle_command_handlers": {}})
case("_hm_dispatch_canonical_command", dict(DC_ARGS, canonical=None),
     io={"self._gateway_plain_command_handlers": {},
         "self._gateway_idle_command_handlers": {}})
case("_hm_dispatch_canonical_command", dict(DC_ARGS, canonical="x"),
     io={"self._gateway_plain_command_handlers": {"x": "handler.plain"},
         "self._async_profile_scope_for_source": None,
         "handler.plain": "plain-result"})
case("_hm_dispatch_canonical_command", dict(DC_ARGS, canonical="x"),
     io={"self._gateway_plain_command_handlers": {},
         "self._gateway_idle_command_handlers": {"x": "handler.idle"},
         "self._async_profile_scope_for_source": None,
         "handler.idle": "idle-result"})
case("_hm_dispatch_canonical_command", dict(DC_ARGS, canonical="moa"),
     io={"self._gateway_plain_command_handlers": {},
         "self._gateway_idle_command_handlers": {}})
case("_hm_dispatch_canonical_command", dict(DC_ARGS, canonical="x"),
     io={"self._gateway_plain_command_handlers": rs(raise_=rais("SystemExit", "10"))})
case("_hm_dispatch_canonical_command", dict(DC_ARGS, canonical="x"),
     io={"self._gateway_plain_command_handlers": {"x": "handler.plain"},
         "self._async_profile_scope_for_source": rs(raise_=rais("RuntimeError"))})

# ── _hm_skill_slash_rewrite ──
SK_ARGS = {"event": EV, "source": SRC, "key": KEY, "command": "myskill"}
SK_IO = {"self._hm_bundle_slash_rewrite": False,
         "get_skill_commands": {"myskill": {"name": "MySkill"}},
         "resolve_skill_command_key": "myskill",
         "event.get_command_args": " do it ",
         "split_stacked_skill_commands": [[[], "do it"]],
         "get_disabled_skill_names": [[]],
         "build_skill_invocation_message": "SKILL-MSG"}
case("_hm_skill_slash_rewrite", dict(SK_ARGS, command=None), io={})
case("_hm_skill_slash_rewrite", SK_ARGS,
     io={"self._hm_bundle_slash_rewrite": True})
case("_hm_skill_slash_rewrite", SK_ARGS, io=SK_IO)
case("_hm_skill_slash_rewrite", SK_ARGS,
     io=dict(SK_IO, **{"resolve_skill_command_key": None,
                       "self._hm_unknown_slash_reply": "unknown-cmd"}))
case("_hm_skill_slash_rewrite", SK_ARGS,
     io=dict(SK_IO, **{"get_disabled_skill_names": [["MySkill"]],
                       "t": "disabled"}))
case("_hm_skill_slash_rewrite", SK_ARGS,
     io=dict(SK_IO, **{"split_stacked_skill_commands": rs(raise_=rais("RuntimeError"))}))
case("_hm_skill_slash_rewrite", SK_ARGS,
     io=dict(SK_IO, **{"split_stacked_skill_commands": [[["extra1"], "rest"]],
                       "get_skill_commands": {"myskill": {"name": "MySkill"},
                                              "extra1": {"name": "Extra"}},
                       "get_disabled_skill_names": [["Extra"]],
                       "t": "stacked-disabled"}))
case("_hm_skill_slash_rewrite", SK_ARGS,
     io=dict(SK_IO, **{"split_stacked_skill_commands": [[["extra1"], "rest"]],
                       "get_skill_commands": {"myskill": {"name": "MySkill"},
                                              "extra1": {"name": "Extra"}},
                       "build_stacked_skill_invocation_message": [["STACKED", [], []]]}))
case("_hm_skill_slash_rewrite", SK_ARGS,
     io=dict(SK_IO, **{"get_skill_commands": rs(raise_=rais("RuntimeError"))}))
case("_hm_skill_slash_rewrite", SK_ARGS,
     io=dict(SK_IO, **{"get_skill_commands": rs(raise_=rais("SystemExit", "11"))}))
case("_hm_skill_slash_rewrite", SK_ARGS,
     io=dict(SK_IO, **{"resolve_skill_command_key": "absent"}))   # KeyError swallowed
case("_hm_skill_slash_rewrite", SK_ARGS,
     io=dict(SK_IO, **{"build_skill_invocation_message": None}))   # no rewrite

# ── _hm_pending_reply_intercepts ──
PI_ARGS = {"event": EV, "source": SRC, "key": KEY}
case("_hm_pending_reply_intercepts", {"event": ev(allow_gateway_control=False),
                                      "source": SRC, "key": KEY}, io={})
case("_hm_pending_reply_intercepts", {"event": ev(_fifo_only=True),
                                      "source": SRC, "key": KEY}, io={})
case("_hm_pending_reply_intercepts", PI_ARGS,
     state={"executing_admission": {"id": "a1"}}, io={})
case("_hm_pending_reply_intercepts", PI_ARGS,
     io={"self._hm_update_prompt_reply": "update-reply"})
case("_hm_pending_reply_intercepts", PI_ARGS,
     io={"self._hm_update_prompt_reply": None, "self._hm_clarify_reply": "clarify-reply"})
case("_hm_pending_reply_intercepts", PI_ARGS,
     io={"self._hm_update_prompt_reply": None, "self._hm_clarify_reply": None,
         "self._hm_slash_confirm_reply": "confirm-reply"})
case("_hm_pending_reply_intercepts", PI_ARGS,
     io={"self._hm_update_prompt_reply": None, "self._hm_clarify_reply": None,
         "self._hm_slash_confirm_reply": None})
case("_hm_pending_reply_intercepts", PI_ARGS,
     io={"self._hm_update_prompt_reply": rs(raise_=rais("SystemExit", "12"))})

# ── _hm_dispatch_idle_commands ──
DI_ARGS = {"event": EV, "source": SRC, "key": KEY}
DI_BASE = {"self._route_stop_epoch": 0, "event.get_command": None,
           "self._gateway_plain_command_handlers": {},
           "self._gateway_idle_command_handlers": {},
           "self._hm_dispatch_quick_and_plugin_commands": [[False, None, None]]}
case("_hm_dispatch_idle_commands", DI_ARGS, io=DI_BASE)
case("_hm_dispatch_idle_commands", DI_ARGS,
     io=dict(DI_BASE, **{"event.get_command": "start",
                         "resolve_command": {"name": "start"},
                         "is_gateway_known_command": True,
                         "self._check_slash_access": None,
                         "self._hm_command_hooks": [[False, None, None]],
                         "is_interrupt_then_dispatch": False}))
case("_hm_dispatch_idle_commands", DI_ARGS,
     io=dict(DI_BASE, **{"event.get_command": "x", "resolve_command": None,
                         "self._hm_quick_commands": {},
                         "is_gateway_known_command": False,
                         "self._hm_dispatch_quick_and_plugin_commands": [[True, "quick", "x"]]}))
case("_hm_dispatch_idle_commands", DI_ARGS,
     io=dict(DI_BASE, **{"event.get_command": "sk", "resolve_command": None,
                         "self._hm_quick_commands": {},
                         "is_gateway_known_command": False,
                         "self._hm_dispatch_quick_and_plugin_commands": [[False, None, "sk"]],
                         "self._hm_bundle_slash_rewrite": False,
                         "get_skill_commands": {"sk": {"name": "S"}},
                         "resolve_skill_command_key": None,
                         "self._hm_unknown_slash_reply": "skill-reply"}))
case("_hm_dispatch_idle_commands", DI_ARGS,
     io=dict(DI_BASE, **{"event.get_command": "sk", "resolve_command": None,
                         "self._hm_quick_commands": {},
                         "is_gateway_known_command": False,
                         "self._hm_dispatch_quick_and_plugin_commands": [[False, None, "sk"]],
                         "self._hm_bundle_slash_rewrite": False,
                         "get_skill_commands": {"sk": {"name": "S"}},
                         "resolve_skill_command_key": "sk",
                         "event.get_command_args": "",
                         "split_stacked_skill_commands": [[[], ""]],
                         "get_disabled_skill_names": [[]],
                         "build_skill_invocation_message": "MSG",
                         "self._admit_command_turn": [[True, "admitted"]]}))
case("_hm_dispatch_idle_commands", DI_ARGS,
     io={"self._route_stop_epoch": rs(raise_=rais("SystemExit", "13"))})
case("_hm_dispatch_idle_commands", DI_ARGS,
     io=dict(DI_BASE, **{"event.get_command": "sk", "resolve_command": None,
                         "self._hm_quick_commands": {},
                         "is_gateway_known_command": False,
                         "self._hm_dispatch_quick_and_plugin_commands": [[False, None, "sk"]],
                         "self._hm_bundle_slash_rewrite": True,
                         "self._admit_command_turn": [[False, None]]}))

# ── _hm_rescue_orphaned_fifo ──
RF_ARGS = {"event": ev(), "source": SRC, "is_internal": False, "key": KEY}
ORPHAN = {"text": "orphan", "message_type": "text",
          "source": {"platform": "telegram", "chat_id": "c1"}, "internal": True}
case("_hm_rescue_orphaned_fifo", RF_ARGS,
     io={"self._delivery_adapter_for": None})
case("_hm_rescue_orphaned_fifo", dict(RF_ARGS, event=ev(internal=True)),
     io={"self._delivery_adapter_for": "ad1"})
case("_hm_rescue_orphaned_fifo", RF_ARGS,
     io={"self._delivery_adapter_for": "ad1", "event.get_command": "stop"})
case("_hm_rescue_orphaned_fifo", RF_ARGS,
     io={"self._delivery_adapter_for": "ad1", "event.get_command": None,
         "self._rescue_orphaned_overflow": None})
case("_hm_rescue_orphaned_fifo", RF_ARGS,
     io={"self._delivery_adapter_for": "ad1", "event.get_command": None,
         "self._rescue_orphaned_overflow": ORPHAN, "self._enqueue_fifo": None})
case("_hm_rescue_orphaned_fifo", RF_ARGS,
     io={"self._delivery_adapter_for": "ad1", "event.get_command": None,
         "self._rescue_orphaned_overflow": {"text": "orphan2"},
         "self._enqueue_fifo": None})
case("_hm_rescue_orphaned_fifo", RF_ARGS,
     io={"self._delivery_adapter_for": rs(raise_=rais("RuntimeError"))})
case("_hm_rescue_orphaned_fifo", RF_ARGS,
     io={"self._delivery_adapter_for": "ad1", "event.get_command": None,
         "self._rescue_orphaned_overflow": rs(raise_=rais("SystemExit", "14"))})

# ── _hm_authority_admission ──
AA_ARGS = {"event": EV, "source": SRC, "key": KEY, "is_internal": False}
AA_FIFO_IO = {"self._is_session_running": False,
              "self._hm_post_admission_consume": [[False, None]],
              "note_busy_admitting": None,
              "admit_message": "admitted",
              "settle_busy_arrival": None}
case("_hm_authority_admission", AA_ARGS,
     io={"active_authority": None, "event.get_command": None})
case("_hm_authority_admission", AA_ARGS,
     state={"session_authority": "other-home"},
     io={"active_authority": None, "event.get_command": None})
case("_hm_authority_admission", AA_ARGS,
     io={"active_authority": "a1", "event.get_command": "stop"})
case("_hm_authority_admission", dict(AA_ARGS, is_internal=True),
     io={"active_authority": "a1", "event.get_command": None})
case("_hm_authority_admission", AA_ARGS,
     state={"executing_admission": {"id": "x"}},
     io={"active_authority": "a1", "event.get_command": None})
case("_hm_authority_admission", AA_ARGS,
     io=dict({"active_authority": "a1", "event.get_command": None}, **AA_FIFO_IO))
case("_hm_authority_admission", AA_ARGS,
     io=dict({"active_authority": "a1", "event.get_command": None,
              "self._is_session_running": True,
              "self._delivery_adapter_for": "ad1",
              "stamp_busy_arrival": None,
              "self._route_plaintext_approval_while_busy": True,
              "settle_busy_arrival": None}))
case("_hm_authority_admission", AA_ARGS,
     io=dict({"active_authority": "a1", "event.get_command": None},
             **dict(AA_FIFO_IO, **{"settle_busy_arrival": rs(raise_=rais("RuntimeError"))})))
case("_hm_authority_admission", AA_ARGS,
     io={"active_authority": rs(raise_=rais("SystemExit", "15"))})

# ── _hm_admit_to_fifo ──
AF_ARGS = {"authority": "a1", "event": EV, "source": SRC, "key": KEY, "busy": False}
AF_BASE = {"self._hm_post_admission_consume": [[False, None]],
           "note_busy_admitting": None, "admit_message": "ok"}
case("_hm_admit_to_fifo", AF_ARGS, io=AF_BASE)
case("_hm_admit_to_fifo", dict(AF_ARGS, busy=True),
     io=dict(AF_BASE, **{"self._route_plaintext_approval_while_busy": True}))
case("_hm_admit_to_fifo", dict(AF_ARGS, busy=True),
     io=dict(AF_BASE, **{"self._route_plaintext_approval_while_busy": False,
                         "self._hm_busy_preempt": True}))
case("_hm_admit_to_fifo", dict(AF_ARGS, busy=True),
     io=dict(AF_BASE, **{"self._route_plaintext_approval_while_busy": False,
                         "self._hm_busy_preempt": False,
                         "self._hm_post_admission_consume": [[True, "consumed"]]}))
case("_hm_admit_to_fifo", dict(AF_ARGS, event=ev(_fifo_only=True, _stop_epoch=5)),
     io=dict(AF_BASE, **{"self._route_stop_epoch": 7}))
case("_hm_admit_to_fifo", dict(AF_ARGS, event=ev(_fifo_only=True, _stop_epoch=5)),
     io=dict(AF_BASE, **{"self._route_stop_epoch": 5}))
case("_hm_admit_to_fifo",
     dict(AF_ARGS, busy=True, event=ev(_gateway_pending_stt_clips=["clip1"])),
     io=dict(AF_BASE, **{"self._route_plaintext_approval_while_busy": False,
                         "self._hm_busy_preempt": False,
                         "hand_prepared_voice_to_turn": None,
                         "pending_setup_busy_notice": "notice",
                         "self._delivery_adapter_for": "ad1",
                         "self._send_busy_ack_reply": None}))
case("_hm_admit_to_fifo", AF_ARGS,
     io=dict(AF_BASE, **{"admit_message": rs(raise_=rais("AuthorityRefusal", reason="busy"))}))
case("_hm_admit_to_fifo", AF_ARGS, io={})   # post_admission fail-closed

# ── _handle_message ──
HM_EV = ev(text="hi there")
HM_SRC = {"platform": "telegram", "chat_id": "c1"}
ADMITTED = [[HM_EV, HM_SRC, False]]   # queue of ONE list result
HM_STATE = {"session_states": {KEY: {"turn": {}}}}
HM_FULL = {
    "self._hm_admit_event": ADMITTED,
    "record_gateway_slash_command": None,
    "self._hm_estop_gate": None,
    "self._session_key_for_source": KEY,
    "self._hm_update_prompt_reply": None,
    "self._hm_clarify_reply": None,
    "self._hm_slash_confirm_reply": None,
    "active_authority": None,
    "event.get_command": None,
    "self._hm_evict_idle_stale_agent": None,
    "self._is_session_running": False,
    "self._route_stop_epoch": 0,
    "self._gateway_plain_command_handlers": {},
    "self._gateway_idle_command_handlers": {},
    "self._hm_dispatch_quick_and_plugin_commands": [[False, None, None]],
    "self._is_telegram_topic_root_lobby": False,
    "self._claim_active_session_slot": [[None, None]],
    "self._delivery_adapter_for": None,
    "time.time": 100,
    "start_reply_clock": None,
    "self._persist_active_agents": None,
    "self._begin_session_run_generation": 1,
    "arm_admitted_moa": None,
    "self._hm_post_admission_consume": [[False, None]],
    "self._handle_message_with_agent": "agent-result",
    "self._run_post_turn_hooks": None,
    "self._restore_pending_one_turn_model_override": None,
    "self._clear_durable_active_turn": True,
    "self._release_running_agent_state": None,
    "self._release_turn_lease": None,
}
case("_handle_message", {"event": HM_EV}, io={"self._hm_admit_event": None})
case("_handle_message", {"event": HM_EV},
     io={"self._hm_admit_event": ADMITTED,
         "record_gateway_slash_command": None,
         "self._hm_estop_gate": "paused"})
case("_handle_message", {"event": HM_EV}, state=HM_STATE,
     io=dict(HM_FULL, **{"self._hm_admit_event": [[HM_EV, HM_SRC, True]]}))
case("_handle_message", {"event": HM_EV},
     io=dict(HM_FULL, **{"self._hm_update_prompt_reply": "intercepted"}))
case("_handle_message", {"event": HM_EV}, io=HM_FULL)   # no session_states: turn AttributeError
case("_handle_message", {"event": HM_EV},
     io=dict(HM_FULL, **{"active_authority": "a1",
                         "self._is_session_running": [False, False, False],
                         "self._hm_post_admission_consume": [[False, None]],
                         "note_busy_admitting": None,
                         "admit_message": "authority-reply",
                         "settle_busy_arrival": None}))
case("_handle_message", {"event": HM_EV},
     io=dict(HM_FULL, **{"self._is_session_running": [True, True],
                         "self._hm_evict_reaped_agent": None,
                         "self._hm_handle_running_session_message": "busy-reply"}))
case("_handle_message", {"event": HM_EV}, state=HM_STATE, io=HM_FULL)
case("_handle_message", {"event": HM_EV}, state=HM_STATE,
     io=dict(HM_FULL, **{"self._handle_message_with_agent":
                         rs(raise_=rais("TurnLeaseTimeoutError", session_id="s1",
                                        owner_key="k1", generation=1, wait_seconds=30)),
                         "t": "another-turn"}))
case("_handle_message", {"event": HM_EV}, state=HM_STATE,
     io=dict(HM_FULL, **{"self._handle_message_with_agent":
                         rs(raise_=rais("SystemExit", "16"))}))
case("_handle_message", {"event": HM_EV}, state=HM_STATE,
     io=dict(HM_FULL, **{"self._run_post_turn_hooks": rs(raise_=rais("RuntimeError"))}))
case("_handle_message", {"event": HM_EV},
     io=dict(HM_FULL, **{"self._claim_active_session_slot": [[None, "session-limit"]]}))
case("_handle_message", {"event": HM_EV},
     state={"_external_drain_active": True},
     io=dict(HM_FULL, **{"t": "draining"}))
case("_handle_message", {"event": HM_EV},
     io=dict(HM_FULL, **{"self._is_telegram_topic_root_lobby": True,
                         "self._should_send_telegram_lobby_reminder": True,
                         "self._telegram_topic_root_lobby_message": "lobby-msg"}))
case("_handle_message", {"event": HM_EV}, state=HM_STATE,
     io=dict(HM_FULL, **{"self._hm_post_admission_consume": [[True, "plugin-consumed"]]}))
case("_handle_message", {"event": HM_EV}, state=HM_STATE,
     io=dict(HM_FULL, **{"self._restore_pending_one_turn_model_override":
                         rs(raise_=rais("RuntimeError"))}))


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    short = {f: n for f, n in counts.items() if n < 5}
    if short:
        raise SystemExit(f"FATAL: funcs with <5 cases: {short}")
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False, separators=(",", ":")) + "\n")
    print(f"wrote {len(CASES)} cases across {len(counts)} funcs -> {OUT}")
    for fn in sorted(counts):
        print(f"  {fn}: {counts[fn]}")


if __name__ == "__main__":
    main()