#!/usr/bin/env python3
"""gen_kanban_db_flow_cases.py — companion generator for the kanban_db FLOW port.

Emits TWO artifacts from the LIVE quarry (hermes_cli/kanban_db.py):

  1. src/hermes_cli/kdbf_flow_sql.h — byte-exact C string literals for every
     static SQL fragment the ported functions build, extracted from the live
     AST (Constant / implicit-concat / f-string static parts). Regenerate
     after an upstream SQL change; the port assembles f-strings from these.
  2. tests/fixtures/kanban_db_flow.jsonl — the scripted-seam case file the
     runner auto-feeds to tests/t_port_kanban_db_flow.c.

Usage: python tests/gen_kanban_db_flow_cases.py [--sql-only]
"""
import ast
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parent
QUARRY = REPO.parent / "hermes-agent"
if not (QUARRY / "hermes_cli" / "kanban_db.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = QUARRY / "hermes_cli" / "kanban_db.py"


def c_escape(s: str) -> str:
    """C string literal body: escape backslash/quote/control bytes; keep UTF-8.

    Hex escapes are split from a following ASCII hex-digit char by closing
    and reopening the literal ("\\xNN" "rest") so the escape never bleeds.
    """
    out = []
    for ch in s:
        b = ch.encode("utf-8")
        if ch == "\\":
            out.append("\\\\")
        elif ch == '"':
            out.append('\\"')
        elif ch == "\n":
            out.append("\\n")
        elif ch == "\r":
            out.append("\\r")
        elif ch == "\t":
            out.append("\\t")
        elif ord(ch) < 0x20:
            for byte in b:
                out.append("\\x%02x" % byte)
            out.append('" "')  # split: next char may be a hex digit
        else:
            out.append(ch)
    text = "".join(out)
    # collapse an empty split at the very end
    if text.endswith('" "'):
        text = text[:-3]
    return text


def lit(s: str) -> str:
    return '"' + c_escape(s) + '"'


def const_str(node):
    """Evaluate a static SQL expression (Constant or +-concat of Constants)."""
    if isinstance(node, ast.Constant) and isinstance(node.value, str):
        return node.value
    if isinstance(node, ast.BinOp) and isinstance(node.op, ast.Add):
        l, r = const_str(node.left), const_str(node.right)
        if l is not None and r is not None:
            return l + r
    return None


def fstring_parts(node):
    """Static parts of a JoinedStr; formatted slots become {<expr source>}."""
    if not isinstance(node, ast.JoinedStr):
        return None
    parts = []
    for v in node.values:
        if isinstance(v, ast.Constant):
            parts.append(v.value)
        elif isinstance(v, ast.FormattedValue):
            parts.append("{" + ast.unparse(v.value) + "}")
        else:
            return None
    return "".join(parts)


def walk_src(node):
    """DFS pre-order = source order (ast.walk is BFS and reorders nesting)."""
    yield node
    for child in ast.iter_child_nodes(node):
        yield from walk_src(child)


def extract_sql():
    src = SRC.read_text(encoding="utf-8")
    tree = ast.parse(src)
    fns = {n.name: n for n in tree.body if isinstance(n, ast.FunctionDef)}
    found = {}

    def walk_exec(fn):
        seq = []
        for node in walk_src(fn):
            if (isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute)
                    and node.func.attr == "execute" and node.args):
                a0 = node.args[0]
                s = const_str(a0)
                if s is not None:
                    seq.append(("static", s))
                else:
                    f = fstring_parts(a0)
                    if f is not None:
                        seq.append(("fstring", f))
                    elif isinstance(a0, ast.Name):
                        seq.append(("var", a0.id))
                    else:
                        seq.append(("expr", ast.unparse(a0)))
        return seq

    for name in ("_task_status", "_current_run_id", "_extend_run_claim",
                 "unsatisfied_parents", "_missing_task_ids", "_latest_event",
                 "_prior_reviewer", "_claim_and_open_run", "_gate_empty_completion",
                 "_extend_live_stale_claim", "_delete_task_relations", "edit_task",
                 "_insert_comment"):
        found[name] = walk_exec(fns[name])
    # _route_block set_sql fragments (Assign, not execute)
    rb = {}
    for node in walk_src(fns["_route_block"]):
        if isinstance(node, ast.Assign):
            for t in node.targets:
                if isinstance(t, ast.Name) and t.id == "set_sql":
                    rb["set_sql"] = const_str(node.value)
    for node in walk_src(fns["_route_block"]):
        if isinstance(node, ast.Return) and isinstance(node.value, ast.Tuple):
            el = node.value.elts[2]
            s = const_str(el)
            if s is not None:
                rb.setdefault("dep_set_sql", s)
    found["_route_block"] = rb
    # _latest_event builds its base SQL in an Assign (execute arg is sql + frag)
    for node in walk_src(fns["_latest_event"]):
        if isinstance(node, ast.Assign):
            for t in node.targets:
                if isinstance(t, ast.Name) and t.id == "sql":
                    found["_latest_base"] = const_str(node.value)
    return found


