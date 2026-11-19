#!/usr/bin/env python3
"""Generate tests/fixtures/conversation_compression_flow2.jsonl — scripted
universal-seam cases for the 20 UNCLAIMED agent/conversation_compression.py
FLOW defs ported in src/agent/conversation_compression_flow2.c.

Each case is {"func": <live_name>, "in": {...}} where in.io carries the seam
scripts (seam-name -> constant / per-call queue / {"__raise__":...} /
{"__script__":true,"ret":...}), in.state carries the mutable carrier, and the
remaining in.* keys are scalar / double inputs. >=5 cases per function cover the
decision branches, the ordered-trace short-circuits, scripted raises,
BaseException passthrough, and fail-closed (missing-script) propagation.

Value model: attr doubles {"__attr__":{...}}; class-tagged Fence /
ContextCompressor / Persistence; callable doubles {"__callable__":"<seam>"};
identity refs {"__ref__":"<id>"}. ASCII text; integral floats == ints.

Run: python tests/gen_conversation_compression_flow2_cases.py
"""
import json
import os
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT = HERE / "fixtures" / "conversation_compression_flow2.jsonl"


def attr(d, cls=None):
    o = {"__attr__": d}
    if cls:
        o["__class__"] = cls
    return o


def fence(d):
    return attr(d, "Fence")


def cc(d):
    return attr(d, "ContextCompressor")


def pers(d):
    return attr(d, "Persistence")


def call(seam):
    return {"__callable__": seam}


def rs(kind, msg=""):
    return {"__raise__": kind, "message": msg}


CASES = []


def case(func, **inp):
    CASES.append({"func": func, "in": inp})


# ── 1. set_precommit_deadline_monotonic ──────────────────────────────────────
case("set_precommit_deadline_monotonic", deadline=100.0, state={"fence": fence({"_deadline": None})})
case("set_precommit_deadline_monotonic", deadline=0, state={"fence": fence({"_deadline": None})})
case("set_precommit_deadline_monotonic", deadline=-5, state={"fence": fence({"_deadline": 7})})
case("set_precommit_deadline_monotonic", deadline="12.5", state={"fence": fence({"_deadline": None})})
case("set_precommit_deadline_monotonic", deadline=True, state={"fence": fence({"_deadline": None})})
case("set_precommit_deadline_monotonic", deadline=None, state={"fence": fence({"_deadline": None})})
case("set_precommit_deadline_monotonic", deadline="abc", state={"fence": fence({"_deadline": None})})
case("set_precommit_deadline_monotonic", deadline=50, state={"fence": fence({"_deadline": 10})})

# ── 2. extend_deadline ───────────────────────────────────────────────────────
case("extend_deadline", seconds=0, state={"fence": fence({"_deadline": 100})},
     io={"lock_acquire": True, "monotonic": [10], "fence_is_cancelled": False, "lock_release": None})
case("extend_deadline", seconds=5, state={"fence": fence({"_deadline": 100})},
     io={"lock_acquire": False, "lock_release": None})
case("extend_deadline", seconds=5, state={"fence": fence({"_deadline": None})},
     io={"lock_acquire": True, "monotonic": [10], "fence_is_cancelled": False, "lock_release": None})
case("extend_deadline", seconds=5, state={"fence": fence({"_deadline": 10})},
     io={"lock_acquire": True, "monotonic": [20], "fence_is_cancelled": False, "lock_release": None})
case("extend_deadline", seconds=5, state={"fence": fence({"_deadline": 100})},
     io={"lock_acquire": True, "monotonic": [10], "fence_is_cancelled": False, "lock_release": None})
case("extend_deadline", seconds=5, state={"fence": fence({"_deadline": 100})},
     io={"lock_acquire": True, "monotonic": [10], "fence_is_cancelled": True, "lock_release": None})
case("extend_deadline", seconds=5, state={"fence": fence({"_deadline": 100})},
     io={"lock_acquire": True, "monotonic": [10], "lock_release": None})  # fail-closed is_cancelled
case("extend_deadline", seconds=5, state={"fence": fence({"_deadline": 100})},
     io={"lock_acquire": True, "monotonic": [10], "fence_is_cancelled": rs("KeyboardInterrupt", "stop"),
         "lock_release": None})

# ── 3. _should_suppress ──────────────────────────────────────────────────────
case("_should_suppress", state={"hb": attr({"_suppressed": True, "_commit_fence": fence({})})},
     io={"fence_is_cancelled": True})
