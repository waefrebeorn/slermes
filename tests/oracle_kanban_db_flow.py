#!/usr/bin/env python3
"""Oracle for the hermes_cli/kanban_db.py FLOW-core port (src/hermes_cli/port_kanban_db_flow.c).

Drives the LIVE kanban_db.py defs with SCRIPTED SQLite seams: conn.execute is a
recording stub returning scripted cursor specs (rowcount / lastrowid / rows /
fetch_raise), write_txn is a context-manager double recording enter/exit, and
the already-ported I/O siblings (_append_event, _end_run, _synthesize_ended_run,
notify_task_updated, _to_epoch, _resolve_claim_ttl_seconds, _worker_alive,
get_toolset_names) are recorders fed the SAME scripts the C harness feeds its
seams. Compares the DECISION SEQUENCE (ordered structured seam events in
"trace", SQL strings verbatim) plus the return ("ret") and state ("state").

The live defs are AST-extracted (Assign AND AnnAssign constants, dataclass
record classes, real exception classes seeded) so the heavy quarry import
chain never runs. Pure internal helpers (_first_line, _substantive_text,
_json_or, _json_dict, _row_get, _json_or_null, _cleaned_artifact_paths,
_latest_event, _task_status, _current_run_id, _extend_run_claim) stay REAL on
both sides. Fail-closed: 0 cases or a missing extraction => nonzero exit.
Summary line: 'ORACLE kanban_db_flow: N cases, M mismatches'.

Deterministic subset (documented): sqlite3.Row is modeled as a dict-like
scripted object (KeyError on a missing column, matching the C row accessor);
rows carry JSON scalars only; _to_epoch is a seam so naive-ISO local-time
values never reach the comparison; non-ASCII casefold specials are out of
fixture scope; JSON numbers lose int/float identity after parsing (integral
doubles serialize as ints on both sides).
"""
import ast
import copy
import json
import os
import sys
import types
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_HERE = Path(__file__).resolve()
_REPO = _HERE.parents[1]
QUARRY = _REPO.parent / "hermes-agent"
if not (QUARRY / "hermes_cli" / "kanban_db.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "hermes_cli" / "kanban_db.py")

# Runner hands the oracle a scrubbed environment; the quarry import chain
# (never executed here, but keep the sandbox honest) wants a home.
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "kdbf_home"))

# C harness func name == LIVE python name (identity map, kept explicit).
PY = {
    "edit_task": "edit_task",
    "_claim_and_open_run": "_claim_and_open_run",
    "_gate_empty_completion": "_gate_empty_completion",
    "_extend_live_stale_claim": "_extend_live_stale_claim",
    "_set_task_override": "_set_task_override",
    "_route_block": "_route_block",
    "_insert_comment": "_insert_comment",
    "_record_reclaim": "_record_reclaim",
    "_delete_task_relations": "_delete_task_relations",
    "_task_status": "_task_status",
    "_current_run_id": "_current_run_id",
    "_extend_run_claim": "_extend_run_claim",
    "unsatisfied_parents": "unsatisfied_parents",
    "_missing_task_ids": "_missing_task_ids",
    "_prior_reviewer": "_prior_reviewer",
    "_validate_model_override": "_validate_model_override",
    "_normalize_task_skills": "_normalize_task_skills",
    "_completed_event_payload": "_completed_event_payload",
    "_claim_is_live": "_claim_is_live",
}

# Real internal helpers the flows call (extracted, NOT stubbed).
REAL_HELPERS = (
    "_first_line", "_substantive_text", "_json_or", "_json_dict", "_row_get",
    "_json_or_null", "_cleaned_artifact_paths", "_latest_event",
)