def emit_header(found):
    H = []
    H.append("/* kdbf_flow_sql.h — byte-exact SQL literals for port_kanban_db_flow.c.")
    H.append(" * GENERATED by tests/gen_kanban_db_flow_cases.py from the LIVE")
    H.append(" * hermes_cli/kanban_db.py AST (upstream 3371c03df6). Do not edit by")
    H.append(" * hand; regenerate after an upstream SQL change. f-string templates")
    H.append(" * keep their {expr} slots; the port substitutes them verbatim. */")
    H.append("#ifndef KDBF_FLOW_SQL_H")
    H.append("#define KDBF_FLOW_SQL_H")
    H.append("")

    def stat(fn, idx):
        kind, val = found[fn][idx]
        assert kind == "static", (fn, idx, kind)
        return val

    defs = [
        ("KDBF_SQL_TASK_STATUS", stat("_task_status", 0)),
        ("KDBF_SQL_CURRENT_RUN_ID", stat("_current_run_id", 0)),
        ("KDBF_SQL_EXTEND_RUN_CLAIM", stat("_extend_run_claim", 0)),
        ("KDBF_SQL_UNSATISFIED_PARENTS", stat("unsatisfied_parents", 0)),
        ("KDBF_SQL_LATEST_BASE", found["_latest_base"]),
        ("KDBF_SQL_PRIOR_RUN", stat("_prior_reviewer", 0)),
        ("KDBF_SQL_CLAIM_TROW", stat("_claim_and_open_run", 1)),
        ("KDBF_SQL_CLAIM_INSERT", stat("_claim_and_open_run", 2)),
        ("KDBF_SQL_CLAIM_SETCURRENT", stat("_claim_and_open_run", 3)),
        ("KDBF_SQL_GATE_SELECT", stat("_gate_empty_completion", 0)),
        ("KDBF_SQL_STALE_CAS", stat("_extend_live_stale_claim", 0)),
        ("KDBF_SQL_DELETE_LINKS", stat("_delete_task_relations", 0)),
        ("KDBF_SQL_INSERT_COMMENT", stat("_insert_comment", 0)),
        ("KDBF_SQL_EDIT_RUN_SELECT", stat("edit_task", 1)),
        ("KDBF_SQL_EDIT_RUN_SUMMARY", stat("edit_task", 2)),
        ("KDBF_SQL_EDIT_RUN_METADATA", stat("edit_task", 3)),
    ]
    for name, val in defs:
        H.append("#define %s %s" % (name, lit(val)))
    H.append("")
    # f-string templates: split into static prefix/suffix around the slot so
    # the C port never re-types SQL whitespace.
    cas = found["_claim_and_open_run"][0]
    assert cas[0] == "fstring"
    pre, post = cas[1].split("{source_status}")
    H.append("#define KDBF_SQL_CLAIM_CAS_PRE %s" % lit(pre))
    H.append("#define KDBF_SQL_CLAIM_CAS_POST %s" % lit(post))
    miss = found["_missing_task_ids"][0]
    assert miss[0] == "fstring"
    pre, post = miss[1].split("{placeholders}")
    H.append("#define KDBF_SQL_MISSING_PRE %s" % lit(pre))
    H.append("#define KDBF_SQL_MISSING_POST %s" % lit(post))
    edt = found["edit_task"][0]
    assert edt[0] == "fstring"
    pre, post = edt[1].split("{', '.join(assignments)}")
    H.append("#define KDBF_SQL_EDIT_UPDATE_PRE %s" % lit(pre))
    H.append("#define KDBF_SQL_EDIT_UPDATE_POST %s" % lit(post))
    dt = found["_delete_task_relations"][1]
    assert dt[0] == "fstring"
    pre, post = dt[1].split("{table}")
    H.append("#define KDBF_SQL_DELETE_TABLE_PRE %s" % lit(pre))
    H.append("#define KDBF_SQL_DELETE_TABLE_POST %s" % lit(post))
    H.append("")
    # _latest_event conditional fragments (sql += / sql + concatenations)
    le = fns_src = SRC.read_text(encoding="utf-8")
    tree = ast.parse(le)
    fn = {n.name: n for n in tree.body if isinstance(n, ast.FunctionDef)}["_latest_event"]
    frags = []
    for node in walk_src(fn):
        if isinstance(node, ast.AugAssign) and isinstance(node.target, ast.Name) and node.target.id == "sql":
            frags.append(const_str(node.value))
        if isinstance(node, ast.Return):
            v = node.value
            if isinstance(v, ast.Call) and isinstance(v.func, ast.Attribute) and v.func.attr == "fetchone":
                inner = v.func.value
                if isinstance(inner, ast.Call) and inner.args:
                    a0 = inner.args[0]
                    if isinstance(a0, ast.BinOp):
                        frags.append(const_str(a0.right))
    assert frags == [" AND run_id = ?", " ORDER BY id DESC LIMIT 1"], frags
    H.append("#define KDBF_SQL_LATEST_RUN %s" % lit(frags[0]))
    H.append("#define KDBF_SQL_LATEST_ORDER %s" % lit(frags[1]))
    H.append("")
    rb = found["_route_block"]
    H.append("#define KDBF_ROUTE_DEP_SET_SQL %s" % lit(rb["dep_set_sql"]))
    H.append("#define KDBF_ROUTE_SET_SQL %s" % lit(rb["set_sql"]))
    H.append("")
    H.append("#endif /* KDBF_FLOW_SQL_H */")
    out = REPO / "src" / "hermes_cli" / "kdbf_flow_sql.h"
    out.write_text("\n".join(H) + "\n", encoding="utf-8", newline="")
    print("wrote", out)


