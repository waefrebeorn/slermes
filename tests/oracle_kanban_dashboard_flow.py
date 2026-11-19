#!/usr/bin/env python3
"""Oracle for the plugins/kanban/dashboard/plugin_api.py FLOW port
(src/hermes_cli/kanban_dashboard_flow.c).

Drives the LIVE plugin_api.py helper defs with SCRIPTED seams: every kanban_db /
kbc / log / conn.execute dependency and every in-function import is a recording
stub fed the SAME scripts the C harness feeds its universal seam. Compares the
DECISION SEQUENCE (ordered structured seam events {"call":name,...args} in
"trace"), the return ("ret") and the mutated task_d ("state").

Live defs are AST-extracted (compiled with the PEP 563 annotations future-flag so
annotations never evaluate at def-exec time); the _StatusRejected class, the
_RUNNING_DIRECT_MSG / _STATUS_HANDLERS / _value_error_400 constants and the
kanban_db.Task dataclass are extracted too. Pure internal helpers (_map_errors /
_value_error_400, the _STATUS_HANDLERS dispatch, _warnings_summary aggregation,
_require / _normalize_slug_or_400 / _drag_to composition) stay REAL on both sides.
_set_status_direct is a SEAMED sibling. The heavy quarry import chain never runs.

Fail-closed: a missing/exhausted script raises RuntimeError "no script for <name>"
(recorded, then propagated); 0 cases or a missing extraction => nonzero exit.
BaseException kinds (SystemExit / KeyboardInterrupt / GeneratorExit /
CancelledError) pass through every `except Exception` site. Scripted raises
round-trip their spec bare; unscripted live raises use exc_spec().
Summary line: 'ORACLE kanban_dashboard_flow: N cases, M mismatches'.

Deterministic subset (documented): ASCII fixture scope; kanban_db.Task is modeled
as its asdict() field object; JSON numbers lose int/float identity after parsing
(integral doubles serialize as ints on both sides).
"""
import ast
import asyncio
import builtins
import copy
import json
import os
import sys
import types
from dataclasses import dataclass, field, asdict, fields, MISSING
from functools import partial
from contextlib import contextmanager
from pathlib import Path
from typing import Any, Optional, Callable, Iterator
import __future__

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_HERE = Path(__file__).resolve()
_REPO = _HERE.parents[1]
QUARRY = _REPO.parent / "hermes-agent"
if not (QUARRY / "plugins/kanban/dashboard/plugin_api.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "plugins/kanban/dashboard/plugin_api.py")
KDB = str(QUARRY / "hermes_cli/kanban_db.py")

os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "kd_home"))

FLAG = __future__.annotations.compiler_flag

# C harness func name == LIVE python name (identity map, kept explicit).
PY = {n: n for n in (
    "_placeholders", "_cap", "_attachment_dict", "_board_display_kwargs",
    "_warnings_summary_from_diagnostics", "_require_ok", "_conflict", "_require",
    "_require_task", "_require_run", "_normalize_slug_or_400", "_existing_board_slug",
    "_ws_upgrade_authorized", "_since_param", "_ws_board", "_ws_board_live",
    "_load_config_or_empty", "_active_profile_name", "_resolve_board", "_conn",
    "_attach_diagnostics", "_task_dict", "_links_for", "_link_tasks", "_drag_to",
    "_apply_status", "_set_priority", "_apply_model_override",
    "_apply_reasoning_effort", "_open_parent_refusal",
)}
REAL_HELPERS = ("_map_errors",)
CONSTS = ("_RUNNING_DIRECT_MSG", "_STATUS_HANDLERS", "_value_error_400")


class HTTPException(Exception):
    """Faithful fastapi.HTTPException double (str == "<status>: <detail>")."""
    def __init__(self, status_code, detail=None, headers=None):
        self.status_code = status_code
        self.detail = detail
        self.headers = headers
        super().__init__(detail)

    def __str__(self):
        return f"{self.status_code}: {self.detail}"

    def __repr__(self):
        return f"HTTPException(status_code={self.status_code!r}, detail={self.detail!r})"