case("_should_suppress", state={"hb": attr({"_suppressed": False, "_commit_fence": None})},
     io={"fence_is_cancelled": True})
case("_should_suppress", state={"hb": attr({"_suppressed": False, "_commit_fence": fence({})})},
     io={"fence_is_cancelled": True})
case("_should_suppress", state={"hb": attr({"_suppressed": False, "_commit_fence": fence({})})},
     io={"fence_is_cancelled": False})
case("_should_suppress", state={"hb": attr({"_suppressed": False, "_commit_fence": fence({})})},
     io={})  # fail-closed
case("_should_suppress", state={"hb": attr({"_suppressed": False, "_commit_fence": fence({})})},
     io={"fence_is_cancelled": rs("SystemExit", "bye")})

# ── 4. _touch ────────────────────────────────────────────────────────────────
HB_AGENT = attr({"_last_activity_provenance": "agent.compression", "_touch_activity": call("touch_activity")})
case("_touch", desc="context compression in progress", state={"hb": attr(
        {"_suppressed": False, "_commit_fence": None, "_agent": HB_AGENT})},
     io={"swallow": None, "normalize_activity_provenance": "agent.compression",
         "touch_activity": None})
case("_touch", desc="d", allow_terminal_overwrite=True, state={"hb": attr(
        {"_suppressed": True, "_commit_fence": fence({}), "_agent": HB_AGENT})},
     io={"swallow": None, "touch_activity": None})  # bypass suppression guard
case("_touch", desc="d", state={"hb": attr(
        {"_suppressed": False, "_commit_fence": fence({}), "_agent": HB_AGENT})},
     io={"swallow": None, "fence_is_cancelled": True})  # _should_suppress latches -> return
case("_touch", desc="d", state={"hb": attr(
        {"_suppressed": False, "_commit_fence": None,
         "_agent": attr({"_last_activity_provenance": "agent.compression_timeout",
                         "_touch_activity": call("touch_activity")})})},
     io={"swallow": None, "normalize_activity_provenance": "agent.compression_timeout"})  # terminal -> suppress
case("_touch", desc="d", state={"hb": attr(
        {"_suppressed": False, "_commit_fence": None, "_agent": attr({"_touch_activity": None})})},
     io={"swallow": None, "normalize_activity_provenance": "unknown"})  # touch not callable
case("_touch", desc="d", force_persist=True, state={"hb": attr(
        {"_suppressed": False, "_commit_fence": None, "_agent": HB_AGENT})},
     io={"swallow": None, "normalize_activity_provenance": "agent.compression",
         "touch_activity": rs("RuntimeError", "boom"), "logger.debug": None})  # Exception swallowed
case("_touch", desc="d", state={"hb": attr(
        {"_suppressed": False, "_commit_fence": None, "_agent": HB_AGENT})},
     io={"swallow": None, "normalize_activity_provenance": "agent.compression",
         "touch_activity": rs("KeyboardInterrupt", "stop")})  # BaseException propagates
case("_touch", desc="d", state={"hb": attr(
        {"_suppressed": False, "_commit_fence": None, "_agent": HB_AGENT})},
     io={"normalize_activity_provenance": "agent.compression", "touch_activity": None})  # fail-closed swallow

# ── 5. complete ──────────────────────────────────────────────────────────────
LC_AGENT = attr({"status_callback": call("status_callback")})
case("complete", state={"lifecycle": attr({"_done_emitted": False, "status_emitted": True,
                                            "commit_status": "committed", "_agent": LC_AGENT})},
     io={"_emit_compaction_done": None})
case("complete", state={"lifecycle": attr({"_done_emitted": True, "status_emitted": True,
                                            "commit_status": "committed", "_agent": LC_AGENT})},
     io={"_emit_compaction_done": None})  # already done -> no seam
case("complete", state={"lifecycle": attr({"_done_emitted": False, "status_emitted": False,
                                            "commit_status": "committed", "_agent": LC_AGENT})},
     io={"_emit_compaction_done": None})  # status not emitted -> no edge
case("complete", force_terminal=True, state={"lifecycle": attr({"_done_emitted": False,
        "status_emitted": True, "commit_status": "aborted", "_agent": LC_AGENT})},
     io={"_emit_compaction_done": None})  # force_terminal opts in