def extract():
    import contextlib
    import contextvars
    import logging
    import re
    import secrets
    import sqlite3
    import subprocess
    import time
    from dataclasses import dataclass, field
    from typing import Any, Iterable, Optional

    ns = {
        "__name__": "hermes_cli.kanban_db",
        "os": os, "json": json, "sys": sys, "re": re, "time": time,
        "logging": logging, "contextlib": contextlib, "sqlite3": sqlite3,
        "secrets": secrets, "subprocess": subprocess,
        "ContextVar": contextvars.ContextVar, "Token": contextvars.Token,
        "dataclass": dataclass, "field": field,
        "Any": Any, "Iterable": Iterable, "Optional": Optional,
        "Path": Path,
        # tail-imported names referenced by annotations of non-target defs
        "DispatchResult": type("DispatchResult", (), {}),
        "DEFAULT_FAILURE_LIMIT": 3,
        "DEFAULT_RATE_LIMIT_COOLDOWN_SECONDS": 300,
        "VALID_STATUSES": {"todo", "ready", "running", "blocked", "review",
                           "done", "archived", "triage"},
        # module-constant seed: KNOWN_TOOLSET_NAMES calls this at Assign-exec
        "get_toolset_names": lambda: [],
        "unlink_files_older_than": lambda *a, **k: 0,
        "_worker_alive": lambda *a, **k: False,
        "write_txn": None,
    }
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    # 1. module constants (Assign AND AnnAssign; plain-Name targets only)
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgt = node.targets[0] if isinstance(node, ast.Assign) else node.target
            if not isinstance(tgt, ast.Name):
                continue
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            except Exception:
                pass
    # 2. record/exception classes BEFORE defs (annotations reference Task)
    for node in tree.body:
        if isinstance(node, ast.ClassDef):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            except Exception:
                pass
    # 3. all function defs
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            except Exception:
                pass
    required = list(PY.values()) + list(REAL_HELPERS) + [
        "EmptyCompletionError", "HallucinatedCardsError", "LiveClaimError",
        "BLOCK_RECURRENCE_LIMIT", "_UNSET", "Task",
    ]
    missing = [n for n in required if n not in ns]
    if missing:
        print("FATAL kanban_db_flow: extraction missing names:", sorted(missing))
        sys.exit(2)
    return ns


NS = extract()


def make_exc(spec):
    import sqlite3
    t = spec.get("type", "Exception")
    msg = spec.get("message", "")
    cls = {
        "OperationalError": sqlite3.OperationalError,
        "IntegrityError": sqlite3.IntegrityError,
        "DatabaseError": sqlite3.DatabaseError,
        "ValueError": ValueError,
        "TypeError": TypeError,
        "RuntimeError": RuntimeError,
        "KeyError": KeyError,
        "IndexError": IndexError,
        "EmptyCompletionError": NS["EmptyCompletionError"],
        "HallucinatedCardsError": NS["HallucinatedCardsError"],
        "LiveClaimError": NS["LiveClaimError"],
    }.get(t)
    if cls is KeyError:
        return KeyError(msg)
    if cls is not None:
        return cls(msg)
    return Exception(msg)


class CaseCtx:
    def __init__(self, inp):
        self.inp = inp
        self.trace = []
        self.io = inp.get("io", {}) or {}
        self.cursors = {}
        self.state = copy.deepcopy(inp.get("state", {}) or {})

    def script_next(self, name):
        entry = self.io.get(name)
        if entry is None:
            return None
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                return None
            self.cursors[name] = idx + 1
            return entry[idx]
        return entry

    def rec(self, name, args):
        ev = {"call": name}
        ev.update(args)
        self.trace.append(ev)
        result = self.script_next(name)
        if isinstance(result, dict):
            eff = result.get("state")
            if isinstance(eff, dict):
                self.state.update(eff)
            raise_spec = result.get("raise")
            if isinstance(raise_spec, dict):
                raise make_exc(raise_spec)
            return result.get("ret")
        return result


class _Cursor:
    def __init__(self, ctx, spec):
        self.ctx = ctx
        spec = spec if isinstance(spec, dict) else {}
        self.rowcount = spec.get("rowcount", 0)
        self.lastrowid = spec.get("lastrowid")
        self.rows = list(spec.get("rows") or [])
        self.idx = 0
        self.fetch_raise = spec.get("fetch_raise")

    def _pre(self, name):
        self.ctx.trace.append({"call": name})
        if isinstance(self.fetch_raise, dict):
            raise make_exc(self.fetch_raise)

    def fetchone(self):
        self._pre("fetchone")
        if self.idx >= len(self.rows):
            return None
        r = self.rows[self.idx]
        self.idx += 1
        return r

    def fetchall(self):
        self._pre("fetchall")
        rs = self.rows[self.idx:]
        self.idx = len(self.rows)
        return rs


