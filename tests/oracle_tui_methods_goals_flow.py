#!/usr/bin/env python3
"""Oracle for the TUI /goal command methods FLOW port
(src/gateway/tui_methods_goals_flow.c).

MODULE RESOLUTION: the task named tui_gateway/methods_goals.py, which does NOT
exist; the TUI goal-command methods live in hermes_cli/goal_command.py (the
/goal semantics dispatch_goal_command that methods_tools.py:_cmd_goal calls),
plus the UNCLAIMED gate/contract manager leaves of hermes_cli/goals.py
(clear / add_gate / remove_gate / clear_gates / render_gates / render_contract /
wait_on_session / _check_gates).

Drives the LIVE goal_command bodies + the live unclaimed goals.py leaves with
SCRIPTED seams: every claimed GoalManager sibling (pause / resume / set /
status_line / has_goal / stop_waiting / wait_on / next_continuation_prompt /
_save / _require_goal / _require_active / _pop_item / _clear_items / _park /
_pause_decision), the claimed module helpers (parse_contract / draft_contract /
goal_kick_prompt / _gate_workspace / run_gate / _decision), the claimed
state/contract methods (has_contract / is_empty / render_block) and every
callback arg (render / progress / authorize_gate) is a recording stub fed the
same scripts the C harness feeds its universal seam. Compares the DECISION
SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
return ("ret") and the mutated state mirror ("state": the mgr carrier).

Live defs are AST-extracted from goal_command.py (the GoalCommandResult frozen
dataclass exec'd WITH @dataclass; the handler-table Assigns exec'd AFTER the
defs) and from goals.py (the GoalGate dataclass + the DEFAULT_GATE_* /
CONTINUATION_PROMPT_GATE_FAILED_TEMPLATE constants + the 8 ported GoalManager
methods, bound onto the mgr double with types.MethodType). The heavy quarry
import chain is never imported.

Fail-closed: missing defs or 0 cases => exit 2; a missing/exhausted seam script
is a recorded RuntimeError raise on BOTH sides. Per-case wrapper catches
BaseException (SystemExit included). Summary:
'ORACLE tui_methods_goals_flow: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
  BaseException-only kinds pass through every except-Exception / except-(RVI)
  site.
- GoalCommandResult projects to its field dict in dataclass order:
  {output, prompt, kickoff, clear_pending, error}. GoalGate to {command,
  timeout_seconds, max_retries, attempts, last_exit_code, last_output_tail}.
- The mgr carrier is state.mgr {"session_id", "_state"}; the GoalManager.state
  property is TRANSPARENT (reads _state; no seam). State/contract/gate doubles
  are dict carriers; missing required fields raise the SimpleNamespace
  AttributeError, non-dict receivers the type-name message. Scripted state
  results for TRUTHINESS sites (_pause) are restricted to {null, non-empty
  record}; `is None` sites (_resume) accept {}.
- Tuples project to lists; integral floats to ints. Seam args drop None-valued
  keys. List-valued seam RESULTS are wrapped one level deeper (bare array ==
  queue): parse_contract / gate_workspace / run_gate.
- status_line / render / render_block / goal_kick_prompt scripts stay in their
  natural str domain. ASCII corpus (the live output templates carry their
  Unicode symbols verbatim on both sides).
"""
import ast
import asyncio
import copy
import json
import os
import re
import sys
import textwrap
import types
import __future__ as _fut
from dataclasses import dataclass, replace, asdict, field
from pathlib import Path
from typing import Optional, Any, Callable

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
SRC_GC = os.path.join(Q, "hermes_cli", "goal_command.py")
SRC_G = os.path.join(Q, "hermes_cli", "goals.py")

_FLAGS = _fut.annotations.compiler_flag


def seg(src, node):
    start = node.lineno
    for d in getattr(node, "decorator_list", []):
        start = min(start, d.lineno)
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    text = textwrap.dedent(seg(src, node))
    exec(compile(text, path, "exec", flags=_FLAGS, dont_inherit=True), ns)


def new_module(name):
    mod = types.ModuleType(name)
    sys.modules[name] = mod
    return mod


PORTED_GC = [
    "_english", "_status", "_show", "_pause", "_resume", "_clear", "_unwait",
    "_wait", "_gate_add", "_gate_remove", "_gate_clear", "_bare_control",
    "_gate", "_set", "is_goal_control", "dispatch_goal_command",
]
GOALS_PORTED = [
    "clear", "add_gate", "remove_gate", "clear_gates", "render_gates",
    "render_contract", "wait_on_session", "_check_gates",
]
PORTED = PORTED_GC + GOALS_PORTED