case("complete", state={"lifecycle": attr({"_done_emitted": False, "status_emitted": True,
                                            "commit_status": "aborted", "_agent": LC_AGENT})},
     io={"_emit_compaction_done": None})  # aborted, no force -> no edge
case("complete", state={"lifecycle": attr({"_done_emitted": False, "status_emitted": True,
                                            "commit_status": "committed", "_agent": LC_AGENT})},
     io={"_emit_compaction_done": rs("SystemExit", "bye")})  # BaseException propagates

# ── 6. status_emitted ────────────────────────────────────────────────────────
case("status_emitted", state={"lease": attr({"_lifecycle": attr({"status_emitted": True})})})
case("status_emitted", state={"lease": attr({"_lifecycle": attr({"status_emitted": False})})})
case("status_emitted", state={"lease": attr({"_lifecycle": attr({"status_emitted": True,
                                                                  "commit_status": "committed"})})})
case("status_emitted", state={"lease": attr({"_lifecycle": attr({"status_emitted": False,
                                                                  "_done_emitted": True})})})
case("status_emitted", state={"lease": attr({"_lifecycle": attr({"status_emitted": True})})})

# ── 7. start_refresher ───────────────────────────────────────────────────────
LEASE_BASE = {"holder": "h1", "db": attr({"k": 1}), "sid": "s1", "ttl": 30,
              "_refresh_interval": 10, "store": None, "_released": False, "_refresher": None}
case("start_refresher", state={"lease": attr(dict(LEASE_BASE))},
     io={"refresher_ctor": attr({"start": call("refresher_start")}), "refresher_start": attr({"id": "r1"})})
case("start_refresher", state={"lease": attr(dict(LEASE_BASE, holder=None))},
     io={"refresher_ctor": attr({}), "refresher_start": None})  # holder None -> return, no seam
case("start_refresher", state={"lease": attr(dict(LEASE_BASE, _released=True))},
     io={"refresher_ctor": attr({"start": call("refresher_start")}), "refresher_start": attr({"id": "r1"})})
case("start_refresher", state={"lease": attr(dict(LEASE_BASE, store=attr({"coordination": {}})))},
     io={"refresher_ctor": attr({"start": call("refresher_start")}), "refresher_start": None})
case("start_refresher", state={"lease": attr(dict(LEASE_BASE))},
     io={"refresher_ctor": rs("RuntimeError", "ctor boom")})  # ctor raise propagates
case("start_refresher", state={"lease": attr(dict(LEASE_BASE))},
     io={"refresher_ctor": attr({"start": call("refresher_start")})})  # fail-closed refresher_start

# ── 8. release_holder_only ───────────────────────────────────────────────────
RL_AGENT = attr({"_active_compression_lock_holder": "h1"})
RL_BASE = {"_released": False, "_agent": RL_AGENT, "holder": "h1",
           "_refresher": attr({"stop": call("refresher_stop")}), "db": attr({"k": 1}),
           "sid": "s1", "store": None}
case("release_holder_only", state={"lease": attr(dict(RL_BASE))},
     io={"swallow": [None, None], "refresher_stop": None, "_release_compression_lock": None})
case("release_holder_only", state={"lease": attr(dict(RL_BASE, _released=True))},
     io={"swallow": [None, None], "refresher_stop": None, "_release_compression_lock": None})  # idempotent
case("release_holder_only", state={"lease": attr(dict(RL_BASE, _refresher=None))},
     io={"swallow": [None], "_release_compression_lock": None})  # no refresher
case("release_holder_only", state={"lease": attr(dict(RL_BASE, db=None))},
     io={"swallow": [None], "refresher_stop": None})  # db None -> no lock release
case("release_holder_only", state={"lease": attr(dict(RL_BASE,
        _agent=attr({"_active_compression_lock_holder": "other"})))},
     io={"swallow": [None, None], "refresher_stop": None, "_release_compression_lock": None})  # holder mismatch
case("release_holder_only", state={"lease": attr(dict(RL_BASE))},
     io={"swallow": [None, None], "refresher_stop": rs("RuntimeError", "stop boom"),
         "logger.debug": None, "_release_compression_lock": None})  # stop Exception swallowed
case("release_holder_only", state={"lease": attr(dict(RL_BASE))},
     io={"swallow": [None, None], "refresher_stop": rs("KeyboardInterrupt", "stop"),
         "_release_compression_lock": None})  # BaseException propagates

