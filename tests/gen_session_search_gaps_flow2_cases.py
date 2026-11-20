#!/usr/bin/env python3
"""Generate tests/fixtures/session_search_gaps_flow2.jsonl.

>=5 cases per ported function.  Fixtures are consumed by BOTH the C harness
(from stdin) and the oracle (which re-drives the live slice from "in").
Domain notes live in tests/oracle_session_search_gaps_flow2.py's docstring.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "session_search_gaps_flow2.jsonl")
CASES = []


def add(func, args, io=None, state=None, no_io=False, **extra):
    inp = {"args": args, "state": state or {}}
    if not no_io:
        inp["io"] = io or {}
    inp.update(extra)
    CASES.append({"func": func, "in": inp})


def rs(type_, message=""):
    return {"__script__": True, "raise": {"type": type_, "message": message}}


def bare(type_, message=""):
    return {"__raise__": type_, "message": message}


def sc(ret):
    return {"__script__": True, "ret": ret}


# ── session_end_reason (21) ──
add("session_end_reason", {"db": "db1", "session_id": ""}, {"db.get_session": None})
add("session_end_reason", {"db": "db1", "session_id": None}, {"db.get_session": None})
add("session_end_reason", {"db": "db1"}, {"db.get_session": None})
add("session_end_reason", {"db": "db1", "session_id": "s1"}, {"db.get_session": None})
add("session_end_reason", {"db": "db1", "session_id": "s1"}, {"db.get_session": {}})
add("session_end_reason", {"db": "db1", "session_id": "s1"},
    {"db.get_session": {"end_reason": "idle"}})
add("session_end_reason", {"db": "db1", "session_id": "s1"},
    {"db.get_session": {"end_reason": ""}})
add("session_end_reason", {"db": "db1", "session_id": "s1"},
    {"db.get_session": {"end_reason": None}})
add("session_end_reason", {"db": "db1", "session_id": "s1"},
    {"db.get_session": {"end_reason": "compression"}})
add("session_end_reason", {"db": "db1", "session_id": "s1"},
    {"db.get_session": {"other": 1}})
add("session_end_reason", {"db": "db1", "session_id": "s1"},
    {"db.get_session": "not-a-dict"})
for kind in ["ValueError", "RuntimeError", "TypeError", "OSError",
             "ModuleNotFoundError", "EOFError", "UnicodeError"]:
    add("session_end_reason", {"db": "db1", "session_id": "s1"},
        {"db.get_session": rs(kind, "boom")})
for kind in ["SystemExit", "KeyboardInterrupt", "GeneratorExit",
             "CancelledError", "BaseExceptionGroup"]:
    add("session_end_reason", {"db": "db1", "session_id": "s1"},
        {"db.get_session": rs(kind, "bypass")})
add("session_end_reason", {"db": "db1", "session_id": "s1"}, no_io=True)
add("session_end_reason", {"db": "db1", "session_id": "s1"},
    {"db.get_session": bare("RuntimeError", "bare")})

# ── session_left_live_context (9) ──
add("session_left_live_context", {"db": "db1", "session_id": "s1"},
    {"db.get_session": None})
add("session_left_live_context", {"db": "db1", "session_id": "s1"},
    {"db.get_session": {"end_reason": "idle"}})
add("session_left_live_context", {"db": "db1", "session_id": "s1"},
    {"db.get_session": {"end_reason": None}})
add("session_left_live_context", {"db": "db1", "session_id": "s1"},
    {"db.get_session": rs("RuntimeError", "boom")})
add("session_left_live_context", {"db": "db1", "session_id": "s1"},
    {"db.get_session": rs("ValueError", "bad")})
add("session_left_live_context", {"db": "db1", "session_id": "s1"},
    {"db.get_session": rs("SystemExit", "7")})
add("session_left_live_context", {"db": "db1", "session_id": ""},
    {"db.get_session": None})
add("session_left_live_context", {"db": "db1", "session_id": "s1"},
    no_io=True)
add("session_left_live_context", {"db": "db1", "session_id": "s1"},
    {"db.get_session": {"end_reason": "compression"}})

# ── is_fresh_reset_session (12) ──
for reason in ["session_reset", "session_switch", "idle", "daily", "suspended",
               "resume_pending_expired", "new_session"]:
    add("is_fresh_reset_session", {"end_reason": reason}, {})
for reason in ["compression", "", None, 42, True, "SESSION_RESET"]:
    add("is_fresh_reset_session", {"end_reason": reason}, {})

# ── session_search_impl (44) ──
B = "_list_recent_sessions"
D = "_discover"

# browse / limit clamp
add("session_search_impl", {"db": "db1"}, {B: "browse-ok"})
add("session_search_impl", {"db": "db1", "query": None}, {B: "browse-null"})
add("session_search_impl", {"db": "db1", "query": ""}, {B: "browse-empty"})
add("session_search_impl", {"db": "db1", "query": "  "}, {B: "browse-blank"})
add("session_search_impl", {"db": "db1", "query": 5}, {B: "browse-int"})
add("session_search_impl", {"db": "db1", "limit": "7"}, {B: "l7"})
add("session_search_impl", {"db": "db1", "limit": "20"}, {B: "l10"})
add("session_search_impl", {"db": "db1", "limit": 0}, {B: "l1"})
add("session_search_impl", {"db": "db1", "limit": -5}, {B: "l1b"})
add("session_search_impl", {"db": "db1", "limit": "abc"}, {B: "l3"})
add("session_search_impl", {"db": "db1", "limit": None}, {B: "l3b"})
add("session_search_impl", {"db": "db1", "limit": True}, {B: "l1c"})
add("session_search_impl", {"db": "db1", "limit": 3.7}, {B: "l3c"})
add("session_search_impl", {"db": "db1", "limit": " 1_0 "}, {B: "l10b"})
add("session_search_impl", {"db": "db1", "limit": []}, {B: "l3d"})

# discovery
add("session_search_impl", {"db": "db1", "query": "hello"}, {D: "disc-ok"})
add("session_search_impl", {"db": "db1", "query": "  hello  ",
                            "role_filter": "a, b,,c", "sort": " NEWEST "}, {D: "disc-full"})
add("session_search_impl", {"db": "db1", "query": "x", "sort": "random"}, {D: "disc-sort"})
add("session_search_impl", {"db": "db1", "query": "x", "role_filter": 5}, {D: "disc-role"})
add("session_search_impl", {"db": "db1", "query": "x"}, {D: rs("RuntimeError", "disc fail")})
add("session_search_impl", {"db": "db1", "query": "x"},
    {D: rs("SystemExit", "9")})
add("session_search_impl", {"db": "db1", "query": "x"}, no_io=True)

# scroll
add("session_search_impl", {"db": "db1", "session_id": "s1", "around_message_id": 5},
    {"_scroll": "scroll-ok"})
add("session_search_impl", {"db": "db1", "session_id": " s1 ",
                            "around_message_id": 0, "window": 9,
                            "current_session_id": "cur"},
    {"_scroll": sc({"slice": [1, 2]})})
add("session_search_impl", {"db": "db1", "session_id": "s1",
                            "around_message_id": 5, "query": "ignored"},
    {"_scroll": rs("ValueError", "scroll fail")})

# read shape
OK = '{"success": true, "text": "hi"}'
MISS = '{"success": false}'
add("session_search_impl", {"db": "db1", "session_id": "s1"}, {"_read_session": OK})
add("session_search_impl", {"db": "db1", "session_id": " s1 "}, {"_read_session": OK})
add("session_search_impl", {"db": "db1", "session_id": "s1", "profile": "p0"},
    {"_read_session": OK})
add("session_search_impl", {"db": "db1", "session_id": "s1"},
    {"_read_session": MISS, "_locate_session_db": None})
add("session_search_impl", {"db": "db1", "session_id": "s1"},
    {"_read_session": MISS, "_locate_session_db": rs("RuntimeError", "locate fail")})
add("session_search_impl", {"db": "db1", "session_id": "s1"},
    {"_read_session": MISS, "_locate_session_db": sc(["db2", "p2"]),
     "_read_session": [MISS, OK]})
add("session_search_impl", {"db": "db1", "session_id": "s1"},
    {"_read_session": MISS, "_locate_session_db": sc(["db2", "p2"]),
     "_read_session": [MISS, MISS]})
add("session_search_impl", {"db": "db1", "session_id": "s1"},
    {"_read_session": MISS, "_locate_session_db": sc(["db2", None]),
     "_read_session": [MISS, '{"success": true}']})
add("session_search_impl", {"db": "db1", "session_id": "s1"},
    {"_read_session": MISS, "_locate_session_db": sc(["db2", "p2"]),
     "_read_session": [MISS, rs("RuntimeError", "second read")],
     "session_db.close": rs("ValueError", "close fail")})
add("session_search_impl", {"db": "db1", "session_id": "s1"},
    {"_read_session": MISS, "_locate_session_db": sc(["db2", "p2"]),
     "_read_session": [MISS, OK], "session_db.close": sc(None)})
add("session_search_impl", {"db": "db1", "session_id": "s1"},
    {"_read_session": None})
add("session_search_impl", {"db": "db1", "session_id": "s1"},
    {"_read_session": rs("RuntimeError", "read fail")})

# profile resolution
add("session_search_impl", {"db": "db1", "profile": "p1"},
    {"_resolve_profile_db": "dbP", B: "prof-browse"},
    {"owned_dbs": []})
add("session_search_impl", {"db": "db1", "profile": "p1"},
    {"_resolve_profile_db": "dbP", B: "prof-browse2"},
    {"owned_dbs": ["seed"]})
add("session_search_impl", {"db": "db1", "profile": "p1"},
    {"_resolve_profile_db": None, B: "prof-null"})
add("session_search_impl", {"db": "db1", "profile": ""}, {B: "empty-prof"})
add("session_search_impl", {"db": "db1", "profile": "p1"},
    {"_resolve_profile_db": rs("ValueError", "bad profile")})
add("session_search_impl", {"db": "db1", "profile": "p1"},
    {"_resolve_profile_db": rs("SystemExit", "5")})
add("session_search_impl", {"db": "db1", "profile": "p1"},
    {"_resolve_profile_db": "dbP", "tool_error": rs("RuntimeError", "tool fail")})

# raw @session:<profile>/<id> link values
add("session_search_impl", {"db": "db1", "session_id": "p1/s1"},
    {"_read_session": OK})
add("session_search_impl", {"db": "db1", "session_id": "p1/s1", "profile": "explicit"},
    {"_read_session": OK})
add("session_search_impl", {"db": "db1", "session_id": "p1/s1", "profile": "  "},
    {"_resolve_profile_db": "dbP", "_read_session": OK})
add("session_search_impl", {"db": "db1", "session_id": "/s1"}, {"_read_session": OK})
add("session_search_impl", {"db": "db1", "session_id": "p1/"}, {"_read_session": OK})
add("session_search_impl", {"db": "db1", "session_id": "a/b/c"}, {"_read_session": OK})


def main():
    counts = {}
    for case in CASES:
        counts[case["func"]] = counts.get(case["func"], 0) + 1
    for func in ["session_end_reason", "session_left_live_context",
                 "is_fresh_reset_session", "session_search_impl"]:
        assert counts.get(func, 0) >= 5, f"{func}: only {counts.get(func, 0)} cases"
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for case in CASES:
            line = json.dumps(case, ensure_ascii=True)
            assert line.isascii(), "non-ASCII fixture"
            f.write(line + "\n")
    print(f"wrote {len(CASES)} cases -> {OUT}")
    for func, n in sorted(counts.items()):
        print(f"  {func}: {n}")


if __name__ == "__main__":
    main()
