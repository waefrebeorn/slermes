#!/usr/bin/env python3
"""Oracle for the UNCLAIMED goal FLOW port (src/cli/goals_flow.c).

Drives the LIVE defs from hermes_cli/goals.py with SCRIPTED I/O: every
collaborator is a recording stub fed the same scripts the C harness feeds
its seams. Compares the ordered seam-name TRACE, the return (ret), the
mutated caller-held mirror (state: manager / goal_state) and the captured
save_goal payloads (saves).

Ported targets stay REAL (module defs + GoalState.clear_wait + the
GoalManager methods). CLAIMED siblings (load_goal, save_goal, has_goal,
_get_session_db, _goal_judge_max_tokens, wait_on, wait_on_session,
wait_for_seconds) and foreign collaborators (time.time, call_llm,
load_config, cast, scoped_session_cwd, resolve_agent_cwd, Path.is_dir,
profile_scope, open_persistence, db.close, _session_records,
read_transcript, as_wire_row) are SEAMED — scripted + fail-closed.

In-function imports are modeled by import-guard sys.modules stubs that
fire ONE import seam token on first attribute access, or raise an
import_raise spec WITHOUT a token (the C __untrace__ retraction).

Live defs are AST-extracted (Assign + decorated ClassDef + FunctionDef,
future annotations). The heavy quarry import chain is never imported.
Fail-closed: a missing seam script raises RuntimeError("unscripted seam:
<name>"); 0 cases => exit 1. Summary: 'ORACLE goals_flow: N cases, M
mismatches'.
"""
import ast
import builtins
import copy
import __future__
import json
import math
import os
import sys
import types
from dataclasses import asdict, dataclass, field
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_HERE = Path(__file__).resolve()
_REPO = _HERE.parents[1]
QUARRY = _REPO.parent / "hermes-agent"
if not (QUARRY / "hermes_cli" / "goals.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
GOALS_PY = QUARRY / "hermes_cli" / "goals.py"

# Scrubbed-env safety: derive home from __file__ (tests/ -> repo -> home).
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home_goals_flow"))

# func key -> live def name.
PY = {
    "decision": "_decision",
    "render_extra_criteria": "_render_extra_criteria",
    "goal_kick_prompt": "goal_kick_prompt",
    "last_user_message_content": "last_user_message_content",
    "count_active_delegations": "count_active_delegations",
    "last_user_message_from_db": "last_user_message_from_db",
    "goal_judge_setting": "_goal_judge_setting",
    "call_goal_judge_llm": "_call_goal_judge_llm",
    "gate_workspace": "_gate_workspace",
    "acquire_session_db": "_acquire_session_db",
    "release_session_db": "_release_session_db",
    "clear_wait": "clear_wait",
    "refresh": "refresh",
    "save": "_save",
    "require_goal": "_require_goal",
    "require_active": "_require_active",
    "pause_state": "_pause_state",
    "pause_decision": "_pause_decision",
    "pop_item": "_pop_item",
    "clear_items": "_clear_items",
    "park": "_park",
    "waiting_decision": "_waiting_decision",
    "apply_wait_directive": "_apply_wait_directive",
    "budget_pause": "_budget_pause",
}
MANAGER_FUNCS = {
    "refresh", "save", "require_goal", "require_active", "pause_state",
    "pause_decision", "pop_item", "clear_items", "park", "waiting_decision",
    "apply_wait_directive", "budget_pause",
}

ANN_FLAG = __future__.annotations.compiler_flag


def make_exc(spec):
    spec = spec or {}
    t = spec.get("type", "RuntimeError")
    msg = spec.get("message", "")
    if t == "SystemExit":
        return SystemExit(spec.get("code", 1))
    cls = getattr(builtins, t, None)
    if isinstance(cls, type) and issubclass(cls, BaseException):
        if t in ("UnicodeDecodeError", "UnicodeEncodeError", "UnicodeTranslateError"):
            # Their __init__ demands 5 args; bypass it (Cls.__new__ is the
            # py3.14-safe form) so str(e) == the scripted message.
            e = cls.__new__(cls)
            Exception.__init__(e, msg)
            return e
        return cls(msg)
    return type(t, (Exception,), {})(msg)


def exc_spec(e):
    spec = {"type": type(e).__name__, "message": str(e)}
    if isinstance(e, SystemExit):
        spec["code"] = e.code
    return spec


class GlPath:
    """Path double: str-only construction (CPython 3.14 message), posix str,
    no-op expanduser (fixture domain excludes '~'), scripted is_dir."""
    def __init__(self, s):
        if not isinstance(s, str):
            raise TypeError(
                "argument should be a str or an os.PathLike object where "
                f"__fspath__ returns a str, not '{type(s).__name__}'")
        self._s = s

    def expanduser(self):
        return self

    def is_dir(self):
        return CURRENT["ctx"].recorder("path_is_dir")(self)

    def __fspath__(self):
        return self._s

    def __str__(self):
        return self._s


class TranscriptReadDouble:
    def __init__(self, **kw):
        self.kw = kw


class SilentLogger:
    def debug(self, *a, **k): pass
    def info(self, *a, **k): pass
    def warning(self, *a, **k): pass
    def error(self, *a, **k): pass


CURRENT = {"ctx": None}


class CaseCtx:
    def __init__(self, inp):
        self.inp = inp
        self.trace = []
        self.saves = []
        self.io = inp.get("io", {}) or {}
        self.cursors = {}
        self.import_fired = set()
        self.state = copy.deepcopy(inp.get("state", {}) or {})

    def peek(self, name):
        entry = self.io.get(name)
        if entry is None:
            return None
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            return entry[idx] if idx < len(entry) else None
        return entry

    def consume(self, name):
        entry = self.io.get(name)
        if isinstance(entry, list):
            self.cursors[name] = self.cursors.get(name, 0) + 1

    def recorder(self, name):
        ctx = self

        def call(*args, **kwargs):
            ctx.trace.append(name)
            entry = ctx.peek(name)
            if entry is None:
                raise RuntimeError("unscripted seam: " + name)
            ctx.consume(name)
            if isinstance(entry, dict):
                if isinstance(entry.get("raise"), dict):
                    raise make_exc(entry["raise"])
                ret = entry.get("ret")
            else:
                ret = entry
            return convert_seam_ret(name, ret)
        return call


def mat_resp(v):
    """call_llm resp: dict -> SimpleNamespace double (attribute-error type
    names read 'types.SimpleNamespace' on both sides), list -> list,
    scalars as-is."""
    if isinstance(v, dict):
        return types.SimpleNamespace(**{k: mat_resp(x) for k, x in v.items()})
    if isinstance(v, list):
        return [mat_resp(x) for x in v]
    return v


def mat_contract(d):
    C = REAL["GoalContract"]
    if not isinstance(d, dict):
        return C()
    return C(**{f: (d[f] if f in d else "") for f in
                ("outcome", "verification", "constraints", "boundaries", "stop_when")})


def mat_gate(d):
    G = REAL["GoalGate"]
    if not isinstance(d, dict):
        return G(command="")
    return G(
        command=d.get("command", ""),
        timeout_seconds=d.get("timeout_seconds", 300),
        max_retries=d.get("max_retries", 3),
        attempts=d.get("attempts", 0),
        last_exit_code=d.get("last_exit_code"),
        last_output_tail=d.get("last_output_tail", ""),
    )


def mat_goal_state(d):
    """Default-fill EXACTLY like the C gl_mat_state: presence-based (a
    present null stays null), values as-is, no str/strip coercion."""
    if d is None:
        return None
    if not isinstance(d, dict):
        return d
    S = REAL["GoalState"]
    return S(
        goal=d["goal"] if "goal" in d else "",
        status=d["status"] if "status" in d else "active",
        turns_used=d["turns_used"] if "turns_used" in d else 0,
        max_turns=d["max_turns"] if "max_turns" in d else 20,
        created_at=d["created_at"] if "created_at" in d else 0,
        last_turn_at=d["last_turn_at"] if "last_turn_at" in d else 0,
        last_verdict=d["last_verdict"] if "last_verdict" in d else None,
        last_reason=d["last_reason"] if "last_reason" in d else None,
        paused_reason=d["paused_reason"] if "paused_reason" in d else None,
        consecutive_parse_failures=d["consecutive_parse_failures"] if "consecutive_parse_failures" in d else 0,
        consecutive_transport_failures=d["consecutive_transport_failures"] if "consecutive_transport_failures" in d else 0,
        subgoals=d["subgoals"] if "subgoals" in d else [],
        waiting_on_pid=d["waiting_on_pid"] if "waiting_on_pid" in d else None,
        waiting_on_session=d["waiting_on_session"] if "waiting_on_session" in d else None,
        waiting_until=d["waiting_until"] if "waiting_until" in d else 0,
        waiting_on_delegations=d["waiting_on_delegations"] if "waiting_on_delegations" in d else 0,
        waiting_reason=d["waiting_reason"] if "waiting_reason" in d else None,
        waiting_since=d["waiting_since"] if "waiting_since" in d else 0,
        contract=mat_contract(d.get("contract")),
        gates=[mat_gate(g) for g in d["gates"]] if isinstance(d.get("gates"), list) else [],
    )


def convert_seam_ret(name, ret):
    ctx = CURRENT["ctx"]
    if name in ("load_goal", "wait_on", "wait_on_session", "wait_for_seconds"):
        return mat_goal_state(ret) if isinstance(ret, dict) else ret
    if name == "_get_session_db":
        if ret is None:
            return None
        return types.SimpleNamespace(
            messages=types.SimpleNamespace(read_transcript=ctx.recorder("read_transcript")))
    if name == "read_transcript":
        if isinstance(ret, list):
            return [types.SimpleNamespace(as_wire_row=ctx.recorder("as_wire_row")) for _ in ret]
        return ret
    if name == "call_llm":
        return mat_resp(ret)
    return ret


def make_import_mod(modname, import_seam, name_map):
    m = types.ModuleType(modname)

    def getattr_(name):
        ctx = CURRENT["ctx"]
        seam = import_seam
        entry = ctx.peek(seam)
        if entry is None:
            ctx.trace.append(seam)
            raise RuntimeError("unscripted seam: " + seam)
        if isinstance(entry, dict) and isinstance(entry.get("import_raise"), dict):
            ctx.consume(seam)
            raise make_exc(entry["import_raise"])   # no token (C __untrace__)
        if seam not in ctx.import_fired:
            ctx.trace.append(seam)
            ctx.import_fired.add(seam)
            ctx.consume(seam)
        if name in name_map:
            return name_map[name]
        raise AttributeError(name)

    m.__getattr__ = getattr_
    return m


def _pkg(name):
    m = types.ModuleType(name)
    m.__path__ = []
    sys.modules[name] = m
    return m


def install_stubs(ctx, inp):
    ns = NS
    rec = ctx.recorder
    # restore every ported REAL def (a prior case may have left a recorder).
    for k, v in REAL.items():
        ns[k] = v
    ns["logger"] = SilentLogger()
    ns["Path"] = GlPath   # _gate_workspace / _acquire_session_db construct Path
    ns["time"] = types.SimpleNamespace(time=rec("time_time"))
    ns["load_goal"] = rec("load_goal")
    ns["_get_session_db"] = rec("_get_session_db")
    ns["_goal_judge_max_tokens"] = rec("goal_judge_max_tokens")

    def save_goal_stub(session_id, state):
        ctx.trace.append("save_goal")
        entry = ctx.peek("save_goal")
        if entry is None:
            raise RuntimeError("unscripted seam: save_goal")
        ctx.consume("save_goal")
        ctx.saves.append({"session_id": session_id, "state": proj(state)})
        if isinstance(entry, dict):
            if isinstance(entry.get("raise"), dict):
                raise make_exc(entry["raise"])
            return entry.get("ret")
        return entry
    ns["save_goal"] = save_goal_stub

    # ── in-function import guards (one import token on first access) ──
    _pkg("tools")
    sys.modules["tools.async_delegation"] = make_import_mod(
        "tools.async_delegation", "async_delegation_import",
        {"_LIVE_STATES": inp.get("live_states", ["running"]),
         "_session_records": rec("_session_records")})
    _pkg("persistence")
    _pkg("persistence.conversation")
    sys.modules["persistence.conversation.messages"] = make_import_mod(
        "persistence.conversation.messages", "transcript_import",
        {"TranscriptRead": TranscriptReadDouble})
    _pkg("hermes_cli")
    sys.modules["hermes_cli.config"] = make_import_mod(
        "hermes_cli.config", "config_import", {"load_config": rec("load_config")})
    _pkg("agent")
    sys.modules["agent.runtime_cwd"] = make_import_mod(
        "agent.runtime_cwd", "runtime_cwd_import",
        {"resolve_agent_cwd": rec("resolve_agent_cwd"),
         "scoped_session_cwd": rec("scoped_session_cwd")})
    sys.modules["persistence_assembly"] = make_import_mod(
        "persistence_assembly", "persistence_assembly_import",
        {"open_persistence": rec("open_persistence"),
         "profile_scope": rec("profile_scope")})


def proj(v):
    if isinstance(v, dict):
        return {k: proj(x) for k, x in v.items()}
    if isinstance(v, (tuple, list)):
        return [proj(x) for x in v]
    if isinstance(v, (set, frozenset)):
        return sorted(v)
    if isinstance(v, GlPath):
        return str(v)
    if isinstance(v, Path):
        return v.as_posix()
    if isinstance(v, BaseException):
        return exc_spec(v)
    if isinstance(v, (REAL["GoalState"], REAL["GoalContract"], REAL["GoalGate"])):
        return proj(asdict(v))
    return v


def canon(v):
    """Integral floats == ints on BOTH sides (libjson has no identity)."""
    if isinstance(v, dict):
        return {k: canon(x) for k, x in v.items()}
    if isinstance(v, list):
        return [canon(x) for x in v]
    if isinstance(v, float) and math.isfinite(v) and v == int(v):
        return int(v)
    return v


def _exec_node(node, ns, src):
    seg = ast.get_source_segment(src, node)
    if isinstance(node, ast.ClassDef) and node.decorator_list:
        # ast.get_source_segment EXCLUDES decorators: rebuild from the first
        # decorator line so @dataclass actually applies.
        lines = src.splitlines()
        seg = "\n".join(lines[node.decorator_list[0].lineno - 1:node.end_lineno])
    exec(compile(seg, src, "exec", flags=ANN_FLAG), ns)


def extract():
    src = GOALS_PY.read_text(encoding="utf-8")
    tree = ast.parse(src)
    # py3.14 dataclasses needs sys.modules[cls.__module__].
    mod = types.ModuleType("goals_flow_oracle")
    sys.modules["goals_flow_oracle"] = mod
    ns = mod.__dict__
    ns.update({
        "__name__": "goals_flow_oracle", "__file__": str(GOALS_PY),
        "dataclass": dataclass, "field": field, "asdict": asdict,
        "json": json, "os": os, "sys": sys, "Path": Path,
        "Any": object, "Dict": dict, "List": list, "Optional": object, "Tuple": tuple,
    })
    import asyncio
    import logging
    import re
    import subprocess
    import threading
    import time as _time
    from datetime import datetime, timezone, UTC
    ns.update({"asyncio": asyncio, "logging": logging, "re": re,
               "subprocess": subprocess, "threading": threading, "time": _time,
               "datetime": datetime, "timezone": timezone, "UTC": UTC})
    # Assigns FIRST (dataclass field defaults + def defaults read them).
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgt = node.targets[0] if isinstance(node, ast.Assign) else node.target
            if not isinstance(tgt, ast.Name):
                continue
            _exec_node(node, ns, src)
    # ClassDefs (GoalContract, GoalGate, GoalState, GoalManager).
    for node in tree.body:
        if isinstance(node, ast.ClassDef):
            _exec_node(node, ns, src)
    # FunctionDefs.
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            _exec_node(node, ns, src)
    # re-seed constants after defs (partial-alias shapes).
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgt = node.targets[0] if isinstance(node, ast.Assign) else node.target
            if isinstance(tgt, ast.Name):
                _exec_node(node, ns, src)
    missing = [v for v in PY.values() if v not in ns and v not in
               ("clear_wait", "refresh", "_save", "_require_goal", "_require_active",
                "_pause_state", "_pause_decision", "_pop_item", "_clear_items",
                "_park", "_waiting_decision", "_apply_wait_directive", "_budget_pause")]
    for cls in ("GoalContract", "GoalGate", "GoalState", "GoalManager"):
        if cls not in ns:
            missing.append(cls)
    for m in PY.values():
        if m in ("clear_wait",) and not hasattr(ns.get("GoalState"), m):
            missing.append("GoalState." + m)
        if m in MANAGER_FUNCS and not hasattr(ns.get("GoalManager"), m):
            missing.append("GoalManager." + m)
    if missing:
        print("FATAL goals_flow: extraction missing names:", sorted(set(missing)))
        sys.exit(2)
    return ns


NS = extract()
REAL = {k: NS[k] for k in PY.values() if k in NS}
for cls in ("GoalContract", "GoalGate", "GoalState", "GoalManager"):
    REAL[cls] = NS[cls]


def mk_manager(ctx, inp):
    mseed = (inp.get("state") or {}).get("manager") or {}
    mgr = object.__new__(NS["GoalManager"])
    mgr.session_id = mseed.get("session_id", "")
    mgr.default_max_turns = mseed.get("default_max_turns", 20)
    mgr._state = mat_goal_state(mseed.get("_state"))
    rec = ctx.recorder
    mgr.has_goal = lambda *a, **k: rec("has_goal")(*a, **k)
    mgr.wait_on = lambda *a, **k: rec("wait_on")(*a, **k)
    mgr.wait_on_session = lambda *a, **k: rec("wait_on_session")(*a, **k)
    mgr.wait_for_seconds = lambda *a, **k: rec("wait_for_seconds")(*a, **k)
    ctx.state.setdefault("manager", {})
    ctx.state["manager"]["session_id"] = mgr.session_id
    ctx.state["manager"]["default_max_turns"] = mgr.default_max_turns
    ctx.state["manager"]["_state"] = mgr._state
    return mgr


def drive(func, inp, ctx):
    ns = NS
    g = inp.get
    if func == "decision":
        return ns["_decision"](g("status"), g("should_continue", False), g("prompt"),
                               g("verdict", ""), g("reason", ""), g("message", ""))
    if func == "render_extra_criteria":
        return ns["_render_extra_criteria"](g("subgoals"))
    if func == "goal_kick_prompt":
        return ns["goal_kick_prompt"](g("goal"), g("last_user_message"))
    if func == "last_user_message_content":
        return ns["last_user_message_content"](g("history"))
    if func == "count_active_delegations":
        return ns["count_active_delegations"](g("session_id"))
    if func == "last_user_message_from_db":
        return ns["last_user_message_from_db"](g("session_id"))
    if func == "goal_judge_setting":
        return ns["_goal_judge_setting"](g("key", "max_tokens"), g("default"), ctx.recorder("cast"))
    if func == "call_goal_judge_llm":
        return ns["_call_goal_judge_llm"](ctx.recorder("call_llm"), g("system_prompt", ""),
                                          g("user_prompt", ""), g("timeout"))
    if func == "gate_workspace":
        return ns["_gate_workspace"]()
    if func == "acquire_session_db":
        return ns["_acquire_session_db"](g("home"))
    if func == "release_session_db":
        db = g("db")
        # opaque handle: any non-null db exposes .close (scripted seam).
        if db is not None:
            db = types.SimpleNamespace(close=ctx.recorder("db_close"))
        return ns["_release_session_db"](db)
    if func == "clear_wait":
        st = mat_goal_state(ctx.state.get("goal_state"))
        st.clear_wait()
        ctx.state["goal_state"] = st
        return None
    if func in MANAGER_FUNCS:
        mgr = mk_manager(ctx, inp)
        try:
            if func == "refresh":
                return mgr.refresh()
            if func == "save":
                return mgr._save()
            if func == "require_goal":
                return mgr._require_goal()
            if func == "require_active":
                return mgr._require_active()
            if func == "pause_state":
                return mgr._pause_state(g("reason"))
            if func == "pause_decision":
                return mgr._pause_decision(g("paused_reason", ""), g("verdict", ""),
                                           g("reason", ""), g("message", ""))
            if func == "pop_item":
                return mgr._pop_item(g("attr", "subgoals"), g("index"))
            if func == "clear_items":
                return mgr._clear_items(g("attr", "subgoals"))
            if func == "park":
                return mgr._park(g("reason"), **(g("barrier") or {}))
            if func == "waiting_decision":
                return mgr._waiting_decision(mgr._state)
            if func == "apply_wait_directive":
                return mgr._apply_wait_directive(g("directive"), g("reason"),
                                                 active_delegations=g("active_delegations", 0))
            if func == "budget_pause":
                return mgr._budget_pause(mgr._state, g("verdict", ""), g("reason", ""),
                                         note=g("note", ""))
        finally:
            ctx.state["manager"]["_state"] = mgr._state
    raise KeyError(func)


def run_case(func, inp):
    ctx = CaseCtx(inp)
    CURRENT["ctx"] = ctx
    install_stubs(ctx, inp)
    out = {"ret": None, "trace": ctx.trace}
    try:
        out["ret"] = drive(func, inp, ctx)
    except BaseException as e:
        out["ret"] = {"__raise__": exc_spec(e)}
    out["ret"] = proj(out["ret"])
    out["state"] = {k: proj(ctx.state[k]) for k in (inp.get("state") or {}) if k in ctx.state}
    out["saves"] = ctx.saves
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
        ok = (canon(got.get("trace")) == canon(want.get("trace"))
              and canon(got.get("ret")) == canon(want.get("ret"))
              and canon(got.get("state")) == canon(want.get("state"))
              and canon(got.get("saves")) == canon(want.get("saves")))
        if not ok:
            mism += 1
            print(f"MISMATCH {func}")
            print(f"  C  ={json.dumps(got, sort_keys=True, default=str)}")
            print(f"  PY ={json.dumps(want, sort_keys=True, default=str)}")
    print(f"ORACLE goals_flow: {cases} cases, {mism} mismatches")
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())