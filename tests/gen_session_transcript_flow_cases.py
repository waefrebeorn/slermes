#!/usr/bin/env python3
"""Deterministic fixture generator for the session_transcript_flow oracle.

Writes tests/fixtures/session_transcript_flow.jsonl — >=5 cases per ported
function (19 functions), covering: normal paths, falsy/None gates, scripted
Exception catches, BaseException passthrough at every except site,
AttributeError fidelity (store handles / row doubles / missing attrs),
cycle guards, head-identity migration, cap eviction, spool-failure ordering,
fail-closed TranscriptReadError, and the suppress(Exception) tip heal.

Fixture domain: ASCII; integral numbers written as integer literals (the
int-vs-float distinction is observable in _bindable_text); session ids are
strings; store handles are string ids registered in state.stores.
"""
import json
import os
from collections import Counter

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "session_transcript_flow.jsonl")

CASES = []


def case(func, **inp):
    CASES.append({"func": func, "in": inp})


def st(dirty=None, failures=None, reroutes=None, spooled=None, hints=None,
       entries=None, stores=None):
    s = {"dirty_transcripts": dirty or {}, "append_failures": failures or {},
         "entries": entries or {}, "stores": stores or {"st1": {}}}
    if reroutes is not None:
        s["reroutes"] = reroutes
    if spooled is not None:
        s["spooled_drop_sessions"] = spooled
    if hints is not None:
        s["session_owner_hints"] = hints
    return s


def raise_(kind, message="boom", **extra):
    d = {"__raise__": kind, "message": message}
    d.update(extra)
    return d


M1 = {"role": "user", "content": "one"}
M2 = {"role": "assistant", "content": "two"}
M3 = {"role": "user", "content": "three"}

# ── TranscriptReadError.__init__ ────────────────────────────────────────────
case("TranscriptReadError", session_id="s1")
case("TranscriptReadError", session_id="")
case("TranscriptReadError", session_id="a b")
case("TranscriptReadError", session_id=None)
case("TranscriptReadError", session_id="s-99/x")

# ── _spool_dropped ──────────────────────────────────────────────────────────
case("_spool_dropped", session_id="s1", message=M1,
     io={"spool_dropped_transcript_message": "C:/spool/s1.jsonl"})
case("_spool_dropped", session_id="s1", message=M1,
     io={"spool_dropped_transcript_message": None})
case("_spool_dropped", session_id="s1", message=M1,
     io={"spool_dropped_transcript_message": raise_("RuntimeError")})
case("_spool_dropped", session_id="s1", message=M1,
     io={"spool_dropped_transcript_message": raise_("ValueError", "bad")})
case("_spool_dropped", session_id="s1", message=M1,
     io={"spool_dropped_transcript_message": raise_("KeyboardInterrupt", "stop")})
case("_spool_dropped", session_id="s1", message=M1,
     io={"spool_dropped_transcript_message": raise_("SystemExit", "bye")})
case("_spool_dropped", session_id="s1", message=M1,
     io={"spool_dropped_transcript_message": ""})

# ── _bindable_text ──────────────────────────────────────────────────────────
case("_bindable_text", value=None)
case("_bindable_text", value="hello")
case("_bindable_text", value=42)
case("_bindable_text", value=-(1 << 63))
case("_bindable_text", value=(1 << 63) - 1024)   # in-range, double-exact
case("_bindable_text", value=(1 << 63))          # 2**63 -> dumps
case("_bindable_text", value=(1 << 63) + 2048)   # out-of-range, double-exact
case("_bindable_text", value=-((1 << 63) + 2048))
case("_bindable_text", value=1.5)
case("_bindable_text", value=True)
case("_bindable_text", value={"a": 1, "b": [1, 2.5, "x"]})
case("_bindable_text", value=[1, "two", None])
case("_bindable_text", value={"q": 'he said "hi"', "p": "C:\\x"})
case("_bindable_text", value="line\nbreak\ttab")
case("_bindable_text", value={"n": {"deep": [True, False, None]}})

# ── transcript_append_kwargs ────────────────────────────────────────────────
case("transcript_append_kwargs", session_id="s1", message={})
case("transcript_append_kwargs", session_id="s1",
     message={"role": "assistant", "content": "hi", "reasoning": "r",
              "reasoning_content": "rc", "reasoning_details": [{"t": 1}],
              "codex_reasoning_items": ["a"], "codex_message_items": ["b"],
              "tool_name": "t", "tool_call_id": "tc", "tool_calls": [{"id": "x"}],
              "api_content": "raw", "display_kind": "internal_notification",
              "display_metadata": {"k": "v"}},
     fallback_ts=123)
case("transcript_append_kwargs", session_id="s1",
     message={"role": "user", "reasoning": "r", "reasoning_content": "rc",
              "reasoning_details": [1], "codex_reasoning_items": ["a"],
              "codex_message_items": ["b"]})
