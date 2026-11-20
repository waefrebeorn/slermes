#!/usr/bin/env python3
"""Generate tests/fixtures/session_search_flow3.jsonl (>=5 cases per function).

Each case is {"func": <harness func>, "in": {"args": {...}, "io": {...},
"state": {...}}}; `io` maps a seam name to a scripted result (plain value
reused, bare array = per-call queue, {"__script__":true,"ret"/"raise"}, or a
bare {"__raise__","message"} dict).
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "session_search_flow3.jsonl"


def case(func, args=None, io=None, state=None):
    rec = {"func": func, "in": {"args": args or {}}}
    if io is not None:
        rec["in"]["io"] = io
    if state is not None:
        rec["in"]["state"] = state
    return rec


RAISE_RUNTIME = {"__script__": True, "raise": {"type": "RuntimeError", "message": "boom"}}
RAISE_SYSEXIT = {"__script__": True, "raise": {"type": "SystemExit", "message": "halt"}}
RAISE_VALUE = {"__script__": True, "raise": {"type": "ValueError", "message": "nope"}}

CASES = []

# ── content_text (6) ──
CASES += [
    case("content_text", {"content": "hello world"}),
    case("content_text", {"content": None}),
    case("content_text", {"content": [{"text": "alpha"}, {"text": "beta"}]}),
    case("content_text", {"content": [{"text": "alpha"}, {"kind": "img"}, {"text": 7}, "raw"]}),
    case("content_text", {"content": ""}),
    case("content_text", {"content": []}),
]

# ── redact_derived_value (6) ──
_RD = {"redact_derived_text": "R"}
CASES += [
    case("redact_derived_value", {"value": "plain text"}, _RD),
    case("redact_derived_value", {"value": "data:image/png;base64,AAAA"}, _RD),
    case("redact_derived_value", {"value": ["a", "b"]}, _RD),
    case("redact_derived_value", {"value": {"x": "a", "y": {"z": "b"}}}, _RD),
    case("redact_derived_value", {"value": 42}, _RD),
    case("redact_derived_value", {"value": None}, _RD),
]

# ── redact_tool_calls (6) ──
CASES += [
    case("redact_tool_calls", {"tool_calls": "not a list"}, _RD),
    case("redact_tool_calls", {"tool_calls": [
        {"function": {"arguments": "secret", "name": "f"}}]}, _RD),
    case("redact_tool_calls", {"tool_calls": [
        {"function": {"arguments": {"k": "secret"}}}]}, _RD),
    case("redact_tool_calls", {"tool_calls": [{"role": "assistant"}]}, _RD),
    case("redact_tool_calls", {"tool_calls": [
        {"function": {"arguments": None}}]}, _RD),
    case("redact_tool_calls", {"tool_calls": []}, _RD),
]

# ── discovery_entry (6) ──
_FIELDS = {
    "session_id": "s1", "when": "Jan 01, 2024 at 12:00 AM", "source": "chat",
    "model": "m", "title": "T", "matched_role": "user", "match_message_id": 5,
    "snippet": "snip", "bookend_start": [], "messages": [], "bookend_end": [],
    "messages_before": 1, "messages_after": 2, "detail": "full",
}
CASES += [
    case("discovery_entry", dict(_FIELDS, lineage_root="s1")),
    case("discovery_entry", dict(_FIELDS, lineage_root="root")),
    case("discovery_entry", dict(_FIELDS, lineage_root=None)),
    case("discovery_entry", dict(_FIELDS, lineage_root="", when=None, title=None)),
    case("discovery_entry", dict(_FIELDS, lineage_root="root", messages=[{"id": 5}])),
    case("discovery_entry", dict(_FIELDS, lineage_root="root", detail="compact")),
]

# ── derived_snippet (6) ──
_LONG = "alpha beta gamma delta " * 14  # ~320 chars, exercises the 200-char crop
DS_IO1 = {"redact_derived_text": "alpha beta gamma delta epsilon"}
DS_IO2 = {"redact_derived_text": _LONG}
CASES += [
    case("derived_snippet", {"fts_snippet": ">>>beta<<<", "anchor": None}, DS_IO1),
    case("derived_snippet", {"fts_snippet": ">>>beta<<<",
         "anchor": {"content": "x", "tool_calls": None}}, DS_IO1),
    case("derived_snippet", {"fts_snippet": "nothing-marker-here",
         "anchor": {"content": "x", "tool_calls": None}}, DS_IO1),
    case("derived_snippet", {"fts_snippet": ">>>beta<<<>>>beta<<<",
         "anchor": {"content": "x", "tool_calls": None}}, DS_IO1),
    case("derived_snippet", {"fts_snippet": ">>>delta<<<",
         "anchor": {"content": "x", "tool_calls": [{"function": {"arguments": "delta"}}]}}, DS_IO1),
    case("derived_snippet", {"fts_snippet": ">>>alpha<<<",
         "anchor": {"content": "x", "tool_calls": None}}, DS_IO2),
]

# ── bookend (6) ──
ROWS = [{"id": 1, "content": "hello", "tool_calls": None},
        {"id": 2, "content": "[CONTEXT COMPACTION] summary", "tool_calls": None}]
CASES += [
    case("bookend", {"rows": []}, {"is_compaction_summary": False, "shape_message": {"id": 0}}),
    case("bookend", {"rows": [ROWS[0]]}, {"is_compaction_summary": False,
         "shape_message": {"id": 1, "content": "hello"}}),
    case("bookend", {"rows": [ROWS[1]]}, {"is_compaction_summary": True, "shape_message": {"id": 2}}),
    case("bookend", {"rows": ROWS}, {"is_compaction_summary": [False, True],
         "shape_message": {"id": 1}}),
    case("bookend", {"rows": [ROWS[0]]}, {"is_compaction_summary": False,
         "shape_message": RAISE_RUNTIME}),
    case("bookend", {"rows": [ROWS[0]]}, {"is_compaction_summary": False,
         "shape_message": RAISE_SYSEXIT}),
]

# ── discover_payload (6) ──
STATUS = {"percent": 42, "indexed": 1234567, "total": 9876543}
DP_IO_NULL = {"search_index_rebuild_status": None}
DP_IO_STATUS = {"search_index_rebuild_status": STATUS}
CASES += [
    case("discover_payload", {"query": "q", "detail": "full", "results": []}, DP_IO_NULL),
    case("discover_payload", {"query": "q", "detail": "full", "results": []}, DP_IO_STATUS),
    case("discover_payload", {"query": "q", "detail": "compact", "results": [{"id": 1}], "extra": {"note": "x"}}, DP_IO_STATUS),
    case("discover_payload", {"query": "q", "detail": "full", "results": []},
         {"search_index_rebuild_status": RAISE_VALUE, "logger.debug": None}),
    case("discover_payload", {"query": "q", "detail": "full", "results": []},
         {"search_index_rebuild_status": RAISE_SYSEXIT, "logger.debug": None}),
    case("discover_payload", {"query": "q", "detail": "full", "results": []},
         {"search_index_rebuild_status": {"percent": 0, "indexed": 0, "total": 0}}),
]

# ── hydrate_hit (6) ──
HIT = {"session_id": "s1", "message_id": 5, "snippet": ">>>alpha<<<",
       "session_started": 1000, "source": "chat", "model": "m", "role": "user"}
VIEW = {"window": [{"id": 5, "content": "hello alpha world", "tool_calls": None}],
        "bookend_start": [], "bookend_end": [], "before": 1, "after": 2}
META = {"started_at": 1000, "source": "chat", "model": "m", "title": "T"}
SHAPED = {"id": 5, "role": "user", "content": "R", "timestamp": 1, "anchor": True}
HH_IO = {
    "read_anchored_view": VIEW,
    "get_session_meta": META,
    "format_timestamp": "Jan 01, 2024 at 12:00 AM",
    "redact_derived_text": "R",
    "shape_message": SHAPED,
    "is_compaction_summary": False,
}
CASES += [
    case("hydrate_hit", {"lineage_root": "s1", "hit": HIT, "result_detail": "short"}, HH_IO),
    case("hydrate_hit", {"lineage_root": "s1", "hit": HIT, "result_detail": "full"},
         dict(HH_IO, read_anchored_view=dict(VIEW, bookend_start=[{"id": 4, "content": "b", "tool_calls": None}],
                                              bookend_end=[{"id": 6, "content": "e", "tool_calls": None}]))),
    case("hydrate_hit", {"lineage_root": "s1", "hit": HIT, "result_detail": "full"},
         dict(HH_IO, read_anchored_view=RAISE_RUNTIME, **{"logger.warning": None})),
    case("hydrate_hit", {"lineage_root": "s1", "hit": HIT, "result_detail": "full"},
         dict(HH_IO, read_anchored_view=RAISE_SYSEXIT, **{"logger.warning": None})),
    case("hydrate_hit", {"lineage_root": "root",
         "hit": dict(HIT, session_id=None), "result_detail": "full"}, HH_IO),
    case("hydrate_hit", {"lineage_root": "root", "hit": HIT, "result_detail": "full"},
         dict(HH_IO, **{"get_session_meta": [None, META]})),
]

assert len(CASES) == 48, len(CASES)
OUT.parent.mkdir(parents=True, exist_ok=True)
OUT.write_text("".join(json.dumps(c, ensure_ascii=False) + "\n" for c in CASES), encoding="utf-8", newline="\n")
print(f"wrote {len(CASES)} cases to {OUT}")
