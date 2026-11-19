#!/usr/bin/env python3
"""Deterministic case generator for the sqlite_sessions_flow oracle.

Writes tests/fixtures/sqlite_sessions_flow.jsonl: {"func", "in"} per line,
>=5 cases per ported func (24 funcs). Covers happy paths, absent/null/empty
edges, scripted seam raises (real exception classes incl. the BaseException
trio), fail-closed missing/exhausted scripts, and the documented ASCII fixture
domain (canonical strict JSON text cells, finite int()/float() strings,
str/bool/null/int origin values).

Run: python tests/gen_sqlite_sessions_flow_cases.py
"""
import json
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "sqlite_sessions_flow.jsonl")

CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def script(ret):
    return {"__script__": True, "ret": ret}


def raise_spec(cls, msg):
    return {"__raise__": cls, "message": msg}


def record_io(config=None, started=None, ended=None, corrupt=False):
    """The three session_record seams: config dict, the two stored_instant
    epoch results (queue), the corrupt flag."""
    return {
        "_parse_model_config": script({} if config is None else config),
        "stored_instant": [started, ended],
        "is_corrupt_model_config": corrupt,
    }


def base_row(**kw):
    row = {"id": "s1", "source": "local", "started_at": 1700000000,
           "message_count": 3, "tool_call_count": 1}
    row.update(kw)
    return row


HEX64 = "a1b2c3d4e5f6" * 5 + "abcd"  # 64 lowercase hex chars
assert len(HEX64) == 64

# ── _stripped ──
add("_stripped", value="  hi  ")
add("_stripped", value="")
add("_stripped", value="   ")
add("_stripped", value=None)
add("_stripped", value=5)
add("_stripped", value="x")
add("_stripped", value="\t\n\r ")
add("_stripped", value=" a b ")

# ── _json_value ──
add("_json_value", text='{"a": 1}')
add("_json_value", text="")
add("_json_value", text=None)
add("_json_value", text=5)
add("_json_value", text="bad")
add("_json_value", text="[1, 2]")
add("_json_value", text='"s"')
add("_json_value", text="01")            # leading zero: Python rejects
add("_json_value", text='{"a": 1} x')    # trailing data
add("_json_value", text="true")
add("_json_value", text="null")
add("_json_value", text='{"a": 1,}')     # trailing comma
add("_json_value", text='{"a": "\\q"}')  # invalid escape
add("_json_value", text="tru")
add("_json_value", text=' \n {"a": 1} \t ')
add("_json_value", text="-")
add("_json_value", text="1.")
add("_json_value", text="1e")

# ── _tool_pin ──
add("_tool_pin", text=HEX64)
add("_tool_pin", text="1" * 64)          # 64 digits: hash wins over JSON number
add("_tool_pin", text="1" * 63)          # 63 digits: not a hash -> JSON number
add("_tool_pin", text="A" * 64)          # uppercase: not a hash, invalid JSON
add("_tool_pin", text='{"tools": []}')
add("_tool_pin", text=None)
add("_tool_pin", text="[]")
add("_tool_pin", text=HEX64 + " ")       # trailing space: not a fullmatch

# ── _ids ──
add("_ids", values=["a", "b"])
add("_ids", values=[])
add("_ids", values=None)
add("_ids", values=["a", "a"])
add("_ids", values=["one"])

# ── _removed ──
add("_removed", values=["a", "b", "a"])
add("_removed", values=[])
add("_removed", values=None)
add("_removed", values=["b", "a", "b", "c", "a"])
add("_removed", values=["x"])
add("_removed", values=["a", "a", "a"])

# ── _listed ──
add("_listed", values=["a"])
add("_listed", values=[])
add("_listed", values=None)
add("_listed", values=[""])
add("_listed", values=["a", "b"])

# ── _optional_list ──
add("_optional_list", values=[])
add("_optional_list", values=None)
add("_optional_list", values=["a"])
add("_optional_list", values=[""])
add("_optional_list", values=["a", "b"])

# ── _sql_limit ──
add("_sql_limit", window={"limit": None})
add("_sql_limit", window={"limit": 5})
add("_sql_limit", window={"limit": 0})
add("_sql_limit", window={"limit": -1})
add("_sql_limit", window={"limit": 3, "offset": -1})
add("_sql_limit", window={"limit": 3, "offset": 2})
add("_sql_limit", window={})

