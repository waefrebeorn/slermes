#!/usr/bin/env python3
"""Oracle for the local execution-environment FLOW cores
(src/tools/environments/local_env_flow.c).

Drives the LIVE tools/environments/local.py functions/methods with SCRIPTED
seams: the targets are AST-extracted from the quarry (no heavy import), module
constants are seeded, and every os.path probe / signal / process poll/wait/kill /
psutil snapshot / clock read / shell discovery / policy read / foreign env
mutator is replaced by a recording stub returning the same scripted results the
C harness feeds its universal seam. Compares the DECISION SEQUENCE (ordered
{ev,args,ret} trace) plus the return value, propagated exception and mutated
env/result state.

Input: stdin JSON lines from t_port_local_env_flow:
    {"func": ..., "in": {...}, "out": {"ret","exc","trace",<state>}}
Summary: ORACLE local_env_flow: N cases, M mismatches. Exit 1 on any mismatch,
and fail-closed (exit 1) when zero cases were compared.
"""
import ast
import builtins
import copy
import json
import os
import re
import shlex
import signal
import subprocess
import sys
import threading
import time
import types
import contextlib
import __future__
from collections.abc import Mapping
from pathlib import Path

for _stream in (sys.stdout, sys.stdin):
    try:
        if hasattr(_stream, "reconfigure"):
            _stream.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

QUARRY = r"C:\Users\eman5\hermes-agent"
ANN_FLAG = __future__.annotations.compiler_flag

CUR = {"func": None, "in": {}, "trace": [], "counts": {}, "state_out": {}}
LIVE = {}


def rec(ev):
    CUR["trace"].append(ev)


def drop_none(d):
    return {k: v for k, v in d.items() if v is not None}


def is_raise(v):
    return isinstance(v, dict) and "__raise__" in v


def mk_exc(kind, msg):
    if kind == "TimeoutExpired":
        # subprocess.TimeoutExpired is not a builtin; always suppressed in the
        # ported flows, so the exact cmd/timeout text never reaches the envelope.
        return subprocess.TimeoutExpired("proc", 2.0)
    cls = getattr(builtins, kind, None)
    if not (isinstance(cls, type) and issubclass(cls, BaseException)):
        cls = type(kind, (Exception,), {})
    return cls(msg)


def consume(name):
    """Script for a seam: single value reused, bare array consumed per call,
    or {"__script__":true,"ret":X} constant (list) result."""
    io = CUR["in"].get("io", {}) or {}
    script = io.get(name)
    if isinstance(script, dict) and script.get("__script__") is True:
        return script.get("ret")
    if isinstance(script, list):
        i = CUR["counts"].get(name, 0)
        CUR["counts"][name] = i + 1
        if not script:
            return None
        return script[min(i, len(script) - 1)]
    return script


def record(name, args, script):
    rec({"ev": name, "args": args, "ret": script})


def maybe_raise(script):
    if is_raise(script):
        raise mk_exc(script["__raise__"], script.get("msg", ""))


# ── AST extraction ──
def extract_consts(rel, ns):
    p = os.path.join(QUARRY, rel)
    src = open(p, encoding="utf-8").read()
    tree = ast.parse(src)
    for node in tree.body:
        tg = []
        if isinstance(node, ast.Assign):
            tg = [t.id for t in node.targets if isinstance(t, ast.Name)]
        elif isinstance(node, ast.AnnAssign) and node.value and isinstance(node.target, ast.Name):
            tg = [node.target.id]
        if tg:
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), p, "exec", ANN_FLAG), ns)
            except Exception:
                pass
    return tree, src, p


def extract_const(rel, name):
    p = os.path.join(QUARRY, rel)
    src = open(p, encoding="utf-8").read()
    for node in ast.parse(src).body:
        if isinstance(node, ast.Assign):
            for t in node.targets:
                if isinstance(t, ast.Name) and t.id == name:
                    ns = {}
                    exec(compile(ast.Module(body=[node], type_ignores=[]), p, "exec", ANN_FLAG), ns)
                    return ns[name]
    raise RuntimeError("const not found: %s in %s" % (name, rel))


def extract_funcs(tree, p, wanted, ns):
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in wanted:
            mm = copy.copy(node)
            mm.decorator_list = []
            exec(compile(ast.Module(body=[mm], type_ignores=[]), p, "exec", ANN_FLAG), ns)