# ── fixture cases ──────────────────────────────────────────────────────────

def cur(rows=None, rowcount=0, lastrowid=None, fetch_raise=None):
    spec = {"rowcount": rowcount, "lastrowid": lastrowid, "rows": rows or []}
    if fetch_raise:
        spec["fetch_raise"] = fetch_raise
    return {"ret": spec}


def locked():
    return {"raise": {"type": "OperationalError", "message": "database is locked"}}


def corrupt():
    return {"raise": {"type": "OperationalError", "message": "database disk image is malformed"}}


CASES = []


def case(func, **inp):
    CASES.append({"func": func, "in": inp})


def build_cases():
    T = "t_aaaa1111"
    # ---- _task_status ----
    case("_task_status", task_id=T, io={"execute": [cur([{"status": "ready"}])]})
    case("_task_status", task_id=T, io={"execute": [cur([])]})
    case("_task_status", task_id=T, io={"execute": [cur([{"status": "archived"}])]})
    case("_task_status", task_id=T, io={"execute": [{"ret": {"rows": [{"status": "done"}]}, "fetch_raise": {"type": "OperationalError", "message": "database disk image is malformed"}}]})
    case("_task_status", task_id=T, io={"execute": [locked()]})
    case("_task_status", task_id="", io={"execute": [cur([{"status": "todo"}])]})
    # ---- _current_run_id ----
    case("_current_run_id", task_id=T, io={"execute": [cur([{"current_run_id": 7}])]})
    case("_current_run_id", task_id=T, io={"execute": [cur([{"current_run_id": None}])]})
    case("_current_run_id", task_id=T, io={"execute": [cur([{"current_run_id": 0}])]})
    case("_current_run_id", task_id=T, io={"execute": [cur([])]})
    case("_current_run_id", task_id=T, io={"execute": [cur([{"current_run_id": "12"}])]})
    case("_current_run_id", task_id=T, io={"execute": [cur([{"current_run_id": "abc"}])]})
    case("_current_run_id", task_id=T, io={"execute": [locked()]})
    # ---- _extend_run_claim ----
    case("_extend_run_claim", task_id=T, expires=900,
         io={"execute": [cur([{"current_run_id": 7}]), cur(rowcount=1)]})
    case("_extend_run_claim", task_id=T, expires=900,
         io={"execute": [cur([{"current_run_id": None}])]})
    case("_extend_run_claim", task_id=T, expires=900,
         io={"execute": [cur([])]})
    case("_extend_run_claim", task_id=T, expires=0,
         io={"execute": [cur([{"current_run_id": 3}]), cur(rowcount=1)]})
    case("_extend_run_claim", task_id=T, expires=900,
         io={"execute": [cur([{"current_run_id": 3}]), locked()]})
    case("_extend_run_claim", task_id=T, expires=900,
         io={"execute": [locked()]})
    # ---- unsatisfied_parents ----
    case("unsatisfied_parents", task_id=T, io={"execute": [cur([])]})
    case("unsatisfied_parents", task_id=T,
         io={"execute": [cur([{"id": "t_b", "status": "todo"}, {"id": "t_c", "status": "running"}])]})
    case("unsatisfied_parents", task_id=T,
         io={"execute": [cur([{"id": "t_a", "status": "blocked"}])]})
    case("unsatisfied_parents", task_id=T, io={"execute": [locked()]})
    case("unsatisfied_parents", task_id=T,
         io={"execute": [{"ret": {"rows": [{"id": "t_a", "status": "todo"}]}, "fetch_raise": {"type": "OperationalError", "message": "database disk image is malformed"}}]})
    case("unsatisfied_parents", task_id="", io={"execute": [cur([{"id": "t_z", "status": "review"}])]})
    # ---- _missing_task_ids ----
    case("_missing_task_ids", ids=[], io={})
    case("_missing_task_ids", ids=["t_a", "t_b"],
         io={"execute": [cur([{"id": "t_a"}, {"id": "t_b"}])]})
    case("_missing_task_ids", ids=["t_a", "t_b", "t_c"],
         io={"execute": [cur([{"id": "t_b"}])]})
    case("_missing_task_ids", ids=["t_a", "t_a", "t_b"],
         io={"execute": [cur([{"id": "t_a"}])]})
    case("_missing_task_ids", ids=["t_x"], io={"execute": [cur([])]})
    case("_missing_task_ids", ids=["t_a"], io={"execute": [locked()]})
    # ---- _prior_reviewer ----
    case("_prior_reviewer", task_id=T, io={"execute": [cur([])]})
    case("_prior_reviewer", task_id=T,
         io={"execute": [cur([{"id": 9}]), cur([{"payload": '{"reviewer": "closer"}'}])]})
    case("_prior_reviewer", task_id=T,
         io={"execute": [cur([{"id": 9}]), cur([{"payload": '{"reviewer": "  "}'}])]})
    case("_prior_reviewer", task_id=T,
         io={"execute": [cur([{"id": 9}]), cur([{"payload": '{"reviewer": 42}'}])]})
    case("_prior_reviewer", task_id=T,
         io={"execute": [cur([{"id": 9}]), cur([])]})
    case("_prior_reviewer", task_id=T,
         io={"execute": [cur([{"id": 9}]), cur([{"payload": "not-json"}])]})
    case("_prior_reviewer", task_id=T,
         io={"execute": [cur([{"id": 9}]), cur([{"payload": '{"reviewer": "dev"}'}]), locked()]})
    # ---- _claim_is_live ----
    case("_claim_is_live", trow={"status": "ready", "claim_lock": "h:1", "worker_pid": 5, "worker_started_at": 10}, io={})
    case("_claim_is_live", trow={"status": "running", "claim_lock": None, "worker_pid": 5, "worker_started_at": 10}, io={})
    case("_claim_is_live", trow={"status": "running", "claim_lock": "h:1", "worker_pid": 0, "worker_started_at": 10}, io={})
    case("_claim_is_live", trow={"status": "running", "claim_lock": "h:1", "worker_pid": None, "worker_started_at": 10}, io={})
    case("_claim_is_live", trow={"status": "running", "claim_lock": "h:1", "worker_pid": 5, "worker_started_at": 10},
         io={"_worker_alive": {"ret": True}})
    case("_claim_is_live", trow={"status": "running", "claim_lock": "h:1", "worker_pid": 5, "worker_started_at": None},
         io={"_worker_alive": {"ret": False}})
    # ---- _validate_model_override ----
    case("_validate_model_override", model=None, provider=None)
    case("_validate_model_override", model="  gpt-x  ", provider="  openrouter ")
    case("_validate_model_override", model="", provider="")
    case("_validate_model_override", model=None, provider="openrouter")
    case("_validate_model_override", model="   ", provider="openrouter")
    case("_validate_model_override", model="m", provider=None)
    case("_validate_model_override", model="m", provider="  ")
    # ---- _normalize_task_skills ----
    case("_normalize_task_skills", skills=None, io={"get_toolset_names": {"ret": ["web", "browser"]}})
    case("_normalize_task_skills", skills=[], io={"get_toolset_names": {"ret": ["web"]}})
    case("_normalize_task_skills", skills=["  github  ", "github", "", None, "  ", "arxiv"],
         io={"get_toolset_names": {"ret": ["web", "browser"]}})
    case("_normalize_task_skills", skills=["a,b"], io={"get_toolset_names": {"ret": ["web"]}})
    case("_normalize_task_skills", skills=["web"], io={"get_toolset_names": {"ret": ["web", "browser"]}})
    case("_normalize_task_skills", skills=["web", "browser"], io={"get_toolset_names": {"ret": ["web", "browser"]}})
    case("_normalize_task_skills", skills=["WEB", "ok"], io={"get_toolset_names": {"ret": ["web"]}})
    case("_normalize_task_skills", skills=["a", "a", "b"], io={"get_toolset_names": {"ret": []}})
    # ---- _completed_event_payload ----
    case("_completed_event_payload", result=None, event_summary=None, verified_cards=[], metadata=None)
    case("_completed_event_payload", result="done work", event_summary="line1\nline2",
         verified_cards=["t_c1"], metadata={"artifacts": [" /tmp/a ", "", 42, "/tmp/b"]})
    case("_completed_event_payload", result="", event_summary="   ", verified_cards=[], metadata={"artifacts": "nope"})
    case("_completed_event_payload", result="r", event_summary="x" * 450, verified_cards=[], metadata=[])
    case("_completed_event_payload", result="r", event_summary=None, verified_cards=[], metadata={"artifacts": []})
    case("_completed_event_payload", result="r", event_summary="s", verified_cards=["t_a", "t_b"],
         metadata={"artifacts": ["/p/q"]})
    # ---- _route_block ----
    case("_route_block", kind="dependency", reason="waiting", source_status="running", prev_kind=None, prev_recurrences=0)
    case("_route_block", kind="needs_input", reason="need keys", source_status="running", prev_kind=None, prev_recurrences=0)
    case("_route_block", kind="needs_input", reason="again", source_status="ready", prev_kind="needs_input", prev_recurrences=1)
    case("_route_block", kind="needs_input", reason="x", source_status="running", prev_kind="needs_input", prev_recurrences=0)
    case("_route_block", kind=None, reason=None, source_status="running", prev_kind=None, prev_recurrences=1)
    case("_route_block", kind="capability", reason="no gpu", source_status="running", prev_kind="transient", prev_recurrences=5)
    case("_route_block", kind="transient", reason="rate", source_status="ready", prev_kind="transient", prev_recurrences=2)
    # ---- _insert_comment ----
    case("_insert_comment", task_id=T, author="dev", body="a comment", created_at=1700000000,
         io={"execute": [cur(rowcount=1, lastrowid=5)]})
    case("_insert_comment", task_id=T, author="dev", body="x", created_at=0,
         io={"execute": [cur(rowcount=1, lastrowid=1)]})
    case("_insert_comment", task_id=T, author="", body="", created_at=1700000000,
         io={"execute": [cur(rowcount=1, lastrowid=2)]})
    case("_insert_comment", task_id=T, author="dev", body="line1\nline2", created_at=1700000001,
         io={"execute": [cur(rowcount=1, lastrowid=3)]})
    case("_insert_comment", task_id=T, author="dev", body="y", created_at=1700000000,
         io={"execute": [locked()]})
    case("_insert_comment", task_id=T, author="dev", body="z", created_at=1700000000,
         io={"execute": [corrupt()]})
    # ---- _delete_task_relations ----
    case("_delete_task_relations", task_id=T, io={"execute": [cur(rowcount=1)] * 5})
    case("_delete_task_relations", task_id=T, io={"execute": [cur(rowcount=0), cur(rowcount=2), locked()]})
    case("_delete_task_relations", task_id=T, io={"execute": [locked()]})
    case("_delete_task_relations", task_id="", io={"execute": [cur(rowcount=0)] * 5})
    case("_delete_task_relations", task_id=T, io={"execute": [cur(rowcount=1), cur(rowcount=1), cur(rowcount=1), cur(rowcount=1), corrupt()]})
    case("_delete_task_relations", task_id=T, io={"execute": [cur(rowcount=3)] * 5})
    # ---- _record_reclaim ----
    case("_record_reclaim", task_id=T, termination={"signal": "SIGTERM", "exited": True},
         error="manual_reclaim: stale", payload={"manual": True, "reason": "stale"},
         io={"_end_run": {"ret": 11}, "_append_event": {"ret": None}})
    case("_record_reclaim", task_id=T, termination={}, error="manual_reclaim lock=h:1",
         payload={"manual": True, "reason": None, "prev_lock": "h:1"},
         io={"_end_run": {"ret": None}, "_append_event": {"ret": None}})
    case("_record_reclaim", task_id=T, termination={"reason": "crash"}, error="e",
         payload={"reason": "operator", "manual": True},
         io={"_end_run": {"ret": 4}, "_append_event": {"ret": None}})
    case("_record_reclaim", task_id=T, termination={"a": 1}, error="e", payload={},
         io={"_end_run": {"ret": 4}, "_append_event": locked()})
    case("_record_reclaim", task_id=T, termination={"a": 1}, error="e", payload={},
         io={"_end_run": locked()})
    case("_record_reclaim", task_id=T, termination={"manual": False}, error="e",
         payload={"manual": True}, io={"_end_run": {"ret": 2}, "_append_event": {"ret": None}})
    # ---- _set_task_override ----
    OV_SQL = "UPDATE tasks SET model_override = ?, provider_override = ? WHERE id = ?"
    case("_set_task_override", task_id=T, sql=OV_SQL, params=["m", "p"],
         event_kind="model_override_set", payload={"model": "m", "provider": "p"},
         changed_fields=["model_override", "provider_override"], archived_msg="cannot set model override",
         io={"execute": [cur([{"status": "ready"}]), cur(rowcount=1)],
             "_append_event": {"ret": None}, "notify_task_updated": {"ret": None}})
    case("_set_task_override", task_id=T, sql=OV_SQL, params=[None, None],
         event_kind="model_override_set", payload={"model": None, "provider": None},
         changed_fields=["model_override", "provider_override"], archived_msg="cannot set model override",
         io={"execute": [cur([])], "_append_event": {"ret": None}, "notify_task_updated": {"ret": None}})
    case("_set_task_override", task_id=T, sql=OV_SQL, params=["m", None],
         event_kind="model_override_set", payload={"model": "m", "provider": None},
         changed_fields=["model_override", "provider_override"], archived_msg="cannot set model override",
         io={"execute": [cur([{"status": "archived"}])]})
    case("_set_task_override", task_id=T, sql="UPDATE tasks SET reasoning_effort = ? WHERE id = ?",
         params=["high"], event_kind="reasoning_effort_set", payload={"reasoning_effort": "high"},
         changed_fields=["reasoning_effort"], archived_msg="cannot set reasoning effort",
         io={"execute": [cur([{"status": "running"}]), cur(rowcount=1)],
             "_append_event": {"ret": None}, "notify_task_updated": {"ret": None}})
    case("_set_task_override", task_id=T, sql=OV_SQL, params=["m", "p"],
         event_kind="model_override_set", payload={"model": "m", "provider": "p"},
         changed_fields=["model_override", "provider_override"], archived_msg="cannot set model override",
         io={"execute": [cur([{"status": "ready"}]), locked()]})
    case("_set_task_override", task_id=T, sql=OV_SQL, params=["m", "p"],
         event_kind="model_override_set", payload={"model": "m", "provider": "p"},
         changed_fields=["model_override", "provider_override"], archived_msg="cannot set model override",
         io={"execute": [cur([{"status": "ready"}]), cur(rowcount=1)],
             "_append_event": locked()})
    case("_set_task_override", task_id=T, sql=OV_SQL, params=["m", "p"],
         event_kind="model_override_set", payload={"model": "m", "provider": "p"},
         changed_fields=["model_override", "provider_override"], archived_msg="cannot set model override",
         io={"write_txn.enter": locked()})
    # ---- _gate_empty_completion ----
    case("_gate_empty_completion", task_id=T, result="real work", summary=None,
         io={"execute": [cur([{"status": "running", "result": None}])]})
    case("_gate_empty_completion", task_id=T, result=None, summary=None,
         io={"execute": [cur([])]})
    case("_gate_empty_completion", task_id=T, result=None, summary=None,
         io={"execute": [cur([{"status": "review", "result": None}])]})
    case("_gate_empty_completion", task_id=T, result=None, summary="  ",
         io={"execute": [cur([{"status": "running", "result": "stored evidence"}])]})
    case("_gate_empty_completion", task_id=T, result="  ", summary=None,
         io={"execute": [cur([{"status": "done", "result": ""}])],
             "_append_event": {"ret": None}})
    case("_gate_empty_completion", task_id=T, result=None, summary="s",
         io={"execute": [cur([{"status": "running", "result": None}])]})
    case("_gate_empty_completion", task_id=T, result=None, summary=None,
         io={"execute": [cur([{"status": "running", "result": None}])],
             "_append_event": locked()})
    case("_gate_empty_completion", task_id=T, result=None, summary=None,
         io={"execute": [locked()]})
    # ---- _extend_live_stale_claim ----
    ROW = {"id": T, "claim_lock": "h:1", "worker_pid": 42, "claim_expires": 500, "last_heartbeat_at": 480}
    case("_extend_live_stale_claim", row=ROW, now=1000,
         io={"_resolve_claim_ttl_seconds": {"ret": 900},
             "execute": [cur(rowcount=1), cur([{"current_run_id": 8}]), cur(rowcount=1)],
             "_to_epoch": [{"ret": 500}, {"ret": 480}],
             "_append_event": {"ret": None}})
    case("_extend_live_stale_claim", row=ROW, now=1000,
         io={"_resolve_claim_ttl_seconds": {"ret": 900}, "execute": [cur(rowcount=0)]})
    case("_extend_live_stale_claim", row=ROW, now=1000,
         io={"_resolve_claim_ttl_seconds": {"ret": 900},
             "execute": [cur(rowcount=1), cur([{"current_run_id": None}])],
             "_to_epoch": [{"ret": 500}, {"ret": None}],
             "_append_event": {"ret": None}})
    case("_extend_live_stale_claim", row=ROW, now=1000,
         io={"_resolve_claim_ttl_seconds": {"ret": 900}, "execute": [locked()]})
    case("_extend_live_stale_claim", row=ROW, now=1000,
         io={"_resolve_claim_ttl_seconds": locked()})
    case("_extend_live_stale_claim", row=dict(ROW, claim_expires="2026-05-10T15:00:00Z", last_heartbeat_at=None), now=1000,
         io={"_resolve_claim_ttl_seconds": {"ret": 60},
             "execute": [cur(rowcount=1), cur([])],
             "_to_epoch": [{"ret": 1778598000}, {"ret": None}],
             "_append_event": {"ret": None}})
    # ---- _claim_and_open_run ----
    case("_claim_and_open_run", task_id=T, source_status="ready", lock="h:1", expires=1900, now=1000,
         event_extra=None, expect_assignee=None,
         io={"execute": [cur(rowcount=1),
                         cur([{"assignee": "dev", "max_runtime_seconds": 600, "current_step_key": None}]),
                         cur(rowcount=1, lastrowid=21), cur(rowcount=1)],
             "_append_event": {"ret": None}})
    case("_claim_and_open_run", task_id=T, source_status="ready", lock="h:1", expires=1900, now=1000,
         event_extra=None, expect_assignee=None,
         io={"execute": [cur(rowcount=0)]})
    case("_claim_and_open_run", task_id=T, source_status="review", lock="h:2", expires=2000, now=1100,
         event_extra={"review": True}, expect_assignee="closer",
         io={"execute": [cur(rowcount=1), cur([]), cur(rowcount=1, lastrowid=22), cur(rowcount=1)],
             "_append_event": {"ret": None}})
    case("_claim_and_open_run", task_id=T, source_status="ready", lock="h:1", expires=1900, now=1000,
         event_extra={"run_id": 99, "lock": "override"}, expect_assignee="dev",
         io={"execute": [cur(rowcount=1),
                         cur([{"assignee": "dev", "max_runtime_seconds": None, "current_step_key": "impl"}]),
                         cur(rowcount=1, lastrowid=23), cur(rowcount=1)],
             "_append_event": {"ret": None}})
    case("_claim_and_open_run", task_id=T, source_status="ready", lock="h:1", expires=1900, now=1000,
         event_extra=None, expect_assignee=None,
         io={"execute": [cur(rowcount=1), cur([{"assignee": None, "max_runtime_seconds": None, "current_step_key": None}]),
                         cur(rowcount=1, lastrowid=None), cur(rowcount=1)],
             "_append_event": {"ret": None}})
    case("_claim_and_open_run", task_id=T, source_status="ready", lock="h:1", expires=1900, now=1000,
         event_extra=None, expect_assignee=None,
         io={"execute": [locked()]})
    case("_claim_and_open_run", task_id=T, source_status="ready", lock="h:1", expires=1900, now=1000,
         event_extra=None, expect_assignee=None,
         io={"execute": [cur(rowcount=1), cur([{"assignee": "dev", "max_runtime_seconds": None, "current_step_key": None}]),
                         cur(rowcount=1, lastrowid=24), cur(rowcount=1)],
             "_append_event": locked()})
    # ---- edit_task ----
    case("edit_task", task_id=T, title="New title", body=None, priority=None,
         result=None, summary=None, metadata=None, board=None,
         io={"execute": [cur([{"status": "todo"}]), cur(rowcount=1)],
             "_append_event": {"ret": None}, "notify_task_updated": {"ret": None}})
    case("edit_task", task_id=T, title=None, body=None, priority=None,
         result=None, summary=None, metadata=None, board=None,
         io={"execute": [cur([{"status": "todo"}])]})
    case("edit_task", task_id=T, title=None, body=None, priority=None,
         result=None, summary=None, metadata=None, board=None,
         io={"execute": [cur([])]})
    case("edit_task", task_id=T, title=None, body=None, priority=5,
         result=None, summary=None, metadata=None, board=None,
         io={"execute": [cur([{"status": "ready"}]), cur(rowcount=1)],
             "_append_event": {"ret": None}, "notify_task_updated": {"ret": None}})
    case("edit_task", task_id=T, title="T", body="B", priority=2,
         result=None, summary=None, metadata=None, board="ops",
         io={"execute": [cur([{"status": "todo"}]), cur(rowcount=1)],
             "_append_event": [{"ret": None}, {"ret": None}], "notify_task_updated": {"ret": None}})
    case("edit_task", task_id=T, title=None, body=None, priority=None,
         result="backfilled", summary=None, metadata=None, board=None,
         io={"execute": [cur([{"status": "running"}])]})
    case("edit_task", task_id=T, title=None, body=None, priority=None,
         result="backfilled", summary="handoff", metadata={"k": "v"}, board=None,
         io={"execute": [cur([{"status": "done"}]), cur(rowcount=1), cur([{"id": 31}]), cur(rowcount=1), cur(rowcount=1)],
             "_append_event": {"ret": None}, "notify_task_updated": {"ret": None}})
    case("edit_task", task_id=T, title=None, body=None, priority=None,
         result="backfilled", summary=None, metadata=None, board=None,
         io={"execute": [cur([{"status": "done"}]), cur(rowcount=1), cur([])],
             "_synthesize_ended_run": {"ret": 32},
             "_append_event": {"ret": None}, "notify_task_updated": {"ret": None}})
    case("edit_task", task_id=T, title=None, body=None, priority=None,
         result="", summary=None, metadata=None, board=None,
         io={"execute": [cur([{"status": "done"}]), cur(rowcount=1), cur([{"id": 33}]), cur(rowcount=1)],
             "_append_event": {"ret": None}, "notify_task_updated": {"ret": None}})
    case("edit_task", task_id=T, title="X", body=None, priority=None,
         result=None, summary=None, metadata=None, board=None,
         io={"execute": [cur([{"status": "todo"}]), locked()]})
    case("edit_task", task_id=T, title=None, body=None, priority=None,
         result="r", summary=None, metadata={"artifacts": ["/a"]}, board=None,
         io={"execute": [cur([{"status": "done"}]), cur(rowcount=1), cur([{"id": 34}]), cur(rowcount=1), cur(rowcount=1)],
             "_append_event": locked()})
    case("edit_task", task_id=T, title=None, body="only body", priority=None,
         result=None, summary=None, metadata=None, board=None,
         io={"write_txn.enter": {"raise": {"type": "RuntimeError", "message": "write_txn: already inside a transaction."}}})


def emit_fixtures():
    build_cases()
    out = HERE / "fixtures" / "kanban_db_flow.jsonl"
    out.parent.mkdir(parents=True, exist_ok=True)
    with out.open("w", encoding="utf-8", newline="") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print("wrote", out, "—", len(CASES), "cases")
    for k in sorted(counts):
        print("  %-28s %d" % (k, counts[k]))
    thin = [k for k, v in counts.items() if v < 6]
    if thin:
        print("FATAL: funcs with <6 cases:", thin)
        sys.exit(1)


if __name__ == "__main__":
    found = extract_sql()
    emit_header(found)
    if "--sql-only" not in sys.argv:
        emit_fixtures()