# ── _last_active ──
add("_last_active", row={"last_active": None})
add("_last_active", row={})
add("_last_active", row={"last_active": 5})
add("_last_active", row={"last_active": "1.5"})
add("_last_active", row={"last_active": "abc"})
add("_last_active", row={"last_active": []})
add("_last_active", row={"last_active": True})
add("_last_active", row={"last_active": " 2e1 "})
add("_last_active", row={"last_active": 0})
add("_last_active", row={"last_active": ""})

# ── _roster_session ──
ROSTER = {"id": "s1", "resolved_id": "s2", "title": "T", "root_title": "R",
          "preview": "p", "started_at": 100, "last_active": 200, "message_count": 4}
add("_roster_session", fields=None)
add("_roster_session", fields=dict(ROSTER))
add("_roster_session", fields={k: v for k, v in ROSTER.items() if k != "id"})
add("_roster_session", fields={k: v for k, v in ROSTER.items() if k != "resolved_id"})
add("_roster_session", fields={k: v for k, v in ROSTER.items() if k != "title"})
add("_roster_session", fields={**ROSTER, "extra": 1})
add("_roster_session", fields={"id": "s1"})

# ── session_record ──
add("session_record", row=base_row(), io=record_io())
add("session_record", row=base_row(model_config='{"_delegate_from": "p1"}'),
    io=record_io(config={"_delegate_from": "p1"}))
add("session_record", row=base_row(model_config='{"_branched_from": "p2", "_reset_from": "p3"}'),
    io=record_io(config={"_branched_from": "p2", "_reset_from": "p3"}))
add("session_record", row=base_row(model_config='{"_reset_from": "  ", "_delegate_from": "d"}'),
    io=record_io(config={"_reset_from": "  ", "_delegate_from": "d"}))
add("session_record", row=base_row(origin_json='{"k": "v"}'), io=record_io())
add("session_record", row=base_row(origin_json="[1, 2]"), io=record_io())
add("session_record", row=base_row(origin_json="bad"), io=record_io())
add("session_record", row=base_row(message_count=None, tool_call_count=None,
                                   git_metadata_generation="3",
                                   compression_fallback_streak=None,
                                   rewind_count=True, transcript_revision="7"),
    io=record_io())
add("session_record", row=base_row(title_source="user", tool_names=HEX64), io=record_io())
add("session_record", row=base_row(title_source="bogus", tool_names='["t1"]'), io=record_io())
add("session_record", row={"source": "local"}, io=record_io())  # missing id -> KeyError
add("session_record", row=base_row(started_at=None, ended_at=1700000005),
    io=record_io(started=None, ended=1700000005))
add("session_record", row=base_row(started_at=1700000001, ended_at=None),
    io=record_io(started=1700000001, ended=None))
add("session_record", row=base_row(model_config="corrupt{", expiry_finalized=1,
                                   archived="yes", pinned=0, hidden=None),
    io=record_io(corrupt=True))
add("session_record", row=base_row(), io={**record_io(), "stored_instant": [None]})  # exhausted
add("session_record", row=base_row(), io={"_parse_model_config": script({})})  # fail-closed

# ── _list_entry ──
add("_list_entry", row=base_row(unread=1, preview="hello"), io=record_io())
add("_list_entry", row=base_row(), io={**record_io(), "session_unread": True})
add("_list_entry", row=base_row(unread=0, preview=None, compact=True),
    io=record_io(), compact=True)
add("_list_entry", row=base_row(_lineage_root_id="root", _lineage_ids=["root", "s1"],
                                continuation_kind="compression", last_active="9.5"),
    io=record_io())
add("_list_entry", row=base_row(_lineage_ids=None, last_active=None),
    io={**record_io(), "session_unread": False})
add("_list_entry", row=base_row(), io=record_io())  # fail-closed session_unread

# ── _recency_entry ──
add("_recency_entry", row=base_row(last_active=123), io=record_io())
add("_recency_entry", row=base_row(last_active="4.5"), io=record_io())
add("_recency_entry", row=base_row(), io=record_io())
add("_recency_entry", row=base_row(last_active="zz"), io=record_io())
add("_recency_entry", row=base_row(last_active=None), io=record_io())