# ── 9. release ───────────────────────────────────────────────────────────────
def lease_for_release(**over):
    lc = attr({"_done_emitted": False, "status_emitted": True, "commit_status": "committed",
               "_agent": LC_AGENT})
    d = dict(RL_BASE, _lifecycle=lc, _commit_fence=fence({}), _lock_setup_entered=True)
    d.update(over)
    return attr(d)

REL_IO = {"_emit_compaction_done": None, "swallow": [None, None], "refresher_stop": None,
          "_release_compression_lock": None, "clear_cancelled_lock_release": None,
          "finish_lock_setup": None}
case("release", state={"lease": lease_for_release()}, io=dict(REL_IO))
case("release", state={"lease": lease_for_release(_commit_fence=None)}, io=dict(REL_IO))  # no fence
case("release", state={"lease": lease_for_release(_lifecycle=attr(
        {"_done_emitted": False, "status_emitted": True, "commit_status": "aborted", "_agent": LC_AGENT}))},
     io=dict(REL_IO))  # complete no edge
case("release", state={"lease": lease_for_release()},
     io=dict(REL_IO, _emit_compaction_done=rs("RuntimeError", "done boom")))  # complete raises, finallys run
case("release", state={"lease": lease_for_release()},
     io=dict(REL_IO, _emit_compaction_done=rs("KeyboardInterrupt", "stop")))  # BaseException through finallys
case("release", state={"lease": lease_for_release()},
     io=dict(REL_IO, clear_cancelled_lock_release=rs("RuntimeError", "clear boom")))  # exit raise replaces
case("release", state={"lease": lease_for_release()},
     io={"_emit_compaction_done": None, "swallow": [None, None], "refresher_stop": None,
         "_release_compression_lock": None, "clear_cancelled_lock_release": None})  # fail-closed finish_lock_setup

# ── 10. restore_compressor ───────────────────────────────────────────────────
ATT_BASE = {"snapshot": {"threshold_tokens": 5000}, "durable_cooldown_authoritative": None,
            "durable_cooldown_state": None, "generation": 3}
case("restore_compressor", compressor=cc({"threshold_tokens": 9000}), state={"attempt": attr(dict(ATT_BASE))},
     io={"_restore_compressor_attempt_state": None})
case("restore_compressor", compressor=cc({"threshold_tokens": 9000}),
     state={"attempt": attr(dict(ATT_BASE, durable_cooldown_authoritative=True,
                                 durable_cooldown_state={"until": 100}, generation=7))},
     io={"_restore_compressor_attempt_state": None})
case("restore_compressor", compressor=cc({}), state={"attempt": attr(dict(ATT_BASE, snapshot={}))},
     io={"_restore_compressor_attempt_state": None})
case("restore_compressor", compressor=cc({"threshold_tokens": 9000}), state={"attempt": attr(dict(ATT_BASE))},
     io={"_restore_compressor_attempt_state": rs("RuntimeError", "restore boom")})
case("restore_compressor", compressor=cc({"threshold_tokens": 9000}), state={"attempt": attr(dict(ATT_BASE))},
     io={"_restore_compressor_attempt_state": rs("SystemExit", "bye")})
case("restore_compressor", compressor=cc({"threshold_tokens": 9000}), state={"attempt": attr(dict(ATT_BASE))},
     io={})  # fail-closed

# ── 11. context_engine_session_db ────────────────────────────────────────────
PERS_DB = pers({"scope": attr({"profile_dir": {"__path__": "/h/p1"}}), "read_only": False})
case("context_engine_session_db", session_db=PERS_DB,
     state={"agent": attr({"context_compressor": cc({"threshold_tokens": 1})})},
     io={})  # type is CC -> return session_db
case("context_engine_session_db", session_db={"plain": True},
     state={"agent": attr({"context_compressor": attr({"x": 1})})},
     io={})  # not Persistence -> return session_db
case("context_engine_session_db", session_db=PERS_DB,
     state={"agent": attr({"context_compressor": attr({"x": 1})})},
     io={"engine_session_db": attr({"id": "pdb1"})})  # plugin path creates db
case("context_engine_session_db", session_db=PERS_DB,
     state={"agent": attr({"context_compressor": attr({"x": 1}),
                           "_context_engine_session_db": attr({"id": "existing"})})},
     io={"engine_session_db": attr({"id": "pdb1"})})  # cached plugin_db -> no seam
