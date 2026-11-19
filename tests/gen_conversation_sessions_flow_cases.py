#!/usr/bin/env python3
"""Generate tests/fixtures/conversation_sessions_flow.jsonl.

Deterministic case corpus for the persistence/conversation/sessions.py FLOW +
leaves port. >=5 cases per ported func. Seam scripts are computed from the LIVE
quarry defs (independent authority) so the scripted sibling results are faithful:
the C universal seam and the oracle's recording stubs consume the SAME scripts.

Run with the quarry venv interpreter:
  .venv/Scripts/python.exe tests/gen_conversation_sessions_flow_cases.py
"""
import json
import sys
import types
from pathlib import Path

_HERE = Path(__file__).resolve()
_REPO = _HERE.parents[1]
QUARRY = _REPO.parent / "hermes-agent"
if not (QUARRY / "persistence/conversation/sessions.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
sys.path.insert(0, str(QUARRY))

from persistence.conversation import sessions as S  # noqa: E402

OUT = _REPO / "tests" / "fixtures" / "conversation_sessions_flow.jsonl"

CASES = []


def add(func, inp, io=None):
    if io:
        inp = dict(inp)
        inp["io"] = io
    CASES.append({"func": func, "in": inp})


def ret(v):
    return {"ret": v}


def raise_spec(t, m=""):
    return {"raise": {"type": t, "message": m}}


# ── a full session-row attribute double template ─────────────────────────────
def row_template(**over):
    r = {
        "session_id": "sess-1", "model_config_json": None,
        "reset_from": None, "branched_from": None,
        "id": "sess-1", "runtime_revision": 0, "runtime_generation": 0,
        "source": "cli", "created_source": None, "user_id": None,
        "session_key": None, "chat_id": None, "chat_type": None, "thread_id": None,
        "display_name": None, "origin_json": None, "expiry_finalized": False,
        "model": None, "model_config": None, "system_prompt": None,
        "system_prompt_hash": None, "parent_session_id": None, "started_at": 100.0,
        "ended_at": None, "end_reason": None, "message_count": 0, "tool_call_count": 0,
        "input_tokens": 0, "output_tokens": 0, "cache_read_tokens": 0,
        "cache_write_tokens": 0, "reasoning_tokens": 0, "cwd": None, "git_branch": None,
        "git_repo_root": None, "git_metadata_generation": 0, "billing_provider": None,
        "billing_base_url": None, "billing_mode": None, "estimated_cost_usd": None,
        "actual_cost_usd": None, "cost_status": None, "cost_source": None,
        "pricing_version": None, "title": None, "title_source": None,
        "last_activity_at": None, "last_activity_description": None,
        "last_activity_provenance": None, "api_call_count": 0, "handoff_state": None,
        "handoff_platform": None, "handoff_error": None,
        "compression_failure_cooldown_until": None, "compression_failure_error": None,
        "compression_fallback_streak": 0, "compression_ineffective_count": 0,
        "compression_recovery_deadline": None, "compression_overload_streak": 0,
        "profile_name": None, "transport_profile": None, "rewind_count": 0,
        "archived": False, "auto_archived": False, "pinned": False, "hidden": False,
        "last_read_at": None, "tool_names": None, "transcript_revision": 0,
        "prompt_cache_clock": None,
    }
    r.update(over)
    return r


def row_double(r):
    d = dict(r)
    ts = d.get("title_source")
    if ts is not None:
        d["title_source"] = S.TitleSource(ts)
    return types.SimpleNamespace(**d)


def live_row_cells(r, compact=False):
    return S.SessionRecord._row_cells(row_double(r), compact=compact)


def live_marker_cells(r):
    return S.SessionRecord._marker_cells(row_double(r))


def live_head_cells(r, compact=False, last_active=None, preview="", extra=()):
    session = row_double(r)
    session._row_cells = types.MethodType(S.SessionRecord._row_cells, session)
    session._marker_cells = types.MethodType(S.SessionRecord._marker_cells, session)
    slf = types.SimpleNamespace(session=session, compact=compact,
                                last_active=last_active, preview=preview)
    return S.SessionListEntry._head_cells(slf, *extra)


# ══ 1. is_automatic_end_reason ══════════════════════════════════════════════
for reason in ["agent_close", "tui_shutdown", "ws_disconnect", "idle_timeout",
               "lru_evict", "compression", "user_close", None, 5, "", "session_reset"]:
    add("is_automatic_end_reason", {"reason": reason})

# ══ 2. is_explicit_close_reason (seams is_automatic_end_reason) ═════════════
for reason in [None, "user_close", "compression", "idle", "session_reset",
               "agent_close", "tui_shutdown", "new_session", "suspended", 7]:
    auto = S.is_automatic_end_reason(reason)
    add("is_explicit_close_reason", {"reason": reason},
        {"is_automatic_end_reason": ret(auto)})
# fail-closed: no script for the sibling
add("is_explicit_close_reason", {"reason": "user_close"}, {})
# BaseException passthrough from the sibling
add("is_explicit_close_reason", {"reason": "user_close"},
    {"is_automatic_end_reason": raise_spec("KeyboardInterrupt", "stop")})

# ══ 3. bound_activity_description ═══════════════════════════════════════════
add("bound_activity_description", {"description": "  hi  "})
add("bound_activity_description", {"description": None})
add("bound_activity_description", {"description": ""})
add("bound_activity_description", {"description": "z" * 120})
add("bound_activity_description", {"description": "y" * 130})
add("bound_activity_description", {"description": "  a\tb\n c  "})
add("bound_activity_description", {"description": "x" * 119 + "é" * 3})

# ══ 4. normalize_activity_provenance ════════════════════════════════════════
for p in [" agent.compression ", "agent.compression_timeout", "unknown", "nope",
          None, "", "agent.compression_cooldown", "agent.compression_turnhold"]:
    add("normalize_activity_provenance", {"provenance": p})

# ══ 5. workspace_key ════════════════════════════════════════════════════════
add("workspace_key", {"row": {"git_repo_root": "  /r  ", "cwd": "/c"}})
add("workspace_key", {"row": {"git_repo_root": "  ", "cwd": " /c "}})
add("workspace_key", {"row": {"git_repo_root": "", "cwd": ""}})
add("workspace_key", {"row": {}})
add("workspace_key", {"row": {"git_repo_root": None, "cwd": None}})
add("workspace_key", {"row": {"cwd": "/only"}})
add("workspace_key", {"row": {"git_repo_root": "/r", "cwd": None}})

# ══ 6. parse_model_config (seams json.loads) ════════════════════════════════
add("parse_model_config", {"raw": '{"a":1}'}, {"json.loads": ret({"a": 1})})
add("parse_model_config", {"raw": "{bad"}, {"json.loads": raise_spec("JSONDecodeError", "Expecting value")})
add("parse_model_config", {"raw": "   "})                       # blank -> no loads
add("parse_model_config", {"raw": "[1,2]"}, {"json.loads": ret([1, 2])})  # not Mapping -> {}
add("parse_model_config", {"raw": {"a": 1}})                    # Mapping -> copy, no loads
add("parse_model_config", {"raw": 5})                           # non-str -> {}
add("parse_model_config", {"raw": None})
add("parse_model_config", {"raw": '"hi"'}, {"json.loads": ret("hi")})     # str result -> {}
add("parse_model_config", {"raw": "x"}, {"json.loads": raise_spec("TypeError", "bad type")})
add("parse_model_config", {"raw": "x"}, {"json.loads": raise_spec("ValueError", "not caught here")})
add("parse_model_config", {"raw": "x"}, {"json.loads": raise_spec("KeyboardInterrupt", "stop")})
add("parse_model_config", {"raw": "x"}, {})                     # fail-closed RuntimeError

# ══ 7. is_corrupt_model_config (seams json.loads) ═══════════════════════════
add("is_corrupt_model_config", {"raw": "{bad"}, {"json.loads": raise_spec("JSONDecodeError", "Expecting value")})
add("is_corrupt_model_config", {"raw": '{"a":1}'}, {"json.loads": ret({"a": 1})})
add("is_corrupt_model_config", {"raw": "[1]"}, {"json.loads": ret([1])})
add("is_corrupt_model_config", {"raw": "  "})
add("is_corrupt_model_config", {"raw": None})
add("is_corrupt_model_config", {"raw": 5})
add("is_corrupt_model_config", {"raw": "x"}, {"json.loads": raise_spec("ValueError", "bad")})
add("is_corrupt_model_config", {"raw": "x"}, {"json.loads": raise_spec("RecursionError", "deep")})
add("is_corrupt_model_config", {"raw": "x"}, {"json.loads": raise_spec("TypeError", "propagates")})
add("is_corrupt_model_config", {"raw": "x"}, {"json.loads": raise_spec("SystemExit", "bye")})
add("is_corrupt_model_config", {"raw": "x"}, {})                # fail-closed

# ══ 8. session_yolo_enabled (seams parse_model_config) ══════════════════════
for mc in ['{"yolo_mode":true}', '{"yolo_mode":0}', {"yolo_mode": "x"}, {"yolo_mode": False},
           {}, None, {"other": 1}]:
    cfg = S.parse_model_config(mc)
    add("session_yolo_enabled", {"model_config": mc}, {"parse_model_config": ret(cfg)})
add("session_yolo_enabled", {"model_config": {}}, {"parse_model_config": ret(None)})  # .get on None
add("session_yolo_enabled", {"model_config": {}}, {})           # fail-closed
add("session_yolo_enabled", {"model_config": {}},
    {"parse_model_config": raise_spec("GeneratorExit", "gone")})

# ══ 9. sanitize_title ═══════════════════════════════════════════════════════
add("sanitize_title", {"title": ""})
add("sanitize_title", {"title": None})
add("sanitize_title", {"title": "a\x00b\x08c\x0bd\x0ce\x1cf\x7fg"})
add("sanitize_title", {"title": "a\tb\nc\rd e"})
add("sanitize_title", {"title": "a\u200bb\u202ec\ufeffd\ufffce\ufff9f"})
add("sanitize_title", {"title": "  a   b  "})
add("sanitize_title", {"title": "x" * 100})
add("sanitize_title", {"title": "x" * 101})
add("sanitize_title", {"title": "\u200b\u200c"})               # all invisible -> None
add("sanitize_title", {"title": "Bot Chat"})
add("sanitize_title", {"title": "a\ud800b\udfffc"})            # lone surrogates -> FFFD
add("sanitize_title", {"title": "  \t\n  "})                   # whitespace only -> None

# ══ 10-12. error constructors ═══════════════════════════════════════════════
add("TitleConflict.__init__", {"title": "My Title", "holder": "sess-1"})
add("TitleConflict.__init__", {"title": "Bot Chat", "holder": "abc"})
add("TitleConflict.__init__", {"title": "", "holder": ""})
add("TitleConflict.__init__", {"title": "a'b", "holder": "s2"})
add("TitleConflict.__init__", {"title": "T", "holder": "h" * 40})
add("CanonicalTitleLocked.__init__", {"session_id": "sess-2"})
add("CanonicalTitleLocked.__init__", {"session_id": "abc-123"})
add("CanonicalTitleLocked.__init__", {"session_id": ""})
add("CanonicalTitleLocked.__init__", {"session_id": "x" * 50})
add("CanonicalTitleLocked.__init__", {"session_id": "s"})
add("SessionExists.__init__", {"existing": {"session_id": "sess-3"}})
add("SessionExists.__init__", {"existing": {"session_id": "abc", "title": "T"}})
add("SessionExists.__init__", {"existing": {"session_id": ""}})
add("SessionExists.__init__", {"existing": {"session_id": "s", "source": "cli"}})
add("SessionExists.__init__", {"existing": {"session_id": "z"}})

# ══ 13. NewSession.__post_init__ ════════════════════════════════════════════
add("NewSession.__post_init__", {"self": {"session_id": "s", "source": "cli",
                                          "lineage_kind": "none", "parent_session_id": None}})
add("NewSession.__post_init__", {"self": {"session_id": "", "source": "cli", "lineage_kind": "none"}})
add("NewSession.__post_init__", {"self": {"session_id": "s", "source": "", "lineage_kind": "none"}})
add("NewSession.__post_init__", {"self": {"session_id": "s", "source": "cli",
                                          "lineage_kind": "delegate", "parent_session_id": None}})
add("NewSession.__post_init__", {"self": {"session_id": "s", "source": "cli",
                                          "lineage_kind": "branch", "parent_session_id": "p"}})
add("NewSession.__post_init__", {"self": {"session_id": "s", "source": "cli",
                                          "lineage_kind": "reset", "parent_session_id": ""}})
add("NewSession.__post_init__", {"self": {"session_id": "s", "source": "cli",
                                          "lineage_kind": "delegate", "parent_session_id": "p"}})

# ══ 14. SessionRecord._row_cells ════════════════════════════════════════════
add("SessionRecord._row_cells", {"self": row_template(), "compact": False})
add("SessionRecord._row_cells", {"self": row_template(), "compact": True})
add("SessionRecord._row_cells", {"self": row_template(title="T", title_source="user",
                                                      archived=True, pinned=True,
                                                      expiry_finalized=True,
                                                      model_config_json='{"a":1}',
                                                      origin_json='{"o":1}',
                                                      tool_names="pin-text"), "compact": False})
add("SessionRecord._row_cells", {"self": row_template(title_source="llm", hidden=True,
                                                      auto_archived=True, started_at=250.5,
                                                      message_count=7), "compact": True})
add("SessionRecord._row_cells", {"self": row_template(source="kanban", cwd="/w",
                                                      git_repo_root="/r", model="m"), "compact": False})
# absent column attr -> AttributeError (fail-closed)
_bad = row_template()
del _bad["message_count"]
add("SessionRecord._row_cells", {"self": _bad, "compact": False})

# ══ 15. SessionRecord._marker_cells ═════════════════════════════════════════
add("SessionRecord._marker_cells", {"self": {"reset_from": None, "branched_from": None}})
add("SessionRecord._marker_cells", {"self": {"reset_from": "r1", "branched_from": None}})
add("SessionRecord._marker_cells", {"self": {"reset_from": None, "branched_from": "b1"}})
add("SessionRecord._marker_cells", {"self": {"reset_from": "r1", "branched_from": "b1"}})
add("SessionRecord._marker_cells", {"self": {"reset_from": "", "branched_from": ""}})
add("SessionRecord._marker_cells", {"self": {"reset_from": "r2", "branched_from": "b2"}})

# ══ 16. SessionRecord.as_session_row (seams _row_cells, _marker_cells) ══════
for over in [{}, {"title": "T", "title_source": "user"}, {"reset_from": "r1"},
             {"branched_from": "b1"}, {"reset_from": "r1", "branched_from": "b1"},
             {"archived": True, "hidden": True}]:
    r = row_template(**over)
    add("SessionRecord.as_session_row", {"self": {}},
        {"SessionRecord._row_cells": ret(live_row_cells(r)),
         "SessionRecord._marker_cells": ret(live_marker_cells(r))})
add("SessionRecord.as_session_row", {"self": {}}, {})           # fail-closed
add("SessionRecord.as_session_row", {"self": {}},
    {"SessionRecord._row_cells": raise_spec("KeyboardInterrupt", "stop"),
     "SessionRecord._marker_cells": ret({})})

# ══ 17. SessionWindow.__post_init__ ═════════════════════════════════════════
add("SessionWindow.__post_init__", {"self": {"limit": 20, "offset": 0}})
add("SessionWindow.__post_init__", {"self": {"limit": None, "offset": 0}})
add("SessionWindow.__post_init__", {"self": {"limit": -1, "offset": 0}})
add("SessionWindow.__post_init__", {"self": {"limit": None, "offset": -2}})
add("SessionWindow.__post_init__", {"self": {"limit": 0, "offset": 0}})
add("SessionWindow.__post_init__", {"self": {"limit": 5, "offset": 3}})
add("SessionWindow.__post_init__", {"self": {"limit": -3, "offset": -4}})

# ══ 18. SessionListEntry._head_cells (seams session._row_cells/_marker_cells) ═
for compact, la, pv, extra in [
        (False, None, "", []),
        (True, 123.0, "preview", []),
        (False, 50.5, "p", [["cron_execution_id", "ex1"]]),
        (True, None, "", [["k", "v"]]),
        (False, 0.0, "x", [["a", 1], ["b", 2]])]:
    r = row_template()
    add("SessionListEntry._head_cells",
        {"self": {"session": {}, "compact": compact, "last_active": la, "preview": pv},
         "extra": extra},
        {"SessionRecord._row_cells": ret(live_row_cells(r, compact=compact)),
         "SessionRecord._marker_cells": ret(live_marker_cells(r))})
add("SessionListEntry._head_cells",
    {"self": {"session": {}, "compact": False, "last_active": None, "preview": ""}, "extra": []},
    {})                                                          # fail-closed

# ══ 19. SessionListEntry.as_list_row (seams _head_cells) ════════════════════
r = row_template()
head = live_head_cells(r, compact=False, last_active=10.0, preview="pv")
for lrid, lids, ck, unread in [
        (None, [], None, False),
        ("root1", ["root1", "tip1"], "compression", True),
        (None, ["a", "b"], None, False),
        ("root2", ["root2"], None, True),
        (None, [], None, True),
        ("root3", ["root3", "x", "y"], "compression", False)]:
    add("SessionListEntry.as_list_row",
        {"self": {"lineage_root_id": lrid, "lineage_ids": lids,
                  "continuation_kind": ck, "unread": unread}},
        {"SessionListEntry._head_cells": ret(head)})
add("SessionListEntry.as_list_row",
    {"self": {"lineage_root_id": None, "lineage_ids": [], "continuation_kind": None, "unread": False}},
    {})                                                          # fail-closed

# ══ 20. CronRunEntry.as_cron_row (seams entry._head_cells) ══════════════════
for ceid, cjid in [("ex1", "job1"), (None, "job2"), ("ex2", "job3"),
                   ("", "job4"), ("ex3", "job5")]:
    hh = live_head_cells(row_template(), compact=False, last_active=5.0, preview="p",
                         extra=(("cron_execution_id", ceid),))
    add("CronRunEntry.as_cron_row",
        {"self": {"entry": {}, "cron_execution_id": ceid, "cron_job_id": cjid}},
        {"SessionListEntry._head_cells": ret(hh)})
add("CronRunEntry.as_cron_row",
    {"self": {"entry": {}, "cron_execution_id": None, "cron_job_id": "j"}}, {})  # fail-closed

# ══ 21. RecentBrowseQuery.__post_init__ (mutates self) ══════════════════════
def rbq_self(**over):
    d = {"limit": 20, "exclude_sources": [], "timeout_seconds": 3.0,
         "candidate_limit": None, "lineage_limit": None}
    d.update(over)
    return d


add("RecentBrowseQuery.__post_init__", {"self": rbq_self()})
add("RecentBrowseQuery.__post_init__", {"self": rbq_self(limit=0, timeout_seconds=-1.5)})
add("RecentBrowseQuery.__post_init__", {"self": rbq_self(limit=5, candidate_limit=2, lineage_limit=1, timeout_seconds=2.5)})
add("RecentBrowseQuery.__post_init__", {"self": rbq_self(limit=300)})
add("RecentBrowseQuery.__post_init__", {"self": rbq_self(limit=2.9, timeout_seconds=True)})
add("RecentBrowseQuery.__post_init__", {"self": rbq_self(limit=10, candidate_limit=5000, lineage_limit=99999)})
add("RecentBrowseQuery.__post_init__", {"self": rbq_self(limit=-5, timeout_seconds=-0.0)})

# ══ 22. RuntimeLock.as_mapping ══════════════════════════════════════════════
add("RuntimeLock.as_mapping", {"self": {"provider": "p", "model": "m",
                                        "model_options": {"a": 1}, "route_source": "r", "confirmed": True}})
add("RuntimeLock.as_mapping", {"self": {"provider": None, "model": None,
                                        "model_options": None, "route_source": None, "confirmed": False}})
add("RuntimeLock.as_mapping", {"self": {"provider": "", "model": "", "model_options": {},
                                        "route_source": "", "confirmed": False}})
add("RuntimeLock.as_mapping", {"self": {"provider": "p", "model": "m",
                                        "model_options": {"x": [1, 2]}, "route_source": "r", "confirmed": True}})
add("RuntimeLock.as_mapping", {"self": {"provider": None, "model": "m", "model_options": None,
                                        "route_source": "r", "confirmed": True}})
add("RuntimeLock.as_mapping", {"self": {"provider": "p", "model": None, "model_options": {"k": "v"},
                                        "route_source": None, "confirmed": False}})

# ══ 23. ReplaceSessionConfig.__post_init__ ══════════════════════════════════
add("ReplaceSessionConfig.__post_init__", {"self": {"model": None}})
add("ReplaceSessionConfig.__post_init__", {"self": {"model": "m"}})
add("ReplaceSessionConfig.__post_init__", {"self": {"model": ""}})
add("ReplaceSessionConfig.__post_init__", {"self": {"model": "model-x"}})
add("ReplaceSessionConfig.__post_init__", {"self": {"model": None}})
add("ReplaceSessionConfig.__post_init__", {"self": {"model": "a"}})


def main():
    # coverage assertion: >=5 cases per func
    from collections import Counter
    counts = Counter(c["func"] for c in CASES)
    expected = set(counts)
    short = [f for f, n in counts.items() if n < 5]
    if short:
        print("FATAL: funcs with <5 cases:", short)
        sys.exit(2)
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as fh:
        for c in CASES:
            fh.write(json.dumps(c, ensure_ascii=True) + "\n")
    print(f"wrote {len(CASES)} cases across {len(counts)} funcs -> {OUT}")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")


if __name__ == "__main__":
    main()