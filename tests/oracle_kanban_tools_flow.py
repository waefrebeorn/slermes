#!/usr/bin/env python3
"""Oracle for the tools/kanban_tools.py FLOW port (src/tools/kanban_tools_flow.c).

Drives the LIVE kanban_tools.py defs with SCRIPTED seams: the delegation
context predicates, profile toolset gate, tool_error, redact_sensitive_text,
owned_kanban_task, registry.get_schema, the wrapped handler (inner_fn),
logger.exception and the kanban_db record reads (kb.latest_run / kb.get_task)
are recording stubs fed the SAME scripts the C harness feeds its seams.
os.environ is driven from in.state.env; in.state.delegation_import_fail
models the agent.delegation_context import failing inside _delegation_ctx.
Compares the DECISION SEQUENCE (ordered structured seam events in "trace")
plus the return ("ret") and state ("state").

The live defs are AST-extracted (Assign AND AnnAssign constants, the _Reject
class, real builtin exceptions) so the heavy quarry import chain never runs.
Already-ported siblings (_is_delegated_child_context, _default_task_id,
_reject_delegated_child_mutation, _enforce_worker_task_ownership,
_worker_run_id, _ok) stay REAL on both sides (unclaimed internal helpers in
the C port), so the nested resolution chains are exercised.
_profile_has_kanban_toolset is a recorder override (claimed elsewhere).
Fail-closed: 0 cases or a missing extraction => nonzero exit.
Summary line: 'ORACLE kanban_tools_flow: N cases, M mismatches'.

Deterministic subset (documented): JSON numbers lose int/float identity
after parsing (integral doubles normalize to int on both sides); record
doubles are non-empty dicts -> types.SimpleNamespace or None (an empty {}
record would be truthy live as a dataclass but falsy as JSON — out of
fixture scope); tuples are modeled as lists; scripted _Reject messages are
the finished payload (e.args[0]); scripted KeyError messages are the RAW
key (str(KeyError(k)) == repr(k) on both sides); the _kanban_handler
decorator is projected to one wrapper execution with the inner_fn seam.
"""
import ast
import copy
import json
import os
import sys
import types
from pathlib import Path

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass  # non-reconfigurable streams (in-process drivers) keep their encoding

_HERE = Path(__file__).resolve()
_REPO = _HERE.parents[1]
QUARRY = _REPO.parent / "hermes-agent"
if not (QUARRY / "tools" / "kanban_tools.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "tools" / "kanban_tools.py")

# Runner hands the oracle a scrubbed environment; keep the sandbox honest.
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "ktf_home"))

# C harness func name == LIVE python name (identity map, kept explicit).
PY = {
    "_delegation_ctx": "_delegation_ctx",
    "_is_dispatcher_owned_worker": "_is_dispatcher_owned_worker",
    "_visible": "_visible",
    "_check": "_check",
    "_require_text": "_require_text",
    "_coerce_str_list": "_coerce_str_list",
    "_require_dict_metadata": "_require_dict_metadata",
    "_merge_artifacts": "_merge_artifacts",
    "_opt_int": "_opt_int",
    "_fields": "_fields",
    "_redact": "_redact",
    "_redact_opt": "_redact_opt",
    "_own_task_env": "_own_task_env",
    "_reject_inherited_task_mutation": "_reject_inherited_task_mutation",
    "_require_task_id": "_require_task_id",
    "_worker_guard": "_worker_guard",
    "_kanban_handler": "_kanban_handler",
    "_ok_landed": "_ok_landed",
}

# Real internal helpers the flows call (extracted, NOT stubbed).
REAL_HELPERS = (
    "_is_delegated_child_context", "_default_task_id",
    "_reject_delegated_child_mutation", "_enforce_worker_task_ownership",
    "_worker_run_id", "_ok", "_profile_has_kanban_toolset",
)

_SCHEMA_NAMES = (
    "KANBAN_ATTACH_SCHEMA", "KANBAN_ATTACH_URL_SCHEMA", "KANBAN_ATTACHMENTS_SCHEMA",
    "KANBAN_BLOCK_SCHEMA", "KANBAN_COMMENT_SCHEMA", "KANBAN_COMPLETE_SCHEMA",
    "KANBAN_CREATE_SCHEMA", "KANBAN_HEARTBEAT_SCHEMA", "KANBAN_LINK_SCHEMA",
    "KANBAN_LIST_SCHEMA", "KANBAN_REQUEST_CHANGES_SCHEMA",
    "KANBAN_REQUEST_REVIEW_SCHEMA", "KANBAN_SCHEDULE_SCHEMA", "KANBAN_SHOW_SCHEMA",
    "KANBAN_UNBLOCK_SCHEMA",
)