def extract():
    mod = new_module("tmg_oracle_goal_command")
    ns = mod.__dict__
    ns.update({"dataclass": dataclass, "replace": replace, "asdict": asdict,
               "field": field, "json": json, "types": types})
    src = open(SRC_GC, encoding="utf-8").read()
    tree = ast.parse(src)
    # the frozen GoalCommandResult dataclass (decorator kept)
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "GoalCommandResult":
            exec_node(src, node, ns, SRC_GC)
    # function defs in source order (no decorators in this module)
    for node in tree.body:
        if isinstance(node, ast.FunctionDef):
            exec_node(src, node, ns, SRC_GC)
    # the handler tables AFTER the defs (they reference the functions)
    for node in tree.body:
        if isinstance(node, ast.Assign) and all(
                isinstance(t, ast.Name) and t.id in (
                    "_GATE_HANDLERS", "_EXACT_HANDLERS",
                    "_BARE_CONTROL_VERBS", "_CONTROL_FIRST_WORDS")
                for t in node.targets):
            exec_node(src, node, ns, SRC_GC)
    required = PORTED_GC + ["GoalCommandResult", "_GATE_HANDLERS",
                            "_EXACT_HANDLERS", "_BARE_CONTROL_VERBS",
                            "_CONTROL_FIRST_WORDS"]
    missing = [k for k in required if k not in ns]
    if missing:
        print(f"FATAL oracle_tui_methods_goals_flow: goal_command missing: {missing}")
        sys.exit(2)

    modg = new_module("tmg_oracle_goals")
    g = modg.__dict__
    g.update({"dataclass": dataclass, "field": field, "asdict": asdict,
              "json": json, "types": types, "Optional": Optional, "Any": Any,
              "Callable": Callable})
    gsrc = open(SRC_G, encoding="utf-8").read()
    gtree = ast.parse(gsrc)
    # the constants the ported leaves reference
    for node in gtree.body:
        if isinstance(node, ast.Assign) and any(
                isinstance(t, ast.Name) and t.id in (
                    "DEFAULT_GATE_TIMEOUT_SECONDS", "DEFAULT_GATE_MAX_RETRIES",
                    "DEFAULT_MAX_TURNS", "CONTINUATION_PROMPT_GATE_FAILED_TEMPLATE")
                for t in node.targets):
            exec_node(gsrc, node, g, SRC_G)
    # the GoalGate dataclass (add_gate constructs it)
    for node in gtree.body:
        if isinstance(node, ast.ClassDef) and node.name == "GoalGate":
            exec_node(gsrc, node, g, SRC_G)
    # the 8 UNCLAIMED GoalManager methods (dedented out of the class body)
    for node in gtree.body:
        if isinstance(node, ast.ClassDef) and node.name == "GoalManager":
            for sub in node.body:
                if isinstance(sub, ast.FunctionDef) and sub.name in GOALS_PORTED:
                    exec_node(gsrc, sub, g, SRC_G)
    greq = GOALS_PORTED + ["GoalGate", "DEFAULT_GATE_TIMEOUT_SECONDS",
                           "DEFAULT_GATE_MAX_RETRIES",
                           "CONTINUATION_PROMPT_GATE_FAILED_TEMPLATE"]
    gmissing = [k for k in greq if k not in g]
    if gmissing:
        print(f"FATAL oracle_tui_methods_goals_flow: goals missing: {gmissing}")
        sys.exit(2)
    return ns, g


NS_GC, NS_G = extract()
REAL_GC = {nm: NS_GC[nm] for nm in PORTED_GC}
REAL_G = {nm: NS_G[nm] for nm in GOALS_PORTED}


