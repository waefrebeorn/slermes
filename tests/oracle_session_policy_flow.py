#!/usr/bin/env python3
"""Oracle for the gateway/session_policy.py launch-policy FLOW cores
(session_policy_flow.c).

Drives the LIVE functions with SCRIPTED I/O: the targets (and the
LocalSessionPolicy dataclass + restore helpers they compose) are AST-extracted
from the quarry in source order, module constants are seeded, the real
AuthorityRefusal is extracted from persistence/core/errors.py, and the only live
I/O (forward-policy classification, the adoption warning log) is replaced by
recording stubs returning the same scripted results the C harness feeds its
seams. Compares the DECISION SEQUENCE (ordered seam-call trace) plus the return
value / named refusal.

Input: stdin JSON lines from t_port_session_policy_flow:
    {"func": ..., "in": {...}, "out": {"ret": ..., "trace": [...], ...}}
Summary: ORACLE session_policy_flow: N cases, M mismatches. Exit 1 on any
mismatch, and fail-closed (exit 1) when zero cases were compared.
"""
import ast
import json
import os
import sys
import types
from dataclasses import MISSING, dataclass, fields
from functools import cached_property
from pathlib import Path, PurePosixPath, PureWindowsPath

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "gateway", "session_policy.py")
ERRORS = os.path.join(QUARRY, "persistence", "core", "errors.py")

WANTED = {
    "session_pinned", "created_surface", "profile_target", "present_sections",
    "terminal_uses_host_cwd", "launch_skills", "_valid_client_env",
    "_stored_cwd_is_absolute", "restore_policy", "_stored_policy",
    "_adopt_stored_policy", "_check_stored_surface", "_policy_refused", "_refuse",
}

CUR = {"func": None, "in": {}, "trace": []}


def rec(ev):
    CUR["trace"].append(ev)


def cin(key, default=None):
    return CUR["in"].get(key, default)


class RecLogger:
    def _rec(self, level, fmt, args):
        rec({"ev": "log", "level": level, "fmt": fmt,
             "args": [str(a) if isinstance(a, BaseException) else a for a in args]})

    def debug(self, fmt, *a, **k):
        self._rec(10, fmt, a)

    def info(self, fmt, *a, **k):
        self._rec(20, fmt, a)

    def warning(self, fmt, *a, **k):
        self._rec(30, fmt, a)

    def error(self, fmt, *a, **k):
        self._rec(40, fmt, a)

    def log(self, level, fmt, *a, **k):
        self._rec(level, fmt, a)


def seed_errors(ns):
    """Extract the real PersistenceError / AuthorityRefusal / _one_line."""
    tree = ast.parse(open(ERRORS, encoding="utf-8").read())
    by_name = {}
    for node in tree.body:
        if isinstance(node, (ast.ClassDef, ast.FunctionDef)):
            by_name[node.name] = node
    for name in ("PersistenceError", "AuthorityRefusal", "_one_line"):
        exec(compile(ast.Module(body=[by_name[name]], type_ignores=[]), ERRORS, "exec"), ns)


def extract():
    src = open(SRC, encoding="utf-8").read()
    import contextlib
    import contextvars
    import logging
    ns = {
        "__name__": "gateway_session_policy_extracted",
        "json": json, "os": os, "Path": Path,
        "PurePosixPath": PurePosixPath, "PureWindowsPath": PureWindowsPath,
        "contextlib": contextlib, "contextvars": contextvars,
        "ContextVar": contextvars.ContextVar,
        "dataclass": dataclass, "fields": fields, "MISSING": MISSING,
        "cached_property": cached_property,
        "logging": types.SimpleNamespace(getLogger=lambda name: RecLogger()),
        "logger": RecLogger(),
    }
    seed_errors(ns)
    tree = ast.parse(src)
    # Single pass in SOURCE order: the LocalSessionPolicy class must be defined
    # before _POLICY_FIELDS / _REQUIRED_POLICY_FIELDS read its fields().
    for node in tree.body:
        if isinstance(node, ast.Assign):
            if not all(isinstance(t, ast.Name) for t in node.targets):
                continue
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            except Exception:
                pass
        elif isinstance(node, ast.AnnAssign) and node.value and isinstance(node.target, ast.Name):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            except Exception:
                pass
        elif isinstance(node, ast.FunctionDef) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
        elif isinstance(node, ast.ClassDef) and node.name == "LocalSessionPolicy":
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    return ns


def install_stubs(ns):
    ns["logger"] = RecLogger()

    def forward_stub(policy):
        r = cin("forward_policy", False)
        rec({"ev": "is_forward_policy", "ret": r})
        return r

    a2a = types.ModuleType("gateway.session_a2a")
    a2a.is_forward_policy = forward_stub
    gateway_pkg = types.ModuleType("gateway")
    gateway_pkg.session_a2a = a2a
    sys.modules["gateway"] = gateway_pkg
    sys.modules["gateway.session_a2a"] = a2a


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


def drive(ns, func, inp):
    AR = ns["AuthorityRefusal"]
    if func == "session_pinned":
        return {"ret": ns["session_pinned"](inp.get("config", {}), inp.get("field", ""),
                                            inp.get("launched"))}
    if func == "created_surface":
        return {"ret": ns["created_surface"](inp.get("label"))}
    if func == "profile_target":
        return {"ret": list(ns["profile_target"](inp.get("config", {})))}
    if func == "present_sections":
        return {"ret": ns["present_sections"](inp.get("config", {}))}
    if func == "terminal_uses_host_cwd":
        return {"ret": ns["terminal_uses_host_cwd"](inp.get("terminal", {}))}
    if func == "launch_skills":
        try:
            return {"ret": ns["launch_skills"](inp.get("params", {}))}
        except AR as e:
            return {"ret": None, "refusal": {"reason": e.reason, "field": e.field, "detail": e.detail}}
    if func == "valid_client_env":
        kind = list if inp.get("kind") == "list" else str
        return {"ret": ns["_valid_client_env"](inp.get("value"), kind)}
    if func == "stored_cwd_is_absolute":
        return {"ret": ns["_stored_cwd_is_absolute"](inp.get("cwd", ""))}
    if func == "restore_policy":
        ns["_ADOPTED_KEY_SETS"].clear()
        try:
            p = ns["restore_policy"](inp.get("data"))
            return {"ret": vars(p)}
        except AR as e:
            return {"ret": None, "refusal": {"reason": e.reason, "field": e.field, "detail": e.detail}}
    raise ValueError("unknown func: %s" % func)


def main():
    ns = extract()
    install_stubs(ns)
    cases = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            record = json.loads(line)
        except json.JSONDecodeError:
            continue
        func = record.get("func")
        inp = record.get("in") or {}
        got = record.get("out") or {}
        cases += 1
        CUR["func"] = func
        CUR["in"] = inp
        CUR["trace"] = []
        try:
            want = drive(ns, func, inp)
        except Exception as e:
            mism += 1
            print("PYERR %s: %r" % (func, e))
            continue
        want["trace"] = CUR["trace"]
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("ORACLE session_policy_flow: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())
