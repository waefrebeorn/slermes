#!/usr/bin/env python3
"""Generate tests/fixtures/curator_flow.jsonl — differential cases for the
UNCLAIMED hermes_cli/curator.py FLOW cores + leaves (src/cli/curator_flow.c).

Every case is {"func", "in"}; "in.seams" scripts the universal seam (single
value reused, array consumed per call with the last element repeating,
{"__raise__": kind, "msg": ...} scripts an exception, a MISSING script is
fail-closed RuntimeError on both sides). Output seams (print / input) default
to null so the flows run to completion unless a case overrides them.

Domain (see the port header): ASCII skill names / paths / messages; paths are
verbatim POSIX strings; _parse_ts inputs stay in the documented ISO subset
(extended/basic date, T/t/space separator, HH[:MM[:SS]] time, optional .ffffff
fraction, optional Z / +-HH:MM / +-HHMM / +-HH offset); no integral floats in
str()-computed positions; ledger byte counts are clean multiples of 2**20;
archive iterdir children are same-depth absolute paths (string sort == Path
parts sort); _parse_iso_timestamp returns null or {"ts": epoch}; cfg_get
returns an int or null.

Run:  python tests/gen_curator_flow_cases.py
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "curator_flow.jsonl")

OUTPUT_SEAMS = {"print": None, "input": None}
ENV = {"STATE_ARCHIVED": "archived"}


def case(func, seams=None, env=None, **inp):
    s = dict(OUTPUT_SEAMS)
    s.update(seams or {})
    d = dict(inp)
    d["seams"] = s
    d["env"] = env if env is not None else dict(ENV)
    return {"func": func, "in": d}


def bare(func, seams=None, **inp):
    """Case WITHOUT the default output scripts (fail-closed probes)."""
    d = dict(inp)
    d["seams"] = dict(seams or {})
    d["env"] = dict(ENV)
    return {"func": func, "in": d}


CASES = []

# ── _parse_ts (pure datetime leaf; no seams) ──
for ts in ["2026-11-18T12:00:00", "2026-11-18T12:00:00+00:00",
           "2026-11-18T12:00:00Z", "2026-11-18 12:00:00", "2026-11-18",
           "2026-11-18T12:00", "2026-11-18T12:00:00.123456",
           "2026-11-18T12:00:00.123", "2026-11-18T12:00:00+05:30",
           "2026-11-18T12:00:00-08:00", "20261118", "20261118T120000",
           "2026-11-18T12:00:00+0530", "2026-11-18T12:00:00+05",
           "2026-11-18t12:00:00", "2026-11-18T12",
           "not-a-date", "", "2026-13-40", "2026-02-30",
           "2026-11-18T25:00:00", "2026-11-18T12:60:00",
           None, 5, True]:
    CASES.append(case("parse_ts", ts=ts))

# ── _arg (pure leaf) ──
CASES += [
    case("arg", flags=["skill"], kwargs={"help": "Skill name"}),
    case("arg", flags=["-y", "--yes"], kwargs={"action": "store_true"}),
    case("arg", flags=[], kwargs={}),
    case("arg", flags=["--sort"], kwargs={"choices": ["activity", "recent", "name"],
                                          "default": "activity"}),
    case("arg", flags=["--days"], kwargs={"type": "int", "default": None}),
    case("arg", flags=["a", "b", "c"], kwargs={"nargs": "*", "help": "h"}),
]

# ── _freq (nested pure leaf in _cmd_status) ──
CASES += [
    case("freq", r={"activity_count": 5, "last_activity_at": "2026-11-18"}),
    case("freq", r={}),
    case("freq", r={"activity_count": 0, "last_activity_at": ""}),
    case("freq", r={"activity_count": None, "last_activity_at": None}),
    case("freq", r={"activity_count": 3}),
    case("freq", r={"last_activity_at": "2026-01-01"}),
    case("freq", r={"activity_count": 0, "last_activity_at": "x"}),
]

# ── _USAGE_SORTS key lambdas ──
CASES += [
    case("usage_key_name", r={"name": "alpha"}),
    case("usage_key_name", r={"name": ""}),
    case("usage_key_name", r={"name": "z", "other": 1}),
    case("usage_key_name", r={}),                       # KeyError 'name'
    case("usage_key_name", r={"name": "a-b_c"}),
    case("usage_key_name", r={"name": "x", "name2": "y"}),
]
CASES += [
    case("usage_key_recent", r={"last_activity_at": "2026-11-18"}),
    case("usage_key_recent", r={}),
    case("usage_key_recent", r={"last_activity_at": ""}),
    case("usage_key_recent", r={"last_activity_at": None}),
    case("usage_key_recent", r={"last_activity_at": "2026-01-01T00:00:00"}),
    case("usage_key_recent", r={"other": 1}),
]
CASES += [
    case("usage_key_activity", r={"activity_count": 7}),
    case("usage_key_activity", r={}),
    case("usage_key_activity", r={"activity_count": 0}),
    case("usage_key_activity", r={"activity_count": None}),   # .get default 0, None kept
    case("usage_key_activity", r={"activity_count": 123}),
    case("usage_key_activity", r={"other": 1}),
]

# ── _report_rollback ──
CASES += [
    case("report_rollback", ok=True, msg="restored 3 files"),
    case("report_rollback", ok=False, msg="blob missing"),
    case("report_rollback", ok=True, msg=""),
    case("report_rollback", ok=False, msg=""),
    case("report_rollback", {"print": {"__raise__": "SystemExit", "msg": "1"}},
         ok=True, msg="x"),
    bare("report_rollback", ok=True, msg="x"),          # print fail-closed
]

# ── _confirm ──
CASES += [
    case("confirm", {"input": "y"}, prompt="Proceed? "),
    case("confirm", {"input": "YES"}, prompt="Proceed? "),
    case("confirm", {"input": "  Yes  "}, prompt="Proceed? "),
    case("confirm", {"input": "n"}, prompt="Proceed? "),
    case("confirm", {"input": ""}, prompt="Proceed? "),
    case("confirm", {"input": "maybe"}, prompt="Proceed? ", cancel="curator: aborted"),
    case("confirm", {"input": {"__raise__": "EOFError", "msg": ""}}, prompt="P? "),
    case("confirm", {"input": {"__raise__": "KeyboardInterrupt", "msg": ""}}, prompt="P? "),
    case("confirm", {"input": {"__raise__": "EOFError", "msg": ""}},
         prompt="P? ", cancel="curator: aborted", eof_prefix=""),
    case("confirm", {"input": {"__raise__": "RuntimeError", "msg": "io"}}, prompt="P? "),
    case("confirm", {"input": {"__raise__": "SystemExit", "msg": "1"}}, prompt="P? "),
    case("confirm", {"input": {"__raise__": "GeneratorExit", "msg": ""}}, prompt="P? "),
    bare("confirm", prompt="P? "),                       # input fail-closed
    case("confirm", {"input": "y", "print": {"__raise__": "ValueError", "msg": "p"}},
         prompt="P? "),
]

# ── _print_skill_rows ──
ROW = {"name": "skill-a", "activity_count": 5, "use_count": 2, "view_count": 1,
       "patch_count": 0, "last_activity_at": "2026-11-18T12:00:00"}
CASES += [
    case("print_skill_rows", {"_fmt_ts": "3s ago"}, title="least active", rows=[ROW]),
    case("print_skill_rows", {"_fmt_ts": "never"}, title="t", rows=[]),
    case("print_skill_rows", {"_fmt_ts": "1d ago"}, title="t",
         rows=[{"name": "b"}, {"name": "a", "activity_count": 9}]),
    case("print_skill_rows", {"_fmt_ts": "2h ago"}, title="t",
         rows=[{"name": "x", "use_count": 3, "view_count": 4, "patch_count": 5}]),
    case("print_skill_rows", {"_fmt_ts": "never"}, title="t", rows=[{}]),   # KeyError 'name'
    case("print_skill_rows", {"_fmt_ts": {"__raise__": "RuntimeError", "msg": "f"}},
         title="t", rows=[ROW]),
    bare("print_skill_rows", title="t", rows=[ROW]),     # print fail-closed
    case("print_skill_rows", {"_fmt_ts": "never"}, title="t",
         rows=[{"name": "n", "last_activity_at": None}]),
]

# ── _print_curator_config ──
def cfg_seams(**over):
    s = {"load_state": {"paused": False, "run_count": 3,
                        "last_run_at": "2026-11-18T12:00:00",
                        "last_run_summary": "ok", "last_report_path": None},
         "is_enabled": True, "get_interval_hours": 24,
         "get_stale_after_days": 14, "get_archive_after_days": 30,
         "get_consolidate": False, "_fmt_ts": "3s ago", "exists": True}
    s.update(over)
    return s


CASES += [
    case("print_curator_config", cfg_seams()),
    case("print_curator_config", cfg_seams(load_state={"paused": True})),
    case("print_curator_config", cfg_seams(is_enabled=False)),
    case("print_curator_config", cfg_seams(get_interval_hours=12)),
    case("print_curator_config", cfg_seams(get_interval_hours=168)),
    case("print_curator_config", cfg_seams(get_consolidate=True)),
    case("print_curator_config", cfg_seams(
        load_state={"last_run_summary": "a\nb\nc", "last_report_path": "/r/p.md"})),
    case("print_curator_config", cfg_seams(
        load_state={"last_run_summary": "a\nb", "last_report_path": "/r/p.md"},
        exists=False)),
    case("print_curator_config", cfg_seams(load_state={})),
    case("print_curator_config", cfg_seams(
        load_state={"last_run_summary": None, "last_report_path": "/r/p.md"})),
    case("print_curator_config", cfg_seams(
        load_state={"__raise__": "RuntimeError", "msg": "state"})),
    bare("print_curator_config"),                        # load_state fail-closed
]

# ── _set_paused ──
CASES += [
    case("set_paused", {"set_paused": None}, paused=True),
    case("set_paused", {"set_paused": None}, paused=False),
    case("set_paused", {"set_paused": {"__raise__": "RuntimeError", "msg": "s"}}, paused=True),
    case("set_paused", {"set_paused": {"__raise__": "SystemExit", "msg": "1"}}, paused=False),
    bare("set_paused", paused=True),                     # set_paused fail-closed
    case("set_paused", {"set_paused": None, "print": {"__raise__": "ValueError", "msg": "p"}},
         paused=True),
]

# ── _set_pin ──
def pin_seams(**over):
    s = {"is_agent_created": True, "set_pinned": True, "is_curator_managed": True}
    s.update(over)
    return s


for pinned in (True, False):
    CASES += [
        case("set_pin", pin_seams(), skill="sk", pinned=pinned),
        case("set_pin", pin_seams(is_agent_created=False), skill="sk", pinned=pinned),
        case("set_pin", pin_seams(set_pinned=False), skill="sk", pinned=pinned),
        case("set_pin", pin_seams(is_curator_managed=False), skill="sk", pinned=pinned),
        case("set_pin", pin_seams(is_agent_created={"__raise__": "RuntimeError", "msg": "a"}),
             skill="sk", pinned=pinned),
        case("set_pin", pin_seams(set_pinned={"__raise__": "SystemExit", "msg": "1"}),
             skill="sk", pinned=pinned),
        bare("set_pin", skill="sk", pinned=pinned),      # is_agent_created fail-closed
    ]

# ── _set_lock / _cmd_lock / _cmd_unlock ──
def lock_seams(**over):
    s = {"_find_skill": {"path": "/skills/myskill"}, "set_locked": True}
    s.update(over)
    return s


for fn in ("set_lock", "cmd_lock", "cmd_unlock"):
    locked = fn != "cmd_unlock"
    CASES += [
        case(fn, lock_seams(), skill="myskill", **({} if fn == "set_lock" else {})),
        case(fn, lock_seams(_find_skill=None), skill="myskill"),
        case(fn, lock_seams(set_locked=False), skill="myskill"),
        case(fn, lock_seams(_find_skill={"path": "/skills/other-name"}), skill="myskill"),
        case(fn, lock_seams(_find_skill={"__raise__": "RuntimeError", "msg": "f"}), skill="myskill"),
        case(fn, lock_seams(set_locked={"__raise__": "SystemExit", "msg": "1"}), skill="myskill"),
        bare(fn, skill="myskill"),                       # _find_skill fail-closed
    ]
# set_lock takes an explicit locked flag; add both polarities
CASES += [
    case("set_lock", lock_seams(), skill="myskill", locked=False),
    case("set_lock", lock_seams(set_locked=False), skill="myskill", locked=False),
]

# ── _as_user ──
def as_user_seams(**over):
    s = {"set_ledger_actor": "tok1", "fn": [[True, "restored sk"]], "reset_ledger_actor": None}
    s.update(over)
    return s


CASES += [
    case("as_user", as_user_seams(), skill="sk"),
    case("as_user", as_user_seams(fn=[[False, "cannot restore"]]), skill="sk"),
    case("as_user", as_user_seams(fn={"__raise__": "RuntimeError", "msg": "boom"}), skill="sk"),
    case("as_user", as_user_seams(fn={"__raise__": "SystemExit", "msg": "1"}), skill="sk"),
    case("as_user", as_user_seams(set_ledger_actor={"__raise__": "RuntimeError", "msg": "a"}),
         skill="sk"),
    case("as_user", as_user_seams(reset_ledger_actor={"__raise__": "ValueError", "msg": "r"}),
         skill="sk"),
    bare("as_user", skill="sk"),                         # set_ledger_actor fail-closed
    case("as_user", as_user_seams(fn=[[True, "ok"]],
         print={"__raise__": "KeyboardInterrupt", "msg": ""}), skill="sk"),
]

# ── _cmd_ledger ──
LEDGER_ROWS = [
    {"id": "e1", "ts": "2026-11-18T12:00:00", "actor": "user", "action": "archive",
     "skill": "sk", "evidence": {"absorbed_into": "umb"}},
    {"id": "e2", "ts": "2026-11-17T12:00:00", "actor": "curator", "action": "stale",
     "skill": "sk2", "evidence": {"rollback_target": "e0"}},
    {"id": "e3", "ts": None, "actor": "agent", "action": "create", "skill": "sk3"},
]


def ledger_seams(**over):
    s = {"list_entries": [LEDGER_ROWS], "_fmt_ts": "3s ago",
         "compact_ledger": [[3, 2097152, 1048576]], "gc_blobs": [[2, 524288]]}
    s.update(over)
    return s


CASES += [
    case("cmd_ledger", ledger_seams()),
    case("cmd_ledger", ledger_seams(list_entries=[[]])),
    case("cmd_ledger", ledger_seams(), compact=True),
    case("cmd_ledger", ledger_seams(list_entries=[[{"id": "x"}]])),
    case("cmd_ledger", ledger_seams(list_entries=[[
        {"id": "e1", "ts": "t", "actor": "user", "action": "archive", "skill": "sk",
         "evidence": {}}]])),
    case("cmd_ledger", ledger_seams(list_entries={"__raise__": "RuntimeError", "msg": "l"})),
    case("cmd_ledger", ledger_seams(compact_ledger={"__raise__": "SystemExit", "msg": "1"}),
         compact=True),
    bare("cmd_ledger"),                                  # list_entries fail-closed
    case("cmd_ledger", ledger_seams(), skill="sk", limit=5),
    case("cmd_ledger", ledger_seams(list_entries=[[
        {"id": "e1", "ts": "t", "actor": "user", "action": "a", "skill": "s",
         "evidence": None}]])),
]

# ── _rollback_ledger_entry ──
ENTRY = {"action": "archive", "skill": "sk", "actor": "user",
         "ts": "2026-11-18T12:00:00",
         "before": [{"path": "/skills/sk/SKILL.md"}],
         "after": [{"path": "/skills/sk/SKILL.md"}, {"path": "/skills/sk/ref.md"}]}


def rb_seams(**over):
    s = {"get_entry": ENTRY, "rollback_entry": [[True, "restored 2 files"]], "input": "y"}
    s.update(over)
    return s


CASES += [
    case("rollback_ledger_entry", rb_seams(), entry_id="e1"),
    case("rollback_ledger_entry", rb_seams(get_entry=None), entry_id="e1"),
    case("rollback_ledger_entry", rb_seams(input="n"), entry_id="e1"),
    case("rollback_ledger_entry", rb_seams(), entry_id="e1", yes=True),
    case("rollback_ledger_entry", rb_seams(rollback_entry=[[False, "blob missing"]]), entry_id="e1"),
    case("rollback_ledger_entry", rb_seams(get_entry={"action": "a"}), entry_id="e1"),
    case("rollback_ledger_entry", rb_seams(get_entry={"__raise__": "RuntimeError", "msg": "g"}),
         entry_id="e1"),
    case("rollback_ledger_entry", rb_seams(rollback_entry={"__raise__": "SystemExit", "msg": "1"}),
         entry_id="e1"),
    bare("rollback_ledger_entry", entry_id="e1"),        # get_entry fail-closed
    case("rollback_ledger_entry", rb_seams(input={"__raise__": "EOFError", "msg": ""}),
         entry_id="e1"),
]

# ── _cmd_purge ──
ARCH = "/home/u/.hermes/skills/.archive"
USAGE = {"sk-a": {"state": "archived", "archived_at": "2026-11-01T00:00:00"},
         "sk-b": {"state": "archived", "archived_at": None},
         "sk-c": {"state": "active"}}


def purge_seams(**over):
    s = {"load_config": {}, "cfg_get": 30, "_archive_dir": ARCH, "exists": True,
         "time": 1800000000.0, "load_usage": USAGE,
         "iterdir": [[ARCH + "/sk-a", ARCH + "/sk-b", ARCH + "/sk-c"]],
         "is_dir": True, "stat": {"st_mtime": 1700000000.0},
         "read_skill_name": "sk-a", "_parse_iso_timestamp": {"ts": 1700000000.0},
         "rmtree": None, "capture_before": [[{"path": "b"}]], "append_entry": None,
         "input": "y"}
    s.update(over)
    return s


CASES += [
    case("cmd_purge", purge_seams()),
    case("cmd_purge", purge_seams(days=0)),
    case("cmd_purge", purge_seams(cfg_get=0)),
    case("cmd_purge", purge_seams(exists=False)),
    case("cmd_purge", purge_seams(iterdir=[[]])),
    case("cmd_purge", purge_seams(), dry_run=True),
    case("cmd_purge", purge_seams(input="n")),
    case("cmd_purge", purge_seams(), yes=True),
    case("cmd_purge", purge_seams(rmtree={"__raise__": "OSError", "msg": "disk full"})),
    case("cmd_purge", purge_seams(rmtree={"__raise__": "RuntimeError", "msg": "x"})),
    case("cmd_purge", purge_seams(_parse_iso_timestamp=None)),
    case("cmd_purge", purge_seams(load_config={"__raise__": "RuntimeError", "msg": "c"})),
    case("cmd_purge", purge_seams(time={"__raise__": "SystemExit", "msg": "1"})),
    bare("cmd_purge"),                                   # load_config fail-closed
    case("cmd_purge", purge_seams(is_dir=False)),
    case("cmd_purge", purge_seams(stat={"st_mtime": 1900000000.0})),
    case("cmd_purge", purge_seams(rmtree={"__raise__": "PermissionError", "msg": "denied"})),
    case("cmd_purge", purge_seams(rmtree={"__raise__": "ProcessLookupError", "msg": "gone"})),
    case("cmd_purge", purge_seams(rmtree={"__raise__": "SystemExit", "msg": "1"})),
    case("cmd_purge", purge_seams(capture_before={"__raise__": "RuntimeError", "msg": "cap"})),
    case("cmd_purge", purge_seams(append_entry={"__raise__": "RuntimeError", "msg": "ae"})),
]

# ── _archived_ts (nested closure in _cmd_purge) ──
CASES += [
    case("archived_ts", {"read_skill_name": "sk-a", "_parse_iso_timestamp": {"ts": 1700000000.0},
                         "stat": {"st_mtime": 1600000000.0}},
         path=ARCH + "/sk-a", usage=USAGE),
    case("archived_ts", {"read_skill_name": "sk-a", "_parse_iso_timestamp": {"ts": 1700000000.0},
                         "stat": {"st_mtime": 1900000000.0}},
         path=ARCH + "/sk-a", usage=USAGE),
    case("archived_ts", {"read_skill_name": "sk-b", "_parse_iso_timestamp": None,
                         "stat": {"st_mtime": 1600000000.0}},
         path=ARCH + "/sk-b", usage=USAGE),
    case("archived_ts", {"read_skill_name": "sk-c", "stat": {"st_mtime": 1600000000.0}},
         path=ARCH + "/sk-c", usage=USAGE),
    case("archived_ts", {"read_skill_name": "unknown", "stat": {"st_mtime": 1600000000.0}},
         path=ARCH + "/unknown", usage=USAGE),
    case("archived_ts", {"read_skill_name": "sk-a",
                         "_parse_iso_timestamp": {"__raise__": "RuntimeError", "msg": "p"},
                         "stat": {"st_mtime": 1600000000.0}},
         path=ARCH + "/sk-a", usage=USAGE),
    bare("archived_ts", path=ARCH + "/sk-a", usage=USAGE),   # read_skill_name fail-closed
]


def main():
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    short = {f: n for f, n in counts.items() if n < 5}
    if short:
        print("GEN curator_flow: FAIL funcs with <5 cases: %s" % short, file=sys.stderr)
        sys.exit(1)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    print("GEN curator_flow: %d cases, %d funcs -> %s" % (len(CASES), len(counts), OUT))
    for fn in sorted(counts):
        print("  %-26s %d" % (fn, counts[fn]))


if __name__ == "__main__":
    main()