# -- exception materialization / projection --
EXC_CLASSES = {
    "ValueError": ValueError, "TypeError": TypeError, "RuntimeError": RuntimeError,
    "KeyError": KeyError, "AttributeError": AttributeError, "ImportError": ImportError,
    "PermissionError": PermissionError, "OSError": OSError, "IndexError": IndexError,
    "TimeoutError": TimeoutError, "Exception": Exception, "SystemExit": SystemExit,
    "KeyboardInterrupt": KeyboardInterrupt, "GeneratorExit": GeneratorExit,
    "CancelledError": asyncio.CancelledError, "StopIteration": StopIteration,
    "AssertionError": AssertionError, "NameError": NameError,
    "NotImplementedError": NotImplementedError, "RecursionError": RecursionError,
    "UnicodeDecodeError": UnicodeDecodeError, "ModuleNotFoundError": ModuleNotFoundError,
    "ConnectionResetError": ConnectionResetError, "FileNotFoundError": FileNotFoundError,
    "OverflowError": OverflowError, "ZeroDivisionError": ZeroDivisionError,
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    if kind == "KeyError":
        cls = type("KeyError", (KeyError,),
                   {"__str__": lambda self: self.args[0] if self.args else ""})
    return cls(spec.get("message", ""))


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# -- canonical projection (compare parsed JSON, never raw strings) --
def canon(v):
    def conv(x):
        if isinstance(x, BaseException):
            return proj_exc(x)
        if isinstance(x, NS_GC["GoalCommandResult"]):
            return {f: conv(getattr(x, f)) for f in
                    ("output", "prompt", "kickoff", "clear_pending", "error")}
        if isinstance(x, NS_G["GoalGate"]):
            return {f: conv(getattr(x, f)) for f in
                    ("command", "timeout_seconds", "max_retries", "attempts",
                     "last_exit_code", "last_output_tail")}
        if isinstance(x, MgrDouble):
            d = object.__getattribute__(x, "_d")
            return {k: conv(val) for k, val in d.items() if not callable(val)}
        if isinstance(x, types.SimpleNamespace):
            return {k: conv(val) for k, val in vars(x).items()
                    if not callable(val) and not isinstance(val, types.MethodType)}
        if isinstance(x, Path):
            return x.as_posix()
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, (set, frozenset)):
            return sorted(conv(i) for i in x)
        if isinstance(x, float) and x.is_integer():
            return int(x)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


# -- per-case script backend (mirrors the C tmg_call exactly) --
_MISSING = object()


class Ctx:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.trace = []

    def _record_raise(self, name, args, exc):
        self.trace.append({"name": name, "args": args, "ret": proj_exc(exc)})
        raise exc

    def call(self, name, args):
        args = {k: v for k, v in (args or {}).items() if v is not None}
        entry = self.io.get(name, _MISSING)
        used = entry
        if entry is _MISSING:
            self._record_raise(name, args, RuntimeError(f"no script for {name}"))
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                self._record_raise(name, args, RuntimeError(f"script exhausted for {name}"))
            self.cursors[name] = idx + 1
            used = entry[idx]
        if isinstance(used, dict) and used.get("__script__"):
            if isinstance(used.get("raise"), dict):
                self._record_raise(name, args, make_exc(used["raise"]))
            ret = used.get("ret")
        else:
            ret = used
        self.trace.append({"name": name, "args": canon(args), "ret": canon(ret)})
        return copy.deepcopy(ret) if isinstance(ret, (dict, list)) else ret


def rec(ctx, name, args_fn=None, build=None):
    def f(*a, **k):
        args = args_fn(*a, **k) if args_fn else {}
        raw = ctx.call(name, args)
        return build(raw) if build else raw
    return f


CURRENT = {}


# -- state / contract / gate doubles --
def contract_build(raw):
    if not isinstance(raw, dict):
        return raw
    ctx = CURRENT["ctx"]
    ns = types.SimpleNamespace(**raw)
    ns.is_empty = rec(ctx, "contract_is_empty", lambda: {})
    ns.render_block = rec(ctx, "contract_render_block", lambda: {})
    return ns


def state_build(raw):
    if not isinstance(raw, dict):
        return raw
    ctx = CURRENT["ctx"]
    ns = types.SimpleNamespace(**raw)
    ns.has_contract = rec(ctx, "state_has_contract", lambda: {})
    if isinstance(raw.get("contract"), dict):
        ns.contract = contract_build(raw["contract"])
    if isinstance(raw.get("gates"), list):
        ns.gates = [types.SimpleNamespace(**g) if isinstance(g, dict) else g
                    for g in raw["gates"]]
    return ns


def gate_build(raw):
    return types.SimpleNamespace(**raw) if isinstance(raw, dict) else raw


def pc_build(raw):
    if isinstance(raw, list) and len(raw) == 2:
        return [raw[0], contract_build(raw[1])]
    return raw


