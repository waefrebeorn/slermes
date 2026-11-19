#!/usr/bin/env python3
"""Generate tests/fixtures/conversation_store_flow.jsonl — input cases for the
conversation-store carried-row coverage FLOW oracle.

The named target persistence/conversation/store.py resolves to the real module
persistence/sqlite/conversation/_store/coverage.py. The oracle computes want
from the LIVE quarry module at gate time; this script only emits {func, in}
inputs.

To guarantee the scripted seams match the live call sequence (no script gaps),
each case is built by driving the live reference ONCE with an auto-recording
context: a provider supplies each collaborator's return (sensible defaults plus
per-case db rows / overrides), and the recorded returns become in.io. The gate
replays the identical scripts through the oracle's Ctx and the C universal seam.

Representation contract (shared with the C port and the oracle):
- None == JSON null; plain JSON numbers are Python ints; JSON bools are bools.
- A Python float travels as {"__float__": true, "v": <number>}; a tuple as
  {"__tuple__": true, "v": [...]}. Row identities are plain JSON arrays (lists).
- A sqlite3.Row is a plain JSON object; conn.execute is scripted via the db arg.
- holder is an opaque token (null). Sets return sorted; integral floats fold.
- Fixture domain: identity components str/int/float/None/list (no bool/nested
  tuple); timestamps int/float/None; text ASCII; ids positive ints; no literal
  "__float__"/"__tuple__" keys inside content/row trees.
"""
import copy
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import oracle_conversation_store_flow as oracle  # noqa: E402

OUT = HERE / "fixtures" / "conversation_store_flow.jsonl"
CASES = []
REPLAY_SIDECARS = oracle.REPLAY_SIDECARS


def fl(v):
    return {"__float__": True, "v": v}


def tu(items):
    return {"__tuple__": True, "v": list(items)}


def ident(role="user", content="x", tcid=None, tcjson=None):
    return [role, content, tcid, tcjson]


def row(id, role="user", content="x", tool_call_id=None, tool_calls=None,
        timestamp=100, message_uid=None, session_id="s1", api_content=None, **kw):
    d = {"id": id, "role": role, "content": content, "tool_call_id": tool_call_id,
         "tool_calls": tool_calls, "timestamp": timestamp, "message_uid": message_uid,
         "session_id": session_id, "api_content": api_content}
    d.update(kw)
    return d


def default_return(name, idx, args):
    if name == "_row_identity":
        tc = args.get("tool_calls")
        return [args.get("role"), args.get("content"), args.get("tool_call_id"),
                json.dumps(tc) if tc else None]
    if name == "_parse_tool_calls":
        return args.get("tool_calls")
    if name in ("_decode_content", "_encode_content", "_loaded_view_content"):
        return args.get("content")
    if name == "_serialized_message_row":
        msg = args.get("msg") or {}
        d = {c: None for c in REPLAY_SIDECARS}
        d["api_content"] = msg.get("content")
        return d
    if name == "message_uid_or_none":
        u = (args.get("msg") or {}).get("message_uid")
        return u if isinstance(u, str) and u else None
    if name == "_is_image_part":
        p = args.get("part")
        return isinstance(p, dict) and p.get("type") in ("image_url", "input_image", "image")
    if name == "coerce_epoch":
        v = args.get("value")
        if isinstance(v, (int, float)) and not isinstance(v, bool) and 0 <= v <= 4200000000:
            return fl(v)
        return None
    if name == "strip_leading_message_timestamps":
        return tu([args.get("content"), None])
    if name == "transcript_row_snapshot":
        return "snap:" + json.dumps(args.get("row"), sort_keys=True, ensure_ascii=False)
    if name == "_placeholders":
        items = args.get("items")
        n = items if isinstance(items, int) else len(items or [])
        return ",".join("?" for _ in range(n))
    if name == "_id_chunks":
        return [args.get("ids") or []]
    if name == "conn.execute":
        return []
    return None