# ── _title_refusals ──
add("_title_refusals", session_id="s1", body={"ret": "ok"})
add("_title_refusals", session_id="s1", body={"ret": None})
add("_title_refusals", session_id="s1",
    body={"raise": raise_spec("ValueError", "Title 'mine' is already in use by session s9")})
add("_title_refusals", session_id="s1",
    body={"raise": raise_spec("ValueError", "Title '' is already in use by session s9")})
add("_title_refusals", session_id="s1",
    body={"raise": raise_spec("ValueError",
                              "Title 'a' is already in use by session s9' is already in use by session s8")})
add("_title_refusals", session_id="s1",
    body={"raise": raise_spec("ValueError", "This is the bot's canonical Bot Chat — no rename")})
add("_title_refusals", session_id="s1",
    body={"raise": raise_spec("ValueError", "Title too long (300 chars, max 200)")})
add("_title_refusals", session_id="s1", body={"raise": raise_spec("RuntimeError", "boom")})
add("_title_refusals", session_id="s1", body={"raise": raise_spec("SystemExit", "bye")})
add("_title_refusals", session_id="s1", body={"raise": raise_spec("KeyboardInterrupt", "")})
add("_title_refusals", session_id="s1",
    body={"raise": raise_spec("ValueError", "Title 'x' is already in use by session ")})  # empty holder

# ── create ──
SPEC = {"session_id": "n1", "source": "local"}
add("create", spec=dict(SPEC), io={"create_session": None})
add("create", spec={**SPEC, "lineage_kind": "delegate", "parent_session_id": "p1"},
    io={"create_session": None})
add("create", spec={**SPEC, "lineage_kind": "branch", "parent_session_id": "p2",
                    "model_config": {"k": 1}}, io={"create_session": None})
add("create", spec={**SPEC, "lineage_kind": "reset", "parent_session_id": "p3",
                    "model_config": {}}, io={"create_session": None})
add("create", spec={**SPEC, "origin": {"plat": "telegram", "n": 5, "ok": True, "z": None},
                    "model": "m", "user_id": "u", "cwd": "/w"}, io={"create_session": None})
add("create", spec=dict(SPEC),
    io={"create_session": raise_spec("AuthorityRefusal", "not_found")})
add("create", spec=dict(SPEC),
    io={"create_session": raise_spec("AuthorityRefusal", "permission_denied")})
add("create", spec=dict(SPEC), io={"create_session": raise_spec("SystemExit", "bye")})
add("create", spec=dict(SPEC), io={})  # fail-closed
add("create", spec={**SPEC, "session_id": ""}, io={})
add("create", spec={**SPEC, "source": ""}, io={})
add("create", spec={**SPEC, "lineage_kind": "delegate"}, io={})
add("create", spec={**SPEC, "lineage_kind": "bogus"}, io={})
add("create", spec={**SPEC, "bogus": 1}, io={})

# ── set_title ──
add("set_title", session_id="s1", title="T", io={"set_session_title": True})
add("set_title", session_id="s1", title="T", io={"set_session_title": False})
add("set_title", session_id="s1", title=None, io={"set_session_title": None})
add("set_title", session_id="s1", title="T",
    io={"set_session_title": raise_spec("ValueError", "Title 'T' is already in use by session s2")})
add("set_title", session_id="s1", title="Bot Chat",
    io={"set_session_title": raise_spec("ValueError", "This is the bot's canonical Bot Chat locked")})
add("set_title", session_id="s1", title="T",
    io={"set_session_title": raise_spec("ValueError", "Title too long (300 chars, max 200)")})
add("set_title", session_id="s1", title="T",
    io={"set_session_title": raise_spec("KeyboardInterrupt", "")})
add("set_title", session_id="s1", title="T",
    io={"set_session_title": raise_spec("KeyError", "s1")})
add("set_title", session_id="s1", title="T", io={})  # fail-closed

# ── set_auto_title ──
add("set_auto_title", session_id="s1", title="T", source="user", io={"set_auto_title": True})
add("set_auto_title", session_id="s1", title="T", source="llm", io={"set_auto_title": False})
add("set_auto_title", session_id="s1", title="T", source="derived", io={"set_auto_title": 1})
add("set_auto_title", session_id="s1", title="T", source="bogus", io={})
add("set_auto_title", session_id="s1", title="T", source="user",
    io={"set_auto_title": raise_spec("ValueError", "Title 'T' is already in use by session s7")})