class SuperRewrite(ast.NodeTransformer):
    """super().<name>(...) -> _base_<name>(...) so an extracted method bound to a
    SimpleNamespace double does not need the __class__ cell."""
    def visit_Call(self, node):
        self.generic_visit(node)
        if (isinstance(node.func, ast.Attribute)
                and isinstance(node.func.value, ast.Call)
                and isinstance(node.func.value.func, ast.Name)
                and node.func.value.func.id == "super"
                and not node.func.value.args):
            fn = ast.copy_location(ast.Name(id="_base_" + node.func.attr.lstrip("_"), ctx=ast.Load()), node)
            return ast.copy_location(ast.Call(func=fn, args=node.args, keywords=node.keywords), node)
        return node


def extract_methods(tree, p, class_name, wanted, ns):
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == class_name:
            for m in node.body:
                if isinstance(m, (ast.FunctionDef, ast.AsyncFunctionDef)) and m.name in wanted:
                    mm = copy.copy(m)
                    mm.decorator_list = []
                    mm = SuperRewrite().visit(mm)
                    ast.fix_missing_locations(mm)
                    exec(compile(ast.Module(body=[mm], type_ignores=[]), p, "exec", ANN_FLAG), ns)


MODULE_TARGETS = [
    "_prepend_missing_path_entries", "_append_dropped_path_entries",
    "_compute_git_bash_bin_dirs", "_user_tools_first", "_terminal_env",
    "_apply_profile_home", "_carry_bridge_markers", "_restore_bootstrap_tokens",
    "_leader_is_ours", "_wait_for_group_exit", "_sweep_escaped_descendants",
    "_kill_known_pids", "_kill_process_group_posix", "_kill_process_windows",
]
METHOD_TARGETS = ["_usable_cwd", "_parse_cwd_marker", "_force_kill_process"]


def build_ns():
    mod = types.ModuleType("local_env_flow_extracted")
    sys.modules["local_env_flow_extracted"] = mod
    ns = mod.__dict__
    ns.update({"__name__": "local_env_flow_extracted", "json": json, "os": os,
               "re": re, "shlex": shlex, "sys": sys, "platform": __import__("platform"),
               "threading": threading, "time": time, "signal": signal,
               "subprocess": subprocess, "contextlib": contextlib,
               "Path": Path, "Mapping": Mapping})
    tree, src, p = extract_consts("tools/environments/local.py", ns)
    extract_funcs(tree, p, set(MODULE_TARGETS), ns)
    extract_methods(tree, p, "LocalEnvironment", set(METHOD_TARGETS), ns)
    for nm in MODULE_TARGETS + METHOD_TARGETS:
        if nm not in ns:
            raise RuntimeError("extraction missing %s" % nm)
        LIVE[nm] = ns[nm]
    ns["BRIDGED_ENV_MARKER_PREFIX"] = extract_const("gateway/bridged_env.py", "BRIDGED_ENV_MARKER_PREFIX")
    return ns


def fake_module(name, **attrs):
    parts = name.split(".")
    for i in range(1, len(parts) + 1):
        pn = ".".join(parts[:i])
        if pn not in sys.modules or sys.modules[pn] is None:
            sys.modules[pn] = types.ModuleType(pn)
    m = sys.modules[name]
    for k, v in attrs.items():
        setattr(m, k, v)
    return m


# ── doubles ──
class ProcDouble:
    def __init__(self, d):
        for k, v in (d or {}).items():
            setattr(self, k, v)

    def poll(self):
        sc = consume("proc_poll")
        record("proc_poll", drop_none({"pid": getattr(self, "pid", None)}), sc)
        maybe_raise(sc)
        return sc

    def wait(self, timeout=None):
        sc = consume("proc_wait")
        record("proc_wait", drop_none({"pid": getattr(self, "pid", None), "timeout": timeout}), sc)
        maybe_raise(sc)
        return sc

    def kill(self):
        sc = consume("proc_kill")
        record("proc_kill", drop_none({"pid": getattr(self, "pid", None)}), sc)
        maybe_raise(sc)
        return sc