class AutoCtx:
    def __init__(self, provider):
        self.provider = provider
        self.cursors = {}
        self.recorded = {}
        self.trace = []

    def call(self, name, args):
        idx = self.cursors.get(name, 0)
        self.cursors[name] = idx + 1
        ret = self.provider(name, idx, args)
        self.recorded.setdefault(name, []).append(ret)
        self.trace.append({"name": name, "args": args, "ret": ret})
        if isinstance(ret, dict) and ret.get("__script__"):
            if isinstance(ret.get("raise"), dict):
                raise oracle.make_exc(ret["raise"])
            return copy.deepcopy(ret.get("ret"))
        return ret


def build_provider(db, overrides):
    db = db or []
    overrides = overrides or {}

    def provider(name, idx, args):
        if name in overrides:
            return overrides[name]
        if name == "conn.execute":
            return db[idx] if idx < len(db) else []
        return default_return(name, idx, args)
    return provider


def add(func, args, db=None, overrides=None, fail_closed=False, io=None):
    if fail_closed:
        inp = {"args": args}
        if io:
            inp["io"] = io
        CASES.append({"func": func, "in": inp, "fail_closed": True})
        return
    ctx = AutoCtx(build_provider(db, overrides))
    oracle.drive(func, {"args": args}, ctx=ctx)
    inp = {"args": args}
    if ctx.recorded:
        inp["io"] = ctx.recorded
    CASES.append({"func": func, "in": inp})


IMG = {"type": "image_url", "image_url": {"url": "u"}}
TXT = {"type": "text", "text": "x"}

# ── _text_of (pure) ──
add("_text_of", {"content": "hello"})
add("_text_of", {"content": ""})
add("_text_of", {"content": None})
add("_text_of", {"content": [{"type": "text", "text": "a"}, {"type": "text", "text": "b"}]})
add("_text_of", {"content": [IMG, {"type": "text", "text": "x"}]})
add("_text_of", {"content": [{"type": "text", "text": ""}, {"type": "input_text", "text": "y"}]})
add("_text_of", {"content": [{"type": "text"}, {"type": "output_text", "text": "z"}]})
add("_text_of", {"content": [1, "notdict", {"type": "text", "text": "ok"}]})
add("_text_of", {"content": 42})
add("_text_of", {"content": [{"type": "text", "text": 5}]})

# ── _holds_as_block (pure) ──
add("_holds_as_block", {"inner": "b", "outer": "a\nb\nc"})
add("_holds_as_block", {"inner": "b", "outer": "abc"})
add("_holds_as_block", {"inner": "b", "outer": "a\nb"})
add("_holds_as_block", {"inner": "b", "outer": "b\nc"})
add("_holds_as_block", {"inner": "x", "outer": "a\nb\nc"})
add("_holds_as_block", {"inner": "b\nb", "outer": "a\nb\nb\nc"})
add("_holds_as_block", {"inner": "b", "outer": " b \nc"})
add("_holds_as_block", {"inner": "b", "outer": "ab\nc"})

# ── _images_replaced (_is_image_part seam) ──
add("_images_replaced", {"stored": [IMG], "held": [TXT]})
add("_images_replaced", {"stored": [IMG], "held": [IMG]})
add("_images_replaced", {"stored": [{"type": "text", "text": "a"}], "held": [{"type": "text", "text": "b"}]})
add("_images_replaced", {"stored": "notlist", "held": [TXT]})
add("_images_replaced", {"stored": [IMG], "held": [{"type": "image"}]})
add("_images_replaced", {"stored": [IMG, IMG], "held": [TXT, TXT]})
add("_images_replaced", {"stored": [IMG], "held": []})
add("_images_replaced", {"stored": [], "held": []})