case("transcript_append_kwargs", session_id="s1",
     message={"role": "user", "timestamp": 0}, fallback_ts=999)
case("transcript_append_kwargs", session_id="s1",
     message={"role": "user", "timestamp": None}, fallback_ts=5)
case("transcript_append_kwargs", session_id="s1",
     message={"role": "user", "platform_message_id": "", "message_id": "m1"})
case("transcript_append_kwargs", session_id="s1",
     message={"role": "user", "platform_message_id": "p1", "message_id": "m1"})
case("transcript_append_kwargs", session_id="s1",
     message={"role": {"x": 1}, "tool_name": [1], "tool_call_id": (1 << 63),
              "reasoning": {"a": 1}, "reasoning_content": "ok",
              "platform_message_id": {"p": 2}})
case("transcript_append_kwargs", session_id="s1",
     message={"role": "user", "api_content": 5, "observed": "yes"})
case("transcript_append_kwargs", session_id="s1",
     message={"role": "user", "observed": 0})
case("transcript_append_kwargs", session_id="s1", message=None)
case("transcript_append_kwargs", session_id="s1", message="x")
case("transcript_append_kwargs", session_id="s1",
     message={"role": "assistant", "timestamp": 7, "api_content": "keep"},
     fallback_ts=123)

# ── _follow_reroutes ────────────────────────────────────────────────────────
case("_follow_reroutes", session_id="a", state=st())
case("_follow_reroutes", session_id="a",
     state=st(reroutes={"a": "b", "b": "c"}))
case("_follow_reroutes", session_id="a",
     state=st(reroutes={"a": "b", "b": "a"}))
case("_follow_reroutes", session_id="a", state=st(reroutes={"a": "a"}))
case("_follow_reroutes", session_id="z", state=st(reroutes={}))
case("_follow_reroutes", session_id=None, state=st(reroutes={"a": "b"}))
case("_follow_reroutes", session_id="a", state=st(reroutes={"a": ""}))

# ── _enqueue_transcript_message ─────────────────────────────────────────────
case("_enqueue_transcript_message", session_id="s1", message=M1, state=st())
case("_enqueue_transcript_message", session_id="s1", message=M3, cap=2,
     state=st(dirty={"s1": [M1, M2]}),
     io={"spool_dropped_transcript_message": "C:/spool/old.jsonl"})
case("_enqueue_transcript_message", session_id="s1", message=M3, cap=2,
     state=st(dirty={"s1": [M1, M2]}),
     io={"spool_dropped_transcript_message": None})
case("_enqueue_transcript_message", session_id="s1", message=M3, cap=2,
     state=st(dirty={"s1": [M1, M2]}),
     io={"spool_dropped_transcript_message": raise_("RuntimeError")})
case("_enqueue_transcript_message", session_id="s1", message=M3, cap=2,
     state=st(dirty={"s1": [M1, M2]}),
     io={"spool_dropped_transcript_message": raise_("KeyboardInterrupt", "ci")})
case("_enqueue_transcript_message", session_id="s1", message=M1, cap=1,
     state=st())
case("_enqueue_transcript_message", session_id="s1", message=M3, cap=2,
     state=st(dirty={"s1": [M1, M2], "s2": [M1]}),
     io={"spool_dropped_transcript_message": ""})
case("_enqueue_transcript_message", session_id="s1", message=M2, cap=2,
     state=st(dirty={"s1": [M1]}, spooled=["s1"]),
     io={"spool_dropped_transcript_message": "C:/spool/x"})

# ── _divert_transcript_after_db_replaced ────────────────────────────────────
DIV_EXC = raise_("StoreReplaced", "db replaced under us")
case("_divert_transcript_after_db_replaced", session_id="s1",
     queue_session_id="s1", exc=DIV_EXC,
     state=st(dirty={"s1": [M1, M2]}, failures={"s1": 4}),
     io={"spool_dropped_transcript_message": "C:/spool/p",
          "get_hermes_home": "C:/h",
          "divert_session_transcript_jsonl": None})
case("_divert_transcript_after_db_replaced", session_id="s1",
     queue_session_id="q1", exc=DIV_EXC,
     state=st(dirty={"q1": [M1, M2]}, failures={"s1": 2, "q1": 9}),
     io={"spool_dropped_transcript_message": [None, "C:/spool/p"],
          "get_hermes_home": "C:/h",
          "divert_session_transcript_jsonl": None})
case("_divert_transcript_after_db_replaced", session_id="s1",
     queue_session_id="s1", exc=DIV_EXC,
     state=st(dirty={"s1": [M1]}),
     io={"spool_dropped_transcript_message": raise_("RuntimeError"),
          "get_hermes_home": "C:/h",
          "divert_session_transcript_jsonl": None})