add("set_auto_title", session_id="s1", title="T", source="user",
    io={"set_auto_title": raise_spec("GeneratorExit", "")})

# ── delete ──
add("delete", session_id="s1", media_root="/m", io={"delete_session": script(["s1"])})
add("delete", session_id="s1", expected_ids=["s1", "c1"], media_root="/m",
    io={"delete_session": script(["s1", "c1", "s1"])})
add("delete", session_id="s1",
    expected_display={"s1": [{"role": "user", "text": "hi"}], "c1": []},
    media_root="/m", io={"delete_session": script([])})
add("delete", session_id="s1", refuse_guarded=True, include_compression_chain=True,
    media_root="/m", io={"delete_session": script(["s1", "s2"])})
add("delete", session_id="s1", media_root="/m",
    io={"delete_session": raise_spec("RuntimeError", "disk")})
add("delete", session_id="s1", media_root="/m", io={})  # fail-closed

# ── delete_if_empty ──
add("delete_if_empty", session_id="s1", media_root="/m",
    io={"delete_session_if_empty": script([])})
add("delete_if_empty", session_id="s1", media_root="/m",
    io={"delete_session_if_empty": script(["s1"])})
add("delete_if_empty", session_id="s1", media_root="/m",
    io={"delete_session_if_empty": script(["s1", "s1"])})
add("delete_if_empty", session_id="s1", media_root="/m",
    io={"delete_session_if_empty": raise_spec("ValueError", "guarded")})
add("delete_if_empty", session_id="s1", media_root="/m", io={})  # fail-closed

# ── backfill_session_owners ──
add("backfill_session_owners", stamp=None,
    io={"_own_profile_name": "p", "backfill_null_session_profiles": 5})
add("backfill_session_owners", stamp=None,
    io={"_own_profile_name": None, "backfill_null_session_profiles": 5})
add("backfill_session_owners", stamp=None,
    io={"_own_profile_name": "", "backfill_null_session_profiles": 5})
add("backfill_session_owners", stamp="s",
    io={"backfill_null_session_profiles": None})
add("backfill_session_owners", stamp="", io={})
add("backfill_session_owners", stamp="s",
    io={"backfill_null_session_profiles": 0})
add("backfill_session_owners", stamp=None, io={"_own_profile_name": "p"})  # fail-closed
add("backfill_session_owners", stamp="s",
    io={"backfill_null_session_profiles": raise_spec("SystemExit", "bye")})

# ── get_title_source ──
add("get_title_source", session_id="s1", io={"get_session_title_source": "user"})
add("get_title_source", session_id="s1", io={"get_session_title_source": "llm"})
add("get_title_source", session_id="s1", io={"get_session_title_source": "derived"})
add("get_title_source", session_id="s1", io={"get_session_title_source": None})
add("get_title_source", session_id="s1", io={"get_session_title_source": "bogus"})
add("get_title_source", session_id="s1",
    io={"get_session_title_source": raise_spec("KeyError", "s1")})

# ── title_holder ──
add("title_holder", title="", excluding="s1", io={})
add("title_holder", title="T", excluding="s1", io={"conn.execute": script(["s9"])})
add("title_holder", title="T", excluding="s1", io={"conn.execute": None})
add("title_holder", title="T", excluding="s1", io={"conn.execute": script([])})
add("title_holder", title=None, excluding="s1", io={})
add("title_holder", title="T", excluding="s1",
    io={"conn.execute": raise_spec("RuntimeError", "closed")})
add("title_holder", title="T", excluding="s1", io={})  # fail-closed

# ── list_recent_sessions ──
RROW = {"id": "r1", "source": "local", "model": "m", "title": "T",
        "started_at": 100, "ended_at": None, "end_reason": None,
        "message_count": 2, "tool_call_count": 0, "last_active": 150,
        "preview": "p", "_lineage_root_id": None, "unread": 1}
add("list_recent_sessions", query={},
    io={"list_recent_sessions_bounded": script([dict(RROW)])})
add("list_recent_sessions", query={"limit": 1},
    io={"list_recent_sessions_bounded": script([])})
add("list_recent_sessions", query={"limit": 500, "exclude_sources": ["tool"]},
    io={"list_recent_sessions_bounded": script([dict(RROW)])})