# ── _carried_tail_slots (pure) ──
add("_carried_tail_slots", {"carried": {"0": 5, "1": 2}, "compacted_count": 6, "tail_count": 2})
add("_carried_tail_slots", {"carried": {"0": 5}, "compacted_count": 6, "tail_count": 0})
add("_carried_tail_slots", {"carried": {"0": 5}, "compacted_count": 6, "tail_count": -1})
add("_carried_tail_slots", {"carried": {}, "compacted_count": 6, "tail_count": 2})
add("_carried_tail_slots", {"carried": {"0": 3, "1": 4}, "compacted_count": 5, "tail_count": 2})
add("_carried_tail_slots", {"carried": {"0": 1}, "compacted_count": 10, "tail_count": 3})

# ── _uncounted_merged_rows (pure) ──
add("_uncounted_merged_rows", {"tail": [{"_merged_durable_rows": 3, "_absorbed_row_ids": [1]}]})
add("_uncounted_merged_rows", {"tail": [{"_merged_durable_rows": 2}]})
add("_uncounted_merged_rows", {"tail": [{"_merged_durable_rows": 1}]})
add("_uncounted_merged_rows", {"tail": [{"_merged_durable_rows": 2, "_row_id": 5}]})
add("_uncounted_merged_rows", {"tail": [{"_unnamed_durable_rows": 3}]})
add("_uncounted_merged_rows", {"tail": [{"_merged_durable_rows": True}]})
add("_uncounted_merged_rows", {"tail": [{"_merged_durable_rows": 4, "_absorbed_row_ids": [1, 2], "_unnamed_durable_rows": 1}]})
add("_uncounted_merged_rows", {"tail": ["notdict", {"_merged_durable_rows": 2}]})
add("_uncounted_merged_rows", {"tail": []})

# ── _tail_originals (pure) ──
add("_tail_originals", {"covered_active": [1, 2, 3, 4, 5], "tail_count": 2, "merged_away": []})
add("_tail_originals", {"covered_active": [1, 2, 3, 4, 5], "tail_count": 2, "merged_away": [4]})
add("_tail_originals", {"covered_active": [1, 2, 3], "tail_count": 5, "merged_away": []})
add("_tail_originals", {"covered_active": [], "tail_count": 2, "merged_away": []})
add("_tail_originals", {"covered_active": [1, 2, 3, 4], "tail_count": 1, "merged_away": [3, 4]})
add("_tail_originals", {"covered_active": [10, 20], "tail_count": 2, "merged_away": [10, 20]})

# ── _index_held_dicts ──
add("_index_held_dicts", {"holder": None, "compacted_messages": [{"role": "user", "content": "a", "_row_id": 1, "message_uid": "u1"}]})
add("_index_held_dicts", {"holder": None, "compacted_messages": [{"role": "user", "content": "a", "message_uid": "u1"}]})
add("_index_held_dicts", {"holder": None, "compacted_messages": [{"role": "user", "content": "a"}]})
add("_index_held_dicts", {"holder": None, "compacted_messages": [{"role": "user", "_row_id": True}]})
add("_index_held_dicts", {"holder": None, "compacted_messages": [{"role": "user", "_row_id": 0}]})
add("_index_held_dicts", {"holder": None, "compacted_messages": ["notdict", {"role": "assistant", "content": "b", "_row_id": 2, "message_uid": "u2"}]})
add("_index_held_dicts", {"holder": None, "compacted_messages": [{"role": "user", "_row_id": 1, "message_uid": "u1"}, {"role": "user", "_row_id": 1, "message_uid": "uX"}]})

# ── _replays_as ──
add("_replays_as", {"holder": None, "row": row(1, content="x"), "held": {"content": "x"}, "same_content": True})
add("_replays_as", {"holder": None, "row": row(1, content="x", api_content="x"), "held": {"content": "x"}, "same_content": True})
add("_replays_as", {"holder": None, "row": row(1, content="x", api_content="other"), "held": {"content": "x"}, "same_content": True})
add("_replays_as", {"holder": None, "row": row(1, content="x", reasoning="r"), "held": {"content": "x"}, "same_content": True})
add("_replays_as", {"holder": None, "row": row(1, content="x", api_content="other"), "held": {"content": "x"}, "same_content": False})
add("_replays_as", {"holder": None, "row": row(1, content="x"), "held": {"content": "y"}, "same_content": True})