case("_divert_transcript_after_db_replaced", session_id="s1",
     queue_session_id="s1", exc=DIV_EXC,
     state=st(dirty={"s1": [M1, M2]}),
     io={"spool_dropped_transcript_message": raise_("KeyboardInterrupt", "ci")})
case("_divert_transcript_after_db_replaced", session_id="s1",
     queue_session_id="s1", exc=DIV_EXC,
     state=st(dirty={"s1": [M1]}),
     io={"spool_dropped_transcript_message": "C:/spool/p",
          "get_hermes_home": raise_("ValueError", "no home"),
          "divert_session_transcript_jsonl": None})
case("_divert_transcript_after_db_replaced", session_id="s1",
     queue_session_id="s1", exc=DIV_EXC,
     state=st(dirty={"s1": [M1]}),
     io={"spool_dropped_transcript_message": "C:/spool/p",
          "get_hermes_home": "C:/h",
          "divert_session_transcript_jsonl": raise_("RuntimeError", "disk full")})
case("_divert_transcript_after_db_replaced", session_id="s1",
     queue_session_id="s1", exc=DIV_EXC,
     state=st(dirty={"s1": [M1]}),
     io={"spool_dropped_transcript_message": "C:/spool/p",
          "get_hermes_home": None})
case("_divert_transcript_after_db_replaced", session_id="s1",
     queue_session_id="s1", exc=DIV_EXC,
     state=st(dirty={"s1": [M1]}),
     io={"spool_dropped_transcript_message": "C:/spool/p",
          "get_hermes_home": ""})
case("_divert_transcript_after_db_replaced", session_id="s1",
     queue_session_id="s1", exc=raise_("StoreQuarantined", "quarantined"),
     state=st(),
     io={"get_hermes_home": "C:/h",
          "divert_session_transcript_jsonl": None})
case("_divert_transcript_after_db_replaced", session_id="s1",
     queue_session_id="s1", exc=DIV_EXC,
     state=st(dirty={"s1": [M1]}),
     io={"spool_dropped_transcript_message": "C:/spool/p",
          "get_hermes_home": "C:/h/",
          "divert_session_transcript_jsonl": raise_("SystemExit", "bye")})

# ── _live_compression_child ─────────────────────────────────────────────────
case("_live_compression_child", session_id="s1", state=st(),
     io={"persistence_for_session_id": None})
case("_live_compression_child", session_id="s1", state=st(),
     io={"persistence_for_session_id": "st1", "lineage.tip": None})
case("_live_compression_child", session_id="s1", state=st(),
     io={"persistence_for_session_id": "st1", "lineage.tip": "s1"})
case("_live_compression_child", session_id="s1", state=st(),
     io={"persistence_for_session_id": "st1", "lineage.tip": "child",
          "sessions.get": {"ended_at": None}})
case("_live_compression_child", session_id="s1", state=st(),
     io={"persistence_for_session_id": "st1", "lineage.tip": "child",
          "sessions.get": {"ended_at": 5}})
case("_live_compression_child", session_id="s1", state=st(),
     io={"persistence_for_session_id": "st1", "lineage.tip": "child",
          "sessions.get": None})
case("_live_compression_child", session_id="s1", state=st(),
     io={"persistence_for_session_id": ""})
case("_live_compression_child", session_id="s1", state=st(),
     io={"persistence_for_session_id": "st1", "lineage.tip": "child",
          "sessions.get": {}})
case("_live_compression_child", session_id="s1", state=st(),
     io={"persistence_for_session_id": "st1", "lineage.tip": "child",
          "sessions.get": "x"})
case("_live_compression_child", session_id="s1", state=st(),
     io={"persistence_for_session_id": raise_("RuntimeError", "no store")})
case("_live_compression_child", session_id="s1", state=st(),
     io={"persistence_for_session_id": "st1", "lineage.tip": 5,
          "sessions.get": {"ended_at": None}})
case("_live_compression_child", session_id="s1", state=st(),
     io={"persistence_for_session_id": "st1",
          "lineage.tip": raise_("KeyboardInterrupt", "ci")})

# ── _migrate_transcript_queue_to_child ──────────────────────────────────────
case("_migrate_transcript_queue_to_child", session_id="s1",
     queue_session_id="q1", child_id="c1", pending=[M1, M2], msg=M1,
     msg_is_head=True, state=st(reroutes={}))
case("_migrate_transcript_queue_to_child", session_id="s1",
     queue_session_id="q1", child_id="c1", pending=[M1, M2], msg=M3,
     msg_is_head=False, state=st(reroutes={}))
case("_migrate_transcript_queue_to_child", session_id="s1",
     queue_session_id="q1", child_id="c1", pending=[M1, M2], msg=M1,
     msg_is_head=True, state=st(dirty={"c1": [M3]}, reroutes={}))
case("_migrate_transcript_queue_to_child", session_id="s1",
     queue_session_id="q1", child_id="c1", pending=[M1], msg=M1,
     msg_is_head=True, state=st(dirty={"c1": [M3]}, reroutes={}))
