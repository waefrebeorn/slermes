#!/usr/bin/env python3
"""Generate tests/fixtures/kanban_cli_flow.jsonl — differential cases for the
UNCLAIMED hermes_cli/kanban.py FLOW cores + leaves (src/cli/kanban_cli_flow.c).

Every case is {"func", "in"}; "in.seams" scripts the universal seam (single
value reused, array consumed per call with the last element repeating,
{"__raise__": kind, "msg": ...} scripts an exception, a MISSING script is
fail-closed RuntimeError on both sides). Output seams (print /
sys.stdout.write / _err / json.dumps) default so the flows run to completion
unless a case overrides them.

SCRIPT SHAPE: a bare array is a per-call QUEUE (one element consumed per
call). To script a seam whose live return is a LIST/TUPLE (conn.execute rows,
mod.list_triage_ids ids, kb.request_changes (ok, detail),
_goal_mode_handoff_rejection (verdict, rejection)), WRAP it: [[...]]. Dict /
scalar scripts are single values reused.

Domain (see the port header): ASCII strings; floats in str()-computed
positions are NON-integral and inside [1e-4, 1e16); data-dict values are
scalar-or-list-of-scalars; ids/outcome/diag doubles are JSON objects
(SimpleNamespace on the live side); conn is the opaque marker "CONN";
scripted exception kinds use canonical builtin names (JSONDecodeError is
json's ValueError subclass; the Unicode* trio str() to "").

Run:  python tests/gen_kanban_cli_flow_cases.py
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "kanban_cli_flow.jsonl")
OUTPUT_SEAMS = {"print": None, "sys.stdout.write": None, "_err": 1,
                "json.dumps": "{}"}


def case(func, seams=None, **inp):
    s = dict(OUTPUT_SEAMS)
    s.update(seams or {})
    d = dict(inp)
    d["seams"] = s
    return {"func": func, "in": d}


def bare(func, seams=None, **inp):
    """Case WITHOUT the default output scripts (fail-closed probes)."""
    d = dict(inp)
    d["seams"] = dict(seams or {})
    return {"func": func, "in": d}


def R(kind, msg=""):
    return {"__raise__": kind, "msg": msg}


CASES = []

# ── _none_profile (pure leaf) ──
for v in ["none", "NONE", "-", "null", "Null", "alice", "", " none ", "None"]:
    CASES.append(case("none_profile", value=v))
CASES += [
    case("none_profile", value=None),   # AttributeError NoneType.lower
    case("none_profile", value=5),      # AttributeError int.lower
]

# ── _parse_metadata_flag ──
CASES += [
    case("parse_metadata_flag", raw=None),
    case("parse_metadata_flag", raw=""),
    case("parse_metadata_flag", raw=0),
    case("parse_metadata_flag", {"json.loads": {"a": 1}}, raw='{"a":1}'),
    case("parse_metadata_flag", {"json.loads": {"a": 1, "b": [1, 2]}},
         raw='{"a":1,"b":[1,2]}'),
    case("parse_metadata_flag", {"json.loads": [1, 2], "_err": 2}, raw="[1,2]"),
    case("parse_metadata_flag", {"json.loads": 5, "_err": 2}, raw="5"),
    case("parse_metadata_flag", {"json.loads": None, "_err": 2}, raw="null"),
    case("parse_metadata_flag",
         {"json.loads": R("JSONDecodeError", "Expecting value: line 1 column 1 (char 0)"),
          "_err": 2}, raw="bad"),
    case("parse_metadata_flag", {"json.loads": R("ValueError", "boom"), "_err": 2},
         raw="bad"),
    case("parse_metadata_flag",
         {"json.loads": R("UnicodeDecodeError", "bad bytes"), "_err": 2}, raw="bad"),
    case("parse_metadata_flag",
         {"json.loads": R("TypeError", "the JSON object must be str, bytes or bytearray, not int")},
         raw="x"),
    case("parse_metadata_flag", {"json.loads": R("SystemExit", "1")}, raw="x"),
    case("parse_metadata_flag", {"json.loads": R("KeyboardInterrupt")}, raw="x"),
    case("parse_metadata_flag", {"json.loads": R("GeneratorExit")}, raw="x"),
    bare("parse_metadata_flag", raw='{"a":1}'),  # fail-closed json.loads
    bare("parse_metadata_flag", {"json.loads": R("ValueError", "boom")}, raw="bad"),
]

# ── _joined_words (pure leaf) ──
CASES += [
    case("joined_words", words=None),
    case("joined_words", words=[]),
    case("joined_words", words=["a", "b"]),
    case("joined_words", words=[" a ", "b "]),
    case("joined_words", words=[""]),
    case("joined_words", words=["a", 5]),       # TypeError sequence item 1
    case("joined_words", words=["a", None]),    # NoneType found
    case("joined_words", words=[True]),         # bool found
    case("joined_words", words="ab"),           # str iterates chars
    case("joined_words", words=5),              # can only join an iterable
    case("joined_words", words={"a": 1, "b": 2}),  # dict iterates keys
    case("joined_words", words={}),             # falsy -> None
]

# ── _stripped_or_none (pure leaf) ──
CASES += [
    case("stripped_or_none", value=None),
    case("stripped_or_none", value=""),
    case("stripped_or_none", value="   "),
    case("stripped_or_none", value=" x "),
    case("stripped_or_none", value="x"),
    case("stripped_or_none", value="\t\n"),
    case("stripped_or_none", value=5),          # AttributeError int.strip
]

# ── _ok_or_err ──
for ok in [True, False, 0, 1, "", "x", [], [1], None, {}, {"a": 1}]:
    CASES.append(case("ok_or_err", ok=ok, fail="cannot do it", done="Did it"))
CASES += [
    bare("ok_or_err", ok=False, fail="cannot do it", done="Did it"),  # fail-closed _err
    bare("ok_or_err", ok=True, fail="cannot do it", done="Did it"),   # fail-closed print
    case("ok_or_err", {"print": R("SystemExit", "1")}, ok=True, fail="f", done="d"),
    case("ok_or_err", {"_err": 3}, ok=False, fail="f", done="d"),
]

# ── _bulk_ids ──
CASES += [
    case("bulk_ids", args={"task_id": "t1", "ids": ["t2", "t3"]}),
    case("bulk_ids", args={"task_id": "t1", "ids": None}),
    case("bulk_ids", args={"task_id": "t1"}),
    case("bulk_ids", args={"task_id": "t1", "ids": []}),
    case("bulk_ids", args={"task_id": "t1", "ids": "ab"}),
    case("bulk_ids", args={"ids": ["t2"]}),          # AttributeError task_id
    case("bulk_ids", args={"task_id": None}),
    case("bulk_ids", args={"task_id": "t1", "ids": 5}),   # TypeError not iterable
    case("bulk_ids", args={"task_id": "t1", "ids": ["t2", None]}),
]

# ── _require_ids ──
CASES += [
    case("require_ids", args={"task_ids": ["t1", "t2"]}),
    case("require_ids", args={"task_ids": []}),
    case("require_ids", args={"task_ids": None}),
    case("require_ids", args={}),                    # AttributeError task_ids
    case("require_ids", args={"task_ids": "ab"}),
    case("require_ids", args={"task_ids": 5}),       # TypeError not iterable
    case("require_ids", args={"task_ids": ["t1"]}),
    bare("require_ids", args={"task_ids": []}),      # fail-closed _err
]

# ── _print_diagnostics ──
DIAG_FULL = {
    "kind": "stale_claim", "title": "Claim is stale", "severity": "warning",
    "data": {"worker": "alice", "age": 5, "xs": [1, "a", None, True, 1.5], "empty": []},
    "actions": [{"suggested": True, "label": "reclaim it"},
                {"suggested": False, "label": "ignore"}],
}
CASES += [
    case("print_diagnostics", diags=[DIAG_FULL], indent="    ", with_kind=True),
    case("print_diagnostics", diags=[DIAG_FULL], indent="", with_kind=False),
    case("print_diagnostics", diags=[
        {"kind": "k", "title": "Bad", "severity": "error", "data": {},
         "actions": []}], indent="  ", with_kind=True),
    case("print_diagnostics", diags=[
        {"kind": "k", "title": "Down", "severity": "critical", "data": None,
         "actions": [{"suggested": True, "label": "repair"}]}],
        indent="", with_kind=True),
    case("print_diagnostics", diags=[
        {"kind": "k", "title": "Odd", "severity": "info", "data": {"n": 0.5},
         "actions": []}], indent="", with_kind=False),
    case("print_diagnostics", diags=[], indent="", with_kind=True),
    case("print_diagnostics", diags=[{"kind": "k", "severity": "warning",
                                      "data": None, "actions": []}],
         indent="", with_kind=True),                 # AttributeError title
    case("print_diagnostics", diags=[DIAG_FULL,
        {"kind": "k2", "title": "Second", "severity": "error", "data": None,
         "actions": [{"suggested": "yes", "label": "act"}]}],
        indent=" ", with_kind=True),
    bare("print_diagnostics", diags=[DIAG_FULL], indent="", with_kind=True),
]

# ── _print_section ──
CASES += [
    case("print_section", title="Comments", lines=["a", "b"]),
    case("print_section", title="Empty", lines=[]),
    case("print_section", title="Mixed", lines=[5, None, "x"]),
    case("print_section", title="Bad", lines=None),   # TypeError after 2 prints
    case("print_section", title="", lines=[""]),
    bare("print_section", title="T", lines=["a"]),    # fail-closed print
]

# ── _rows_by_task ──
ROWS = [{"task_id": "t1", "id": 1, "body": "a"},
        {"task_id": "t1", "id": 2, "body": "b"},
        {"task_id": "t2", "id": 3, "body": "c"}]
CASES += [
    case("rows_by_task", {"conn.execute": [ROWS]}, conn="CONN",
         table="task_events", ids=["t1", "t2"]),
    case("rows_by_task", {"conn.execute": [[]]}, conn="CONN",
         table="task_events", ids=[]),
    case("rows_by_task", {"conn.execute": [ROWS[:2]]}, conn="CONN",
         table="task_events", ids=["t1", "t1"]),
    case("rows_by_task", {"conn.execute": [ROWS + [{"task_id": "t9", "id": 9}]]},
         conn="CONN", table="task_runs", ids=["t1"]),
    case("rows_by_task", {"conn.execute": [[{"id": 1}]]}, conn="CONN",
         table="task_events", ids=["t1"]),           # KeyError 'task_id'
    bare("rows_by_task", conn="CONN", table="task_events", ids=["t1"]),
    case("rows_by_task", {"conn.execute": R("ValueError", "bad sql")},
         conn="CONN", table="task_events", ids=["t1"]),
    case("rows_by_task", {"conn.execute": None}, conn="CONN",
         table="task_events", ids=["t1"]),           # TypeError not iterable
    case("rows_by_task", {"conn.execute": [ROWS]}, conn="CONN",
         table="task_runs", ids=["t2", "t1"]),
]

# ── _goal_gate_error ──
GG = dict(conn="CONN", tid="t1", evidence="done it", handoff="completion",
          blocked_hint="Re-scope with kanban edit.",
          continue_hint="Provide evidence matching the criteria.")
CASES += [
    case("goal_gate_error", {"kb.get_task": None,
         "_goal_mode_handoff_rejection": [["done", None]]}, **GG),
    case("goal_gate_error", {"kb.get_task": None,
         "_goal_mode_handoff_rejection": [["blocked", "goal impossible"]]}, **GG),
    case("goal_gate_error", {"kb.get_task": {"id": "t1", "goal_mode": True},
         "_goal_mode_handoff_rejection": [["continue", "need more evidence"]]}, **GG),
    case("goal_gate_error", {"kb.get_task": None,
         "_goal_mode_handoff_rejection": [["wait", ""]]}, **GG),
    case("goal_gate_error", {"kb.get_task": None,
         "_goal_mode_handoff_rejection": [["continue", None]]}, **GG),
    case("goal_gate_error", {"kb.get_task": None,
         "_goal_mode_handoff_rejection": [["blocked", None]]}, **GG),
    case("goal_gate_error", {"kb.get_task": None,
         "_goal_mode_handoff_rejection": [[1, 2, 3]]}, **GG),   # ValueError unpack
    case("goal_gate_error", {"kb.get_task": None,
         "_goal_mode_handoff_rejection": 5}, **GG),             # TypeError unpack
    case("goal_gate_error", {"kb.get_task": None,
         "_goal_mode_handoff_rejection": [["only"]]}, **GG),    # ValueError unpack
    case("goal_gate_error", {"kb.get_task": R("RuntimeError", "db down")}, **GG),
    bare("goal_gate_error", {"kb.get_task": None}, **GG),       # fail-closed rejection
    case("goal_gate_error", {"kb.get_task": None,
         "_goal_mode_handoff_rejection": "ab"}, **GG),          # str unpack chars
    case("goal_gate_error", {"kb.get_task": R("SystemExit", "1")}, **GG),
]

# ── _commented ──
CASES += [
    case("commented", {"kb.add_comment": None, "op": True}, conn="CONN",
         reason="why", author="alice", prefix="BLOCKED", tid="t1"),
    case("commented", {"op": True}, conn="CONN", reason="", author="alice",
         prefix="BLOCKED", tid="t1"),
    case("commented", {"op": True}, conn="CONN", reason=None, author="alice",
         prefix="SCHEDULED", tid="t1"),
    case("commented", {"kb.add_comment": None, "op": True}, conn="CONN",
         reason="why", author=None, prefix="UNBLOCK", tid="t1"),
    case("commented", {"kb.add_comment": None, "op": R("ValueError", "nope")},
         conn="CONN", reason="why", author="alice", prefix="BLOCKED", tid="t1"),
    bare("commented", {"kb.add_comment": None}, conn="CONN", reason="why",
         author="alice", prefix="BLOCKED", tid="t1"),           # fail-closed op
    case("commented", {"kb.add_comment": None, "op": [True, False]}, conn="CONN",
         reason="why", author="alice", prefix="BLOCKED", tids=["t1", "t2"]),
    case("commented", {"kb.add_comment": R("RuntimeError", "locked"), "op": True},
         conn="CONN", reason="why", author="alice", prefix="BLOCKED", tid="t1"),
    case("commented", {"kb.add_comment": None, "op": True}, conn="CONN",
         reason=5, author="alice", prefix="BLOCKED", tid="t1"),
]

# ── _caller_identity_for_target ──
CI = dict(platform="telegram", chat_id="c1", thread_id="t1")
ENV_MATCH = {"get_session_env": ["telegram", "c1", "t1", "u9", ""]}
CASES += [
    case("caller_identity_for_target", user_id="u1", user_id_alt=None, **CI),
    case("caller_identity_for_target", user_id=None, user_id_alt="a1", **CI),
    case("caller_identity_for_target", ENV_MATCH, user_id="", user_id_alt=None, **CI),
    case("caller_identity_for_target", {"get_session_env": ["discord"]},
         user_id="", user_id_alt=None, **CI),                   # platform mismatch
    case("caller_identity_for_target", {"get_session_env": ["telegram", "c2"]},
         user_id=None, user_id_alt=None, **CI),                 # chat mismatch
    case("caller_identity_for_target", {"get_session_env": ["telegram", "c1", "t2"]},
         user_id=None, user_id_alt=None, **CI),                 # thread mismatch
    case("caller_identity_for_target",
         {"get_session_env": ["Telegram", "c1", "t1", "u1", "a1"]},
         user_id=None, user_id_alt=None,
         platform="TELEGRAM", chat_id="c1", thread_id="t1"),
    case("caller_identity_for_target",
         {"get_session_env": ["telegram", "c1", "t1", "", 0]},
         user_id=None, user_id_alt=None, **CI),                 # falsy -> None
    case("caller_identity_for_target",
         {"get_session_env": [5, "c1", "t1", "u9", ""]},
         user_id=None, user_id_alt=None, platform="5", chat_id="c1", thread_id="t1"),
    case("caller_identity_for_target", ENV_MATCH, user_id=0, user_id_alt=None, **CI),
    bare("caller_identity_for_target", user_id=None, user_id_alt=None, **CI),
    case("caller_identity_for_target", {"get_session_env": R("SystemExit", "1")},
         user_id=None, user_id_alt=None, **CI),
    case("caller_identity_for_target",
         {"get_session_env": ["telegram", "c1", "", "u9", ""]},
         user_id=None, user_id_alt=None,
         platform="telegram", chat_id="c1", thread_id=None),
]

# ── _run_triage_sweep ──
SW = dict(verb="specify", json_key="specified",
          json_fields=["task_id", "ok", "reason", "new_title"])
OK_OUT = {"task_id": "t1", "ok": True, "reason": None, "new_title": "New Title"}
BAD_OUT = {"task_id": "t1", "ok": False, "reason": "llm unavailable", "new_title": None}
CASES += [
    case("run_triage_sweep", {"run_one": OK_OUT, "human_ok": "Specified t1 -> todo"},
         args={"task_id": "t1", "author": "bob"}, **SW),
    case("run_triage_sweep", {"_profile_author": "alice", "run_one": OK_OUT,
         "human_ok": "Specified t1 -> todo"}, args={"task_id": "t1"}, **SW),
    case("run_triage_sweep", {"run_one": BAD_OUT},
         args={"task_id": "t1", "author": "bob"}, **SW),
    case("run_triage_sweep", {"_err": 2},
         args={"task_id": "t1", "all_triage": True, "author": "bob"}, **SW),
    case("run_triage_sweep", {"_err": 2}, args={"author": "bob"}, **SW),
    case("run_triage_sweep", {"mod.list_triage_ids": [[]],
         "json.dumps": '{"specified": 0, "total": 0}'},
         args={"all_triage": True, "json": True, "author": "bob"}, **SW),
    case("run_triage_sweep", {"mod.list_triage_ids": [[]]},
         args={"all_triage": True, "tenant": "acme", "author": "bob"}, **SW),
    case("run_triage_sweep", {"mod.list_triage_ids": [[]]},
         args={"all_triage": True, "author": "bob"}, **SW),
    case("run_triage_sweep", {"mod.list_triage_ids": [["t1", "t2"]],
         "run_one": [OK_OUT, BAD_OUT], "human_ok": "Specified t1 -> todo"},
         args={"all_triage": True, "author": "bob"}, **SW),
    case("run_triage_sweep", {"mod.list_triage_ids": [["t1", "t2"]],
         "run_one": [BAD_OUT, dict(BAD_OUT, task_id="t2")]},
         args={"all_triage": True, "author": "bob"}, **SW),
    case("run_triage_sweep", {"run_one": OK_OUT, "_obj_dict":
         {"task_id": "t1", "ok": True, "reason": None, "new_title": "New Title"},
         "json.dumps": '{"task_id": "t1", "ok": true}'},
         args={"task_id": "t1", "json": True, "author": "bob"}, **SW),
    case("run_triage_sweep", {"run_one": {"task_id": "t1", "reason": "r"}},
         args={"task_id": "t1", "author": "bob"}, **SW),        # AttributeError ok
    bare("run_triage_sweep", args={"task_id": "t1", "author": "bob"}, **SW),
    case("run_triage_sweep", {"run_one": R("SystemExit", "1")},
         args={"task_id": "t1", "author": "bob"}, **SW),
    case("run_triage_sweep", {"run_one": OK_OUT, "human_ok": R("ValueError", "bad line")},
         args={"task_id": "t1", "author": "bob"}, **SW),
    case("run_triage_sweep", {"run_one": dict(BAD_OUT, ok=0)},
         args={"task_id": "t1", "author": "bob"}, **SW),
    case("run_triage_sweep", {"run_one": dict(OK_OUT, ok="yes"),
         "human_ok": "Specified t1 -> todo"},
         args={"task_id": "t1", "author": "bob"}, **SW),
    case("run_triage_sweep", {"run_one": {"task_id": "t1", "ok": False}},
         args={"task_id": "t1", "author": "bob"}, **SW),        # AttributeError reason
    case("run_triage_sweep", {"run_one": dict(OK_OUT, fanout=True, child_ids=["c1"]),
         "_obj_dict": {"task_id": "t1", "ok": True, "reason": None,
                       "fanout": True, "child_ids": ["c1"], "new_title": "New Title"},
         "json.dumps": '{"task_id": "t1", "ok": true}'},
         args={"task_id": "t1", "json": True, "author": "bob"},
         verb="decompose", json_key="decomposed",
         json_fields=["task_id", "ok", "reason", "fanout", "child_ids", "new_title"]),
    bare("run_triage_sweep", {"mod.list_triage_ids": [[]]},
         args={"all_triage": True, "json": True, "author": "bob"}, **SW),
    case("run_triage_sweep", {"_profile_author": "alice", "run_one": OK_OUT,
         "human_ok": "Specified t1 -> todo"},
         args={"task_id": "t1", "author": ""}, **SW),
    case("run_triage_sweep", {"_err": 2}, args={"task_id": None}, **SW),
]

# ── _retitled_suffix ──
CASES += [
    case("retitled_suffix", outcome={"new_title": "New"}),
    case("retitled_suffix", outcome={"new_title": ""}),
    case("retitled_suffix", outcome={"new_title": None}),
    case("retitled_suffix", outcome={}),                 # AttributeError new_title
    case("retitled_suffix", outcome={"new_title": "it's"}),
    case("retitled_suffix", outcome={"new_title": 'say "hi"'}),
    case("retitled_suffix", outcome={"new_title": 5}),
    case("retitled_suffix", outcome=None),               # AttributeError NoneType
]

# ── _decompose_ok_line ──
CASES += [
    case("decompose_ok_line", outcome={"task_id": "t1", "fanout": True,
         "child_ids": ["c1", "c2"], "new_title": None}),
    case("decompose_ok_line", outcome={"task_id": "t1", "fanout": False,
         "child_ids": ["c1"], "new_title": "N"}),
    case("decompose_ok_line", outcome={"task_id": "t1", "fanout": True,
         "child_ids": [], "new_title": None}),
    case("decompose_ok_line", outcome={"task_id": "t1", "fanout": True,
         "child_ids": None, "new_title": ""}),
    case("decompose_ok_line", outcome={"task_id": "t1", "child_ids": ["c1"],
         "new_title": None}),                            # AttributeError fanout
    case("decompose_ok_line", outcome={"fanout": True, "child_ids": ["c1"],
         "new_title": None}),                            # AttributeError task_id
    case("decompose_ok_line", outcome={"task_id": "t1", "fanout": False,
         "child_ids": []}),                              # AttributeError new_title
    case("decompose_ok_line", outcome={"task_id": "t1", "fanout": True,
         "child_ids": ["c1"], "new_title": None}),
    case("decompose_ok_line", outcome={"task_id": "t1", "fanout": True,
         "child_ids": "ab", "new_title": None}),
]

# ── _cmd_log ──
CASES += [
    case("cmd_log", {"kb.read_worker_log": "line1\nline2\n"},
         args={"task_id": "t1", "tail": 100}),
    case("cmd_log", {"kb.read_worker_log": "no newline"},
         args={"task_id": "t1", "tail": 100}),
    case("cmd_log", {"kb.read_worker_log": ""}, args={"task_id": "t1", "tail": 0}),
    case("cmd_log", {"kb.read_worker_log": None}, args={"task_id": "t1", "tail": 100}),
    case("cmd_log", {"kb.read_worker_log": "x\n"}, args={"task_id": "t1", "tail": None}),
    case("cmd_log", {"kb.read_worker_log": "x\n"}, args={"task_id": "t1"}),
    bare("cmd_log", args={"task_id": "t1", "tail": 100}),
    case("cmd_log", {"kb.read_worker_log": 5}, args={"task_id": "t1", "tail": 100}),
    case("cmd_log", {"kb.read_worker_log": R("SystemExit", "1")},
         args={"task_id": "t1", "tail": 100}),
    case("cmd_log", {"kb.read_worker_log": "x", "sys.stdout.write": R("OSError", "disk full")},
         args={"task_id": "t1", "tail": 100}),
]

# ── _cmd_request_changes ──
RC = dict(args={"task_id": "t1", "reason": ["needs", "work"]})
CASES += [
    case("cmd_request_changes", {"kbc.connect_closing": "CONN",
         "_worker_run_id_for": None, "kb.request_changes": [[True, "alice"]]}, **RC),
    case("cmd_request_changes", {"kbc.connect_closing": "CONN",
         "_worker_run_id_for": None, "kb.request_changes": [[True, None]]}, **RC),
    case("cmd_request_changes", {"kbc.connect_closing": "CONN",
         "_worker_run_id_for": None, "kb.request_changes": [[False, "bad state"]]}, **RC),
    case("cmd_request_changes", {"kbc.connect_closing": "CONN",
         "_worker_run_id_for": None, "kb.request_changes": [[False, None]]}, **RC),
    case("cmd_request_changes", {"kbc.connect_closing": "CONN",
         "_worker_run_id_for": None, "kb.request_changes": [[False, ""]]}, **RC),
    case("cmd_request_changes", {"kbc.connect_closing": "CONN",
         "_worker_run_id_for": 42, "kb.request_changes": [[True, "alice"]]}, **RC),
    case("cmd_request_changes", {"kbc.connect_closing": "CONN",
         "_worker_run_id_for": None, "kb.request_changes": [[True, "alice"]]},
         args={"task_id": "t1", "reason": []}),
    case("cmd_request_changes", {"kbc.connect_closing": "CONN"},
         args={"task_id": "t1", "reason": [5]}),          # TypeError join
    case("cmd_request_changes", {"kbc.connect_closing": "CONN",
         "_worker_run_id_for": None, "kb.request_changes": [[True]]}, **RC),
    case("cmd_request_changes", {"kbc.connect_closing": "CONN",
         "_worker_run_id_for": None, "kb.request_changes": 5}, **RC),
    case("cmd_request_changes", {"kbc.connect_closing": "CONN",
         "_worker_run_id_for": R("ValueError", "worker is scoped to task t9")}, **RC),
    bare("cmd_request_changes", {"kbc.connect_closing": "CONN",
         "_worker_run_id_for": None}, **RC),              # fail-closed request_changes
    case("cmd_request_changes", {"kbc.connect_closing": "CONN",
         "_worker_run_id_for": None, "kb.request_changes": [[True, ""]]}, **RC),
    case("cmd_request_changes", {"kbc.connect_closing": R("OSError", "db locked")}, **RC),
    case("cmd_request_changes", {"kbc.connect_closing": "CONN"},
         args={"reason": ["x"]}),                         # AttributeError task_id
]


def main():
    per = {}
    for c in CASES:
        per[c["func"]] = per.get(c["func"], 0) + 1
    short = {f: n for f, n in per.items() if n < 5}
    if short:
        print("GEN kanban_cli_flow: FATAL funcs with <5 cases: %s" % short,
              file=sys.stderr)
        sys.exit(2)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    print("GEN kanban_cli_flow: %d cases, %d funcs -> %s"
          % (len(CASES), len(per), OUT))
    for fn in sorted(per):
        print("  %-28s %d" % (fn, per[fn]))


if __name__ == "__main__":
    main()
