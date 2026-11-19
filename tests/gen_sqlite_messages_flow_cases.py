#!/usr/bin/env python3
"""Deterministic case generator for the sqlite_messages_flow oracle.

Writes tests/fixtures/sqlite_messages_flow.jsonl: {"func", "in"} per line,
>=5 cases per ported func (31 funcs). Covers happy paths, absent/null/empty
edges, scripted seam raises (real exception classes incl. the BaseException
trio), fail-closed missing/exhausted scripts, the mutating-verb stamp scripts,
the append_batch batch_progress attachment, and the documented ASCII fixture
domain (valid post-__post_init__ record specs, canonical strict JSON text
cells, finite int()/float() strings, decimal-string stored-map keys).

Run: python tests/gen_sqlite_messages_flow_cases.py
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "sqlite_messages_flow.jsonl")

CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def script(ret):
    return {"__script__": True, "ret": ret}


def rs(cls, msg, **extra):
    d = {"__raise__": cls, "message": msg}
    d.update(extra)
    return d


def nm(**kw):
    d = {"role": "user", "content": "hi"}
    d.update(kw)
    return d


def hm(**kw):
    d = {"role": "user", "content": "held"}
    d.update(kw)
    return d


def row(**kw):
    d = {"id": 1, "session_id": "s1", "role": "user", "content": "hi",
         "timestamp": 100.0, "active": 1, "compacted": 0}
    d.update(kw)
    return d


def change(**kw):
    d = {"original": hm(row_id=5), "replacement": nm(content="new"),
         "before": nm(content="old")}
    d.update(kw)
    return d


FK = "FOREIGN KEY constraint failed: session 's1' has no row"
REWIND_TEXT = "rewind target changed before it could be persisted"
HISTORY_TEXT = "session history changed before the rewind could be persisted"

# ── _visibility ──
add("_visibility", active=1, compacted=0)
add("_visibility", active=True, compacted=True)
add("_visibility", active=0, compacted=1)
add("_visibility", active=None, compacted=None)
add("_visibility", active="", compacted="x")
add("_visibility", active=[], compacted=[])
add("_visibility", active=0.0, compacted=False)

# ── _sidecar ──
add("_sidecar", value='{"a": 1}')
add("_sidecar", value="[1, 2, 3]")
add("_sidecar", value='"text"')
add("_sidecar", value="42")
add("_sidecar", value="true")
add("_sidecar", value="null")
add("_sidecar", value="bad")
add("_sidecar", value="01")
add("_sidecar", value='{"a": 1,}')
add("_sidecar", value="")
add("_sidecar", value=5)
add("_sidecar", value=None)
add("_sidecar", value=[1])
add("_sidecar", value=False)
add("_sidecar", value='{"nested": {"x": [1, null, true]}}')

# ── _message_dict ──
add("_message_dict", message=nm())
add("_message_dict", message=nm(timestamp=0, tool_calls=[{"id": "c1"}],
                                tool_name="t", tool_call_id="c1", display_kind="dk",
                                api_content="api", platform_message_id="p1",
                                effect_disposition="ed", finish_reason="stop",
                                reasoning="r", reasoning_content="rc",
                                reasoning_details={"k": 1}, codex_reasoning_items=[1],
                                codex_message_items=[2], token_count=9,
                                message_uid="u1"))
add("_message_dict", message=nm(timestamp=5.5, display_metadata={"m": 1},
                                compressed_summary=True, observed=True))
add("_message_dict", message=nm(tool_calls=[]))
add("_message_dict", message=nm(repair_target="7", db_row_snapshot="snap"))
add("_message_dict", message=nm(absorbed_message_uids=["a", "b"],
                                tool_call_uids={"c1": "u"}, tool_call_uid="tu",
                                data_use=["d1"], merged_durable_rows=2,
                                unnamed_durable_rows=1, absorbed_row_ids=[3, 4],
                                restored_from_store=True))
add("_message_dict", message=nm(repair_target="abc"))
add("_message_dict", message=nm(merged_durable_rows=0, unnamed_durable_rows=0,
                                absorbed_row_ids=[], data_use=[]))
add("_message_dict", message={"role": "user", "content": "hi", "bogus": 1})
add("_message_dict", message={"content": "hi"})
add("_message_dict", message=None)
add("_message_dict", message=nm(display_metadata={}))

# ── _held_dict ──
add("_held_dict", held=hm())
add("_held_dict", held=hm(tool_call_id="c1", tool_calls=[{"id": "c1"}],
                          timestamp=0, row_id=3, persisted=True,
                          db_row_snapshot="snap", message_uid="u1"))
add("_held_dict", held=hm(absorbed_row_ids=[2, 3], merged_durable_rows=2,
                          tool_call_ids=["c1", "c2"], own_row=True, retired_row=True))
add("_held_dict", held=hm(retired_durable_rows=[hm(role="tool", content="r"),
                                                hm(role="user", content="u")]))
add("_held_dict", held=hm(row_id=9))
add("_held_dict", held=hm(timestamp=2.5, tool_call_id=None, message_uid=""))
add("_held_dict", held={"role": "user"})
add("_held_dict", held=None)

# ── _pruned_original ──
add("_pruned_original", change=change())
add("_pruned_original", change=change(before=nm(content="old", repair_target=8,
                                                  db_row_snapshot="bsnap", timestamp=3.0),
                                      original=hm(row_id=5, timestamp=9.0, message_uid="ou")))
add("_pruned_original", change=change(before=nm(content="old", tool_calls=[{"id": "c"}]),
                                      original=hm(tool_calls=[{"id": "h"}], tool_call_id="tc")))
add("_pruned_original", change=change(original=hm(row_id=5, persisted=True,
                                                  db_row_snapshot="osnap")))
add("_pruned_original", change={"original": hm(), "replacement": nm(),
                                "before": nm(), "extra": 1})
add("_pruned_original", change={"original": hm(), "replacement": nm()})
add("_pruned_original", change={})
add("_pruned_original", change={"original": hm()})
add("_pruned_original", change=None)

# ── _transcript_row ──
add("_transcript_row", row=row())
add("_transcript_row", row={"id": 2, "session_id": "s2"})
add("_transcript_row", row=row(tool_calls=[{"id": "c"}], display_metadata={"m": 1},
                               message_uid="u", observed=1, token_count=5,
                               reasoning_details="raw", tool_call_uids="rawuids"))
add("_transcript_row", row=row(active=0, compacted=1))
add("_transcript_row", row=row(active=0, compacted=0))
add("_transcript_row", row=row(_compressed_summary=True))
add("_transcript_row", row=row(_compressed_summary=1))
add("_transcript_row", row=row(timestamp="1.5"))
add("_transcript_row", row={"session_id": "s1"})
add("_transcript_row", row={"id": 1})
add("_transcript_row", row=None)
add("_transcript_row", row=[1])
add("_transcript_row", row=row(id="abc"))

# ── _message_record ──
add("_message_record", row=row())
add("_message_record", row=row(display_metadata={"reactions": [{"emoji": "x"}], "m": 1}))
add("_message_record", row=row(display_metadata={"reactions": [{"emoji": "x"}]}))
add("_message_record", row=row(display_metadata="notdict"))
add("_message_record", row=row(reasoning_details='{"a": 1}',
                               codex_reasoning_items="[1]",
                               codex_message_items="bad"))
add("_message_record", row=row(tool_calls=[{"id": "c"}], tool_name="t",
                               tool_call_id="c1", display_kind="dk",
                               _compressed_summary=1, api_content="api",
                               platform_message_id="p", observed=1,
                               effect_disposition="ed", finish_reason="stop",
                               reasoning="r", reasoning_content="rc", message_uid="u"))
add("_message_record", row=row(timestamp="2.5"))
add("_message_record", row=row(tool_calls=[]))
add("_message_record", row=row(tool_calls=0))
add("_message_record", row=row(tool_calls=5))
add("_message_record", row={"id": 1, "role": "user"})
add("_message_record", row={"role": "user", "timestamp": 1.0})
add("_message_record", row=None)
add("_message_record", row=row(active=0, compacted=1))

# ── _stamp ──
add("_stamp", msg={"_row_id": 5, "timestamp": 2.0, "message_uid": "u1"}, stored={})
add("_stamp", msg={"_row_id": 5}, stored={"5": {"timestamp": 9.0}})
add("_stamp", msg={"_row_id": 5}, stored={})
add("_stamp", msg={"_row_id": 5, "message_uid": ""},
    stored={"5": {"timestamp": 1.0, "message_uid": "su"}})
add("_stamp", msg={"_row_id": 5, "message_uid": ""},
    stored={"5": {"timestamp": 1.0, "message_uid": ""}})
add("_stamp", msg={"_row_id": 5, "message_uid": ""}, stored={})
add("_stamp", msg={"_row_id": 5, "timestamp": "3.5", "_db_row_snapshot": "snap",
                   "_tool_call_uids": {"c": "u"}, "_tool_call_uid": "tu"}, stored={})
add("_stamp", msg={"_row_id": 5}, stored={"5": {"timestamp": "4.5"}})
add("_stamp", msg={"_row_id": 5}, stored={"5": {}})
add("_stamp", msg={}, stored={})
add("_stamp", msg=None, stored={})
add("_stamp", msg={"_row_id": "abc"}, stored={})

# ── _row_outcomes ──
add("_row_outcomes", rows=[{"_row_id": 5, "timestamp": 1.0},
                           {"_row_id": 3, "timestamp": 2.0}], count=1)
add("_row_outcomes", rows=[{"_row_id": 5, "timestamp": 1.0},
                           {"_row_id": 3, "timestamp": 2.0}], count=2)
add("_row_outcomes", rows=[{"_row_id": 5, "timestamp": 1.0},
                           {"_row_id": 3, "timestamp": 2.0}], count=0)
add("_row_outcomes", rows=[{"_row_id": 5, "timestamp": 1.0,
                           "_canonical_row": {"_metadata_only": True}},
                          {"_row_id": 3, "timestamp": 2.0, "_canonical_row": {}},
                          {"_row_id": 1, "timestamp": 3.0}], count=1)
add("_row_outcomes", rows=[{"_row_id": 5, "timestamp": 1.0},
                           {"_row_id": 5, "timestamp": 2.0}], count=1)
add("_row_outcomes", rows=[{"_row_id": 5, "timestamp": 1.0}], count=None)
add("_row_outcomes", rows=[{"_row_id": 5, "timestamp": 1.0}], count=9)
add("_row_outcomes", rows=[{"_row_id": 5}], count=1)
add("_row_outcomes", rows=[{"timestamp": 1.0}], count=1)
add("_row_outcomes", rows=None, count=1)
add("_row_outcomes", rows=[], count=0)
add("_row_outcomes", rows=[{"_row_id": 5, "timestamp": 1.0,
                           "_canonical_row": "notdict"}], count=0)
add("_row_outcomes", rows=[{"_row_id": 5, "timestamp": 1.0}], count="x")

# ── _conversation ──
add("_conversation", messages=[{"role": "user", "content": "hi"}],
    io={"from_openai_message": script({"role": "user", "content": "hi"})})
add("_conversation", messages=[{"role": "user", "content": "a"},
                               {"role": "assistant", "content": "b"}],
    io={"from_openai_message": [{"role": "user", "content": "a", "timestamp": 1.0},
                                {"role": "assistant", "content": "b",
                                 "tool_calls": [{"id": "c"}], "db_persisted": False,
                                 "absorbed_message_uids": ["x"], "data_use": ["d"]}]})
add("_conversation", messages=[], io={"from_openai_message": script({})})
add("_conversation", messages=[{"role": "user", "content": "hi"}],
    io={"from_openai_message": rs("ValueError", "unknown replay projection key 'bogus'")})
add("_conversation", messages=[{"role": "user", "content": "hi"}], io={})
add("_conversation", messages=[{"role": "user", "content": "hi"}],
    io={"from_openai_message": script({"content": "hi"})})
add("_conversation", messages=[{"role": "user", "content": "hi"}],
    io={"from_openai_message": script({"role": "user", "content": "hi", "bogus": 1})})
add("_conversation", messages=None, io={})
add("_conversation", messages=[{"role": "user", "content": "hi"}],
    io={"from_openai_message": script(5)})

# ── _refusals ──
add("_refusals", session_id="s1", body={"ret": 42})
add("_refusals", session_id="s1", body={})
add("_refusals", session_id="s1",
    body={"raise": rs("TransientStoreError", "disk write failed",
                      cause=rs("OSError", "FOREIGN KEY constraint failed"))},
    io={"classify_persistence_error": "session_row_missing"})
add("_refusals", session_id="s1",
    body={"raise": rs("TransientStoreError", "disk write failed",
                      cause=rs("OSError", "database is locked"))},
    io={"classify_persistence_error": "locked"})
add("_refusals", session_id="s1",
    body={"raise": rs("TransientStoreError", "no cause")},
    io={"classify_persistence_error": "unknown"})
add("_refusals", session_id="s1",
    body={"raise": rs("TransientStoreError", "disk write failed",
                      cause=rs("OSError", "fk"))}, io={})
add("_refusals", session_id=None,
    body={"raise": rs("TransientStoreError", "disk write failed",
                      cause=rs("OSError", "fk"))})
add("_refusals", session_id="s1", body={"raise": rs("RuntimeError", REWIND_TEXT)})
add("_refusals", session_id="s1", body={"raise": rs("RuntimeError", HISTORY_TEXT)})
add("_refusals", session_id="s1",
    body={"raise": rs("RuntimeError", "rewind target has no durable row identity")})
add("_refusals", session_id="s1",
    body={"raise": rs("RuntimeError", "rewind did not retain its compaction handoff")})
add("_refusals", session_id="s1", body={"raise": rs("RuntimeError", "some other failure")})
add("_refusals", session_id=None, body={"raise": rs("RuntimeError", REWIND_TEXT)})
add("_refusals", session_id="s1", body={"raise": rs("ValueError", "bad input")})
add("_refusals", session_id="s1", body={"raise": rs("KeyError", "missing")})
add("_refusals", session_id="s1", body={"raise": rs("SystemExit", "bye")})
add("_refusals", session_id="s1", body={"raise": rs("KeyboardInterrupt", "")})
add("_refusals", session_id="s1", body={"raise": rs("GeneratorExit", "")})
add("_refusals", session_id="s1", body={"raise": rs("CustomFailure", "custom")})
add("_refusals", session_id="s1",
    body={"raise": rs("RewindRefused", REWIND_TEXT)},
    io={"classify_persistence_error": "unknown"})
add("_refusals", session_id="s1",
    body={"raise": rs("TransientStoreError", "x", cause=rs("OSError", "fk"))},
    io={"classify_persistence_error": rs("RuntimeError", "classifier broke")})

# ── _append_chunk ──
add("_append_chunk", session_id="s1", messages=[], io={})
add("_append_chunk", session_id="s1", messages=None, io={})
add("_append_chunk", session_id="s1", messages=[{"role": "user", "content": "hi"}],
    guards={"turn_lease_holder": "w1", "turn_lease_ttl_seconds": 60.0},
    io={"_append_messages_in_transaction": script(
        {"inserted": 1, "stamps": [{"_row_id": 10, "timestamp": 5.0,
                                    "_db_row_snapshot": "snap10"}]})})
add("_append_chunk", session_id="s1",
    messages=[{"role": "user", "content": "x", "_restored_from_store": True,
               "_db_persisted": True, "message_uid": "u1"}],
    io={"_append_messages_in_transaction": script({"inserted": 0, "stamps": [{}]}),
        "message_uid_or_none": "u1",
        "_active_logical_message_row": script({"id": 7, "timestamp": 3.0}),
        "transcript_row_snapshot": "snap7",
        "_restore_row_identity": script({"msg": {"message_uid": "u1"}})})
add("_append_chunk", session_id="s1",
    messages=[{"role": "user", "content": "x", "_restored_from_store": True,
               "_db_persisted": True, "message_uid": "u1"}],
    io={"_append_messages_in_transaction": script({"inserted": 0, "stamps": [{}]}),
        "message_uid_or_none": "u1",
        "_active_logical_message_row": script(None)})
add("_append_chunk", session_id="s1",
    messages=[{"content": "x", "_restored_from_store": True, "_db_persisted": True}],
    io={"_append_messages_in_transaction": script({"inserted": 0, "stamps": [{}]})})
add("_append_chunk", session_id="s1", messages=[{"role": "user", "content": "hi"}],
    io={"_append_messages_in_transaction": rs("RuntimeError", "write failed")})
add("_append_chunk", session_id="s1", messages=[{"role": "user", "content": "hi"}], io={})
add("_append_chunk", session_id="s1",
    messages=[{"role": "user", "content": "x", "_restored_from_store": True,
               "_db_persisted": True, "_row_id": 4}],
    io={"_append_messages_in_transaction": script({"inserted": 1, "stamps": [{}]})})
# ── append ──
add("append", session_id="s1", message=nm(), io={"append_message": 42})
add("append", session_id="s1",
    message=nm(timestamp=5.0, tool_calls=[{"id": "c1"}], tool_name="t",
               tool_call_id="c1", display_metadata={"m": 1}, observed=True,
               compressed_summary=True, message_uid="u1", token_count=3),
    turn_lease_holder="w1", turn_lease_ttl_seconds=60.0,
    io={"append_message": 7})
add("append", session_id="s1", message=nm(),
    io={"append_message": rs("RuntimeError", REWIND_TEXT)})
add("append", session_id="s1", message=nm(),
    io={"append_message": rs("TransientStoreError", "fk failed",
                             cause=rs("OSError", "FOREIGN KEY constraint failed")),
        "classify_persistence_error": "session_row_missing"})
add("append", session_id="s1", message=nm(), io={})
add("append", session_id="s1", message=nm(),
    io={"append_message": rs("SystemExit", "bye")})
add("append", session_id="s1", message=nm(bogus=1), io={"append_message": 1})
add("append", session_id="s1", message=nm(),
    io={"append_message": rs("KeyboardInterrupt", "")})

# ── append_batch ──
add("append_batch", session_id="s1", messages=[], io={})
add("append_batch", session_id="s1", messages=[nm(), nm(content="second")],
    io={"_append_messages_in_transaction": script(
        {"inserted": 2, "stamps": [
            {"_row_id": 10, "timestamp": 1.0, "message_uid": "u1", "_db_row_snapshot": "s10"},
            {"_row_id": 11, "timestamp": 2.0, "message_uid": "u2", "_db_row_snapshot": "s11"}]})})
add("append_batch", session_id="s1",
    messages=[nm(tool_calls=[{"id": "c1"}]), nm(content="second"), nm(content="third")],
    chunk_rows=2,
    io={"_parse_tool_calls": [None, None, None],
        "_stamp_tool_call_uids": [
            script({"msg": {"_tool_call_uids": {"c1": "tu1"}}, "index": {"c1": "tu1"}}),
            script({"msg": {}, "index": {}}),
            script({"msg": {"_tool_call_uid": "tu1"}, "index": {"c1": "tu1"}})],
        "_append_messages_in_transaction": [
            {"inserted": 2, "stamps": [
                {"_row_id": 10, "timestamp": 1.0, "_db_row_snapshot": "s10"},
                {"_row_id": 11, "timestamp": 2.0, "_db_row_snapshot": "s11"}]},
            {"inserted": 1, "stamps": [
                {"_row_id": 12, "timestamp": 3.0, "_db_row_snapshot": "s12"}]}]})
add("append_batch", session_id="s1",
    messages=[nm(), nm(content="second"), nm(content="third")], chunk_rows=2,
    io={"_parse_tool_calls": [None, None, None],
        "_stamp_tool_call_uids": [script({"msg": {}, "index": {}})] * 3,
        "_append_messages_in_transaction": [
            {"inserted": 2, "stamps": [
                {"_row_id": 10, "timestamp": 1.0, "message_uid": "u1"},
                {"_row_id": 11, "timestamp": 2.0}]},
            rs("RuntimeError", "boom")]})
add("append_batch", session_id="s1", messages=[nm()],
    io={"_append_messages_in_transaction": script({"inserted": 1, "stamps": [{}]})})
add("append_batch", session_id="s1",
    messages=[nm(restored_from_store=True, message_uid="u1")],
    io={"_append_messages_in_transaction": script(
            {"inserted": 0, "stamps": [{"_db_persisted": True}]}),
        "message_uid_or_none": "u1",
        "_active_logical_message_row": script({"id": 7, "timestamp": 3.0}),
        "transcript_row_snapshot": "snap7",
        "_restore_row_identity": script({"msg": {"message_uid": "u1"}})})
add("append_batch", session_id="s1", messages=[nm(), nm(content="second")],
    chunk_rows=1,
    io={"_parse_tool_calls": [None, None],
        "_stamp_tool_call_uids": [script({"msg": {}, "index": {}})] * 2,
        "_append_messages_in_transaction": [
            {"inserted": 1, "stamps": [{"_row_id": 10, "timestamp": 1.0}]},
            rs("SystemExit", "halt")]})
add("append_batch", session_id="s1", messages=[nm(), nm(content="second")],
    chunk_rows=0,
    io={"_parse_tool_calls": [None, None],
        "_stamp_tool_call_uids": [script({"msg": {}, "index": {}})] * 2})
add("append_batch", session_id="s1", messages=[nm(), nm(content="second")],
    chunk_rows=-1,
    io={"_parse_tool_calls": [None, None],
        "_stamp_tool_call_uids": [script({"msg": {}, "index": {}})] * 2})
add("append_batch", session_id="s1", messages=[nm(), nm(content="second")],
    chunk_rows=5,
    io={"_append_messages_in_transaction": script(
        {"inserted": 2, "stamps": [
            {"_row_id": 10, "timestamp": 1.0}, {"_row_id": 11, "timestamp": 2.0}]})})
add("append_batch", session_id="s1", messages=[nm()], io={})
add("append_batch", session_id="s1", messages=[nm()], chunk_rows=1,
    io={"_parse_tool_calls": [None],
        "_stamp_tool_call_uids": [script({"msg": {}, "index": {}})],
        "_append_messages_in_transaction": script(
            {"inserted": 1, "stamps": [{"_row_id": 10, "timestamp": 1.0}]})})

# ── replace_transcript ──
add("replace_transcript", session_id="s1", messages=[nm(), nm(content="second")],
    mode="destructive",
    io={"replace_messages": script({"stamps": [
        {"_row_id": 10, "timestamp": 1.0, "message_uid": "u1", "_db_row_snapshot": "s10"},
        {"_row_id": 11, "timestamp": 2.0, "message_uid": "u2", "_db_row_snapshot": "s11"}]})})
add("replace_transcript", session_id="s1", messages=[nm(db_row_snapshot="keepme")],
    mode="preserve_existing_archive", reject_active_turn_lease=True,
    io={"replace_messages": script({"stamps": [
        {"_row_id": 10, "timestamp": 1.0, "message_uid": "u1", "_db_row_snapshot": "s10"}]})})
add("replace_transcript", session_id="s1", messages=[nm()], mode="archive_dropped",
    io={"replace_messages": script({"stamps": [
        {"_row_id": 10, "timestamp": 1.0, "message_uid": "u1"}]})})
add("replace_transcript", session_id="s1", messages=[nm(), nm(content="second")],
    mode="destructive",
    io={"replace_messages": script({"stamps": [{"_row_id": 10}, {"_row_id": 11}]}),
        "get_messages": script([
            {"id": 10, "timestamp": 5.0, "message_uid": "su10"},
            {"id": 11, "timestamp": 6.0}])})
add("replace_transcript", session_id="s1", messages=[nm()], mode="bogus", io={})
add("replace_transcript", session_id="s1", messages=[nm()], mode=None, io={})
add("replace_transcript", session_id="s1", messages=[nm()], mode="destructive",
    io={"replace_messages": rs("RuntimeError", REWIND_TEXT)})
add("replace_transcript", session_id="s1", messages=[nm(), nm(content="second")],
    mode="destructive",
    io={"replace_messages": script({"stamps": [
            {"_row_id": 10, "timestamp": 1.0, "message_uid": "u1"}, {}]}),
        "get_messages": script([{"id": 10, "timestamp": 5.0}])})
add("replace_transcript", session_id="s1", messages=[], mode="destructive",
    io={"replace_messages": script({"stamps": []})})

# ── compact ──
add("compact", session_id="s1", replacement=[nm()],
    io={"archive_and_compact": script({"active": 3, "stamps": [
        {"_row_id": 20, "timestamp": 7.0, "_db_row_snapshot": "s20", "message_uid": "cu"}]})})
add("compact", session_id="s1", replacement=[nm()], watermark={"last_active": 5},
    carried_tail_count=2, lock_holder="comp1", model_config_patch={"k": 1},
    io={"archive_and_compact": script({"active": 1, "stamps": [
        {"_row_id": 20, "timestamp": 7.0}]})})
add("compact", session_id="s1", replacement=[nm()], watermark={"last_active": 0},
    io={"archive_and_compact": script({"active": 1, "stamps": [
        {"_row_id": 20, "timestamp": 7.0}]})})
add("compact", session_id="s1", replacement=[nm()], watermark={"last_active": None},
    io={"archive_and_compact": script({"active": 1, "stamps": [
        {"_row_id": 20, "timestamp": 7.0}]})})
add("compact", session_id="s1", replacement=[nm()], carried=[hm(row_id=3)],
    coverage={"covered_ids": [1, 2], "unresolved": [hm()]},
    held_for_cas=[hm(row_id=4)],
    io={"archive_and_compact": script({"active": 2, "stamps": [
        {"_row_id": 20, "timestamp": 7.0}]})})
add("compact", session_id="s1", replacement=[nm()], carried=[], held_for_cas=[],
    io={"archive_and_compact": script({"active": 1, "stamps": [
        {"_row_id": 20, "timestamp": 7.0}]})})
add("compact", session_id="s1", replacement=[nm()],
    io={"archive_and_compact": rs("RuntimeError", HISTORY_TEXT)})
add("compact", session_id="s1", replacement=[nm()],
    io={"archive_and_compact": script({"active": 1, "stamps": [{}]})})
add("compact", session_id="s1", replacement=[nm()], carried=[hm(absorbed_row_ids=["abc"])],
    io={"archive_and_compact": script({"active": 1, "stamps": []})})
add("compact", session_id="s1", replacement=[nm()], watermark={},
    io={"archive_and_compact": script({"active": 1, "stamps": []})})
add("compact", session_id="s1", replacement=[], io={})

# ── rewrite_pruned_rows ──
add("rewrite_pruned_rows", session_id="s1", changes=[change(), change()],
    io={"rewrite_pruned_rows": script({"rewritten": 2, "stamps": [
        {"_db_row_snapshot": "n1"}, {"_db_row_snapshot": "n2"}]})})
add("rewrite_pruned_rows", session_id="s1", changes=[],
    io={"rewrite_pruned_rows": script({"rewritten": 0, "stamps": []})})
add("rewrite_pruned_rows", session_id="s1", changes=[change()],
    io={"rewrite_pruned_rows": script({"rewritten": 0, "stamps": [{}]})})
add("rewrite_pruned_rows", session_id="s1", changes=[change()],
    model_config_patch={"k": 1}, turn_lease_holder="w1", turn_lease_ttl_seconds=10.0,
    io={"rewrite_pruned_rows": script({"rewritten": 1, "stamps": [
        {"_db_row_snapshot": "n1"}]})})
add("rewrite_pruned_rows", session_id="s1", changes=[change()],
    io={"rewrite_pruned_rows": rs("RuntimeError", "a pruned message of session s1 is not rewritten in place")})
add("rewrite_pruned_rows", session_id="s1",
    changes=[{"original": hm(), "replacement": nm()}], io={})
add("rewrite_pruned_rows", session_id="s1", changes=[change()], io={})

# ── set_reaction ──
add("set_reaction", session_id="s1", message_id=1, emoji="+1", author="user",
    io={"set_message_reaction": script(None)})
add("set_reaction", session_id="s1", message_id=1, emoji="+1", author="user",
    io={"set_message_reaction": script([
        {"emoji": "+1", "author": "user", "at": 3.0, "seen": True}])})
add("set_reaction", session_id="s1", message_id=1, emoji=None, author="agent",
    io={"set_message_reaction": script([{"emoji": None, "author": "agent"}])})
add("set_reaction", session_id="s1", message_id=1, emoji="+1", author="user",
    io={"set_message_reaction": script([{"emoji": "+1", "author": "user", "at": "2.5"}])})
add("set_reaction", session_id="s1", message_id=1, emoji="+1", author="user",
    io={"set_message_reaction": rs("RuntimeError", REWIND_TEXT)})
add("set_reaction", session_id="s1", message_id=1, emoji="+1", author="user",
    io={"set_message_reaction": script(5)})
add("set_reaction", session_id="s1", message_id=1, emoji="+1", author="user",
    io={"set_message_reaction": script(["notdict"])})
add("set_reaction", session_id="s1", message_id=1, emoji="+1", author="user", io={})

# ── take_unseen_reactions ──
add("take_unseen_reactions", session_id="s1",
    io={"take_unseen_reactions": script([
        {"row_id": 1, "role": "user", "emoji": "+1", "text": "hi"}])})
add("take_unseen_reactions", session_id="s1", io={"take_unseen_reactions": script([])})
add("take_unseen_reactions", session_id="s1", author="agent",
    io={"take_unseen_reactions": script([
        {"row_id": 2, "role": "assistant", "emoji": "x", "text": ""}])})
add("take_unseen_reactions", session_id="s1",
    io={"take_unseen_reactions": script([{"row_id": 1, "role": "user", "emoji": "+1"}])})
add("take_unseen_reactions", session_id="s1",
    io={"take_unseen_reactions": rs("RuntimeError", "boom")})
add("take_unseen_reactions", session_id="s1", io={"take_unseen_reactions": script(None)})
add("take_unseen_reactions", session_id="s1", io={})

# ── read_transcript ──
add("read_transcript", session_id="s1", io={"get_messages": script([row()])})
add("read_transcript", session_id="s1", request={"view": "display"},
    io={"get_messages": script([row(active=0, compacted=1)])})
add("read_transcript", session_id="s1", request={"view": "all", "include_lineage": True},
    io={"get_messages": script([row(active=0, compacted=0)])})
add("read_transcript", session_id="s1", request={"after_id": 5},
    io={"get_messages": script([])})
add("read_transcript", session_id="s1",
    request={"limit": 10, "offset": 2, "latest": True},
    io={"get_messages": script([row(), row(id=2)])})
add("read_transcript", session_id="s1", request={"view": "bogus"}, io={})
add("read_transcript", session_id="s1", io={"get_messages": rs("RuntimeError", REWIND_TEXT)})
add("read_transcript", session_id="s1", io={"get_messages": script([{"id": 1}])})
add("read_transcript", session_id="s1", io={})

# ── active_watermark ──
add("active_watermark", session_id="s1", io={"get_active_message_watermark": 0})
add("active_watermark", session_id="s1", io={"get_active_message_watermark": 7})
add("active_watermark", session_id="s1", io={"get_active_message_watermark": None})
add("active_watermark", session_id="s1",
    io={"get_active_message_watermark": rs("RuntimeError", "boom")})
add("active_watermark", session_id="s1", io={})

# ── conversation_revision ──
add("conversation_revision", session_id="s1",
    io={"conversation_revision": script([["s1"], 3, 9, 2])})
add("conversation_revision", session_id="s1", include_lineage=True,
    io={"conversation_revision": script([["root", "s1"], 5, 12, 4])})
add("conversation_revision", session_id="s1", io={"conversation_revision": script([1, 2])})
add("conversation_revision", session_id="s1",
    io={"conversation_revision": script([1, 2, 3, 4, 5])})
add("conversation_revision", session_id="s1", io={"conversation_revision": script(None)})
add("conversation_revision", session_id="s1",
    io={"conversation_revision": script([5, 3, 9, 2])})
add("conversation_revision", session_id="s1", io={})

# ── read_around ──
add("read_around", session_id="s1", anchor=1,
    io={"get_messages_around": script(
        {"window": [row()], "messages_before": 1, "messages_after": 2})})
add("read_around", session_id="s1", anchor=1, window=0,
    io={"get_messages_around": script(
        {"window": [], "messages_before": 0, "messages_after": 0})})
add("read_around", session_id="s1", anchor=1,
    io={"get_messages_around": script({"messages_before": 0, "messages_after": 0})})
add("read_around", session_id="s1", anchor=1,
    io={"get_messages_around": script({"window": [], "messages_after": 0})})
add("read_around", session_id="s1", anchor=1,
    io={"get_messages_around": rs("RuntimeError", "boom")})
add("read_around", session_id="s1", anchor=1, io={"get_messages_around": script(None)})
add("read_around", session_id="s1", anchor=1, io={})

# ── read_session_timeline ──
TL_PAGE = {"limit": 500, "after_row_id": 0, "returned": 1, "total": 5,
           "has_more": True, "next_cursor": 1}
add("read_session_timeline", session_id="s1",
    io={"get_session_timeline": script(
        {"entries": [{"row_id": 1, "preview": "p", "timestamp": 1.0}],
         "pagination": TL_PAGE})})
add("read_session_timeline", session_id="s1", limit=10, after_row_id=3,
    io={"get_session_timeline": script({"entries": [], "pagination": dict(
        TL_PAGE, returned=0, has_more=False, next_cursor=None, limit=10, after_row_id=3)})})
add("read_session_timeline", session_id="s1",
    io={"get_session_timeline": script({"entries": []})})
add("read_session_timeline", session_id="s1",
    io={"get_session_timeline": script(
        {"entries": [{"row_id": 1, "timestamp": 1.0}], "pagination": TL_PAGE})})
add("read_session_timeline", session_id="s1",
    io={"get_session_timeline": rs("RuntimeError", "boom")})
add("read_session_timeline", session_id="s1",
    io={"get_session_timeline": script(
        {"entries": [], "pagination": {"limit": 500, "after_row_id": 0,
                                       "returned": 0, "total": 0, "has_more": False}})})
add("read_session_timeline", session_id="s1", io={})

# ── read_prompt_page ──
PP_PAGE = {"row_id": 1, "limit": 120, "returned": 1, "offset": 0, "total": 3,
           "has_older": False, "has_newer": True}
add("read_prompt_page", session_id="s1", row_id=1,
    io={"get_session_messages_around": script(None)})
add("read_prompt_page", session_id="s1", row_id=1,
    io={"get_session_messages_around": script(
        {"messages": [row()], "pagination": PP_PAGE})})
add("read_prompt_page", session_id="s1", row_id=1, limit=5,
    io={"get_session_messages_around": script(
        {"messages": [], "pagination": dict(PP_PAGE, returned=0, limit=5,
                                            has_newer=False)})})
add("read_prompt_page", session_id="s1", row_id=1,
    io={"get_session_messages_around": script({"pagination": PP_PAGE})})
add("read_prompt_page", session_id="s1", row_id=1,
    io={"get_session_messages_around": script(
        {"messages": [], "pagination": {"limit": 120, "returned": 0, "offset": 0,
                                        "total": 0, "has_older": False, "has_newer": False}})})
add("read_prompt_page", session_id="s1", row_id=1,
    io={"get_session_messages_around": rs("RuntimeError", "boom")})
add("read_prompt_page", session_id="s1", row_id=1, io={})

# ── read_compression_cooldown_snapshot ──
add("read_compression_cooldown_snapshot", session_id="s1",
    io={"get_compression_failure_cooldown_row": script({"session_exists": False})})
add("read_compression_cooldown_snapshot", session_id="s1",
    io={"get_compression_failure_cooldown_row": script(
        {"session_exists": True, "cooldown_until": 5.0, "error": "e"})})
add("read_compression_cooldown_snapshot", session_id="s1",
    io={"get_compression_failure_cooldown_row": script(
        {"session_exists": 0, "cooldown_until": 9.0, "error": "x"})})
add("read_compression_cooldown_snapshot", session_id="s1",
    io={"get_compression_failure_cooldown_row": script({"session_exists": True})})
add("read_compression_cooldown_snapshot", session_id="s1",
    io={"get_compression_failure_cooldown_row": rs("RuntimeError", "boom")})
add("read_compression_cooldown_snapshot", session_id="s1",
    io={"get_compression_failure_cooldown_row": script(None)})
add("read_compression_cooldown_snapshot", session_id="s1", io={})

# ── read_compression_cooldown ──
add("read_compression_cooldown", session_id="s1", at=1.0,
    io={"get_compression_failure_cooldown_row": script({"session_exists": False})})
add("read_compression_cooldown", session_id="s1", at=5.0,
    io={"get_compression_failure_cooldown_row": script(
        {"session_exists": True, "cooldown_until": 5.0, "error": "e"})})
add("read_compression_cooldown", session_id="s1", at=6.0,
    io={"get_compression_failure_cooldown_row": script(
        {"session_exists": True, "cooldown_until": 5.0, "error": None})})
add("read_compression_cooldown", session_id="s1", at=4.0,
    io={"get_compression_failure_cooldown_row": script(
        {"session_exists": True, "cooldown_until": 5.0, "error": "e"})})
add("read_compression_cooldown", session_id="s1", at=1.0,
    io={"get_compression_failure_cooldown_row": script(
        {"session_exists": True, "cooldown_until": "7.5", "error": None})})
add("read_compression_cooldown", session_id="s1", at=1.0,
    io={"get_compression_failure_cooldown_row": rs("RuntimeError", REWIND_TEXT)})
add("read_compression_cooldown", session_id="s1", at=1.0, io={})

# ── restore_compression_cooldown ──
add("restore_compression_cooldown", session_id="s1",
    snapshot={"present": True, "until": 5.0, "error": "e"},
    io={"restore_compression_failure_cooldown_row": script(None)})
add("restore_compression_cooldown", session_id="s1", snapshot={"present": False},
    io={"restore_compression_failure_cooldown_row": script(None)})
add("restore_compression_cooldown", session_id="s1",
    snapshot={"present": True, "until": 5.0, "error": None},
    io={"restore_compression_failure_cooldown_row": rs("RuntimeError", "cooldown mismatch")})
add("restore_compression_cooldown", session_id="s1",
    snapshot={"present": True, "until": None, "error": None},
    io={"restore_compression_failure_cooldown_row": rs("TransientStoreError", "disk"),
        "classify_persistence_error": "disk"})
add("restore_compression_cooldown", session_id="s1",
    snapshot={"present": True, "until": None, "error": None},
    io={"restore_compression_failure_cooldown_row": rs("RewindRefused", REWIND_TEXT),
        "classify_persistence_error": "unknown"})
add("restore_compression_cooldown", session_id="s1",
    snapshot={"present": True, "until": None, "error": None},
    io={"restore_compression_failure_cooldown_row": rs("TransientStoreError", "fk",
            cause=rs("OSError", "FOREIGN KEY constraint failed")),
        "classify_persistence_error": "session_row_missing"})
add("restore_compression_cooldown", session_id="s1", snapshot={"until": 5.0}, io={})
add("restore_compression_cooldown", session_id="s1", snapshot={"present": False}, io={})

# ── latest_row_id ──
add("latest_row_id", session_id="s1", io={"latest_message_row_id": 7})
add("latest_row_id", session_id="s1", io={"latest_message_row_id": None})
add("latest_row_id", session_id="s1", role="assistant", offset=1, require_text=False,
    io={"latest_message_row_id": 3})
add("latest_row_id", session_id="s1", io={"latest_message_row_id": rs("RuntimeError", "boom")})
add("latest_row_id", session_id="s1", io={})

# ── message_storage_state ──
add("message_storage_state", message_id=0, io={})
add("message_storage_state", message_id=None, io={})
add("message_storage_state", message_id=5, io={"conn.execute": script(None)})
add("message_storage_state", message_id=5,
    io={"conn.execute": script({"session_id": "s1", "active": 1, "compacted": 0})})
add("message_storage_state", message_id=5,
    io={"conn.execute": script({"session_id": "s1", "active": 0, "compacted": 1})})
add("message_storage_state", message_id=5,
    io={"conn.execute": script({"session_id": "s1", "active": 0, "compacted": 0})})
add("message_storage_state", message_id=5,
    io={"conn.execute": script({"session_id": "s1", "compacted": 0})})
add("message_storage_state", message_id=5, io={"conn.execute": rs("RuntimeError", "boom")})
add("message_storage_state", message_id=5, io={})

# ── plan_user_turn_rewind ──
PLAN = {"target_row_id": 5, "expected_active_ids": [5, 6],
        "expected_target_content": "hi", "scaffold": None,
        "prefix": [{"role": "user", "content": "p"}], "durable_prefix": [],
        "live_view": {"k": 1}, "live_text": "t", "durable_user": [0],
        "user_ordinal": 1}
add("plan_user_turn_rewind", session_id="s1", user_ordinal=1,
    io={"plan_user_turn_rewind": script(PLAN)})
add("plan_user_turn_rewind", session_id="s1", user_ordinal=2,
    warm_history=[{"role": "user", "content": "w"}],
    require_retryable=True, require_composite=True,
    io={"plan_user_turn_rewind": script(dict(PLAN, scaffold={"s": 1}, live_text=None))})
add("plan_user_turn_rewind", session_id="s1", user_ordinal=1,
    io={"plan_user_turn_rewind": script({k: v for k, v in PLAN.items() if k != "live_text"})})
add("plan_user_turn_rewind", session_id="s1", user_ordinal=1,
    io={"plan_user_turn_rewind": script(dict(PLAN, bogus=1))})
add("plan_user_turn_rewind", session_id="s1", user_ordinal=1,
    io={"plan_user_turn_rewind": rs("RuntimeError", REWIND_TEXT)})
add("plan_user_turn_rewind", session_id="s1", user_ordinal=1,
    io={"plan_user_turn_rewind": script(dict(PLAN, prefix=5))})
add("plan_user_turn_rewind", session_id="s1", user_ordinal=1, io={})
add("plan_user_turn_rewind", session_id="s1", user_ordinal=1,
    io={"plan_user_turn_rewind": script(None)})


def main():
    # ASCII scope + per-func coverage checks before writing.
    funcs = {}
    for c in CASES:
        funcs[c["func"]] = funcs.get(c["func"], 0) + 1
        json.dumps(c, ensure_ascii=True)  # raises on non-ASCII? no — escapes; check below
    for c in CASES:
        s = json.dumps(c, ensure_ascii=False)
        if any(ord(ch) > 0x7f for ch in s):
            raise SystemExit("non-ASCII case: %s" % s[:120])
    expected = [
        "_visibility", "_sidecar", "_message_dict", "_held_dict", "_pruned_original",
        "_transcript_row", "_message_record", "_stamp", "_row_outcomes", "_conversation",
        "_refusals", "_append_chunk", "append", "append_batch", "replace_transcript",
        "compact", "rewrite_pruned_rows", "set_reaction", "take_unseen_reactions",
        "read_transcript", "active_watermark", "conversation_revision", "read_around",
        "read_session_timeline", "read_prompt_page", "read_compression_cooldown_snapshot",
        "read_compression_cooldown", "restore_compression_cooldown", "latest_row_id",
        "message_storage_state", "plan_user_turn_rewind",
    ]
    missing = [f for f in expected if funcs.get(f, 0) < 5]
    extra = [f for f in funcs if f not in expected]
    if missing or extra:
        raise SystemExit("coverage gap: missing=%s extra=%s" % (missing, extra))
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    print("wrote %d cases (%d funcs, min %d/func) -> %s"
          % (len(CASES), len(funcs), min(funcs.values()), OUT))


if __name__ == "__main__":
    main()