add("list_recent_sessions",
    query={"limit": 3, "candidate_limit": 5, "lineage_limit": 2, "timeout_seconds": -2},
    io={"list_recent_sessions_bounded": script([dict(RROW)])})
add("list_recent_sessions", query={"limit": 0, "timeout_seconds": 0},
    io={"list_recent_sessions_bounded": script([{**RROW, "started_at": None,
                                                 "last_active": "8.5", "preview": None,
                                                 "unread": None, "_lineage_root_id": "root"}])})
add("list_recent_sessions", query={},
    io={"list_recent_sessions_bounded": raise_spec("TimeoutError", "browse timed out after 3s")})
add("list_recent_sessions", query={},
    io={"list_recent_sessions_bounded": raise_spec("SystemExit", "bye")})
add("list_recent_sessions", query={},
    io={"list_recent_sessions_bounded": script([{k: v for k, v in RROW.items() if k != "model"}])})
add("list_recent_sessions", query={}, io={})  # fail-closed
add("list_recent_sessions", query={"bogus": 1}, io={})

# ── list_unlinked_telegram_sessions ──
FROW = base_row(id="f1", last_active=99, preview="hi")
add("list_unlinked_telegram_sessions", user_id="u1", profile_name=None, limit=10,
    io={"_own_profile_name": "p", "list_unlinked_telegram_sessions_for_user": script([dict(FROW)])})
add("list_unlinked_telegram_sessions", user_id="u1", profile_name=None,
    io={"_own_profile_name": None, "list_unlinked_telegram_sessions_for_user": script([dict(FROW)])})
add("list_unlinked_telegram_sessions", user_id="u1", profile_name="custom", limit=3,
    io={"list_unlinked_telegram_sessions_for_user": script([dict(FROW)])})
add("list_unlinked_telegram_sessions", user_id="u1", profile_name="custom",
    io={"list_unlinked_telegram_sessions_for_user": script([])})
add("list_unlinked_telegram_sessions", user_id="u1", profile_name=None,
    io={"_own_profile_name": "", "list_unlinked_telegram_sessions_for_user": script([dict(FROW)])})
add("list_unlinked_telegram_sessions", user_id="u1", profile_name="custom",
    io={"list_unlinked_telegram_sessions_for_user": raise_spec("KeyboardInterrupt", "")})
add("list_unlinked_telegram_sessions", user_id="u1", profile_name="custom", io={})  # fail-closed

# ── seeded fuzz over the pure leaves (verified ASCII domain) ──
rng = random.Random(20261119)


def rand_json(depth=0):
    r = rng.random()
    if depth >= 3 or r < 0.35:
        return rng.choice([None, True, False, rng.randint(-1000, 1000),
                           "".join(rng.choice("abcXYZ019 _-") for _ in range(rng.randint(0, 8)))])
    if r < 0.65:
        return [rand_json(depth + 1) for _ in range(rng.randint(0, 4))]
    return {("k%d" % i): rand_json(depth + 1) for i in range(rng.randint(0, 4))}


for _ in range(12):
    tree = rand_json()
    add("_json_value", text=json.dumps(tree))
for _ in range(6):
    add("_tool_pin", text=json.dumps(rand_json()))
for _ in range(8):
    n = rng.randint(0, 6)
    vals = ["id%d" % rng.randint(0, 4) for _ in range(n)]
    add("_removed", values=list(vals))
    add("_ids", values=list(vals))
for _ in range(8):
    r = rng.random()
    if r < 0.4:
        v = repr(rng.uniform(-1e6, 1e6))
    elif r < 0.6:
        v = str(rng.randint(-10000, 10000))
    elif r < 0.8:
        v = "  " + repr(rng.uniform(-100, 100)) + "\t"
    else:
        v = "".join(rng.choice("abcXYZ") for _ in range(rng.randint(1, 5)))
    add("_last_active", row={"last_active": v})
for _ in range(6):
    s = "".join(rng.choice("ab \t\n") for _ in range(rng.randint(0, 10)))
    add("_stripped", value=s)


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    short = {f: n for f, n in counts.items() if n < 5}
    if short:
        print("TOO FEW CASES: %r" % short)
        return 1
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    print("wrote %s: %d cases over %d funcs (min %d/func)"
          % (OUT, len(CASES), len(counts), min(counts.values())))
    return 0


if __name__ == "__main__":
    sys.exit(main())