case("_migrate_transcript_queue_to_child", session_id="s1",
     queue_session_id="q1", child_id="c1", pending=[M1], msg=M1,
     msg_is_head=True, state=st(reroutes={}))
case("_migrate_transcript_queue_to_child", session_id="s1",
     queue_session_id="q1", child_id="c1", pending=[M1], msg=M1,
     msg_is_head=True,
     state=st(failures={"q1": 3, "c1": 1}, reroutes={}))
case("_migrate_transcript_queue_to_child", session_id="s1",
     queue_session_id="q1", child_id="c1", pending=[M1], msg=M1,
     msg_is_head=True,
     state=st(failures={"q1": 0, "c1": 2}, reroutes={}))
case("_migrate_transcript_queue_to_child", session_id="s1",
     queue_session_id="q1", child_id="c1", pending=[M1], msg=M1,
     msg_is_head=True,
     state=st(failures={"q1": 2, "c1": 5}, reroutes={}))
case("_migrate_transcript_queue_to_child", session_id="s1",
     queue_session_id="q1", child_id="c1", pending=[M1], msg=M1,
     msg_is_head=True, state=st())   # reroutes ABSENT -> AttributeError
case("_migrate_transcript_queue_to_child", session_id="s1",
     queue_session_id="q1", child_id="q1", pending=[M1, M2], msg=M1,
     msg_is_head=True, state=st(reroutes={}))
case("_migrate_transcript_queue_to_child", session_id="s1",
     queue_session_id="q1", child_id="c1", pending=[], msg=M1,
     msg_is_head=True, state=st(reroutes={}))

# ── _publish_transcript_reroute ─────────────────────────────────────────────
case("_publish_transcript_reroute", session_id="s1", child_id="c1",
     state=st(entries={"k1": {"session_id": "s1"}, "k2": {"session_id": "other"},
                        "k3": {"session_id": "s1"}},
              hints={"c1": "ownerA", "z": "ownerB"}),
     io={"save": None})
case("_publish_transcript_reroute", session_id="s1", child_id="c1",
     state=st(entries={"k1": {"session_id": "s1"}}), io={"save": None})
case("_publish_transcript_reroute", session_id="s1", child_id="c1",
     state=st(entries={"k1": {"session_id": "s1"}}, hints={}),
     io={"save": None})
case("_publish_transcript_reroute", session_id="s1", child_id="c1",
     state=st(entries={"k1": {"session_id": "s1"}}, hints={"c1": "o"}),
     io={"save": None})
case("_publish_transcript_reroute", session_id="s1", child_id="c1",
     state=st(entries={"k1": {"session_id": "s1"}}),
     io={"save": raise_("RuntimeError", "disk")})
case("_publish_transcript_reroute", session_id="s1", child_id="c1",
     state=st(entries={"k1": {"session_id": "s1"}}),
     io={"save": raise_("SystemExit", "bye")})
case("_publish_transcript_reroute", session_id="s1", child_id="c1",
     state=st(entries={"k1": {}, "k2": {"session_id": "s1"}}),
     io={"save": None})
case("_publish_transcript_reroute", session_id="s1", child_id="c1",
     state=st(entries={"k1": {"session_id": "other"}}), io={"save": None})

# ── _spool_stalled_backlog ──────────────────────────────────────────────────
case("_spool_stalled_backlog", session_id="s1", queue_session_id="s1",
     state=st(dirty={"s1": [M1, M2, M3]}),
     io={"spool_dropped_transcript_message": "C:/spool/p"})
case("_spool_stalled_backlog", session_id="s1", queue_session_id="s1",
     state=st(dirty={"s1": [M1, M2, M3]}),
     io={"spool_dropped_transcript_message": ["C:/spool/p", None, "C:/spool/p"]})
case("_spool_stalled_backlog", session_id="s1", queue_session_id="s1",
     state=st(dirty={"s1": [M1]}),
     io={"spool_dropped_transcript_message": raise_("RuntimeError")})
case("_spool_stalled_backlog", session_id="s1", queue_session_id="s1",
     state=st(dirty={"s1": [M1, M2]}),
     io={"spool_dropped_transcript_message": raise_("KeyboardInterrupt", "ci")})
case("_spool_stalled_backlog", session_id="s1", queue_session_id="s1",
     state=st())
case("_spool_stalled_backlog", session_id="s1", queue_session_id="s1",
     state=st(dirty={"s1": [M1, M2]}),
     io={"spool_dropped_transcript_message": ["", "C:/spool/p"]})
case("_spool_stalled_backlog", session_id="s1", queue_session_id="s1",
     state=st(dirty={"s1": [M1, M2, M3]}, spooled=["s1"]),
     io={"spool_dropped_transcript_message": ["C:/spool/p", "C:/spool/p", None]})