case("context_engine_session_db", session_db=PERS_DB,
     state={"agent": attr({"context_compressor": attr({"x": 1})})},
     io={"engine_session_db": rs("RuntimeError", "db boom")})
case("context_engine_session_db", session_db=PERS_DB,
     state={"agent": attr({"context_compressor": attr({"x": 1})})},
     io={})  # fail-closed engine_session_db

# ── 12. close_context_engine_session_db ──────────────────────────────────────
case("close_context_engine_session_db",
     state={"agent": attr({"_context_engine_session_db": attr({"close": call("plugin_db_close")})})},
     io={"plugin_db_close": None})
case("close_context_engine_session_db", state={"agent": attr({})}, io={"plugin_db_close": None})  # absent
case("close_context_engine_session_db",
     state={"agent": attr({"_context_engine_session_db": None})}, io={"plugin_db_close": None})  # present-null
case("close_context_engine_session_db",
     state={"agent": attr({"_context_engine_session_db": attr({"close": call("plugin_db_close")})})},
     io={"plugin_db_close": rs("RuntimeError", "close boom")})
case("close_context_engine_session_db",
     state={"agent": attr({"_context_engine_session_db": attr({"close": call("plugin_db_close")})})},
     io={"plugin_db_close": rs("KeyboardInterrupt", "stop")})
case("close_context_engine_session_db",
     state={"agent": attr({"_context_engine_session_db": attr({"close": call("plugin_db_close")})})},
     io={})  # fail-closed

# ── 13. _lower_threshold_to_aux_context ──────────────────────────────────────
def lt_agent(**comp):
    base = {"threshold_tokens": 80000, "context_length": 200000, "summary_target_ratio": 0.5,
            "tail_mode": "legacy", "max_tokens": 4096}
    base.update(comp)
    return attr({"context_compressor": cc(base), "model": "main-model", "provider": "nous"})

LT_IO = {"effective_threshold_percent": 0.4, "compute_threshold_tokens": 70000,
         "_emit_feasibility_notice": None, "logger.warning": None}
case("_lower_threshold_to_aux_context", aux_model="aux", aux_context=32000, aux_provider="auto",
     aux_base_url="http://Aux.Example:8080/v1", state={"agent": lt_agent()}, io=dict(LT_IO))
case("_lower_threshold_to_aux_context", aux_model="aux", aux_context=32000, aux_provider="openrouter",
     aux_base_url="http://x/v1", state={"agent": lt_agent()}, io=dict(LT_IO))
case("_lower_threshold_to_aux_context", aux_model="aux", aux_context=32000, aux_provider="",
     aux_base_url="http://Host.Example/v1", state={"agent": lt_agent(tail_mode="lean")}, io=dict(LT_IO))
case("_lower_threshold_to_aux_context", aux_model="aux", aux_context=32000, aux_provider="",
     aux_base_url="http://h/v1", state={"agent": lt_agent(context_length=0)}, io=dict(LT_IO))  # main_ctx 0
case("_lower_threshold_to_aux_context", aux_model="aux", aux_context=32000, aux_provider="",
     aux_base_url="http://h/v1", state={"agent": lt_agent()},
     io=dict(LT_IO, compute_threshold_tokens=90000))  # not viable branch
case("_lower_threshold_to_aux_context", aux_model="aux", aux_context=32000, aux_provider="",
     aux_base_url="http://[bad", state={"agent": lt_agent()}, io=dict(LT_IO))  # urlparse ValueError -> except
case("_lower_threshold_to_aux_context", aux_model="aux", aux_context=32000, aux_provider="",
     aux_base_url="", state={"agent": lt_agent(summary_target_ratio=None)}, io=dict(LT_IO))
case("_lower_threshold_to_aux_context", aux_model="aux", aux_context=32000, aux_provider="",
     aux_base_url="http://h/v1", state={"agent": lt_agent()},
     io=dict(LT_IO, _emit_feasibility_notice=rs("KeyboardInterrupt", "stop")))  # BaseException

# ── 14. _configured_custom_providers ─────────────────────────────────────────
case("_configured_custom_providers", state={"agent": attr({"_custom_providers": [{"name": "p1"}]})},
     io={"session_auxiliary_get": "sid", "load_config_readonly": {}, "get_compatible_custom_providers": []})