def extract():
    ns = {
        "__name__": "plugins.kanban.dashboard.plugin_api",
        "HTTPException": HTTPException,
        "asdict": asdict, "partial": partial, "contextmanager": contextmanager,
        "Any": Any, "Optional": Optional, "Callable": Callable, "Iterator": Iterator,
        "Path": Path, "json": json, "os": os,
        "Query": lambda *a, **k: None, "File": lambda *a, **k: None, "Form": lambda *a, **k: None,
        "BaseModel": object, "Field": lambda *a, **k: None,
        "kanban_db": types.SimpleNamespace(DEFAULT_BOARD="default"),
        "kd": types.SimpleNamespace(SEVERITY_ORDER=("warning", "error", "critical")),
        "kbc": types.SimpleNamespace(),
        "log": types.SimpleNamespace(),
    }
    mod = types.ModuleType(ns["__name__"])
    mod.__dict__.update(ns)
    sys.modules[ns["__name__"]] = mod
    ns = mod.__dict__

    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    # exception class first (referenced by _apply_status)
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "_StatusRejected":
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec", flags=FLAG), ns)
    # target defs + real helpers
    want = set(PY.values()) | set(REAL_HELPERS)
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in want:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec", flags=FLAG), ns)
    # constants AFTER defs (lambdas reference _drag_to / _map_errors)
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgt = node.targets[0] if isinstance(node, ast.Assign) else node.target
            if isinstance(tgt, ast.Name) and tgt.id in CONSTS:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec", flags=FLAG), ns)
    # kanban_db.Task dataclass (for _task_dict's asdict)
    ksrc = open(KDB, encoding="utf-8").read()
    ktree = ast.parse(ksrc)
    kns = {"__name__": "hermes_cli.kanban_db", "dataclass": dataclass, "field": field,
           "Optional": Optional, "Any": Any}
    kmod = types.ModuleType("hermes_cli.kanban_db")
    kmod.__dict__.update(kns)
    sys.modules["hermes_cli.kanban_db"] = kmod
    for node in ktree.body:
        if isinstance(node, ast.ClassDef) and node.name == "Task":
            exec(compile(ast.Module(body=[node], type_ignores=[]), KDB, "exec", flags=FLAG), kmod.__dict__)
    ns["Task"] = kmod.__dict__["Task"]

    required = list(PY.values()) + list(REAL_HELPERS) + list(CONSTS) + ["_StatusRejected", "Task"]
    missing = [n for n in required if n not in ns]
    if missing:
        print("FATAL kanban_dashboard_flow: extraction missing names:", sorted(missing))
        sys.exit(2)
    return ns


NS = extract()


def make_exc(spec):
    t = spec.get("type", "Exception")
    msg = spec.get("message", "")
    if t == "HTTPException":
        return HTTPException(spec.get("status_code", 500), spec.get("detail", msg))
    if t == "SystemExit":
        return SystemExit(spec.get("code", msg))
    if t == "KeyboardInterrupt":
        return KeyboardInterrupt(msg)
    if t == "GeneratorExit":
        return GeneratorExit(msg)
    if t == "CancelledError":
        return asyncio.CancelledError(msg)
    if t == "_StatusRejected":
        return NS["_StatusRejected"](msg)
    cls = getattr(builtins, t, None)
    if isinstance(cls, type) and issubclass(cls, BaseException):
        return cls(msg)
    return type(t, (Exception,), {})(msg)


def exc_spec(e):
    spec = {"__raise__": type(e).__name__, "message": str(e)}
    if isinstance(e, HTTPException):
        spec["status_code"] = e.status_code
        spec["detail"] = e.detail
    if isinstance(e, SystemExit):
        spec["code"] = e.code
    return spec


def sentinel_from_spec(spec):
    out = {"__raise__": spec.get("type", "Exception"), "message": spec.get("message", "")}
    for k in ("status_code", "detail", "code", "name"):
        if k in spec:
            out[k] = spec[k]
    return out


class Raise:
    def __init__(self, spec):
        self.spec = spec


class CaseCtx:
    def __init__(self, inp):
        self.inp = inp
        self.trace = []
        self.io = inp.get("io", {}) or {}
        self.cursors = {}
        self.scripted = []

    def peek(self, name):
        entry = self.io.get(name)
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            return entry[idx] if idx < len(entry) else None
        return entry

    def consume(self, name):
        if isinstance(self.io.get(name), list):
            self.cursors[name] = self.cursors.get(name, 0) + 1

    def script_next(self, name):
        entry = self.io.get(name)
        if entry is None:
            return Raise({"type": "RuntimeError", "message": f"no script for {name}"})
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                return Raise({"type": "RuntimeError", "message": f"no script for {name}"})
            self.cursors[name] = idx + 1
            return entry[idx]
        return entry

    def raise_scripted(self, spec):
        e = make_exc(spec)
        self.scripted.append((e, spec))
        raise e

    def rec(self, name, args):
        ev = {"call": name}
        ev.update(args)
        self.trace.append(ev)
        result = self.script_next(name)
        if isinstance(result, Raise):
            self.raise_scripted(result.spec)
        if isinstance(result, dict):
            if isinstance(result.get("raise"), dict):
                self.raise_scripted(result["raise"])
            return result.get("ret")
        return result

    def recorder(self, name, proj=None):
        def call(*a, **k):
            return self.rec(name, proj(a, k) if proj else {})
        return call


CURRENT = {"ctx": None}