# ── _copy_of_current_row (override snapshot to a constant) ──
add("_copy_of_current_row", {"row": row(1), "held": {"_db_row_snapshot": "S"}}, overrides={"transcript_row_snapshot": "S"})
add("_copy_of_current_row", {"row": row(1), "held": {"_db_row_snapshot": "T"}}, overrides={"transcript_row_snapshot": "S"})
add("_copy_of_current_row", {"row": row(1), "held": {}}, overrides={"transcript_row_snapshot": "S"})
add("_copy_of_current_row", {"row": row(1), "held": {"_db_row_snapshot": 5}}, overrides={"transcript_row_snapshot": "S"})
add("_copy_of_current_row", {"row": row(2), "held": {"_db_row_snapshot": "S"}}, overrides={"transcript_row_snapshot": "S"})

# ── _claim_stored_copy ──
CR_ROW = row(1, content="x", message_uid="u1")
CR_HELD = {"role": "user", "content": "x", "_row_id": 1, "message_uid": "u1"}
add("_claim_stored_copy", {"holder": None, "row": CR_ROW, "compacted_messages": [CR_HELD],
                           "by_row": {"1": [0, ident(), "u1"]}, "by_uid": {}, "claimed": []})
add("_claim_stored_copy", {"holder": None, "row": CR_ROW, "compacted_messages": [CR_HELD],
                           "by_row": {"1": [0, ident(), "u1"]}, "by_uid": {}, "claimed": [0]})
add("_claim_stored_copy", {"holder": None, "row": CR_ROW, "compacted_messages": [CR_HELD],
                           "by_row": {"1": [0, ident(content="different"), "u1"]}, "by_uid": {"u1": [[0, ident()]]}, "claimed": []})
add("_claim_stored_copy", {"holder": None, "row": CR_ROW, "compacted_messages": [CR_HELD],
                           "by_row": {}, "by_uid": {}, "claimed": []})
add("_claim_stored_copy", {"holder": None, "row": row(1, content="x", message_uid="u1", reasoning="r"),
                           "compacted_messages": [CR_HELD], "by_row": {"1": [0, ident(), "u1"]}, "by_uid": {}, "claimed": []})
add("_claim_stored_copy", {"holder": None, "row": CR_ROW, "compacted_messages": [CR_HELD],
                           "by_row": {}, "by_uid": {"u1": [[0, ident()]]}, "claimed": []})

# ── _rewrite_explains ──
add("_rewrite_explains", {"holder": None, "row": row(1, role="user"), "held": {"role": "assistant"}})
add("_rewrite_explains", {"holder": None, "row": row(1, content=[IMG]), "held": {"role": "user", "content": [TXT]}})
add("_rewrite_explains", {"holder": None, "row": row(1, content="b"), "held": {"role": "user", "content": "a\nb\nc"}})
add("_rewrite_explains", {"holder": None, "row": row(1, content="b"), "held": {"role": "user", "content": "a\nb\nc", "_db_row_snapshot": "nomatch"}})
add("_rewrite_explains", {"holder": None, "row": row(1, content="b", tool_calls=[{"id": "c1"}]), "held": {"role": "user", "content": "b", "tool_calls": [{"id": "c2"}]}})
add("_rewrite_explains", {"holder": None, "row": row(1, content=""), "held": {"role": "user", "content": "a\nb\nc"}})

# ── _rewritten_carry ──
RC_ROW = row(1, content=[IMG], message_uid="u1")
RC_HELD = {"role": "user", "content": [TXT], "_row_id": 1, "message_uid": "u1"}
add("_rewritten_carry", {"holder": None, "row": RC_ROW, "row_uid": "u1", "compacted_messages": [RC_HELD],
                         "by_row": {"1": [0, ident(content=[IMG]), "u1"]}, "by_uid": {}, "claimed": []})