# claimed mgr-method seams: live name -> (seam name, args projector, build)
MGR_SEAMS = {
    "has_goal": ("has_goal", lambda: {}, None),
    "pause": ("pause", lambda reason="user-paused": {"reason": reason}, state_build),
    "resume": ("resume", lambda: {}, state_build),
    "set": ("set",
            lambda goal, *, max_turns=None, contract=None:
            {"goal": goal, "max_turns": max_turns, "contract": contract},
            state_build),
    "status_line": ("status_line", lambda: {}, None),
    "stop_waiting": ("stop_waiting", lambda: {}, None),
    "wait_on": ("wait_on", lambda pid, reason="": {"pid": pid, "reason": reason},
                state_build),
    "next_continuation_prompt": ("next_continuation_prompt", lambda: {}, None),
    "_save": ("save", lambda: {}, None),
    "_require_goal": ("require_goal", lambda: {}, state_build),
    "_require_active": ("require_active", lambda: {}, state_build),
    "_pop_item": ("pop_item",
                  lambda attr, index_1based: {"attr": attr, "index_1based": index_1based},
                  gate_build),
    "_clear_items": ("clear_items", lambda attr: {"attr": attr}, None),
    "_park": ("park", lambda reason, **barrier: {"reason": reason, **barrier},
              state_build),
    "_pause_decision": ("pause_decision",
                        lambda paused_reason, verdict, reason, message:
                        {"paused_reason": paused_reason, "verdict": verdict,
                         "reason": reason, "message": message}, None),
}


class MgrDouble:
    """Dict-backed GoalManager double. The state property is TRANSPARENT (the
    live property returns self._state; no seam). Claimed methods are seam
    recorders; the ported goals.py leaves bind live via MethodType. Methods
    live in a SEPARATE overlay (_m) so the backing dict (_d) stays pure data
    for the out-state echo."""
    def __init__(self, d, ctx):
        object.__setattr__(self, "_d", d)
        object.__setattr__(self, "_m", {})
        if isinstance(d.get("_state"), dict):
            d["_state"] = state_build(d["_state"])
        m = object.__getattribute__(self, "_m")
        for name, (seam, afn, build) in MGR_SEAMS.items():
            m[name] = rec(ctx, seam, afn, build=build)
        for name in GOALS_PORTED:
            m[name] = types.MethodType(NS_G[name], self)

    @property
    def state(self):
        return self._state

    def __getattr__(self, k):
        m = object.__getattribute__(self, "_m")
        if k in m:
            return m[k]
        d = object.__getattribute__(self, "_d")
        if k in d:
            return d[k]
        raise AttributeError(f"'types.SimpleNamespace' object has no attribute '{k}'")

    def __setattr__(self, k, v):
        object.__getattribute__(self, "_d")[k] = v


def install_stubs(ctx, args):
    CURRENT["ctx"] = ctx
    NS_GC["goals"] = types.SimpleNamespace(
        draft_contract=rec(ctx, "draft_contract",
                           lambda objective: {"objective": objective},
                           build=contract_build),
        parse_contract=rec(ctx, "parse_contract", lambda text: {"text": text},
                           build=pc_build),
        goal_kick_prompt=rec(ctx, "goal_kick_prompt",
                             lambda goal, last_user_message:
                             {"goal": goal, "last_user_message": last_user_message}))
    NS_GC["logger"] = types.SimpleNamespace(
        debug=rec(ctx, "logger_debug", lambda fmt, *a: {"msg": fmt % a if a else fmt}))
    NS_G["_gate_workspace"] = rec(ctx, "gate_workspace", lambda: {})
    NS_G["run_gate"] = rec(ctx, "run_gate", lambda gate, *, cwd=None:
                           {"gate": gate, "cwd": cwd})
    NS_G["_decision"] = rec(ctx, "decision",
                            lambda status, should_continue, prompt, verdict, reason, message:
                            {"status": status, "should_continue": should_continue,
                             "prompt": prompt, "verdict": verdict,
                             "reason": reason, "message": message})