# ── mark_spooled_drop_sessions ──────────────────────────────────────────────
case("mark_spooled_drop_sessions", session_ids=None, state=st())
case("mark_spooled_drop_sessions", session_ids=[], state=st())
case("mark_spooled_drop_sessions", session_ids=["a", "b"], state=st())
case("mark_spooled_drop_sessions", session_ids=["a", "b"],
     state=st(spooled=["a"]))
case("mark_spooled_drop_sessions", session_ids=["c"], state=st(spooled=[]))
case("mark_spooled_drop_sessions", session_ids="", state=st())

# ── has_platform_message_id ─────────────────────────────────────────────────
case("has_platform_message_id", session_id="s1", platform_message_id="p1",
     state=st(), io={"persistence_for_session_id": None})
case("has_platform_message_id", session_id="s1", platform_message_id="p1",
     state=st(), io={"persistence_for_session_id": ""})
case("has_platform_message_id", session_id="s1", platform_message_id="p1",
     state=st(), io={"persistence_for_session_id": "st1",
                     "messages.has_platform_message_id": True})
case("has_platform_message_id", session_id="s1", platform_message_id="p1",
     state=st(), io={"persistence_for_session_id": "st1",
                     "messages.has_platform_message_id": False})
case("has_platform_message_id", session_id="s1", platform_message_id="p1",
     state=st(), io={"persistence_for_session_id": "st1",
                     "messages.has_platform_message_id": raise_("RuntimeError")})
case("has_platform_message_id", session_id="s1", platform_message_id="p1",
     state=st(), io={"persistence_for_session_id": "st1",
                     "messages.has_platform_message_id": raise_("KeyboardInterrupt", "ci")})
case("has_platform_message_id", session_id="s1", platform_message_id="p1",
     state=st(), io={"persistence_for_session_id": "unknown"})
case("has_platform_message_id", session_id="s1", platform_message_id="p1",
     state=st(), io={"persistence_for_session_id": raise_("ValueError", "scope")})

# ── transcript_tail_role ────────────────────────────────────────────────────
case("transcript_tail_role", session_id="a",
     state=st(reroutes={"a": "b"}),
     io={"compression_tip_for_session_id": "b",
          "persistence_for_session_id": "st1",
          "messages.latest_conversation_role": "user"})
case("transcript_tail_role", session_id="a", state=st(),
     io={"compression_tip_for_session_id": None,
          "persistence_for_session_id": None})
case("transcript_tail_role", session_id="a", state=st(),
     io={"compression_tip_for_session_id": "a",
          "persistence_for_session_id": ""})
case("transcript_tail_role", session_id="a", state=st(),
     io={"compression_tip_for_session_id": "a",
          "persistence_for_session_id": "st1",
          "messages.latest_conversation_role": raise_("RuntimeError")})
case("transcript_tail_role", session_id="a", state=st(),
     io={"compression_tip_for_session_id": "a",
          "persistence_for_session_id": "st1",
          "messages.latest_conversation_role": raise_("SystemExit", "bye")})
case("transcript_tail_role", session_id="a", state=st(),
     io={"compression_tip_for_session_id": raise_("ValueError", "tip")})
case("transcript_tail_role", session_id="a", state=st(),
     io={"compression_tip_for_session_id": "a",
          "persistence_for_session_id": "unknown"})
case("transcript_tail_role", session_id="a", state=st(),
     io={"compression_tip_for_session_id": "a",
          "persistence_for_session_id": "st1",
          "messages.latest_conversation_role": None})

# ── is_published_compression_child ──────────────────────────────────────────
PUB_IO = {"persistence_for_session_id": "st1",
          "sessions.get": {"parent_session_id": "p"},
          "lineage.is_compression_child": True,
          "messages.active_watermark": {"last_active": 5}}
case("is_published_compression_child", parent_session_id="", child_session_id="c",
     state=st())
case("is_published_compression_child", parent_session_id="p", child_session_id="",
     state=st())
case("is_published_compression_child", parent_session_id="p", child_session_id="c",
     state=st(), io={"persistence_for_session_id": None})
case("is_published_compression_child", parent_session_id="p", child_session_id="c",
     state=st(), io=PUB_IO)
case("is_published_compression_child", parent_session_id="p", child_session_id="c",
     state=st(), io=dict(PUB_IO, **{"messages.active_watermark": {"last_active": None}}))
case("is_published_compression_child", parent_session_id="p", child_session_id="c",
     state=st(), io=dict(PUB_IO, **{"sessions.get": {"parent_session_id": "other"}}))
case("is_published_compression_child", parent_session_id="p", child_session_id="c",
     state=st(), io=dict(PUB_IO, **{"sessions.get": None}))
case("is_published_compression_child", parent_session_id="p", child_session_id="c",
     state=st(), io=dict(PUB_IO, **{"sessions.get": {}}))
