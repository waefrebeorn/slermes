#!/usr/bin/env python3
"""Deterministic fixture generator for the session_transcript_rows_flow2 oracle.

Writes tests/fixtures/session_transcript_rows_flow2.jsonl with >=5 cases per
ported function (7 functions), covering normal paths, falsy/None gates,
type-exact int gates, json.dumps(default=str) projections, per-call seam
scripts, set dedupe/sort, row-handle object doubles, scripted Exception and
BaseException raises, int(str) ValueError and missing-key KeyError paths.

Fixture domain: JSON-serializable ASCII (plus one ensure_ascii unicode case);
integral numbers written as integer literals; occurrence uids are strings or
None so sorted(set) has a total order; seam result lists are scripted one
level deeper where the result itself is a list.
"""
import json
import os
from collections import Counter

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "session_transcript_rows_flow2.jsonl")

CASES = []


def case(func, **inp):
    CASES.append({"func": func, "in": inp})


def raise_(kind, message="boom", **extra):
    d = {"__raise__": kind, "message": message}
    d.update(extra)
    return d


SUM = {"summary": "sum text", "compacted_messages": 3}
WIRE = {"message_uid": "u1", "content": {"x": 1}, "id": 9}
MARK0 = {"role": "system", "content": "compaction handoff",
         "display_metadata": {"__session_transcript_handoff__": 0}}
MARK1 = {"role": "system", "content": "compaction handoff",
         "display_metadata": {"__session_transcript_handoff__": 1}}
MARK5 = {"role": "system", "content": "compaction handoff",
         "display_metadata": {"__session_transcript_handoff__": 5}}
MARKN = {"role": "system", "content": "compaction handoff",
         "display_metadata": {"__session_transcript_handoff__": -1}}
PLAIN = {"role": "user", "content": "hi"}
COMP1 = ["compaction", {"kind": "handoff1"}]
COMP2 = ["compaction", {"kind": "handoff2"}]
MSG1 = ["message", {"role": "user", "content": "hi"}]


# ── handoff_row ─────────────────────────────────────────────────────────────
case("handoff_row", message={"id": 5, "timestamp": 123}, summary=SUM)
case("handoff_row", message={"id": "r1"}, summary=SUM)
case("handoff_row", message={"id": 1, "timestamp": 0}, summary=SUM)
case("handoff_row", message={"id": 2, "timestamp": -5}, summary=SUM)
case("handoff_row", message={"id": 3, "timestamp": 2.5}, summary=SUM)
case("handoff_row", message={"id": 4, "timestamp": True}, summary=SUM)
case("handoff_row", message={"id": 6, "timestamp": None}, summary=SUM)
case("handoff_row", message={"id": 7, "timestamp": False}, summary=SUM)
case("handoff_row", message={"id": 8, "timestamp": (1 << 63)}, summary=SUM)
case("handoff_row", message={}, summary=SUM)                    # KeyError 'id'
case("handoff_row", message={"id": 1}, summary={"summary": "s"})  # KeyError

# ── _mark_index ─────────────────────────────────────────────────────────────
case("_mark_index", row=MARK1)
case("_mark_index", row={"role": "user", "content": "compaction handoff",
                         "display_metadata": {"__session_transcript_handoff__": 2}})
case("_mark_index", row={"role": "system", "content": "other",
                         "display_metadata": {"__session_transcript_handoff__": 2}})
case("_mark_index", row={"role": "system", "content": "compaction handoff",
                         "display_metadata": "not-a-dict"})
case("_mark_index", row={"role": "system", "content": "compaction handoff",
                         "display_metadata": {"__session_transcript_handoff__": 2.5}})
case("_mark_index", row={"role": "system", "content": "compaction handoff",
                         "display_metadata": {"__session_transcript_handoff__": True}})
case("_mark_index", row={"role": "system", "content": "compaction handoff"})
case("_mark_index", row=MARKN)
case("_mark_index", row={"role": "system", "content": "compaction handoff",
                         "display_metadata": {"__session_transcript_handoff__": 1 << 63}})

# ── handoff_occurrence ──────────────────────────────────────────────────────
case("handoff_occurrence", message={"message_uid": "u1", "content": {"a": 1}})
case("handoff_occurrence", message={"content": [1, 2.5, "x"]})
case("handoff_occurrence", message={"message_uid": 0, "content": {"b": True, "a": None}})
case("handoff_occurrence", message={"message_uid": "u2",
                                    "content": {"n": {"deep": [True, False, None]}}})
case("handoff_occurrence", message={"message_uid": "u3"})
case("handoff_occurrence", message={"message_uid": 7, "content": "caf\u00e9"})
case("handoff_occurrence", message={"message_uid": "", "content": []})

# ── shown_occurrences ──────────────────────────────────────────────────────
M1 = {"message_uid": "u1", "content": {"a": 1}}
M2 = {"message_uid": "u2", "content": [1, 2]}
case("shown_occurrences", messages=[M1, M2],
     io={"compaction_summary": {"summary": "s", "compacted_messages": 1}})
case("shown_occurrences", messages=[M1, M2],
     io={"compaction_summary": [{"summary": "s"}, None]})
case("shown_occurrences", messages=[M1],
     io={"compaction_summary": None})
case("shown_occurrences", messages=[], io={})
case("shown_occurrences", messages=[M1, dict(M1)],
     io={"compaction_summary": {"summary": "s"}})