def _import_guard_module(modname, scriptkey, call_names):
    m = types.ModuleType(modname)

    def getattr_(name):
        ctx = CURRENT["ctx"]
        entry = ctx.peek(scriptkey)
        if isinstance(entry, dict) and isinstance(entry.get("import_raise"), dict):
            ctx.consume(scriptkey)
            ctx.raise_scripted(entry["import_raise"])
        if name in call_names:
            return ctx.recorder(name)
        raise AttributeError(name)

    m.__getattr__ = getattr_
    return m


def _ns_double(r):
    return types.SimpleNamespace(**r) if isinstance(r, dict) else r


class _Conn:
    def __init__(self, ctx):
        self.ctx = ctx

    def execute(self, sql, params=()):
        return self.ctx.rec("conn.execute", {"sql": sql, "params": list(params)})


class _Log:
    def __init__(self, ctx):
        self.ctx = ctx

    def warning(self, fmt, *args):
        exc = args[0] if args else None
        proj = {"type": type(exc).__name__, "message": str(exc)} if isinstance(exc, BaseException) else exc
        return self.ctx.rec("log.warning", {"fmt": fmt, "exc": proj})


def install_stubs(ctx):
    ns = NS
    kdb = ns["kanban_db"]
    rec = ctx.rec

    kdb._normalize_board_slug = lambda slug: rec("kanban_db._normalize_board_slug", {"slug": slug})
    kdb.board_exists = lambda board=None: rec("kanban_db.board_exists", {"board": board})
    kdb.read_board_metadata = lambda board: rec("kanban_db.read_board_metadata", {"board": board})
    kdb.get_task = lambda c, t: _ns_double(rec("kanban_db.get_task", {"task_id": t}))
    kdb.get_run = lambda c, r: _ns_double(rec("kanban_db.get_run", {"run_id": r}))
    kdb.init_db = lambda board=None: rec("kanban_db.init_db", {"board": board})
    kdb.task_age = lambda task: rec("kanban_db.task_age", {"task_id": task.id})
    kdb.unblock_task = lambda c, t: rec("kanban_db.unblock_task", {"task_id": t})
    kdb.reopen_review_task = lambda c, t: rec("kanban_db.reopen_review_task", {"task_id": t})
    kdb.complete_task = lambda c, t, result=None, summary=None, metadata=None, force=False: rec(
        "kanban_db.complete_task", {"task_id": t, "result": result, "summary": summary,
                                    "metadata": metadata, "force": force})
    kdb.block_task = lambda c, t, reason=None: rec("kanban_db.block_task", {"task_id": t, "reason": reason})
    kdb.schedule_task = lambda c, t, reason=None: rec("kanban_db.schedule_task", {"task_id": t, "reason": reason})
    kdb.request_review = lambda c, t, summary=None, metadata=None, reviewer=None, force=False: rec(
        "kanban_db.request_review", {"task_id": t, "summary": summary, "metadata": metadata,
                                     "reviewer": reviewer, "force": force})
    kdb.set_model_override = lambda c, t, model, provider=None: rec(
        "kanban_db.set_model_override", {"task_id": t, "model": model, "provider": provider})
    kdb.set_reasoning_effort = lambda c, t, effort: rec(
        "kanban_db.set_reasoning_effort", {"task_id": t, "effort": effort})
    kdb.edit_task = lambda c, t, priority=None, board=None: rec(
        "kanban_db.edit_task", {"task_id": t, "priority": priority, "board": board})
    kdb.unsatisfied_parents = lambda c, t: rec("kanban_db.unsatisfied_parents", {"task_id": t})

    ns["kbc"] = types.SimpleNamespace(connect=lambda board=None: rec("kbc.connect", {"board": board}))
    ns["log"] = _Log(ctx)
    ns["_set_status_direct"] = lambda c, t, s: rec("_set_status_direct", {"task_id": t, "status": s})

    hc = types.ModuleType("hermes_cli")
    hc.__path__ = []
    sys.modules["hermes_cli"] = hc
    wsc = _import_guard_module("hermes_cli.web_server_chat", "import:hermes_cli.web_server_chat",
                               {"_ws_auth_ok", "_ws_request_is_allowed"})
    sys.modules["hermes_cli.web_server_chat"] = wsc
    hc.web_server_chat = wsc
    cfg = _import_guard_module("hermes_cli.config", "import:hermes_cli.config", {"load_config"})
    sys.modules["hermes_cli.config"] = cfg
    hc.config = cfg
    prof = _import_guard_module("hermes_cli.profiles", "import:hermes_cli.profiles", {"get_active_profile_name"})
    sys.modules["hermes_cli.profiles"] = prof
    hc.profiles = prof
    return _Conn(ctx)