add("_rewritten_carry", {"holder": None, "row": RC_ROW, "row_uid": "u1", "compacted_messages": [RC_HELD],
                         "by_row": {"1": [0, ident(content=[IMG]), "u1"]}, "by_uid": {}, "claimed": [0]})
add("_rewritten_carry", {"holder": None, "row": RC_ROW, "row_uid": "u1", "compacted_messages": [RC_HELD],
                         "by_row": {}, "by_uid": {"u1": [[0, ident(content=[IMG])]]}, "claimed": []})
add("_rewritten_carry", {"holder": None, "row": RC_ROW, "row_uid": "u1", "compacted_messages": [RC_HELD],
                         "by_row": {}, "by_uid": {"u1": [[0, ident(content=[IMG])], [1, ident(content=[IMG])]]}, "claimed": []})
add("_rewritten_carry", {"holder": None, "row": row(1, content="b", message_uid="u1"), "row_uid": "u1",
                         "compacted_messages": [{"role": "user", "content": "b"}], "by_row": {"1": [0, ident(content="b"), "u1"]}, "by_uid": {}, "claimed": []})

# ── _uids_active_outside ──
add("_uids_active_outside", {"rows": [row(1, message_uid="u1"), row(2, message_uid="u2")], "tail_ids": [1],
                             "named_uids": ["u1", "u2"]}, db=[[{"id": 3, "message_uid": "u1"}, {"id": 1, "message_uid": "u2"}]])
add("_uids_active_outside", {"rows": [], "tail_ids": [1], "named_uids": ["u1"]})
add("_uids_active_outside", {"rows": [row(1, message_uid="u1")], "tail_ids": [1], "named_uids": []})
add("_uids_active_outside", {"rows": [row(1, message_uid="u1")], "tail_ids": [1], "named_uids": ["u1"]}, db=[[]])
add("_uids_active_outside", {"rows": [row(1, message_uid="u1")], "tail_ids": [1], "named_uids": ["u1"]},
    db=[[{"id": 1, "message_uid": "u1"}]])
add("_uids_active_outside", {"rows": [row(1, message_uid="u1"), row(2, message_uid="u2")], "tail_ids": [],
                             "named_uids": ["u1", "u2"]}, db=[[{"id": 5, "message_uid": "u1"}, {"id": 6, "message_uid": "u2"}]])

# ── _carried_tail_ids ──
CT_HELD = {"role": "user", "content": "x", "_row_id": 1, "message_uid": "u1"}
CT_ROW = row(1, content="x", message_uid="u1")
add("_carried_tail_ids", {"holder": None, "tail_ids": [], "compacted_messages": [CT_HELD]})
add("_carried_tail_ids", {"holder": None, "tail_ids": [1], "compacted_messages": []})
add("_carried_tail_ids", {"holder": None, "tail_ids": [1], "compacted_messages": [CT_HELD]}, db=[[CT_ROW], []])
add("_carried_tail_ids", {"holder": None, "tail_ids": [1], "compacted_messages": [{"role": "user", "content": "other"}]}, db=[[CT_ROW], []])
add("_carried_tail_ids", {"holder": None, "tail_ids": [1, 2], "compacted_messages": [CT_HELD]},
    db=[[CT_ROW, row(2, content="x", message_uid="u2")], []])
add("_carried_tail_ids", {"holder": None, "tail_ids": [1], "compacted_messages": [CT_HELD]},
    db=[[CT_ROW], [{"id": 9, "message_uid": "u1"}]])

# ── _matching_active_ids ──
add("_matching_active_ids", {"holder": None, "session_id": "s1", "message": {"content": "x", "role": "user"}}, db=[[{"id": 1}, {"id": 2}]])
add("_matching_active_ids", {"holder": None, "session_id": "s1", "message": {"content": None}})
add("_matching_active_ids", {"holder": None, "session_id": "s1", "message": {"content": "x"}}, db=[[]])
add("_matching_active_ids", {"holder": None, "session_id": "s1", "message": {"content": "x", "role": "assistant"}}, db=[[{"id": 5}]])
add("_matching_active_ids", {"holder": None, "session_id": "s1", "message": {}})
add("_matching_active_ids", {"holder": None, "session_id": "s1", "message": {"content": "x"}}, db=[[{"id": 1}]])