case("shown_occurrences", messages=[M1],
     io={"compaction_summary": raise_("ValueError", "bad")})
case("shown_occurrences", messages=[M1],
     io={"compaction_summary": raise_("SystemExit", "bye")})
case("shown_occurrences", messages=[{"content": {"k": "v"}}],
     io={"compaction_summary": {"summary": "s"}})

# ── handoff_rows ────────────────────────────────────────────────────────────
case("handoff_rows", shown_ids=[1, 2], segments=["s1"],
     io={"messages.transcript_handoff_rows": {"1": ["h1", "h2"]},
         "row.as_wire_row": [{"w": 1}, {"w": 2}]})
case("handoff_rows", shown_ids=[1], segments=[],
     io={"messages.transcript_handoff_rows": {"7": ["h1"]},
         "row.as_wire_row": [{"w": 1}]})
case("handoff_rows", shown_ids=[], segments=[],
     io={"messages.transcript_handoff_rows": {}})
case("handoff_rows", shown_ids=[1], segments=[],
     io={"messages.transcript_handoff_rows": {"1": ["h1", "h2"]},
         "row.as_wire_row": [{"w": 1}, raise_("KeyError", "mid")]})
case("handoff_rows", shown_ids=[1], segments=[],
     io={"messages.transcript_handoff_rows": raise_("RuntimeError", "db")})
case("handoff_rows", shown_ids=[1], segments=[],
     io={"messages.transcript_handoff_rows": raise_("KeyboardInterrupt", "ci")})
case("handoff_rows", shown_ids=[1], segments=[],
     io={"messages.transcript_handoff_rows": {"abc": ["h1"]},
         "row.as_wire_row": [{"w": 1}]})
case("handoff_rows", shown_ids=[1], segments=[],
     io={"messages.transcript_handoff_rows": {" 7 ": ["h1"]},
         "row.as_wire_row": [{"w": 1}]})

# ── page_handoffs ───────────────────────────────────────────────────────────
PH_IO = {"messages.transcript_handoff_rows": {"1": ["h1"]},
         "row.as_wire_row": [WIRE],
         "compaction_summary": {"summary": "S", "compacted_messages": 2}}
case("page_handoffs", rows=[{"_row_id": 1}], segments=[], io=PH_IO)
case("page_handoffs", rows=[{"_row_id": 1}], segments=[], earlier_present=True,
     io=dict(PH_IO, **{"earlier": [[["u1", '{"x": 1}']]]}))
case("page_handoffs", rows=[{"_row_id": 1}], segments=[],
     io=dict(PH_IO, **{"compaction_summary": None}))
case("page_handoffs", rows=[{"_row_id": 1}], segments=[],
     io=dict(PH_IO, **{"compaction_summary": raise_("ValueError", "bad")}))
case("page_handoffs", rows=[{"_row_id": 1}], segments=[],
     io=dict(PH_IO, **{"compaction_summary": {"summary": "S"}}))   # KeyError
case("page_handoffs", rows=[{"_row_id": "1"}], segments=[],
     earlier_present=True, io={"messages.transcript_handoff_rows": {}})
case("page_handoffs", rows=[{"_row_id": 1}, {"_row_id": 2}], segments=[],
     io={"messages.transcript_handoff_rows": {"1": ["h1"], "2": ["h2"]},
         "row.as_wire_row": [WIRE, dict(WIRE, message_uid="u2", id=10)],
         "compaction_summary": {"summary": "S", "compacted_messages": 2}})
case("page_handoffs", rows=[{"_row_id": 1}], segments=[], earlier_present=True,
     io=dict(PH_IO, **{"earlier": [[]]}))
case("page_handoffs", rows=[{"_row_id": 1}], segments=[],
     io=dict(PH_IO, **{"messages.transcript_handoff_rows": raise_("SystemExit", "bye")}))

# ── project_entries ────────────────────────────────────────────────────────
case("project_entries", entries=[COMP1, MSG1], io={"project": [[MARK0, PLAIN]]})
case("project_entries", entries=[COMP1, COMP2, MSG1],
     io={"project": [[MARK1, PLAIN]]})
case("project_entries", entries=[COMP1, MSG1], io={"project": [[MARK5, PLAIN]]})
case("project_entries", entries=[COMP1, MSG1],
     io={"project": raise_("RuntimeError", "proj")})
case("project_entries", entries=[], io={"project": [[]]})
case("project_entries", entries=[MSG1], io={"project": [[PLAIN]]})
case("project_entries", entries=[COMP1, COMP2], io={"project": [[MARKN]]})
case("project_entries", entries=[MSG1], io={"project": [[None]]})
case("project_entries", entries=[COMP1], io={"project": [[MARK0]]})


FUNCS = ["handoff_row", "_mark_index", "handoff_occurrence",
         "shown_occurrences", "handoff_rows", "page_handoffs",
         "project_entries"]


def main():
    counts = Counter(c["func"] for c in CASES)
    assert set(counts) == set(FUNCS), (set(counts) ^ set(FUNCS))
    for fn in FUNCS:
        assert counts[fn] >= 5, "%s has only %d cases" % (fn, counts[fn])
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    print("wrote %s: %d cases across %d functions" %
          (OUT, len(CASES), len(counts)))
    for fn in FUNCS:
        print("  %-22s %d" % (fn, counts[fn]))


if __name__ == "__main__":
    main()