def drive(func, inp):
    args = inp.get("args") or {}
    ctx = Ctx(inp.get("io"))
    state_in = copy.deepcopy(inp.get("state") or {})
    CURRENT["ctx"] = ctx
    for nm, real in REAL_GC.items():
        NS_GC[nm] = real
    for nm, real in REAL_G.items():
        NS_G[nm] = real
    mgr_raw = state_in.get("mgr")
    mgr = MgrDouble(mgr_raw, ctx) if isinstance(mgr_raw, dict) else mgr_raw
    install_stubs(ctx, args)
    render = rec(ctx, "render",
                 lambda key, default, **values: {"key": key, "default": default, **values})
    progress = rec(ctx, "progress", lambda message: {"message": message})
    authorize = rec(ctx, "authorize_gate", lambda: {})
    try:
        if func == "_english":
            ret = NS_GC["_english"](args.get("key"), args.get("default"),
                                    **(args.get("values") or {}))
        elif func in ("_status", "_show", "_pause", "_resume", "_clear", "_unwait"):
            ret = NS_GC[func](mgr, args.get("arg", ""), render)
        elif func in ("_wait", "_gate_add", "_gate_remove", "_gate_clear"):
            ret = NS_GC[func](mgr, args.get("arg"))
        elif func == "_bare_control":
            ret = NS_GC[func](mgr, args.get("verb"), args.get("rest", ""), render)
        elif func == "_gate":
            ret = NS_GC[func](mgr, args.get("arg"), authorize)
        elif func == "_set":
            ret = NS_GC[func](mgr, args.get("arg"), drafting=args.get("drafting", False),
                              last_user_message=args.get("last_user_message"),
                              render=render,
                              progress=progress if args.get("progress") else None)
        elif func == "is_goal_control":
            ret = NS_GC[func](args.get("arg"))
        elif func == "dispatch_goal_command":
            ret = NS_GC[func](mgr, args.get("arg"), authorize_gate=authorize,
                              last_user_message=args.get("last_user_message"),
                              render=render,
                              progress=progress if args.get("progress") else None)
        elif func == "clear":
            ret = NS_G["clear"](mgr)
        elif func == "add_gate":
            ret = NS_G["add_gate"](mgr, args.get("command"),
                                   timeout_seconds=args.get("timeout_seconds"),
                                   max_retries=args.get("max_retries"))
        elif func == "remove_gate":
            ret = NS_G["remove_gate"](mgr, args.get("index_1based"))
        elif func == "clear_gates":
            ret = NS_G["clear_gates"](mgr)
        elif func == "render_gates":
            ret = NS_G["render_gates"](mgr)
        elif func == "render_contract":
            ret = NS_G["render_contract"](mgr)
        elif func == "wait_on_session":
            ret = NS_G["wait_on_session"](mgr, args.get("session_id"),
                                          args.get("reason", ""))
        elif func == "_check_gates":
            ret = NS_G["_check_gates"](mgr)
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:  # SystemExit included
        ret = proj_exc(exc)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state_in)}


# -- three-way seam-name cross-check --
SEAM_NAMES = [
    "render", "progress", "authorize_gate", "status_line", "pause", "resume",
    "set", "has_goal", "stop_waiting", "wait_on", "next_continuation_prompt",
    "save", "require_goal", "require_active", "pop_item", "clear_items",
    "park", "pause_decision", "state_has_contract", "contract_is_empty",
    "contract_render_block", "draft_contract", "parse_contract",
    "goal_kick_prompt", "logger_debug", "gate_workspace", "run_gate",
    "decision",
]