class ChildDouble:
    def __init__(self, d):
        self.pid = (d or {}).get("pid")

    def is_running(self):
        sc = consume("child_is_running")
        record("child_is_running", drop_none({"pid": self.pid}), sc)
        maybe_raise(sc)
        return bool(sc)

    def kill(self):
        sc = consume("child_kill")
        record("child_kill", drop_none({"pid": self.pid}), sc)
        maybe_raise(sc)
        return sc


class PsutilProcessDouble:
    def __init__(self, pid):
        self.pid = pid

    def children(self, recursive=False):
        sc = consume("psutil_children")
        record("psutil_children", drop_none({"pid": self.pid}), sc)
        maybe_raise(sc)
        return [ChildDouble(d) for d in (sc or [])]


PSUTIL_MOD = types.ModuleType("psutil")
PSUTIL_MOD.Process = PsutilProcessDouble


def mk_os(inp):
    def dirname(path):
        sc = consume("dirname"); record("dirname", {"path": path}, sc); maybe_raise(sc); return sc

    def basename(path):
        sc = consume("basename"); record("basename", {"path": path}, sc); maybe_raise(sc); return sc

    def join(*parts):
        sc = consume("path_join"); record("path_join", {"parts": list(parts)}, sc); maybe_raise(sc); return sc

    def isdir(path):
        sc = consume("isdir"); record("isdir", {"path": path}, sc); maybe_raise(sc); return bool(sc)

    def isabs(path):
        sc = consume("isabs"); record("isabs", {"path": path}, sc); maybe_raise(sc); return bool(sc)

    def expanduser(path):
        sc = consume("expanduser"); record("expanduser", {"path": path}, sc); maybe_raise(sc); return sc

    def normcase(path):
        sc = consume("normcase"); record("normcase", {"path": path}, sc); maybe_raise(sc); return sc

    def getpgid(pid):
        sc = consume("getpgid"); record("getpgid", drop_none({"pid": pid}), sc); maybe_raise(sc); return sc

    def getpgrp():
        sc = consume("getpgrp"); record("getpgrp", {}, sc); maybe_raise(sc); return sc

    def killpg(pgid, sig):
        sc = consume("killpg"); record("killpg", drop_none({"pgid": pgid, "sig": sig}), sc); maybe_raise(sc); return sc

    return types.SimpleNamespace(
        path=types.SimpleNamespace(dirname=dirname, basename=basename, join=join,
                                   isdir=isdir, isabs=isabs, expanduser=expanduser,
                                   normcase=normcase),
        getpgid=getpgid, getpgrp=getpgrp, killpg=killpg,
        pathsep=inp.get("pathsep", ":"))


def mk_time():
    def monotonic():
        sc = consume("monotonic"); record("monotonic", {}, sc); maybe_raise(sc); return sc

    def sleep(seconds):
        sc = consume("sleep"); record("sleep", {"seconds": seconds}, sc); maybe_raise(sc); return sc

    return types.SimpleNamespace(monotonic=monotonic, sleep=sleep)


# ── claimed / foreign module-global recorders ──
def rec_find_bash():
    sc = consume("find_bash"); record("find_bash", {}, sc); maybe_raise(sc); return sc


def rec_git_bash_bin_dirs():
    sc = consume("git_bash_bin_dirs"); record("git_bash_bin_dirs", {}, sc); maybe_raise(sc); return sc


def rec_path_env_key(run_env):
    sc = consume("path_env_key"); record("path_env_key", {"env": dict(run_env)}, sc); maybe_raise(sc); return sc


def rec_msys(cwd):
    sc = consume("msys_to_windows_path"); record("msys_to_windows_path", drop_none({"cwd": cwd}), sc); maybe_raise(sc); return sc


def rec_resolve_safe_cwd(cwd):
    sc = consume("resolve_safe_cwd"); record("resolve_safe_cwd", drop_none({"cwd": cwd}), sc); maybe_raise(sc); return sc


def rec_bridged_digest(value):
    sc = consume("bridged_env_digest"); record("bridged_env_digest", {"value": value}, sc); maybe_raise(sc); return sc


def rec_bootstrap():
    sc = consume("secret_source_bootstrap_env"); record("secret_source_bootstrap_env", {}, sc); maybe_raise(sc); return sc


def rec_base_parse(result):
    sc = consume("base_parse_cwd_marker"); record("base_parse_cwd_marker", {"result": dict(result)}, sc); maybe_raise(sc)
    if isinstance(sc, dict):
        result.update(sc)