ENV_UNIVERSE = (
    "HERMES_KANBAN_TASK", "HERMES_KANBAN_RUN_ID", "HERMES_SESSION_ID",
    "HERMES_KANBAN_CLAIM_LOCK", "HERMES_TENANT", "HERMES_PROFILE",
    "HERMES_SESSION_KEY",
)


def extract():
    import contextlib
    import functools
    import logging
    import time
    from typing import Any, Callable, Optional

    mod = types.ModuleType("tools.kanban_tools")
    sys.modules["tools.kanban_tools"] = mod
    sys.modules.setdefault("tools", types.ModuleType("tools"))
    ns = mod.__dict__
    ns.update({
        "__name__": "tools.kanban_tools",
        "os": os, "json": json, "sys": sys, "time": time,
        "logging": logging, "functools": functools, "contextlib": contextlib,
        "ExitStack": contextlib.ExitStack, "contextmanager": contextlib.contextmanager,
        "Any": Any, "Callable": Callable, "Optional": Optional,
        # top-level ImportFrom names (seeder skips ImportFrom nodes)
        "no_cache_check_fn": lambda f: f,
        "registry": types.SimpleNamespace(),
        "tool_error": lambda message: json.dumps({"error": message}),
        "owned_kanban_task": lambda: None,
        "redact_sensitive_text": lambda text, force=False: text,
        "judge_goal": lambda **kw: ("done", "", None, None, False),
        "cfg_get": lambda *a, **k: True,
        "load_config": lambda *a, **k: {},
    })
    for name in _SCHEMA_NAMES:
        ns[name] = {"parameters": {"properties": {}, "required": []}}
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
    # 2. classes (the _Reject exception) BEFORE defs
    for node in tree.body:
        if isinstance(node, ast.ClassDef):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            except Exception:
                pass
    # 3. all function defs (source order: decorators resolve)
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            try:
                seg = ast.get_source_segment(src, node)
                exec(compile(seg, SRC, "exec"), ns)
            except Exception:
                pass
    required = list(PY.values()) + list(REAL_HELPERS) + [
        "_Reject", "_UNDECLARED_ARGS", "_RUN_LIFECYCLE_TOOLS",
        "tool_error", "registry", "no_cache_check_fn", "logger",
    ]
    missing = [n for n in required if n not in ns]
    if missing:
        print("FATAL kanban_tools_flow: extraction missing names:", sorted(missing))
        sys.exit(2)
    return ns


NS = extract()
REAL = {nm: NS[nm] for nm in set(PY.values()) | set(REAL_HELPERS)}

# The agent package double: _delegation_ctx does `from agent import
# delegation_context` at call time; the submodule attribute is the seam.
_AGENT_PKG = types.ModuleType("agent")
_AGENT_PKG.__path__ = []
sys.modules["agent"] = _AGENT_PKG


def make_exc(spec):
    t = spec.get("type", "Exception")
    msg = spec.get("message", "")
    if t == "_Reject":
        # Scripted message is the finished payload (e.args[0]); bypass
        # __init__ so no tool_error seam fires at construction.
        cls = NS["_Reject"]
        e = cls.__new__(cls)
        Exception.__init__(e, msg)
        return e
    cls = {
        "ValueError": ValueError, "TypeError": TypeError,
        "RuntimeError": RuntimeError, "KeyError": KeyError,
        "IndexError": IndexError, "AttributeError": AttributeError,
        "PermissionError": PermissionError, "Exception": Exception,
        "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
        "GeneratorExit": GeneratorExit,
    }.get(t)
    if cls is KeyError:
        return KeyError(msg)  # str() == repr(msg), matching the C spec
    if cls is not None:
        return cls(msg)
    return type(t, (Exception,), {})(msg)


def rec_double(v):
    """Record attribute doubles: dicts become types.SimpleNamespace."""
    return types.SimpleNamespace(**v) if isinstance(v, dict) else v


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