case("_configured_custom_providers", state={"agent": attr({"_custom_providers": [{"name": "p1"}]})},
     io={"session_auxiliary_get": None, "load_config_readonly": {"a": 1},
         "get_compatible_custom_providers": [{"name": "live"}]})
case("_configured_custom_providers", state={"agent": attr({"_custom_providers": [{"name": "p1"}]})},
     io={"session_auxiliary_get": None, "load_config_readonly": rs("RuntimeError", "cfg boom"),
         "logger.debug": None})  # except -> fallback
case("_configured_custom_providers", state={"agent": attr({})},
     io={"session_auxiliary_get": None, "load_config_readonly": {},
         "get_compatible_custom_providers": rs("ValueError", "bad")})  # except -> getattr None
case("_configured_custom_providers", state={"agent": attr({"_custom_providers": []})},
     io={"session_auxiliary_get": None, "load_config_readonly": {},
         "get_compatible_custom_providers": rs("SystemExit", "bye")})  # BaseException propagates
case("_configured_custom_providers", state={"agent": attr({"_custom_providers": [{"name": "p1"}]})},
     io={"session_auxiliary_get": None, "get_compatible_custom_providers": []})  # fail-closed load_config

# ── 15. _hold_attempt_telemetry ──────────────────────────────────────────────
FIELDS = ("_last_compression_telemetry", "_active_compression_telemetry", "_compression_telemetry_seed")
case("_hold_attempt_telemetry",
     state={"compressor": attr({FIELDS[0]: {"a": 1}, FIELDS[1]: {"b": 2}, FIELDS[2]: None})},
     reset={FIELDS[0]: None, FIELDS[1]: None})  # restored after reset
case("_hold_attempt_telemetry",
     state={"compressor": attr({FIELDS[0]: "x", FIELDS[1]: "y", FIELDS[2]: "z"})}, reset={})
case("_hold_attempt_telemetry", state={"compressor": attr({})},
     reset={FIELDS[0]: {"new": 1}})  # held None -> restored to None
case("_hold_attempt_telemetry",
     state={"compressor": attr({FIELDS[0]: {"a": 1}, FIELDS[1]: {"b": 2}, FIELDS[2]: {"c": 3}})},
     reset={FIELDS[0]: "reset", FIELDS[1]: "reset", FIELDS[2]: "reset"})
case("_hold_attempt_telemetry",
     state={"compressor": attr({FIELDS[0]: 1, FIELDS[1]: 2, FIELDS[2]: 3})},
     reset={FIELDS[0]: 9})
case("_hold_attempt_telemetry",
     state={"compressor": attr({FIELDS[0]: {"a": 1}, FIELDS[1]: None, FIELDS[2]: None})},
     reset={FIELDS[1]: {"b": 2}})

# ── 16. _fence_gated_worker ──────────────────────────────────────────────────
MSGS = [{"role": "user", "content": "hi"}]
case("_fence_gated_worker", worker_fence=fence({}), worker=call("worker"), messages=MSGS,
     io={"fence_deadline_exceeded": False, "fence_is_cancelled": False, "worker": [MSGS, "summary"]})
case("_fence_gated_worker", worker_fence=fence({}), worker=call("worker"), messages=MSGS,
     io={"fence_deadline_exceeded": True, "fence_is_cancelled": False, "worker": [MSGS, "s"]})  # TimeoutError
case("_fence_gated_worker", worker_fence=fence({}), worker=call("worker"), messages=MSGS,
     io={"fence_deadline_exceeded": False, "fence_is_cancelled": True, "logger.info": None})  # cancelled
case("_fence_gated_worker", worker_fence=fence({}), worker=call("worker"), messages=MSGS,
     io={"fence_deadline_exceeded": False, "fence_is_cancelled": False,
         "worker": rs("RuntimeError", "worker boom")})
case("_fence_gated_worker", worker_fence=fence({}), worker=call("worker"), messages=MSGS,
     io={"fence_deadline_exceeded": False, "fence_is_cancelled": False,
         "worker": rs("SystemExit", "bye")})
case("_fence_gated_worker", worker_fence=fence({}), worker=call("worker"), messages=MSGS,
     io={"fence_is_cancelled": False, "worker": [MSGS, "s"]})  # fail-closed deadline_exceeded