# ── gateway.status / hermes_constants / pm.user_facing recorders ──
def rec_gpst(pid):
    sc = consume("get_process_start_time"); record("get_process_start_time", drop_none({"pid": pid}), sc); maybe_raise(sc); return sc


def rec_fpmatch(expected, current):
    sc = consume("fingerprints_match"); record("fingerprints_match", drop_none({"expected": expected, "current": current}), sc); maybe_raise(sc); return bool(sc)


def rec_terminate(pid, force=False, expected_start_time=None):
    sc = consume("terminate_pid"); record("terminate_pid", drop_none({"pid": pid, "force": force, "expected_start_time": expected_start_time}), sc); maybe_raise(sc); return sc


def rec_home_override():
    sc = consume("get_hermes_home_override"); record("get_hermes_home_override", {}, sc); maybe_raise(sc); return sc


def rec_apply_home(env):
    sc = consume("apply_subprocess_home_env"); record("apply_subprocess_home_env", {"env": dict(env)}, sc); maybe_raise(sc)
    if isinstance(sc, dict):
        env.update(sc)


def rec_user_facing(path):
    sc = consume("user_facing_path"); record("user_facing_path", {"path": path}, sc); maybe_raise(sc); return sc


def install_stubs(ns, inp):
    ns["_IS_WINDOWS"] = inp.get("is_windows", False)
    ns["os"] = mk_os(inp)
    ns["time"] = mk_time()
    ns["signal"] = types.SimpleNamespace(SIGTERM=15, SIGKILL=9)
    ns["subprocess"] = subprocess
    ns["contextlib"] = contextlib
    ns["shlex"] = shlex
    ns["_find_bash"] = rec_find_bash
    ns["_git_bash_bin_dirs"] = rec_git_bash_bin_dirs
    ns["_path_env_key"] = rec_path_env_key
    ns["_msys_to_windows_path"] = rec_msys
    ns["_resolve_safe_cwd"] = rec_resolve_safe_cwd
    ns["bridged_env_digest"] = rec_bridged_digest
    ns["_secret_source_bootstrap_env"] = rec_bootstrap
    ns["_base_parse_cwd_marker"] = rec_base_parse
    gs = fake_module("gateway.status", get_process_start_time=rec_gpst,
                     start_time_fingerprints_match=rec_fpmatch, terminate_pid=rec_terminate)
    if inp.get("status_import_fail"):
        for a in ("get_process_start_time", "terminate_pid"):
            if hasattr(gs, a):
                delattr(gs, a)
    fake_module("hermes_constants", get_hermes_home_override=rec_home_override,
                apply_subprocess_home_env=rec_apply_home)
    fake_module("pm.user_facing", user_facing_path=rec_user_facing)
    sys.modules["psutil"] = None if inp.get("psutil_import_fail") else PSUTIL_MOD


def mk_self(inp):
    self = types.SimpleNamespace()
    self.cwd = inp.get("self_cwd", "")

    def kill_process(proc):
        sc = consume("kill_process")
        record("kill_process", drop_none({"pid": getattr(proc, "pid", None)}), sc)
        maybe_raise(sc)
        return sc

    self._kill_process = kill_process
    self._usable_cwd = types.MethodType(LIVE["_usable_cwd"], self)
    self._parse_cwd_marker = types.MethodType(LIVE["_parse_cwd_marker"], self)
    self._force_kill_process = types.MethodType(LIVE["_force_kill_process"], self)
    return self


