#!/usr/bin/env python3
"""Generate tests/fixtures/conversation_messages_flow.jsonl — input cases for
the transcript-contract FLOW oracle (persistence/conversation/messages.py).

The oracle computes want from the LIVE quarry module at gate time; this script
only emits {func, in} inputs. Deterministic handcrafted branch-coverage cases.

Representation contract (shared with the C port and the oracle):
- None == JSON null; plain JSON numbers are Python ints; JSON bools are bools.
- A Python float travels as {"__float__": true, "v": <number>}.
- A Python tuple (where isinstance(tuple) gates read it) travels as
  {"__tuple__": true, "v": [...]}.
- A nested HeldMessage instance: {"__held__": true, "v": {<fields>}}.
- A BatchAppendProgress instance: {"__progress__": true, "v": {"outcomes":
  [...], "uids": [...]}}.
- Enum fields (RowOutcome.disposition, TranscriptRead.view,
  TranscriptRow.visibility) travel as the enum VALUE string; an invalid value
  is a ValueError "'x' is not a valid <EnumName>" at materialization.
- Record specs are plain field dicts; absent keys take the dataclass default;
  absent required keys / unknown keys are the live __init__ TypeErrors.
- stamp_live result: null | {"kind":"progress"|"batch"|"replace", ...}.
- batch_progress error: {"attrs": {...}} (a SimpleNamespace double) or a plain
  JSON value (no attributes).
- ReadCheck check param: "seam" -> the recording stub named "check"; any other
  value is a non-callable attribute.
- json.loads is a scripted seam (in.io["json.loads"]); args {"s": <str>}.
- Fixture domain: int() string literals are ASCII digits with optional +/- and
  single underscores between digits (plus ASCII whitespace padding); str() of
  non-strings covers int/bool/None only; content/metadata trees never carry
  literal "__float__"/"__tuple__"/"__held__"/"__progress__" keys; check-seam
  scripts are plain JSON values; no case combines a binding error with an
  invalid nested HeldMessage.
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "conversation_messages_flow.jsonl"
CASES = []


def add(func, args, io=None, fail_closed=False):
    inp = {"args": args}
    if io:
        inp["io"] = io
    case = {"func": func, "in": inp}
    if fail_closed:
        # An intentional missing-script case: both sides must fail closed with
        # the universal seam's RuntimeError. The self-test verifies it.
        case["fail_closed"] = True
    CASES.append(case)


def fl(v):
    return {"__float__": True, "v": v}


def tu(items):
    return {"__tuple__": True, "v": list(items)}


def hd(fields):
    return {"__held__": True, "v": fields}


def pg(outcomes, uids):
    return {"__progress__": True, "v": {"outcomes": outcomes, "uids": uids}}


def ro(id=1, disposition="appended", timestamp=1.0, snapshot="snap", **kw):
    d = {"id": id, "disposition": disposition, "timestamp": fl(timestamp),
         "db_row_snapshot": snapshot}
    d.update(kw)
    return d


def rs(id=1, timestamp=1.0, snapshot="snap", uid="mu", **kw):
    d = {"id": id, "timestamp": fl(timestamp), "db_row_snapshot": snapshot,
         "message_uid": uid}
    d.update(kw)
    return d


def ru(**kw):
    return kw


# ── _live_or_column ──
add("_live_or_column", {"message": {"_x": 1, "x": 2}, "live_key": "_x", "column": "x"})
add("_live_or_column", {"message": {"_x": None, "x": 2}, "live_key": "_x", "column": "x"})
add("_live_or_column", {"message": {"x": 2}, "live_key": "_x", "column": "x"})
add("_live_or_column", {"message": {}, "live_key": "_x", "column": "x"})
add("_live_or_column", {"message": {"_x": "", "x": "col"}, "live_key": "_x", "column": "x"})
add("_live_or_column", {"message": {"x": None}, "live_key": "_x", "column": "x"})
add("_live_or_column", {"message": {"_x": {"a": 1}}, "live_key": "_x", "column": "x"})

# ── _count ──
for v in [5, 0, -3, True, False, "5", None, fl(3.0), fl(1.5), 1]:
    add("_count", {"value": v})

# ── _json_text ──
add("_json_text", {"value": 5, "empty": []})
add("_json_text", {"value": None, "empty": "fallback"})
add("_json_text", {"value": {"a": 1}, "empty": []})
add("_json_text", {"value": [1, 2], "empty": {}})
add("_json_text", {"value": "[1, 2]", "empty": []}, {"json.loads": [1, 2]})
add("_json_text", {"value": '{"a": 1}', "empty": {}}, {"json.loads": {"a": 1}})
add("_json_text", {"value": "3.5", "empty": None}, {"json.loads": fl(3.5)})
add("_json_text", {"value": '"txt"', "empty": []}, {"json.loads": "txt"})
add("_json_text", {"value": "bad", "empty": []},
    {"json.loads": {"__script__": True, "raise": {"type": "ValueError", "message": "Expecting value: line 1 column 1 (char 0)"}}})
add("_json_text", {"value": "bad", "empty": {"d": 1}},
    {"json.loads": {"__script__": True, "raise": {"type": "ValueError", "message": "Expecting value"}}})
add("_json_text", {"value": "x", "empty": []},
    {"json.loads": {"__script__": True, "raise": {"type": "TypeError", "message": "boom"}}})
add("_json_text", {"value": "null", "empty": "e"}, {"json.loads": None})

# ── NewMessage.__post_init__ ──
NM = "NewMessage.__post_init__"
add(NM, {"fields": {"role": "user", "content": "hi"}})
add(NM, {"fields": {"role": "user", "content": "hi", "merged_durable_rows": 2,
                    "unnamed_durable_rows": 3, "absorbed_row_ids": tu([1, 2])}})
add(NM, {"fields": {"role": "", "content": "hi"}})
add(NM, {"fields": {"role": None, "content": "hi"}})
add(NM, {"fields": {"role": "u", "content": "c", "merged_durable_rows": True}})
add(NM, {"fields": {"role": "u", "content": "c", "merged_durable_rows": fl(2.0)}})
add(NM, {"fields": {"role": "u", "content": "c", "merged_durable_rows": -1}})
add(NM, {"fields": {"role": "u", "content": "c", "unnamed_durable_rows": "2"}})
add(NM, {"fields": {"role": "u", "content": "c", "absorbed_row_ids": [1, 2]}})
add(NM, {"fields": {"role": "u", "content": "c", "absorbed_row_ids": tu([True])}})
add(NM, {"fields": {"role": "u", "content": "c", "absorbed_row_ids": tu([0])}})
add(NM, {"fields": {"role": "u", "content": "c", "absorbed_row_ids": tu([1, -2])}})
add(NM, {"fields": {"content": "c"}})
add(NM, {"fields": {}})
add(NM, {"fields": {"role": "u", "content": "c", "bogus": 1}})

# ── NewMessage.from_live ──
FL = "NewMessage.from_live"
add(FL, {"message": {}})
add(FL, {"message": {"role": "user", "content": "hi", "timestamp": fl(1.5),
                     "tool_name": "t", "tool_call_id": "c1", "display_kind": "dk",
                     "api_content": "ap", "display_metadata": {"m": 1},
                     "effect_disposition": "ed", "finish_reason": "stop",
                     "reasoning": "r", "reasoning_content": "rc",
                     "reasoning_details": {"d": 1}, "codex_reasoning_items": [1],
                     "codex_message_items": {"x": 1}, "token_count": 9}})
add(FL, {"message": {"role": "u", "content": "c", "tool_calls": '[{"id": "c1"}]'}},
    {"json.loads": [{"id": "c1"}]})
add(FL, {"message": {"role": "u", "content": "c", "tool_calls": "bad"}},
    {"json.loads": {"__script__": True, "raise": {"type": "ValueError", "message": "Expecting value"}}})
add(FL, {"message": {"role": "u", "content": "c", "tool_calls": [{"id": "x"}]}})
add(FL, {"message": {"role": "u", "content": "c", "tool_calls": 5}})
add(FL, {"message": {"role": "u", "content": "c", "tool_calls": "notjson"}},
    {"json.loads": "notjson"})
add(FL, {"message": {"role": "u", "content": "c", "_tool_call_uids": {"a": "u1"},
                     "tool_call_uids": {"b": "u2"}}})
add(FL, {"message": {"role": "u", "content": "c", "tool_call_uids": '{"b": "u2"}'}},
    {"json.loads": {"b": "u2"}})
add(FL, {"message": {"role": "u", "content": "c", "_tool_call_uids": "bad"}},
    {"json.loads": {"__script__": True, "raise": {"type": "ValueError", "message": "Expecting value"}}})
add(FL, {"message": {"role": "u", "content": "c", "_tool_call_uids": "5"}},
    {"json.loads": 5})
add(FL, {"message": {"role": "u", "content": "c", "_absorbed_message_uids": '["a", "b"]'}},
    {"json.loads": ["a", "b"]})
add(FL, {"message": {"role": "u", "content": "c", "absorbed_message_uids": ["c"]}})
add(FL, {"message": {"role": "u", "content": "c", "_absorbed_message_uids": 5}})
add(FL, {"message": {"role": "u", "content": "c", "_tool_call_uid": "tu1", "tool_call_uid": "tu2"}})
add(FL, {"message": {"role": "u", "content": "c", "tool_call_uid": ""}})
add(FL, {"message": {"role": "u", "content": "c", "tool_call_uid": 5}})
add(FL, {"message": {"role": "u", "content": "c", "_row_id": 7, "_db_row_snapshot": "s1",
                     "message_uid": "m1"}})
add(FL, {"message": {"role": "u", "content": "c", "_row_id": True}})
add(FL, {"message": {"role": "u", "content": "c", "_row_id": fl(3.0)}})
add(FL, {"message": {"role": "u", "content": "c", "_row_id": "7"}})
add(FL, {"message": {"role": "u", "content": "c", "_db_row_snapshot": 5, "message_uid": ""}})
add(FL, {"message": {"role": "u", "content": "c", "message_uid": 9}})
add(FL, {"message": {"role": "u", "content": "c", "platform_message_id": "", "message_id": "pm"}})
add(FL, {"message": {"role": "u", "content": "c", "platform_message_id": "direct", "message_id": "pm"}})
add(FL, {"message": {"role": "u", "content": "c", "message_id": ""}})
add(FL, {"message": {"role": "u", "content": "c", "data_use": ["x", "y"]}})
add(FL, {"message": {"role": "u", "content": "c", "data_use": "ab"}})
add(FL, {"message": {"role": "u", "content": "c", "data_use": 5}})
add(FL, {"message": {"role": "u", "content": "c", "data_use": None}})
add(FL, {"message": {"role": "u", "content": "c", "_absorbed_row_ids": [1, 2, True, 0, -3, "x", fl(4.0)]}})
add(FL, {"message": {"role": "u", "content": "c", "_absorbed_row_ids": "nope"}})
add(FL, {"message": {"role": "u", "content": "c", "_restored_from_store": True}})
add(FL, {"message": {"role": "u", "content": "c", "_restored_from_store": 1}})
add(FL, {"message": {"role": "u", "content": "c", "_compressed_summary": 1, "observed": "yes"}})
add(FL, {"message": {"role": "", "content": "c"}})
add(FL, {"message": {"content": "c"}})

# ── RowOutcome.as_row_annotation ──
RA = "RowOutcome.as_row_annotation"
add(RA, {"outcome": ro(canonical_row={"k": "v"}, message_uid="mu",
                       tool_call_uids={"c": "u"}, tool_call_uid="tu")})
add(RA, {"outcome": ro()})
add(RA, {"outcome": ro(tool_call_uids={})})
add(RA, {"outcome": ro(id=fl(3.7))})
add(RA, {"outcome": ro(id="7")})
add(RA, {"outcome": ro(id=None)})
add(RA, {"outcome": ro(disposition="bogus")})
add(RA, {"outcome": {"id": 1}})
add(RA, {"outcome": ro(bogus=2)})

# ── BatchAppendResult.inserted ──
BI = "BatchAppendResult.inserted"
add(BI, {"outcomes": []})
add(BI, {"outcomes": [ro(), ro(id=2), ro(id=3)]})
add(BI, {"outcomes": [ro(), ro(id=2, disposition="repaired"), ro(id=3, disposition="adopted")]})
add(BI, {"outcomes": [ro(disposition="repaired"), ro(id=2, disposition="adopted")]})
add(BI, {"outcomes": [ro(disposition="bogus")]})
add(BI, {})

# ── batch_progress ──
BP = "batch_progress"
add(BP, {"error": {"attrs": {"batch_progress": pg([ro()], [ru(message_uid="mu")])}}})
add(BP, {"error": {"attrs": {"batch_progress": pg([ro(disposition="repaired"), ro(id=2)],
                                                  [ru(), ru(tool_call_uid="tu", tool_call_uids={"c": "u"})])}}})
add(BP, {"error": {"attrs": {"batch_progress": pg([ro(disposition="bogus")], [])}}})
add(BP, {"error": {"attrs": {"batch_progress": 5}}})
add(BP, {"error": {"attrs": {"batch_progress": None}}})
add(BP, {"error": {"attrs": {}}})
add(BP, {"error": 5})
add(BP, {"error": None})
add(BP, {"error": {"attrs": {"batch_progress": pg([], [])}}})

# ── HeldMessage.__post_init__ ──
HM = "HeldMessage.__post_init__"
add(HM, {"fields": {"role": "user", "content": "hi"}})
add(HM, {"fields": {"role": "user", "content": "hi", "tool_call_id": "c1",
                    "tool_calls": tu([{"id": "t"}]), "timestamp": fl(1.5), "row_id": 3,
                    "persisted": True, "db_row_snapshot": "s", "message_uid": "m",
                    "absorbed_row_ids": tu([1]), "merged_durable_rows": 2,
                    "retired_durable_rows": tu([hd({"role": "user", "content": "old"})]),
                    "tool_call_ids": tu(["t"]), "own_row": True, "retired_row": False}})
add(HM, {"fields": {"role": 5, "content": "c"}})
add(HM, {"fields": {"role": "", "content": "c"}})
add(HM, {"fields": {"role": "u", "content": "c", "tool_call_id": 5}})
add(HM, {"fields": {"role": "u", "content": "c", "tool_calls": []}})
add(HM, {"fields": {"role": "u", "content": "c", "timestamp": True}})
add(HM, {"fields": {"role": "u", "content": "c", "timestamp": "1"}})
add(HM, {"fields": {"role": "u", "content": "c", "timestamp": fl(0.5)}})
add(HM, {"fields": {"role": "u", "content": "c", "row_id": 0}})
add(HM, {"fields": {"role": "u", "content": "c", "row_id": True}})
add(HM, {"fields": {"role": "u", "content": "c", "persisted": 1}})
add(HM, {"fields": {"role": "u", "content": "c", "merged_durable_rows": -1}})
add(HM, {"fields": {"role": "u", "content": "c", "retired_durable_rows": []}})
add(HM, {"fields": {"role": "u", "content": "c", "retired_durable_rows": tu([{"role": "u", "content": "c"}])}})
add(HM, {"fields": {"role": "u", "content": "c", "retired_durable_rows": tu([hd({"role": "", "content": "c"})])}})
add(HM, {"fields": {"role": "u", "content": "c", "tool_call_ids": []}})
add(HM, {"fields": {"role": "u", "content": "c", "own_row": 1}})
add(HM, {"fields": {"role": "u", "content": "c", "retired_row": "x"}})
add(HM, {"fields": {"content": "c"}})
add(HM, {"fields": {"role": "u", "content": "c", "timestamp": None, "row_id": None}})

# ── HeldCoverage.__post_init__ ──
HC = "HeldCoverage.__post_init__"
add(HC, {"fields": {}})
add(HC, {"fields": {"covered_ids": tu([1, 2])}})
add(HC, {"fields": {"covered_ids": [1]}})
add(HC, {"fields": {"covered_ids": tu([True])}})
add(HC, {"fields": {"covered_ids": tu([0])}})
add(HC, {"fields": {"unresolved": tu([hd({"role": "u", "content": "c"})])}})
add(HC, {"fields": {"unresolved": tu([{"role": "u", "content": "c"}])}})
add(HC, {"fields": {"unresolved": []}})
add(HC, {"fields": {"unresolved": tu([hd({"role": "u", "content": "c", "persisted": 1})])}})
add(HC, {"fields": {"covered_ids": tu([1]), "unresolved": tu([hd({"role": "a", "content": "b"})])}})

# ── ReplaceResult.survivor_ids ──
SV = "ReplaceResult.survivor_ids"
add(SV, {"rows": []})
add(SV, {"rows": [rs(), rs(id=2), rs(id=3)]})
add(SV, {"rows": [rs(id=fl(3.7))]})
add(SV, {"rows": [rs(id="7")]})
add(SV, {})
add(SV, {"rows": [rs(id=None)]})

# ── stamp_live ──
SL = "stamp_live"
add(SL, {"messages": [{"role": "u", "content": "a", "_canonical_row": {"x": 1}},
                      {"role": "a", "content": "b"}], "result": None})
add(SL, {"messages": [{"role": "u", "content": "a", "_canonical_row": {"x": 1}}],
         "result": {"kind": "batch", "outcomes": [ro(canonical_row={"k": "v"}, message_uid="mu",
                                                     tool_call_uids={"c": "u"}, tool_call_uid="tu")]}})
add(SL, {"messages": [{"role": "u", "content": "a"}, {"role": "u", "content": "b"}],
         "result": {"kind": "batch", "outcomes": [ro()]}})
add(SL, {"messages": [{"role": "u", "content": "a"}, {"role": "u", "content": "b"}],
         "result": {"kind": "batch", "outcomes": [ro(), ro(id=None, disposition="repaired")]}})
add(SL, {"messages": [{"role": "u", "content": "a"}, {"role": "u", "content": "b"}],
         "result": {"kind": "progress", "outcomes": [ro()],
                    "uids": [ru(message_uid="mu", tool_call_uid="tu", tool_call_uids={"c": "u"}),
                             ru(message_uid="m2")]}})
add(SL, {"messages": [{"role": "u", "content": "a"}],
         "result": {"kind": "progress", "outcomes": [],
                    "uids": [ru(message_uid="", tool_call_uid=None, tool_call_uids={})]}})
add(SL, {"messages": [{"role": "u", "content": "a"}, {"role": "u", "content": "b"}],
         "result": {"kind": "replace", "rows": [rs(), rs(id=2, timestamp=2.5, snapshot="s2",
                                                           uid="u2", tool_call_uids={"c": "u"},
                                                           tool_call_uid="tu")]}})
add(SL, {"messages": [{"role": "u", "content": "a", "message_uid": "keep", "timestamp": fl(9.0)}],
         "result": {"kind": "replace", "rows": [rs(snapshot=None)]}})
add(SL, {"messages": [{"role": "u", "content": "a"}],
         "result": {"kind": "replace", "rows": [rs(uid="")]}})
add(SL, {"messages": [{"role": "u", "content": "a"}, {"role": "u", "content": "b"}],
         "result": {"kind": "replace", "rows": [rs()]}})
add(SL, {"messages": [], "result": {"kind": "batch", "outcomes": [ro()]}})
add(SL, {"messages": [{"role": "u", "content": "a"}],
         "result": {"kind": "progress", "outcomes": [ro(disposition="bogus")], "uids": [ru()]}})
add(SL, {"messages": [{"role": "u", "content": "a", "_canonical_row": None}],
         "result": {"kind": "replace", "rows": [rs(id="7", timestamp=1.0)]}})

# ── CompactionResult.row_ids ──
CR = "CompactionResult.row_ids"
add(CR, {"active_count": 0, "rows": []})
add(CR, {"active_count": 3, "rows": [rs(), rs(id=2), rs(id=3)]})
add(CR, {"active_count": 1, "rows": [rs(id=fl(3.7))]})
add(CR, {"rows": []})
add(CR, {"active_count": 1})
add(CR, {"active_count": 1, "rows": [rs(id="7")]})

# ── CompactionResult.row_snapshots ──
CS = "CompactionResult.row_snapshots"
add(CS, {"active_count": 0, "rows": []})
add(CS, {"active_count": 2, "rows": [rs(), rs(id=2, snapshot=None)]})
add(CS, {"active_count": 1, "rows": [rs(snapshot="")]})
add(CS, {"active_count": 1})
add(CS, {"active_count": 2, "rows": [rs(snapshot="a"), rs(id=2, snapshot="b")]})

# ── CooldownSnapshot.__post_init__ ──
CD = "CooldownSnapshot.__post_init__"
add(CD, {"fields": {"present": True, "until": fl(5.0), "error": "e"}})
add(CD, {"fields": {"present": False}})
add(CD, {"fields": {"present": False, "until": None, "error": None}})
add(CD, {"fields": {"present": False, "until": fl(1.0)}})
add(CD, {"fields": {"present": False, "error": "e"}})
add(CD, {"fields": {"present": 0, "until": fl(1.0)}})
add(CD, {"fields": {"present": "yes", "error": "e"}})
add(CD, {"fields": {}})
add(CD, {"fields": {"present": True}})

# ── ConversationRevision.as_tuple ──
CV = "ConversationRevision.as_tuple"
add(CV, {"fields": {"lineage_ids": tu(["s1", "s2"]), "active_count": 3,
                    "max_active_id": 9, "transcript_revision": 12}})
add(CV, {"fields": {"lineage_ids": ["s1"], "active_count": 0,
                    "max_active_id": 0, "transcript_revision": 0}})
add(CV, {"fields": {"lineage_ids": tu([]), "active_count": 1,
                    "max_active_id": 1, "transcript_revision": 1}})
add(CV, {"fields": {"lineage_ids": tu(["s1"])}})
add(CV, {"fields": {"lineage_ids": tu(["s1"]), "active_count": 2,
                    "max_active_id": 5, "transcript_revision": 7, "bogus": 1}})

# ── ResumeTooLarge.__init__ ──
RT = "ResumeTooLarge.__init__"
add(RT, {"message_count": 12, "limit": 10})
add(RT, {"message_count": 11, "limit": 10, "scope": "the tip session"})
add(RT, {"message_count": 0, "limit": 0})
add(RT, {"message_count": 100001, "limit": 100000})
add(RT, {"message_count": 1, "limit": 0, "scope": ""})

# ── DelegationDelivery.__post_init__ ──
DD = "DelegationDelivery.__post_init__"
add(DD, {"fields": {"content": "done", "metadata": {"delegation_id": "d1"}}})
add(DD, {"fields": {"content": "done", "metadata": {}}})
add(DD, {"fields": {"content": "done", "metadata": {"delegation_id": ""}}})
add(DD, {"fields": {"content": "done", "metadata": {"delegation_id": False}}})
add(DD, {"fields": {"content": "done", "metadata": {"delegation_id": 0}}})
add(DD, {"fields": {"content": "done", "metadata": None}})
add(DD, {"fields": {"content": "done", "metadata": []}})
add(DD, {"fields": {"content": "done"}})
add(DD, {"fields": {"content": "done", "metadata": {"delegation_id": 5}}})

# ── DelegationDelivery.identity ──
DI = "DelegationDelivery.identity"
add(DI, {"fields": {"content": "done", "metadata": {"delegation_id": "d1", "delivery_notice": "n"}}})
add(DI, {"fields": {"content": "done", "metadata": {"delegation_id": "d1"}}})
add(DI, {"fields": {"content": "done", "metadata": {"delegation_id": "d1", "delivery_notice": None}}})
add(DI, {"fields": {"content": "done", "metadata": {"delegation_id": "d1", "delivery_notice": ""}}})
add(DI, {"fields": {"content": "done", "metadata": {"delegation_id": 5}}})
add(DI, {"fields": {"content": "done", "metadata": {"delegation_id": "d1", "delivery_notice": fl(1.5)}}})

# ── ConversationMessage.as_openai_message ──
AO = "ConversationMessage.as_openai_message"
add(AO, {"fields": {"role": "user", "content": "hi"}})
add(AO, {"fields": {"role": "assistant", "content": None, "db_persisted": False}})
add(AO, {"fields": {"role": "u", "content": "c", "db_persisted": 0}})
add(AO, {"fields": {"role": "assistant", "content": "x", "db_row_snapshot": "s", "row_id": 5,
                    "message_uid": "m", "tool_call_uids": {"c": "u"}, "tool_call_uid": "tu",
                    "absorbed_message_uids": tu(["a"]), "api_content": "ap", "display_kind": "dk",
                    "display_metadata": {"m": 1}, "data_use": tu(["x"]), "compressed_summary": True,
                    "timestamp": fl(1.5), "tool_call_id": "c1", "tool_name": "t",
                    "effect_disposition": "ed", "tool_calls": tu([{"id": "t"}]), "message_id": "pm",
                    "observed": True, "finish_reason": "stop", "reasoning": "r",
                    "reasoning_content": "rc", "reasoning_details": {"d": 1},
                    "codex_reasoning_items": [1], "codex_message_items": {"x": 1},
                    "absorbed_row_ids": tu([1, 2]), "merged_turn_prefix": "mp",
                    "merged_durable_rows": 2, "unnamed_durable_rows": 1,
                    "retired_durable_rows": [{"role": "u"}]}})
add(AO, {"fields": {"role": "u", "content": "c", "tool_call_uid": "u9", "_tool_call_uid_stored": True}})
add(AO, {"fields": {"role": "u", "content": "c", "tool_call_uid": "u9", "_tool_call_uid_stored": True,
                    "absorbed_message_uids": tu(["a", "b"])}})
add(AO, {"fields": {"role": "u", "content": "c", "tool_call_uid": "", "_tool_call_uid_stored": True}})
add(AO, {"fields": {"role": "u", "content": "c", "tool_call_uid": "u9"}})
add(AO, {"fields": {"role": "u", "content": "c", "reasoning_content": "", "reasoning_details": [],
                    "codex_reasoning_items": [], "codex_message_items": None}})
add(AO, {"fields": {"role": "u", "content": "c", "row_id": fl(3.7)}})
add(AO, {"fields": {"role": "u", "content": "c", "row_id": "7"}})
add(AO, {"fields": {"role": "u", "content": "c", "tool_calls": 5}})
add(AO, {"fields": {"role": "u", "content": "c", "absorbed_row_ids": tu(["x"])}})
add(AO, {"fields": {"role": "u", "content": "c", "timestamp": 0, "message_uid": "",
                    "data_use": tu([]), "observed": False, "compressed_summary": False}})
add(AO, {"fields": {"role": "u", "content": "c", "absorbed_message_uids": tu(["a"]),
                    "data_use": tu(["x", "y"]), "tool_calls": tu([])}})
add(AO, {"fields": {"role": "u", "content": "c", "row_id": True}})
add(AO, {"fields": {"role": "u"}})

# ── ConversationMessage.from_openai_message ──
FO = "ConversationMessage.from_openai_message"
add(FO, {"message": {"role": "user", "content": "hi"}})
add(FO, {"message": {"role": "user", "content": "hi", "_db_persisted": True}})
add(FO, {"message": {"role": "u", "content": "c", "_db_persisted": 1}})
add(FO, {"message": {"role": "u", "content": "c", "_db_persisted": False}})
add(FO, {"message": {"role": "u", "content": "c", "bogus": 1}})
add(FO, {"message": {"content": "c"}})
add(FO, {"message": {"role": "u", "content": "c", "_tool_call_uids": {"a": "b"}, "_tool_call_uid": "u1"}})
add(FO, {"message": {"role": "u", "content": "c", "api_content": "ap", "_tool_call_uid": "u1"}})
add(FO, {"message": {"role": "u", "content": "c", "_tool_call_uid": "u1", "timestamp": fl(1.0)}})
add(FO, {"message": {"role": "u", "content": "c", "_row_id": "7"}})
add(FO, {"message": {"role": "u", "content": "c", "_row_id": fl(3.7)}})
add(FO, {"message": {"role": "u", "content": "c", "_row_id": None}})
add(FO, {"message": {"role": "u", "content": "c", "_row_id": "x"}})
add(FO, {"message": {"role": "u", "content": "c", "tool_calls": [{"id": "t"}]}})
add(FO, {"message": {"role": "u", "content": "c", "tool_calls": "ab"}})
add(FO, {"message": {"role": "u", "content": "c", "tool_calls": 5}})
add(FO, {"message": {"role": "u", "content": "c", "_absorbed_row_ids": [1, 2]}})
add(FO, {"message": {"role": "u", "content": "c", "_absorbed_row_ids": ["x"]}})
add(FO, {"message": {"role": "u", "content": "c", "_absorbed_row_ids": None}})
add(FO, {"message": {"role": "u", "content": "c", "data_use": {"a": 1, "b": 2}}})
add(FO, {"message": {"role": "u", "content": "c", "observed": 1, "_compressed_summary": "yes"}})
add(FO, {"message": {"role": 5, "content": "c"}})
add(FO, {"message": {"role": "u", "content": "c", "_db_persisted": True, "_row_id": 5,
                    "message_uid": "m", "_tool_call_uids": {"c": "u"},
                    "_absorbed_message_uids": ["a"], "api_content": "ap", "display_kind": "dk",
                    "display_metadata": {"m": 1}, "data_use": ["x"], "_compressed_summary": True,
                    "timestamp": fl(1.5), "tool_call_id": "c1", "tool_name": "t",
                    "effect_disposition": "ed", "tool_calls": [{"id": "t"}], "message_id": "pm",
                    "observed": True, "finish_reason": "stop", "reasoning": "r",
                    "reasoning_content": "rc", "reasoning_details": {"d": 1},
                    "codex_reasoning_items": [1], "codex_message_items": {"x": 1},
                    "_tool_call_uid": "tu", "_absorbed_row_ids": [1, 2], "_merged_turn_prefix": "mp",
                    "_merged_durable_rows": 2, "_unnamed_durable_rows": 1,
                    "_retired_durable_rows": [{"role": "u"}]}})
add(FO, {"message": {"role": "u", "content": "c", "_absorbed_message_uids": "ab"}})
add(FO, {"message": {"role": "u", "content": "c", "weird'key": 1}})

# ── TranscriptRead.__post_init__ ──
TR = "TranscriptRead.__post_init__"
add(TR, {"fields": {}})
add(TR, {"fields": {"after_id": 5, "latest": True}})
add(TR, {"fields": {"after_id": 5, "offset": 2}})
add(TR, {"fields": {"after_id": 5, "view": "display"}})
add(TR, {"fields": {"after_id": 5, "include_lineage": True}})
add(TR, {"fields": {"after_id": 5, "view": "all"}})
add(TR, {"fields": {"after_id": None, "latest": True, "offset": 3}})
add(TR, {"fields": {"after_id": 5, "offset": 0, "view": "active"}})
add(TR, {"fields": {"view": "bogus"}})
add(TR, {"fields": {"after_id": 5, "latest": "yes"}})
add(TR, {"fields": {"view": "display", "limit": 10, "offset": 2}})

# ── TranscriptRow.as_wire_row ──
WR = "TranscriptRow.as_wire_row"
add(WR, {"fields": {"id": 3, "session_id": "s", "role": "user", "content": "c",
                    "timestamp": fl(1.5), "visibility": "active"}})
add(WR, {"fields": {"id": 3, "session_id": "s", "role": "user", "content": "c",
                    "timestamp": fl(1.5), "visibility": "compaction_archived",
                    "observed": True, "compressed_summary": True}})
add(WR, {"fields": {"id": 3, "session_id": "s", "role": "user", "content": "c",
                    "timestamp": fl(1.5), "visibility": "rewound"}})
add(WR, {"fields": {"id": 3, "session_id": "s", "role": "tool", "content": {"parts": [1]},
                    "timestamp": fl(1.5), "visibility": "active", "tool_call_id": "c1",
                    "tool_calls": [{"id": "t"}], "tool_name": "t", "effect_disposition": "ed",
                    "token_count": 5, "finish_reason": "stop", "reasoning": "r",
                    "reasoning_content": "rc", "reasoning_details": "rd",
                    "codex_reasoning_items": "cri", "codex_message_items": "cmi",
                    "platform_message_id": "pm", "api_content": "ap", "display_kind": "dk",
                    "display_metadata": {"m": 1}, "message_uid": "m",
                    "absorbed_message_uids": "amu", "tool_call_uids": "tcu", "tool_call_uid": "tcuid"}})
add(WR, {"fields": {"id": "7", "session_id": "s", "role": "u", "content": "c",
                    "timestamp": fl(1.0), "visibility": "active"}})
add(WR, {"fields": {"id": None, "session_id": "s", "role": "u", "content": "c",
                    "timestamp": fl(1.0), "visibility": "active"}})
add(WR, {"fields": {"id": 1, "session_id": 9, "role": "u", "content": "c",
                    "timestamp": fl(1.0), "visibility": "active"}})
add(WR, {"fields": {"id": 1, "session_id": "s", "role": "u", "content": "c",
                    "timestamp": fl(1.0), "visibility": "bogus"}})
add(WR, {"fields": {"id": 1}})
add(WR, {"fields": {"id": fl(3.7), "session_id": "s", "role": "u", "content": "c",
                    "timestamp": fl(1.0), "visibility": "active", "observed": "yes"}})

# ── ReadCheck.__init__ ──
RI = "ReadCheck.__init__"
add(RI, {"check": "seam", "pinned": True})
add(RI, {"check": "seam", "pinned": False})
add(RI, {"check": "seam", "pinned": "x"})
add(RI, {"check": "seam"})
add(RI, {"check": 5, "pinned": True})
add(RI, {"pinned": True})

# ── ReadCheck.__call__ ──
RC = "ReadCheck.__call__"
add(RC, {"check": "seam", "pinned": True}, {"check": True})
add(RC, {"check": "seam", "pinned": True}, {"check": False})
add(RC, {"check": "seam", "pinned": True}, {"check": 5})
add(RC, {"check": "seam", "pinned": True}, {"check": None})
add(RC, {"check": "seam", "pinned": True}, {"check": ""})
add(RC, {"check": "seam", "pinned": True}, {"check": [1]})
add(RC, {"check": "seam", "pinned": True},
    {"check": {"__script__": True, "raise": {"type": "ValueError", "message": "check failed"}}})
add(RC, {"check": "seam", "pinned": True},
    {"check": {"__script__": True, "raise": {"type": "SystemExit", "message": "bye"}}})
add(RC, {"check": "seam", "pinned": True}, {"check": [True, False]})
add(RC, {"check": 5, "pinned": True})
add(RC, {"check": None, "pinned": True})
add(RC, {"check": "seam", "pinned": True}, fail_closed=True)

# ── _same ──
for v in [5, "txt", None, {"a": 1}, [1, 2], fl(1.5), True]:
    add("_same", {"value": v})


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for case in CASES:
            f.write(json.dumps(case, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"wrote {len(CASES)} cases -> {OUT}")
    for fn in sorted(counts):
        print(f"  {fn}: {counts[fn]}")
    thin = [fn for fn, n in counts.items() if n < 5]
    if thin:
        raise SystemExit(f"FUNCS WITH <5 CASES: {thin}")


if __name__ == "__main__":
    main()