def seed_case(ctx, inp):
    # Restore every target/helper to REAL before installing case stubs.
    NS.update(REAL)
    # os.environ mirror (absent key == unset variable).
    env = ctx.state.get("env") or {}
    for k in ENV_UNIVERSE:
        os.environ.pop(k, None)
    for k, v in env.items():
        os.environ[k] = str(v)
    # delegation_context import double (or a halted import).
    if ctx.state.get("delegation_import_fail", False):
        sys.modules["agent.delegation_context"] = None
        if hasattr(_AGENT_PKG, "delegation_context"):
            delattr(_AGENT_PKG, "delegation_context")
    else:
        dc_mod = types.ModuleType("agent.delegation_context")
        dc_mod.__getattr__ = lambda name: (
            lambda: ctx.rec("delegation_ctx", {"predicate": name}))
        sys.modules["agent.delegation_context"] = dc_mod
        _AGENT_PKG.delegation_context = dc_mod
    NS["tool_error"] = lambda message: ctx.rec("tool_error", {"message": message})
    NS["owned_kanban_task"] = lambda: ctx.rec("owned_kanban_task", {})
    NS["redact_sensitive_text"] = lambda text, force=False: ctx.rec(
        "redact_sensitive_text", {"text": text, "force": force})
    NS["registry"] = types.SimpleNamespace(
        get_schema=lambda tool_name: ctx.rec("registry.get_schema",
                                             {"tool_name": tool_name}))
    NS["logger"] = types.SimpleNamespace(
        exception=lambda msg: ctx.rec("logger.exception", {"message": msg}),
        warning=lambda *a, **k: None, debug=lambda *a, **k: None)
    NS["_profile_has_kanban_toolset"] = lambda: ctx.rec(
        "profile_has_kanban_toolset", {})


def norm(v):
    """Compare parsed JSON: integral floats -> int, tuples -> lists,
    BaseException -> raise spec (message == str(exc))."""
    if isinstance(v, BaseException):
        return {"__raise__": type(v).__name__, "message": str(v)}
    if isinstance(v, bool):
        return v
    if isinstance(v, float):
        return int(v) if v == int(v) else v
    if isinstance(v, tuple):
        return [norm(x) for x in v]
    if isinstance(v, list):
        return [norm(x) for x in v]
    if isinstance(v, dict):
        return {k: norm(x) for k, x in v.items()}
    return v


def run_case(func, inp):
    ctx = CaseCtx(inp)
    seed_case(ctx, inp)
    out = {"ret": None, "trace": ctx.trace, "state": ctx.state}
    py = PY[func]
    args = inp.get("args") or {}
    try:
        if func == "_delegation_ctx":
            r = NS[py](inp.get("predicate", ""), inp.get("default", False))
        elif func == "_is_dispatcher_owned_worker":
            r = NS[py]()
        elif func == "_visible":
            r = NS[py](to_env_worker=inp.get("to_env_worker", False))
        elif func == "_check":
            r = NS[py](inp.get("cond"), inp.get("message", ""))
        elif func == "_require_text":
            r = NS[py](args, inp.get("name", ""), inp.get("message"))
        elif func == "_coerce_str_list":
            r = NS[py](inp.get("value"), inp.get("name", ""), inp.get("what", ""),
                       strip=inp.get("strip", False))
        elif func == "_require_dict_metadata":
            r = NS[py](inp.get("metadata"))
        elif func == "_merge_artifacts":
            r = NS[py](inp.get("metadata"), inp.get("artifacts") or [])
        elif func == "_opt_int":
            r = NS[py](inp.get("value"), inp.get("default"))
        elif func == "_fields":
            r = NS[py](rec_double(inp.get("obj")), tuple(inp.get("names") or []))
        elif func == "_redact":
            r = NS[py](inp.get("value"))
        elif func == "_redact_opt":
            r = NS[py](inp.get("value"))
        elif func == "_own_task_env":
            r = NS[py](inp.get("task_id", ""), inp.get("var", ""))
        elif func == "_reject_inherited_task_mutation":
            r = NS[py](inp.get("tid", ""), inp.get("env_tid"))
        elif func == "_require_task_id":
            r = NS[py](args)
        elif func == "_worker_guard":
            r = NS[py](inp.get("tool_name", ""), args)
        elif func == "_kanban_handler":
            deco = NS[py](inp.get("tool_name", ""))
            fn = lambda a, **kw: ctx.rec("inner_fn", {"args": a})
            r = deco(fn)(args)
        elif func == "_ok_landed":
            kb = types.SimpleNamespace(
                latest_run=lambda conn, tid: rec_double(
                    ctx.rec("kb.latest_run", {"task_id": tid})),
                get_task=lambda conn, tid: rec_double(
                    ctx.rec("kb.get_task", {"task_id": tid})))
            r = NS[py](kb, object(), inp.get("task_id", ""),
                       inp.get("default_status", ""), **(inp.get("extra") or {}))
        else:
            raise KeyError(func)
        out["ret"] = norm(r)
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
        got = norm(c.get("out") or {})
        ok = (got.get("trace") == want.get("trace")
              and got.get("ret") == want.get("ret")
              and got.get("state") == want.get("state"))
        if not ok:
            mism += 1
            print(f"MISMATCH {func}")
            print(f"  C  ={json.dumps(got, sort_keys=True, ensure_ascii=False)}")
            print(f"  PY ={json.dumps(want, sort_keys=True, ensure_ascii=False, default=str)}")
    print(f"ORACLE kanban_tools_flow: {cases} cases, {mism} mismatches")
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())