# ── _matching_retired_ids ──
add("_matching_retired_ids", {"holder": None, "session_id": "s1", "retired": {"role": "user", "content": "x", "tool_call_id": None, "tool_call_ids": []},
                              "watermark": None}, db=[[row(1, content="x")]])
add("_matching_retired_ids", {"holder": None, "session_id": "s1", "retired": {"role": "user", "content": "y", "tool_call_ids": []},
                              "watermark": None}, db=[[row(1, content="x")]])
add("_matching_retired_ids", {"holder": None, "session_id": "s1", "retired": {"role": "user", "content": "x", "tool_call_ids": ["c1"]},
                              "watermark": None}, db=[[row(1, content="x", tool_calls=[{"id": "c1"}])]])
add("_matching_retired_ids", {"holder": None, "session_id": "s1", "retired": {"role": "user", "content": "x", "tool_call_ids": ["c1"]},
                              "watermark": None}, db=[[row(1, content="x", tool_calls=[{"id": "c2"}])]])
add("_matching_retired_ids", {"holder": None, "session_id": "s1", "retired": {"role": "user", "content": "x", "tool_call_ids": []},
                              "watermark": 5}, db=[[row(1, content="x")]])
add("_matching_retired_ids", {"holder": None, "session_id": "s1", "retired": {"role": None, "content": "x", "tool_call_ids": []},
                              "watermark": None}, db=[[row(1, role="", content="x")]])

# ── _resolve_carried_row_ids ──
add("_resolve_carried_row_ids", {"holder": None, "session_id": "s1", "carried_messages": []})
add("_resolve_carried_row_ids", {"holder": None, "session_id": "s1", "carried_messages": [{"role": "user", "content": "x", "_row_id": 1, "timestamp": 100}]},
    db=[[row(1, content="x", timestamp=100)]])
add("_resolve_carried_row_ids", {"holder": None, "session_id": "s1", "carried_messages": [{"role": "user", "content": "x", "timestamp": 100}]},
    db=[[row(1, content="x", timestamp=100)]])
add("_resolve_carried_row_ids", {"holder": None, "session_id": "s1", "carried_messages": [{"role": "user", "content": "x", "timestamp": 100}]},
    db=[[row(1, content="x", timestamp=100), row(2, content="x", timestamp=100)]])
add("_resolve_carried_row_ids", {"holder": None, "session_id": "s1", "carried_messages": [{"role": "user", "content": "x", "timestamp": None}]},
    db=[[row(1, content="x", timestamp=100)]])
add("_resolve_carried_row_ids", {"holder": None, "session_id": "s1", "carried_messages": [{"role": "user", "content": "x", "_row_id": 1, "timestamp": 100}]},
    db=[[row(1, content="DIFFERENT", timestamp=100)], [row(1, content="x", timestamp=100)]])

# ── _merged_user_run ──
add("_merged_user_run", {"holder": None, "session_id": "s1", "message": {"role": "assistant", "content": "a\n\nb", "_merged_durable_rows": 2}, "watermark": None})
add("_merged_user_run", {"holder": None, "session_id": "s1", "message": {"role": "user", "content": "a\n\nb", "_merged_durable_rows": 1}, "watermark": None})
add("_merged_user_run", {"holder": None, "session_id": "s1", "message": {"role": "user", "content": None, "_merged_durable_rows": 2}, "watermark": None})
add("_merged_user_run", {"holder": None, "session_id": "s1", "message": {"role": "user", "content": "a\n\nb", "_merged_durable_rows": 2}, "watermark": None},
    db=[[row(1, role="user", content="a"), row(2, role="user", content="b")]])