case("is_published_compression_child", parent_session_id="p", child_session_id="c",
     state=st(), io=dict(PUB_IO, **{"lineage.is_compression_child": False}))
case("is_published_compression_child", parent_session_id="p", child_session_id="c",
     state=st(), io={"persistence_for_session_id": ""})
case("is_published_compression_child", parent_session_id="p", child_session_id="c",
     state=st(), io=dict(PUB_IO, **{"messages.active_watermark": None}))
case("is_published_compression_child", parent_session_id="p", child_session_id="c",
     state=st(), io=dict(PUB_IO, **{"messages.active_watermark": {}}))
case("is_published_compression_child", parent_session_id="p", child_session_id="c",
     state=st(), io=dict(PUB_IO, **{"sessions.get": {"x": 1}}))
case("is_published_compression_child", parent_session_id="p", child_session_id="c",
     state=st(), io=dict(PUB_IO, **{"sessions.get": raise_("RuntimeError")}))
case("is_published_compression_child", parent_session_id="p", child_session_id="c",
     state=st(), io=dict(PUB_IO, **{"messages.active_watermark": raise_("KeyboardInterrupt", "ci")}))

# ── persist_rotated_compression_child ───────────────────────────────────────
case("persist_rotated_compression_child", parent_session_id="p",
     child_session_id="c", messages=[M1], state=st(), io=PUB_IO)
case("persist_rotated_compression_child", parent_session_id="p",
     child_session_id="c", messages=[M1], state=st(),
     io={"persistence_for_session_id": None, "rewrite_transcript": True})
case("persist_rotated_compression_child", parent_session_id="p",
     child_session_id="c", messages=[M1], state=st(),
     io={"persistence_for_session_id": None, "rewrite_transcript": False})
case("persist_rotated_compression_child", parent_session_id="p",
     child_session_id="c", messages=[M1], state=st(),
     io={"persistence_for_session_id": None,
          "rewrite_transcript": raise_("RuntimeError", "write")})
case("persist_rotated_compression_child", parent_session_id="p",
     child_session_id="c", messages=[M1], state=st(),
     io={"persistence_for_session_id": None,
          "rewrite_transcript": raise_("SystemExit", "bye")})
case("persist_rotated_compression_child", parent_session_id="p",
     child_session_id="c", messages=[M1, M2], state=st(),
     io={"persistence_for_session_id": "st1",
          "sessions.get": {"parent_session_id": "p"},
          "lineage.is_compression_child": True,
          "messages.active_watermark": {"last_active": 0}})

# ── has_input_owner ─────────────────────────────────────────────────────────
HIO = {"persistence_for_session_id": "st1", "lineage.tip": None}
case("has_input_owner", session_id="s1", owner="o1", state=st(),
     io=dict(HIO, **{"messages.has_gateway_input_owner": True}))
case("has_input_owner", session_id="s1", owner="o1", state=st(),
     io=dict(HIO, **{"messages.has_gateway_input_owner": False,
                     "sessions.get": None}))
case("has_input_owner", session_id="s1", owner="o1", state=st(),
     io=dict(HIO, **{"messages.has_gateway_input_owner": [False, True],
                     "sessions.get": {"parent_session_id": "p2"},
                     "lineage.is_compression_child": True}))
case("has_input_owner", session_id="s1", owner="o1", state=st(),
     io=dict(HIO, **{"messages.has_gateway_input_owner": False,
                     "sessions.get": {"parent_session_id": "s1"},
                     "lineage.is_compression_child": True}))
case("has_input_owner", session_id="s1", owner="o1", state=st(),
     io={"persistence_for_session_id": None})
case("has_input_owner", session_id="s1", owner="o1", state=st(),
     io=dict(HIO, **{"messages.has_gateway_input_owner": raise_("RuntimeError")}))
case("has_input_owner", session_id="s1", owner="o1", state=st(),
     io=dict(HIO, **{"messages.has_gateway_input_owner": raise_("KeyboardInterrupt", "ci")}))
case("has_input_owner", session_id="s1", owner="o1", state=st(),
     io=dict(HIO, **{"messages.has_gateway_input_owner": False,
                     "sessions.get": {"x": 1},
                     "lineage.is_compression_child": True}))
case("has_input_owner", session_id="a", owner="o1",
     state=st(reroutes={"a": "b"}),
     io=dict(HIO, **{"messages.has_gateway_input_owner": True}))
case("has_input_owner", session_id="s1", owner="o1", state=st(),
     io=dict(HIO, **{"lineage.tip": "tip1",
                     "messages.has_gateway_input_owner": True}))
case("has_input_owner", session_id="s1", owner="o1", state=st(),
     io=dict(HIO, **{"messages.has_gateway_input_owner": False,
                     "sessions.get": {}}))