# ── 17. _is_unchanged_snapshot ───────────────────────────────────────────────
case("_is_unchanged_snapshot", result=[{"__ref__": "m1"}, "extra"], messages=MSGS, messages_id="m1")
case("_is_unchanged_snapshot", result=[{"role": "user"}], messages=MSGS, messages_id="m1")  # different
case("_is_unchanged_snapshot", result=[], messages=MSGS, messages_id="m1")  # IndexError
case("_is_unchanged_snapshot", result=None, messages=MSGS, messages_id="m1")  # TypeError
case("_is_unchanged_snapshot", result=[{"__ref__": "m1"}], messages=MSGS, messages_id="m1")
case("_is_unchanged_snapshot", result=5, messages=MSGS, messages_id="m1")  # TypeError int

# ── 18. _compression_cancel_requested ────────────────────────────────────────
case("_compression_cancel_requested", hard_cancel_event=attr({"is_set": call("event_is_set")}),
     commit_fence=fence({}), io={"event_is_set": True, "fence_is_cancelled": False})
case("_compression_cancel_requested", hard_cancel_event=attr({"is_set": call("event_is_set")}),
     commit_fence=fence({}), io={"event_is_set": False, "fence_is_cancelled": True})
case("_compression_cancel_requested", hard_cancel_event=None, commit_fence=fence({}),
     io={"event_is_set": True, "fence_is_cancelled": False})
case("_compression_cancel_requested", hard_cancel_event=None, commit_fence=None,
     io={"event_is_set": True, "fence_is_cancelled": True})  # both None -> False, no seams
case("_compression_cancel_requested", hard_cancel_event=attr({"is_set": call("event_is_set")}),
     commit_fence=None, io={"event_is_set": False, "fence_is_cancelled": True})
case("_compression_cancel_requested", hard_cancel_event=attr({"is_set": call("event_is_set")}),
     commit_fence=fence({}), io={"event_is_set": rs("KeyboardInterrupt", "stop")})

# ── 19. _stop_heartbeat ──────────────────────────────────────────────────────
case("_stop_heartbeat", desc="context compression completed",
     state={"heartbeat": attr({"stop": call("heartbeat_stop")})}, io={"heartbeat_stop": None})
case("_stop_heartbeat", desc="d", state={"heartbeat": None}, io={"heartbeat_stop": None})  # None -> no seam
case("_stop_heartbeat", desc="d", state={"heartbeat": attr({"stop": call("heartbeat_stop")})},
     io={"heartbeat_stop": rs("RuntimeError", "stop boom")})  # raise -> heartbeat NOT cleared
case("_stop_heartbeat", desc="d", state={"heartbeat": attr({"stop": call("heartbeat_stop")})},
     io={"heartbeat_stop": rs("SystemExit", "bye")})
case("_stop_heartbeat", desc="d", state={"heartbeat": attr({"stop": call("heartbeat_stop")})},
     io={})  # fail-closed
case("_stop_heartbeat", desc="context compression completed",
     state={"heartbeat": attr({"stop": call("heartbeat_stop")})}, io={"heartbeat_stop": None})

# ── 20. _resolve_fallback_prompt ─────────────────────────────────────────────
case("_resolve_fallback_prompt", system_prompt_fallback=call("system_prompt_fallback"),
     io={"system_prompt_fallback": "fallback prompt"})
case("_resolve_fallback_prompt", system_prompt_fallback="static prompt",
     io={"system_prompt_fallback": "x"})  # not callable -> passthrough, no seam
case("_resolve_fallback_prompt", system_prompt_fallback=call("system_prompt_fallback"),
     io={"system_prompt_fallback": ""})
case("_resolve_fallback_prompt", system_prompt_fallback=call("system_prompt_fallback"),
     io={"system_prompt_fallback": rs("RuntimeError", "prompt boom")})
case("_resolve_fallback_prompt", system_prompt_fallback=call("system_prompt_fallback"),
     io={"system_prompt_fallback": rs("GeneratorExit", "")})
case("_resolve_fallback_prompt", system_prompt_fallback=call("system_prompt_fallback"),
     io={})  # fail-closed


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    print("wrote %d cases to %s" % (len(CASES), OUT))
    for fn in sorted(counts):
        flag = "" if counts[fn] >= 5 else "  (<5!)"
        print("  %-40s %d%s" % (fn, counts[fn], flag))
    assert all(v >= 5 for v in counts.values()), "a function has <5 cases"
    assert len(counts) == 20, "expected 20 functions, got %d" % len(counts)


if __name__ == "__main__":
    main()