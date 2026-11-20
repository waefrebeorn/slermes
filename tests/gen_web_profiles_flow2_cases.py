#!/usr/bin/env python3
"""Generate tests/fixtures/web_profiles_flow2.jsonl — scripted-seam FLOW cases
for the UNCLAIMED-helper port src/cli/web_profiles_flow2.c.

Every case: {"func": <harness name>, "in": {... inputs ..., "seams": {name: script}}}
A seam script is a single result, or a JSON array = one result PER CALL (last
repeats). {"__raise__": cls, "msg": m} raises a REAL builtin of that class in the
live drive and is delivered as a raise descriptor to the C universal seam. A
MISSING seam key fail-closes (RuntimeError) on BOTH sides; a present JSON null
is a valid None.

Domains: ASCII strings; JSON integral numbers; storage state scripts are
{"state": "corrupt"|"ok"} (StorageState.CORRUPT.value == "corrupt"); targets are
[[name, home], ...] pairs; exception families cover the full builtin hierarchy.
Run: python tests/gen_web_profiles_flow2_cases.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "web_profiles_flow2.jsonl")

CASES = []


def add(func, inp, seams=None):
    inp = dict(inp)
    inp["seams"] = seams or {}
    CASES.append({"func": func, "in": inp})


def rs(cls, msg="boom"):
    return {"__raise__": cls, "msg": msg}


# ══════════════ _recency ══════════════
for s in [
    {"last_active": 100},
    {"started_at": 200},
    {"last_active": 0, "started_at": 300},
    {},
    {"last_active": None, "started_at": "recent"},
    {"last_active": "la", "started_at": 5},
    {"last_active": [], "started_at": 9},
    {"last_active": False, "started_at": 7},
]:
    add("recency", {"s": s})

# ══════════════ _csv_list ══════════════
for v in ["a,b,c", " a , b ,, c ", "", None, "  ", "one", "x,,y,", 0, " a\tb , c\n"]:
    add("csv_list", {"value": v})

# ══════════════ _slice_has_rows ══════════════
for s in [
    {"recents": [{"id": 1}]},
    {"cron": {}, "messaging": []},
    {},
    {"messaging": [1]},
    {"recents": None, "cron": 0, "messaging": False},
    {"recents": 0, "cron": "x"},
]:
    add("slice_has_rows", {"slices": s})

# ══════════════ _failed_load_slice ══════════════
add("failed_load_slice", {"errors": [], "extra": {}})
add("failed_load_slice", {"errors": [{"profile": "p", "error": "e"}], "extra": {}})
add("failed_load_slice", {"errors": [{"profile": "p"}], "extra": {"hint": "h"}})
add("failed_load_slice", {"errors": [], "extra": {"failed": False, "extra_key": 1}})
add("failed_load_slice", {"errors": None, "extra": {}})
add("failed_load_slice", {"errors": [{"profile": "p", "error": "e"}],
                          "extra": {"hint": "h2", "sessions": None}})

# ══════════════ _profiles_failed ══════════════
add("profiles_failed", {"errors": []})
add("profiles_failed", {"errors": [{"profile": "p", "error": "e"}]})
add("profiles_failed", {"errors": [{"profile": "", "error": "e"}, {"profile": "q"}]})
add("profiles_failed", {"errors": [{"error": "x"}]})
add("profiles_failed", {"errors": [{"profile": "p", "error": None}]})
add("profiles_failed", {"errors": [{"profile": "p", "error": "a"},
                                   {"profile": "p", "error": "b"}]})

# ══════════════ _no_store_file ══════════════
add("no_store_file", {"name": "p", "errors": []})
add("no_store_file", {"name": "p", "errors": [{"profile": "x", "error": "y"}]})
add("no_store_file", {"name": "p", "errors": None})
add("no_store_file", {"name": "", "errors": []})
add("no_store_file", {"name": "default",
                      "errors": [{"profile": "a", "error": "b"}, {"profile": "c", "error": "d"}]})

# ══════════════ _tag_rows ══════════════
add("tag_rows", {"rows": [], "name": "default", "now": 100})
add("tag_rows", {"rows": [{"id": 1}], "name": "default", "now": 100})
add("tag_rows", {"rows": [{"id": 1, "last_active": 90, "ended_at": None,
                           "archived": 1, "pinned": 0}], "name": "p", "now": 100})
add("tag_rows", {"rows": [{"id": 2, "started_at": 10, "ended_at": "2020"}],
                 "name": "p", "now": 1000})
add("tag_rows", {"rows": [{"id": 3, "last_active": 50, "ended_at": None}],
                 "name": "p", "now": 400})
add("tag_rows", {"rows": [{"id": 4, "archived": 0, "pinned": True}],
                 "name": "default", "now": 10})
add("tag_rows", {"rows": [{"id": 5, "ended_at": None}], "name": "x", "now": 5})
add("tag_rows", {"rows": [{"id": 6, "last_active": 290, "pinned": 1},
                          {"id": 7, "started_at": 1, "archived": "yes"}],
                 "name": "default", "now": 300})

# ══════════════ _pinned_window ══════════════
ROWS5 = [{"id": i, "pinned": (i in (3, 4))} for i in range(5)]
add("pinned_window", {"rows": ROWS5, "offset": 0, "cap": 2})
add("pinned_window", {"rows": ROWS5, "offset": 1, "cap": 2})
add("pinned_window", {"rows": ROWS5, "offset": 2, "cap": 10})
add("pinned_window", {"rows": ROWS5, "offset": 5, "cap": 2})
add("pinned_window", {"rows": [{"id": 0, "pinned": True}, {"id": 1, "pinned": False}],
                      "offset": 0, "cap": 0})
add("pinned_window", {"rows": [], "offset": 0, "cap": 3})
add("pinned_window", {"rows": [{"id": 0}, {"id": 1, "pinned": 1}],
                      "offset": 0, "cap": 1})

# ══════════════ _linux_terminal_commands ══════════════
for cmd in ["hermes setup", "", "a'b", "x; echo hi", "default setup"]:
    add("linux_terminal_commands", {"command": cmd})

# ══════════════ _profile_state_db ══════════════
add("profile_state_db", {"home": "/h/p"}, {"store_location": "/db/path"})
add("profile_state_db", {"home": "/h/p"}, {"store_location": None})
add("profile_state_db", {"home": "/h/p"}, {"store_location": rs("ValueError")})
add("profile_state_db", {"home": "/h/p"}, {"store_location": rs("FileNotFoundError")})
add("profile_state_db", {"home": ""}, {"store_location": "db"})

# ══════════════ _corrupt_profile_stores ══════════════
add("corrupt_profile_stores", {"targets": [["p", "/h"]]},
    {"storage_health": {"state": "corrupt"}})
add("corrupt_profile_stores", {"targets": [["p", "/h"]]},
    {"storage_health": {"state": "ok"}})
add("corrupt_profile_stores", {"targets": [["a", "/a"], ["b", "/b"]]},
    {"storage_health": [{"state": "corrupt"}, {"state": "ok"}]})
add("corrupt_profile_stores", {"targets": [["p", "/h"]]},
    {"storage_health": rs("OSError")})
add("corrupt_profile_stores", {"targets": []}, {})
add("corrupt_profile_stores", {"targets": [["p", "/h"], ["q", "/q"]]},
    {"storage_health": [{"state": "ok"}, {"state": "corrupt"}]})

# ══════════════ _retryable_profile_errors ══════════════
add("retryable_profile_errors",
    {"errors": [{"profile": "p", "error": "e"}, {"profile": "q", "error": "f"}],
     "scanned": [["p", "/h"]]},
    {"storage_health": {"state": "corrupt"}})
add("retryable_profile_errors",
    {"errors": [{"profile": "p"}, {"profile": "q"}], "scanned": [["p", "/h"]]},
    {"storage_health": {"state": "ok"}})
add("retryable_profile_errors",
    {"errors": [{"profile": "p"}, {"error": "no profile"}], "scanned": []}, {})
add("retryable_profile_errors",
    {"errors": [{"profile": "p"}], "scanned": [["p", "/h"]]},
    {"storage_health": rs("RuntimeError")})
add("retryable_profile_errors",
    {"errors": [{"profile": "p"}, {"profile": "q"}, {"profile": "r"}],
     "scanned": [["p", "/h"], ["r", "/r"]]},
    {"storage_health": [{"state": "corrupt"}, {"state": "corrupt"}]})

# ══════════════ _scoped_profile_targets ══════════════
TAR = [["default", "/d"], ["p", "/p"], ["q", "/q"]]
# The target list is a single VALUE, not a per-call queue -> wrap with __arr__.
add("scoped_profile_targets", {"log_label": "scan", "profile": None},
    {"profile_targets": {"__arr__": TAR}})
add("scoped_profile_targets", {"log_label": "scan", "profile": "p"},
    {"profile_targets": {"__arr__": TAR}})
add("scoped_profile_targets", {"log_label": "scan", "profile": "  p  "},
    {"profile_targets": {"__arr__": TAR}})
add("scoped_profile_targets", {"log_label": "scan", "profile": ""},
    {"profile_targets": {"__arr__": TAR}})
add("scoped_profile_targets", {"log_label": "scan", "profile": "p"},
    {"profile_targets": rs("OSError")})
add("scoped_profile_targets", {"log_label": "scan", "profile": None},
    {"profile_targets": {"__arr__": []}})

# ══════════════ _best_effort (FULL builtin exception hierarchy) ══════════════
add("best_effort", {"log_msg": "m", "args": ["x"], "default": {"d": 1}},
    {"fn": {"ok": 1}})
add("best_effort", {"log_msg": "m", "args": [], "default": "unused"},
    {"fn": None})
add("best_effort", {"log_msg": "m", "args": ["a", "b"], "default": {"d": 1}},
    {"fn": rs("ValueError"), "log_exception": None})
add("best_effort", {"log_msg": "m", "args": [], "default": None},
    {"fn": rs("TypeError"), "log_exception": None})
add("best_effort", {"log_msg": "m", "args": [], "default": "fallback"},
    {"fn": rs("FileNotFoundError"), "log_exception": None})
add("best_effort", {"log_msg": "m", "args": [], "default": 0},
    {"fn": rs("StopIteration"), "log_exception": None})
add("best_effort", {"log_msg": "m", "args": [], "default": 0},
    {"fn": rs("ExceptionGroup"), "log_exception": None})
# BaseException family -> NOT caught, no log_exception seam fires
add("best_effort", {"log_msg": "m", "args": [], "default": 0},
    {"fn": rs("KeyboardInterrupt")})
add("best_effort", {"log_msg": "m", "args": [], "default": 0},
    {"fn": rs("SystemExit")})
add("best_effort", {"log_msg": "m", "args": [], "default": 0},
    {"fn": rs("GeneratorExit")})
add("best_effort", {"log_msg": "m", "args": [], "default": 0},
    {"fn": rs("BaseExceptionGroup")})
# array script: first call raises (caught), never reaches the second
add("best_effort", {"log_msg": "m", "args": [], "default": "dflt"},
    {"fn": [rs("ValueError"), {"ok": 2}], "log_exception": None})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print("wrote %d cases (%d funcs) to %s" % (len(CASES), len(counts), OUT))
    thin = {k: v for k, v in counts.items() if v < 5}
    if thin:
        print("WARNING <5 cases: %s" % thin)


if __name__ == "__main__":
    main()