def norm_ret(r):
    if isinstance(r, BaseException):
        return exc_spec(r)
    if isinstance(r, types.SimpleNamespace):
        return {k: norm_ret(v) for k, v in vars(r).items()}
    if isinstance(r, float) and r.is_integer():
        return int(r)
    if isinstance(r, tuple):
        return [norm_ret(x) for x in r]
    if isinstance(r, list):
        return [norm_ret(x) for x in r]
    if isinstance(r, dict):
        return {k: norm_ret(v) for k, v in r.items()}
    return r


def run_case(func, inp):
    ctx = CaseCtx(inp)
    CURRENT["ctx"] = ctx
    conn = install_stubs(ctx)
    ns = NS
    out = {"ret": None, "trace": ctx.trace, "state": {}}
    py = PY[func]
    try:
        if func == "_placeholders":
            r = ns[py](inp.get("ids", []))
        elif func == "_cap":
            r = ns[py](inp.get("s"), inp.get("n", 0))
        elif func == "_attachment_dict":
            r = ns[py](types.SimpleNamespace(**(inp.get("a") or {})))
        elif func == "_board_display_kwargs":
            r = ns[py](types.SimpleNamespace(**(inp.get("p") or {})))
        elif func == "_warnings_summary_from_diagnostics":
            r = ns[py](inp.get("diagnostics", []))
        elif func == "_require_ok":
            r = ns[py](inp.get("ok"))
        elif func == "_conflict":
            r = ns[py](inp.get("detail", ""))
        elif func == "_require":
            gn = inp.get("getter_name", "kanban_db.get_task")
            ik = inp.get("ident_key", "task_id")
            getter = lambda c, i: _ns_double(ctx.rec(gn, {ik: i}))
            r = ns[py](getter, conn, inp.get("ident"), inp.get("label", "task"))
        elif func == "_require_task":
            r = ns[py](conn, inp.get("task_id"))
        elif func == "_require_run":
            r = ns[py](conn, inp.get("run_id"))
        elif func == "_normalize_slug_or_400":
            r = ns[py](inp.get("slug"))
        elif func == "_existing_board_slug":
            r = ns[py](inp.get("slug"))
        elif func == "_ws_upgrade_authorized":
            r = ns[py](object())
        elif func == "_since_param":
            ws = types.SimpleNamespace(query_params=inp.get("query_params", {}) or {})
            r = ns[py](ws)
        elif func == "_ws_board":
            r = ns[py](inp.get("raw"))
        elif func == "_ws_board_live":
            r = ns[py](inp.get("normed"))
        elif func == "_load_config_or_empty":
            r = ns[py]()
        elif func == "_active_profile_name":
            r = ns[py]()
        elif func == "_resolve_board":
            r = ns[py](inp.get("board"))
        elif func == "_conn":
            r = ns[py](board=inp.get("board"))
        elif func == "_attach_diagnostics":
            td = copy.deepcopy(inp.get("task_d", {}) or {})
            ns[py](td, inp.get("diags"))
            r = None
            out["state"] = {"task_d": td}
        elif func == "_task_dict":
            task = ns["Task"](**(inp.get("task") or {}))
            r = ns[py](task, latest_summary=inp.get("latest_summary"),
                       current_run_started_at=inp.get("current_run_started_at"))
        elif func == "_links_for":
            r = ns[py](conn, inp.get("task_id"))
        elif func == "_link_tasks":
            r = ns[py](conn, inp.get("links") or {})
        elif func == "_drag_to":
            r = ns[py](conn, inp.get("task_id"), inp.get("s", ""))
        elif func == "_apply_status":
            p = types.SimpleNamespace(**(inp.get("p") or {}))
            r = ns[py](conn, inp.get("task_id"), inp.get("s", ""), p, inp.get("unknown_detail", ""))
        elif func == "_set_priority":
            r = ns[py](conn, inp.get("task_id"), inp.get("priority"), inp.get("board"))
        elif func == "_apply_model_override":
            p = types.SimpleNamespace(**(inp.get("p") or {}))
            r = ns[py](conn, inp.get("task_id"), p)
        elif func == "_apply_reasoning_effort":
            p = types.SimpleNamespace(**(inp.get("p") or {}))
            r = ns[py](conn, inp.get("task_id"), p)
        elif func == "_open_parent_refusal":
            r = ns[py](conn, inp.get("task_id"), inp.get("s", ""))
        else:
            raise KeyError(func)
        out["ret"] = norm_ret(r)
    except BaseException as e:
        for (se, spec) in ctx.scripted:
            if e is se:
                out["ret"] = sentinel_from_spec(spec)
                break
        else:
            out["ret"] = exc_spec(e)
    out["ret"] = norm_ret(out["ret"])
    if out["state"]:
        out["state"] = {k: norm_ret(v) for k, v in out["state"].items()}
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
    print(f"ORACLE kanban_dashboard_flow: {cases} cases, {mism} mismatches")
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())