case("has_input_owner", session_id="s1", owner="o1", state=st(),
     io=dict(HIO, **{"messages.has_gateway_input_owner": False,
                     "sessions.get": {"parent_session_id": "p2"},
                     "lineage.is_compression_child": False}))
case("has_input_owner", session_id="s1", owner="o1", state=st(),
     io={"persistence_for_session_id": raise_("ValueError", "scope")})

# ── load_transcript ─────────────────────────────────────────────────────────
LIO = {"persistence_for_session_id": "st1", "lineage.tip": None,
       "messages.read_conversation": {"__script__": True,
                                      "ret": [{"role": "user", "content": "a"},
                                              {"role": "assistant", "content": "b"}]},
       "as_openai_message": [{"role": "user", "content": "a"},
                             {"role": "assistant", "content": "b"}]}
case("load_transcript", session_id="s1", state=st(),
     io={"persistence_for_session_id": None})
case("load_transcript", session_id="s1", state=st(), io=LIO)
case("load_transcript", session_id="a", state=st(reroutes={"a": "b"}),
     io=dict(LIO, **{"persistence_for_session_id": ["st1", "st1", "st1"],
                     "lineage.tip": "c"}))
case("load_transcript", session_id="s1", state=st(),
     io=dict(LIO, **{"persistence_for_session_id": ["st1", None, "st1"]}))
case("load_transcript", session_id="s1", state=st(),
     io=dict(LIO, **{"messages.read_conversation": raise_("RuntimeError", "read")}))
case("load_transcript", session_id="s1", state=st(),
     io=dict(LIO, **{"messages.read_conversation": None}))
case("load_transcript", session_id="s1", state=st(),
     io=dict(LIO, **{"as_openai_message": [{"role": "user"},
                                          raise_("ValueError", "bad row")]}))
case("load_transcript", session_id="s1", state=st(),
     io={"persistence_for_session_id": raise_("KeyboardInterrupt", "ci")})
case("load_transcript", session_id="s1", state=st(),
     io=dict(LIO, **{"lineage.tip": raise_("RuntimeError", "tip")}))
case("load_transcript", session_id="s1", state=st(),
     io=dict(LIO, **{"persistence_for_session_id": ["st1", "st1",
                                                    raise_("ValueError", "gone")]}))
case("load_transcript", session_id="s1", state=st(),
     io=dict(LIO, **{"messages.read_conversation": {"__script__": True, "ret": []},
                     "as_openai_message": []}))
case("load_transcript", session_id="s1", state=st(),
     io=dict(LIO, **{"persistence_for_session_id": ["st1",
                                                    raise_("SystemExit", "bye"),
                                                    "st1"]}))
case("load_transcript", session_id="s1", state=st(),
     io=dict(LIO, **{"persistence_for_session_id": ["st1", "unknown", "st1"]}))

# ── retain_refused_turn_for_display ─────────────────────────────────────────
RIO = {"persistence_for_session_id": "st1",
       "get_transcript_drain_lock": None,
       "messages.retain_refused_turn_for_display": {"retained_rows": 1},
       "clear_dirty_transcript": None}
case("retain_refused_turn_for_display", session_id="s1", notice="refused",
     state=st(), io={"persistence_for_session_id": None})
case("retain_refused_turn_for_display", session_id="s1", notice="refused",
     boundary_row_id=7,
     state=st(dirty={"s1": [M1]}, failures={"s1": 2}), io=RIO)
case("retain_refused_turn_for_display", session_id="s1", notice="refused",
     state=st(), io=dict(RIO, **{"messages.retain_refused_turn_for_display": False}))
case("retain_refused_turn_for_display", session_id="s1", notice="refused",
     state=st(), io=dict(RIO, **{"messages.retain_refused_turn_for_display": None}))
case("retain_refused_turn_for_display", session_id="s1", notice="refused",
     state=st(), io=dict(RIO, **{"messages.retain_refused_turn_for_display":
                                 raise_("RuntimeError", "retain")}))
case("retain_refused_turn_for_display", session_id="s1", notice="refused",
     state=st(), io=dict(RIO, **{"messages.retain_refused_turn_for_display":
                                 raise_("KeyboardInterrupt", "ci")}))
case("retain_refused_turn_for_display", session_id="s1", notice="refused",
     state=st(), io={"persistence_for_session_id": "unknown",
                     "get_transcript_drain_lock": None})
case("retain_refused_turn_for_display", session_id="s1", notice="refused",
     state=st(), io={"persistence_for_session_id": "st1",
                     "get_transcript_drain_lock": raise_("ValueError", "lock")})
case("retain_refused_turn_for_display", session_id="s1", notice="refused",
     state=st(dirty={"s1": [M1]}),
     io=dict(RIO, **{"clear_dirty_transcript": raise_("RuntimeError", "clear")}))