class _Conn:
    def __init__(self, ctx):
        self.ctx = ctx

    def execute(self, sql, params=()):
        ctx = self.ctx
        ctx.trace.append({"call": "execute", "sql": sql, "params": list(params)})
        result = ctx.script_next("execute")
        spec = None
        if isinstance(result, dict):
            eff = result.get("state")
            if isinstance(eff, dict):
                ctx.state.update(eff)
            raise_spec = result.get("raise")
            if isinstance(raise_spec, dict):
                raise make_exc(raise_spec)
            spec = result.get("ret")
        else:
            spec = result
        return _Cursor(ctx, spec)


class _WriteTxnCM:
    def __init__(self, ctx, conn, allow_nested):
        self.ctx = ctx
        self.conn = conn
        self.allow_nested = allow_nested

    def __enter__(self):
        self.ctx.rec("write_txn.enter", {"allow_nested": self.allow_nested})
        return self.conn

    def __exit__(self, et, ev, tb):
        exc = {"type": et.__name__, "message": str(ev)} if et is not None else None
        self.ctx.rec("write_txn.exit", {"exc": exc})
        return False


CURRENT = {"ctx": None}


def seed_case(ctx, inp):
    conn = _Conn(ctx)
    NS["write_txn"] = lambda c, allow_nested=False: _WriteTxnCM(ctx, c, allow_nested)
    unset = NS["_UNSET"]

    def append_event(c, task_id, kind, payload=None, *, run_id=None):
        return ctx.rec("_append_event", {
            "task_id": task_id, "kind": kind, "payload": payload, "run_id": run_id})

    def end_run(c, task_id, *, outcome, summary=None, error=None, metadata=None, status=None):
        return ctx.rec("_end_run", {
            "task_id": task_id, "outcome": outcome, "status": status,
            "summary": summary, "error": error, "metadata": metadata})

    def synthesize(c, task_id, *, outcome, summary=None, error=None, metadata=None, profile=unset):
        return ctx.rec("_synthesize_ended_run", {
            "task_id": task_id, "outcome": outcome, "summary": summary,
            "error": error, "metadata": metadata,
            "profile": None if profile is unset else profile})

    def notify(c, task_id, changed_fields, *, board=None):
        return ctx.rec("notify_task_updated", {
            "task_id": task_id, "changed_fields": list(changed_fields), "board": board})

    def resolve_ttl(ttl_seconds=None):
        return ctx.rec("_resolve_claim_ttl_seconds", {"ttl_seconds": ttl_seconds})

    def worker_alive(pid, started_at):
        return ctx.rec("_worker_alive", {"pid": pid, "started_at": started_at})

    def to_epoch(val):
        return ctx.rec("_to_epoch", {"val": val})

    NS["_append_event"] = append_event
    NS["_end_run"] = end_run
    NS["_synthesize_ended_run"] = synthesize
    NS["notify_task_updated"] = notify
    NS["_resolve_claim_ttl_seconds"] = resolve_ttl
    NS["_worker_alive"] = worker_alive
    NS["_to_epoch"] = to_epoch
    NS["get_toolset_names"] = lambda: ctx.rec("get_toolset_names", {})
    return conn


def norm_ret(r):
    if isinstance(r, BaseException):
        return {"__raise__": type(r).__name__, "message": str(r)}
    if isinstance(r, tuple):
        return [norm_ret(x) for x in r]
    if isinstance(r, list):
        return [norm_ret(x) for x in r]
    if isinstance(r, dict):
        return {k: norm_ret(v) for k, v in r.items()}
    return r