# ── drive the LIVE target for one case ──
def drive(ns, func, inp):
    CUR["state_out"] = {}
    if func == "prepend_missing_path_entries":
        r = ns["_prepend_missing_path_entries"](inp.get("existing_path", ""), inp.get("dirs", []))
        return {"ret": r}
    if func == "append_dropped_path_entries":
        r = ns["_append_dropped_path_entries"](inp.get("cmd_string", ""), inp.get("parent_path", ""))
        return {"ret": r}
    if func == "compute_git_bash_bin_dirs":
        r = ns["_compute_git_bash_bin_dirs"]()
        return {"ret": r}
    if func == "user_tools_first":
        r = ns["_user_tools_first"](inp.get("existing_path", ""))
        return {"ret": r}
    if func == "terminal_env":
        env = dict(inp.get("env", {}))
        CUR["state_out"]["env"] = env
        r = ns["_terminal_env"](env)
        return {"ret": r, "env": env}
    if func == "apply_profile_home":
        env = dict(inp.get("env", {}))
        CUR["state_out"]["env"] = env
        ns["_apply_profile_home"](env)
        return {"ret": None, "env": env}
    if func == "carry_bridge_markers":
        env = dict(inp.get("env", {}))
        CUR["state_out"]["env"] = env
        ns["_carry_bridge_markers"](env, inp.get("source", {}), own_profile=inp.get("own_profile", False))
        return {"ret": None, "env": env}
    if func == "restore_bootstrap_tokens":
        env = dict(inp.get("env", {}))
        CUR["state_out"]["env"] = env
        ns["_restore_bootstrap_tokens"](env, inp.get("source", {}))
        return {"ret": None, "env": env}
    if func == "usable_cwd":
        self = mk_self(inp)
        r = self._usable_cwd(inp.get("cwd", ""))
        return {"ret": [r[0], r[1]]}
    if func == "parse_cwd_marker":
        self = mk_self(inp)
        result = dict(inp.get("result", {}))
        CUR["state_out"]["result"] = result
        self._parse_cwd_marker(result)
        return {"ret": None, "result": result}
    if func == "leader_is_ours":
        r = ns["_leader_is_ours"](inp.get("pgid"), inp.get("expected_start"))
        return {"ret": r}
    if func == "wait_for_group_exit":
        proc = ProcDouble(inp.get("proc", {}))
        r = ns["_wait_for_group_exit"](proc, inp.get("pgid"), inp.get("timeout", 1.0))
        return {"ret": r}
    if func == "sweep_escaped_descendants":
        desc = [ChildDouble(d) for d in inp.get("descendants", [])]
        ns["_sweep_escaped_descendants"](desc, inp.get("pgid"))
        return {"ret": None}
    if func == "kill_known_pids":
        proc = ProcDouble(inp.get("proc", {}))
        desc = [ChildDouble(d) for d in inp.get("descendants", [])]
        ns["_kill_known_pids"](proc, desc)
        return {"ret": None}
    if func == "kill_process_group_posix":
        proc = ProcDouble(inp.get("proc", {}))
        ns["_kill_process_group_posix"](proc)
        return {"ret": None}
    if func == "kill_process_windows":
        proc = ProcDouble(inp.get("proc", {}))
        ns["_kill_process_windows"](proc)
        return {"ret": None}
    if func == "force_kill_process":
        self = mk_self(inp)
        proc = ProcDouble(inp.get("proc", {}))
        self._force_kill_process(proc)
        return {"ret": None}
    raise ValueError("unknown func: %s" % func)


def run_case(ns, func, inp):
    CUR["func"] = func
    CUR["in"] = inp
    CUR["trace"] = []
    CUR["counts"] = {}
    CUR["state_out"] = {}
    try:
        out = drive(ns, func, inp)
        out["exc"] = None
    except BaseException as e:
        out = {"ret": None, "exc": {"__raise__": type(e).__name__, "msg": str(e)}}
        out.update(CUR["state_out"])
    out["trace"] = CUR["trace"]
    return out


# ── comparison ──
def canon(x):
    if isinstance(x, dict):
        return ("obj", sorted(((k, canon(v)) for k, v in x.items()), key=lambda kv: kv[0]))
    if isinstance(x, (list, tuple)):
        return ("arr", [canon(v) for v in x])
    if isinstance(x, bool):
        return ("bool", x)
    if isinstance(x, (int, float)):
        return ("num", float(x))
    if x is None:
        return ("null",)
    return ("str", x)


def compare(want, got):
    if canon(want.get("trace", [])) != canon(got.get("trace", [])):
        return False
    for k in set(want) | set(got):
        if k == "trace":
            continue
        if canon(want.get(k)) != canon(got.get(k)):
            return False
    return True


def main():
    ns = build_ns()
    cases = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            record_in = json.loads(line)
        except json.JSONDecodeError:
            continue
        func = record_in.get("func")
        inp = record_in.get("in") or {}
        got = record_in.get("out") or {}
        cases += 1
        install_stubs(ns, inp)
        try:
            want = run_case(ns, func, inp)
        except BaseException as e:
            mism += 1
            print("PYERR %s: %r" % (func, e))
            continue
        if not compare(want, got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("ORACLE local_env_flow: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())