def cross_check():
    csrc = open(_REPO / "src" / "gateway" / "tui_methods_goals_flow.c",
                encoding="utf-8").read()
    registry = set(SEAM_NAMES)
    missing_c = [n for n in SEAM_NAMES if f'"{n}"' not in csrc]
    if missing_c:
        print(f"FATAL oracle_tui_methods_goals_flow: seams absent from C port: {missing_c}")
        sys.exit(2)
    c_seams = set(re.findall(r'tmg_call\(ctx,\s*"([^"]+)"', csrc))
    unknown_c = c_seams - registry
    if unknown_c:
        print(f"FATAL oracle_tui_methods_goals_flow: C seams outside registry: {sorted(unknown_c)}")
        sys.exit(2)
    osrc = open(__file__, encoding="utf-8").read()
    used = set(re.findall(r'rec\(\s*ctx,\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'rec\(\s*\n\s*ctx,\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'ctx\.call\(\s*"([^"]+)"', osrc))
    unknown = used - registry
    if unknown:
        print(f"FATAL oracle_tui_methods_goals_flow: oracle seams outside registry: {sorted(unknown)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "tui_methods_goals_flow.jsonl"
    if fix.exists():
        fix_keys = set()
        fix_funcs = set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            case = json.loads(line)
            fix_funcs.add(case.get("func"))
            inp = case.get("in") or {}
            fix_keys |= set(inp.get("io") or {})
        bad = fix_keys - registry
        if bad:
            print(f"FATAL oracle_tui_methods_goals_flow: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        badf = fix_funcs - set(PORTED)
        if badf:
            print(f"FATAL oracle_tui_methods_goals_flow: fixture funcs outside PORTED: {sorted(badf)}")
            sys.exit(2)


def first_trace_diff(want, got):
    wt, gt = want.get("trace") or [], got.get("trace") or []
    for i in range(max(len(wt), len(gt))):
        w = wt[i] if i < len(wt) else None
        g = gt[i] if i < len(gt) else None
        if w != g:
            return i, w, g
    return None, None, None


def report_mismatch(func, want, got, shown, case_no=0):
    if shown >= 100:
        return
    print(f"MISMATCH {func} (case {case_no}):")
    if want.get("ret") != got.get("ret"):
        print(f"  ret want={json.dumps(want.get('ret'), ensure_ascii=False)[:2000]}")
        print(f"  ret got ={json.dumps(got.get('ret'), ensure_ascii=False)[:2000]}")
    i, w, g = first_trace_diff(want, got)
    if i is not None:
        print(f"  trace[{i}] want={json.dumps(w, ensure_ascii=False)[:2000]}")
        print(f"  trace[{i}] got ={json.dumps(g, ensure_ascii=False)[:2000]}")
    ws, gs = want.get("state") or {}, got.get("state") or {}
    if ws != gs:
        keys = [k for k in set(ws) | set(gs) if ws.get(k) != gs.get(k)]
        print(f"  state diff keys: {sorted(keys)[:10]}")
        for k in sorted(keys)[:3]:
            print(f"    {k} want={json.dumps(ws.get(k), ensure_ascii=False)[:1000]}")
            print(f"    {k} got ={json.dumps(gs.get(k), ensure_ascii=False)[:1000]}")


def main():
    cross_check()
    n = m = shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec_in = json.loads(line)
        except Exception as exc:
            m += 1
            print(f"MISMATCH <unparsable harness line>: {exc}")
            continue
        n += 1
        func = rec_in.get("func", "")
        inp = rec_in.get("in") or {}
        got = rec_in.get("out")
        if not isinstance(got, dict) or "error" in got:
            m += 1
            print(f"MISMATCH {func}: harness emitted no result: {got}")
            continue
        try:
            want = drive(func, inp)
        except BaseException as exc:
            m += 1
            print(f"MISMATCH {func}: oracle drive raised {type(exc).__name__}: {exc}")
            continue
        g = canon(got)
        if g != want:
            m += 1
            report_mismatch(func, want, g, shown, n)
            shown += 1
    print(f"ORACLE tui_methods_goals_flow: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    """Python-only drive() self-test over every fixture case (no C involved)."""
    fix = _REPO / "tests" / "fixtures" / "tui_methods_goals_flow.jsonl"
    n = bad = 0
    counts = {}
    ported_set = set(PORTED) | set(SEAM_NAMES)
    for line in open(fix, encoding="utf-8"):
        line = line.strip()
        if not line:
            continue
        case = json.loads(line)
        n += 1
        func = case["func"]
        counts[func] = counts.get(func, 0) + 1
        try:
            out = drive(func, case.get("in") or {})
        except BaseException as exc:
            bad += 1
            print(f"SELFTEST DRIVE RAISED {func} case {n}: {type(exc).__name__}: {exc}")
            continue
        r = out["ret"]
        if isinstance(r, dict) and r.get("__raise__") == "RuntimeError" and \
                str(r.get("message", "")).startswith(("no script", "script exhausted")):
            if not case.get("fail_closed"):
                bad += 1
                print(f"SELFTEST SCRIPT GAP {func} case {n}: {r['message']}")
        if isinstance(r, dict) and r.get("__raise__") == "AttributeError":
            msg = str(r.get("message", ""))
            builtin_types = ("'NoneType'", "'str'", "'int'", "'float'", "'bool'",
                             "'list'", "'dict'")
            if not msg.startswith(builtin_types) and \
                    any(f"'{nm}'" in msg for nm in ported_set):
                bad += 1
                print(f"SELFTEST ATTR FAMILY {func} case {n}: {msg}")
    thin = [f for f in PORTED if counts.get(f, 0) < 5]
    if thin:
        bad += 1
        print(f"SELFTEST THIN COVERAGE (<5 cases): {thin}")
    print(f"SELFTEST tui_methods_goals_flow: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()