def run_case(func, inp):
    ctx = CaseCtx(inp)
    conn = seed_case(ctx, inp)
    out = {"ret": None, "trace": ctx.trace, "state": ctx.state}
    py = PY[func]
    tid = inp.get("task_id", "")
    try:
        if func == "edit_task":
            r = NS[py](conn, tid, title=inp.get("title"), body=inp.get("body"),
                       priority=inp.get("priority"), result=inp.get("result"),
                       summary=inp.get("summary"), metadata=inp.get("metadata"),
                       board=inp.get("board"))
        elif func == "_claim_and_open_run":
            r = NS[py](conn, tid, inp.get("source_status", ""), inp.get("lock", ""),
                       inp.get("expires", 0), inp.get("now", 0),
                       event_extra=inp.get("event_extra"),
                       expect_assignee=inp.get("expect_assignee"))
        elif func == "_gate_empty_completion":
            r = NS[py](conn, tid, result=inp.get("result"), summary=inp.get("summary"))
        elif func == "_extend_live_stale_claim":
            r = NS[py](conn, inp.get("row") or {}, inp.get("now", 0))
        elif func == "_set_task_override":
            r = NS[py](conn, tid, inp.get("sql", ""), tuple(inp.get("params") or []),
                       inp.get("event_kind", ""), inp.get("payload"),
                       tuple(inp.get("changed_fields") or []),
                       archived_msg=inp.get("archived_msg", ""))
        elif func == "_route_block":
            r = NS[py](inp.get("kind"), inp.get("reason"), inp.get("source_status", ""),
                       prev_kind=inp.get("prev_kind"),
                       prev_recurrences=inp.get("prev_recurrences", 0))
        elif func == "_record_reclaim":
            r = NS[py](conn, tid, inp.get("termination") or {},
                       error=inp.get("error", ""), payload=inp.get("payload") or {})
        elif func == "_insert_comment":
            r = NS[py](conn, tid, inp.get("author", ""), inp.get("body", ""),
                       inp.get("created_at", 0))
        elif func == "_delete_task_relations":
            r = NS[py](conn, tid)
        elif func == "_task_status":
            r = NS[py](conn, tid)
        elif func == "_current_run_id":
            r = NS[py](conn, tid)
        elif func == "_extend_run_claim":
            r = NS[py](conn, tid, inp.get("expires", 0))
        elif func == "unsatisfied_parents":
            r = NS[py](conn, tid)
        elif func == "_missing_task_ids":
            r = NS[py](conn, inp.get("ids") or [])
        elif func == "_prior_reviewer":
            r = NS[py](conn, tid)
        elif func == "_validate_model_override":
            r = NS[py](inp.get("model"), inp.get("provider"))
        elif func == "_normalize_task_skills":
            # module constant recomputed per case via the SAME seam the C
            # port calls at function entry — but ONLY when skills is not
            # None: C returns before the seam call on the null path (live
            # makes no runtime call at all), so recording it there leaks a
            # call neither side makes.
            if inp.get("skills") is not None:
                names = NS["get_toolset_names"]()
            else:
                script = (inp.get("io") or {}).get("get_toolset_names") or {}
                names = script.get("ret")
            NS["KNOWN_TOOLSET_NAMES"] = frozenset(
                str(n).casefold() for n in (names or []))
            r = NS[py](inp.get("skills"))
        elif func == "_completed_event_payload":
            r = NS[py](inp.get("result"), inp.get("event_summary"),
                       inp.get("verified_cards") or [], inp.get("metadata"))
        elif func == "_claim_is_live":
            r = NS[py](inp.get("trow") or {})
        else:
            raise KeyError(func)
        out["ret"] = norm_ret(r)
    except BaseException as e:
        out["ret"] = {"__raise__": type(e).__name__, "message": str(e)}
    return out


def main():
    cases = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            c = json.loads(line)
        except json.JSONDecodeError:
            continue
        cases += 1
        func = c.get("func", "")
        if func not in PY:
            mism += 1
            print(f"PYERR {func}: unknown func")
            continue
        try:
            want = run_case(func, c.get("in") or {})
        except Exception as e:
            mism += 1
            print(f"PYERR {func}: {type(e).__name__}: {e}")
            continue
        got = c.get("out") or {}
        ok = (got.get("trace") == want.get("trace")
              and got.get("ret") == want.get("ret")
              and got.get("state") == want.get("state"))
        if not ok:
            mism += 1
            print(f"MISMATCH {func}")
            print(f"  C  ={json.dumps(got, sort_keys=True, ensure_ascii=False)}")
            print(f"  PY ={json.dumps(want, sort_keys=True, ensure_ascii=False, default=str)}")
    print(f"ORACLE kanban_db_flow: {cases} cases, {mism} mismatches")
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())