add("_merged_user_run", {"holder": None, "session_id": "s1", "message": {"role": "user", "content": "a\n\nb", "_merged_durable_rows": 2}, "watermark": 5},
    db=[[row(1, role="user", content="a"), row(2, role="user", content="b")]])
add("_merged_user_run", {"holder": None, "session_id": "s1", "message": {"role": "user", "content": "a\n\nb", "_merged_durable_rows": 2}, "watermark": None},
    db=[[row(1, role="user", content="a"), row(2, role="user", content="b"), row(3, role="user", content="a"), row(4, role="user", content="b")]])

# ── _proved_coverage ──
add("_proved_coverage", {"holder": None, "session_id": "s1", "covered_ids": None, "unresolved_held": [], "watermark": None})
add("_proved_coverage", {"holder": None, "session_id": "s1", "covered_ids": [1, 2], "unresolved_held": [], "watermark": None})
add("_proved_coverage", {"holder": None, "session_id": "s1", "covered_ids": [1, 2, 3], "unresolved_held": ["notdict"], "watermark": None})
add("_proved_coverage", {"holder": None, "session_id": "s1", "covered_ids": [1], "unresolved_held": [{"role": "user", "content": "x"}], "watermark": None},
    db=[[{"id": 7}]])
add("_proved_coverage", {"holder": None, "session_id": "s1", "covered_ids": [1], "unresolved_held": [{"role": "user", "content": "x"}], "watermark": None},
    db=[[{"id": 7}, {"id": 8}]])
add("_proved_coverage", {"holder": None, "session_id": "s1", "covered_ids": [1], "unresolved_held": [{"role": "user", "content": "x", "_db_persisted": True}], "watermark": None},
    db=[[]])
add("_proved_coverage", {"holder": None, "session_id": "s1", "covered_ids": [1],
                         "unresolved_held": [{"_retired_row": True, "role": "user", "content": "x", "tool_call_ids": []}], "watermark": None},
    db=[[row(9, content="x")]])

# ── fail-closed cases (missing script -> universal seam RuntimeError) ──
add("_images_replaced", {"stored": [IMG], "held": [TXT]}, fail_closed=True)   # no _is_image_part script
add("_replays_as", {"holder": None, "row": row(1), "held": {"content": "x"}, "same_content": True}, fail_closed=True)  # no _serialized_message_row
add("_matching_active_ids", {"holder": None, "session_id": "s1", "message": {"content": "x", "role": "user"}}, fail_closed=True)  # no _loaded_view_content

# ── scripted-raise cases (real exception / BaseException passthrough) ──
add("_images_replaced", {"stored": [IMG], "held": [TXT]},
    overrides={"_is_image_part": {"__script__": True, "raise": {"type": "ValueError", "message": "bad image part"}}})
add("_replays_as", {"holder": None, "row": row(1), "held": {"content": "x"}, "same_content": True},
    overrides={"_serialized_message_row": {"__script__": True, "raise": {"type": "RuntimeError", "message": "serialize failed"}}})
add("_copy_of_current_row", {"row": row(1), "held": {"_db_row_snapshot": "S"}},
    overrides={"transcript_row_snapshot": {"__script__": True, "raise": {"type": "SystemExit", "message": "shutdown"}}})
add("_index_held_dicts", {"holder": None, "compacted_messages": [{"role": "user", "content": "a", "_row_id": 1}]},
    overrides={"message_uid_or_none": {"__script__": True, "raise": {"type": "ValueError", "message": "uid lookup failed"}}})


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as f:
        for case in CASES:
            f.write(json.dumps(case, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"gen_conversation_store_flow: {len(CASES)} cases -> {OUT}")
    for fn in sorted(counts):
        flag = "  <-- <5!" if counts[fn] < 5 else ""
        print(f"  {fn}: {counts[fn]}{flag}")
    missing = [k for k in oracle.KINDS if counts.get(k, 0) < 5]
    if missing:
        print(f"FUNCS WITH <5 CASES: {missing}")
        sys.exit(1)


if __name__ == "__main__":
    main()