# ── extra edge coverage ─────────────────────────────────────────────────────
case("TranscriptReadError", session_id=42)
case("_spool_dropped", session_id="s1", message=M1,
     io={"spool_dropped_transcript_message": raise_("GeneratorExit", "gen")})
case("_spool_dropped", session_id="s1", message=M1,
     io={"spool_dropped_transcript_message": raise_("CancelledError", "cancel")})
case("_bindable_text", value={})
case("_bindable_text", value=[])
case("_bindable_text", value={"f": 0.1, "g": 123456789.25})
case("_follow_reroutes", session_id="a",
     state=st(reroutes={"a": "b", "b": "c", "c": "b"}))
case("_divert_transcript_after_db_replaced", session_id="s1",
     queue_session_id="s1", exc=DIV_EXC,
     state=st(dirty={"s1": [M1]}),
     io={"spool_dropped_transcript_message": "C:/spool/p",
          "get_hermes_home": 5})
case("_live_compression_child", session_id="s1", state=st(),
     io={"persistence_for_session_id": "st1", "lineage.tip": ""})
case("_live_compression_child", session_id="s1", state=st(),
     io={"persistence_for_session_id": "st1", "lineage.tip": "child",
          "sessions.get": raise_("RuntimeError", "row read")})
case("_publish_transcript_reroute", session_id="s1", child_id="c1",
     state=st(entries={"k1": "x"}), io={"save": None})
case("mark_spooled_drop_sessions", session_ids="ab", state=st())
case("has_platform_message_id", session_id="s1", platform_message_id="p1",
     state=st(), io={"persistence_for_session_id": "st1",
                     "messages.has_platform_message_id": "yes"})
case("transcript_tail_role", session_id="a", state=st(),
     io={"compression_tip_for_session_id": "",
          "persistence_for_session_id": None})
case("is_published_compression_child", parent_session_id="p", child_session_id="c",
     state=st(), io=dict(PUB_IO, **{"messages.active_watermark": {"last_active": 0}}))
case("is_published_compression_child", parent_session_id="p", child_session_id="c",
     state=st(), io=dict(PUB_IO, **{"lineage.is_compression_child":
                                    raise_("RuntimeError", "lineage")}))
case("persist_rotated_compression_child", parent_session_id="p",
     child_session_id="c", messages=[M1], state=st(),
     io=dict(PUB_IO, **{"messages.active_watermark": raise_("KeyboardInterrupt", "ci")}))
case("has_input_owner", session_id="s1", owner="o1", state=st(),
     io=dict(HIO, **{"lineage.tip": raise_("RuntimeError", "tip read")}))
case("has_input_owner", session_id="s1", owner="o1", state=st(),
     io=dict(HIO, **{"messages.has_gateway_input_owner": False,
                     "sessions.get": {"parent_session_id": None},
                     "lineage.is_compression_child": True}))
case("load_transcript", session_id="s1", state=st(),
     io=dict(LIO, **{"as_openai_message": [None, {"role": "assistant"}]}))
case("load_transcript", session_id="s1", state=st(),
     io=dict(LIO, **{"lineage.tip": "c",
                     "persistence_for_session_id": ["st1", "st1", "st1"]}))
case("_enqueue_transcript_message", session_id="s1", message=M3, cap=2,
     state=st(dirty={"s1": [M1, M2]}),
     io={"spool_dropped_transcript_message": raise_("SystemExit", "bye")})
case("_migrate_transcript_queue_to_child", session_id="s1",
     queue_session_id="q1", child_id="c1", pending=[M1, M2], msg=M2,
     msg_is_head=False,
     state=st(dirty={"c1": [M3]}, failures={"q1": 4, "c1": 2}, reroutes={}))
case("retain_refused_turn_for_display", session_id="s1", notice=None,
     state=st(), io=dict(RIO, **{"messages.retain_refused_turn_for_display": 1}))


def main():
    counts = Counter(c["func"] for c in CASES)
    expected = [
        "TranscriptReadError", "_spool_dropped", "_bindable_text",
        "transcript_append_kwargs", "_follow_reroutes",
        "_enqueue_transcript_message", "_divert_transcript_after_db_replaced",
        "_live_compression_child", "_migrate_transcript_queue_to_child",
        "_publish_transcript_reroute", "_spool_stalled_backlog",
        "mark_spooled_drop_sessions", "has_platform_message_id",
        "transcript_tail_role", "is_published_compression_child",
        "persist_rotated_compression_child", "has_input_owner",
        "load_transcript", "retain_refused_turn_for_display",
    ]
    assert set(counts) == set(expected), (set(counts) ^ set(expected))
    for fn in expected:
        assert counts[fn] >= 5, "%s has only %d cases" % (fn, counts[fn])
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    print("wrote %s: %d cases across %d functions" %
          (OUT, len(CASES), len(counts)))
    for fn in expected:
        print("  %-42s %d" % (fn, counts[fn]))


if __name__ == "__main__":
    main()