#!/usr/bin/env python3
"""Oracle for console_engine_flow.c — the console-engine FLOW cores + leaves of
hermes_cli/console_engine.py (LIVE authority) that NO existing PoP claims.

Drives the LIVE functions/methods/closures with SCRIPTED I/O: defs are
AST-extracted from the quarry (no heavy hermes_cli import), module constants
seeded, and every foreign collaborator (the _ArgumentParser ctor + methods,
importlib.import_module/getattr, the persistence_assembly / session_export /
persistence.core.errors / cron.jobs in-function imports, tools.ansi_strip.
strip_ansi, the run/namespace_update/func callbacks) plus the CLAIMED siblings
(_capture_output, _expect_no_args, _format_job, _summaries_from_parser,
_noop_console_command) replaced by recording monkeypatch stubs returning the
SAME scripted results the C harness feeds its universal seam. Compares the
ordered DECISION SEQUENCE + return.

Fail-closed: a missing/empty seam script returns a RuntimeError raise spec
(recorded as the event ret, then raised), matching the C harness. Uncaught
raises (incl. the BaseException trio) are caught into a ret spec; `except
Exception` (_surface_summaries) swallows plain Exceptions but passes
BaseException through; `except ExportTooLarge` / `except AmbiguousJobReference`
catch those kinds by IDENTITY and re-raise as ConsoleCommandError(str(exc)).

Input: stdin JSON lines from t_port_console_engine_flow:
    {"func":..., "in":{...}, "out":{"ret":..., "trace":[...]}}
Summary: ORACLE console_engine_flow: N cases, M mismatches. Fail-closed (exit 2)
when extraction misses defs or zero cases were compared.
"""
import ast
import json
import os
import sys
import types
import functools
import argparse
import contextlib
import difflib
import importlib as _importlib_real
import io
from dataclasses import dataclass
from pathlib import Path
from types import SimpleNamespace
from typing import Any, Callable, Iterable, Literal, NoReturn, Sequence

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "hermes_cli", "console_engine.py")

# func target -> extraction key
PY = {
    "paths": "_paths", "sub": "_sub", "reg": "_reg", "send_surface": "_SEND_SURFACE",
    "shell_tokens": "_SHELL_TOKENS", "confirmed_kinds": "_CONFIRMED_KINDS",
    "blocked_top": "_BLOCKED_TOP", "blocked_pairs": "_BLOCKED_PAIRS",
    "drop_trailing_blank": "_drop_trailing_blank", "parse": "_parse",
    "captured": "_captured", "captured_wrapper": "wrapper",
    "simple_command": "_simple_command", "simple_command_handler": "handler",
    "doctor": "_doctor", "build_main_handler": "__mh__", "surface_build": "build",
    "surface_summaries": "_surface_summaries", "dispatch_lambda": "__dispatch_lam__",
    "dispatch": "_dispatch", "session_db": "_session_db", "guard_exports": "_guard_exports",
    "cron_job_action": "_cron_job_action",
}
WANTED_FUNCS = {"_drop_trailing_blank", "_surface_summaries", "_dispatch", "_paths", "_sub",
                "_reg", "_parse", "_captured", "_simple_command", "_session_db",
                "_guard_exports", "_cron_job_action"}
REAL_KEYS = set(PY.values())

CUR = {"scripts": {}, "idx": {}, "trace": []}
COMPILED = {}


# ── exceptions ──
class ExportTooLarge(ValueError):
    pass


class PersistenceError(ValueError):
    pass


class AmbiguousJobReference(LookupError):
    pass


BASE_EXC = {"SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
            "GeneratorExit": GeneratorExit}
BUILTIN_EXC = {
    "RuntimeError": RuntimeError, "ValueError": ValueError, "TypeError": TypeError,
    "KeyError": KeyError, "AttributeError": AttributeError, "Exception": Exception,
    "StopIteration": StopIteration, "OSError": OSError, "ImportError": ImportError,
    "ModuleNotFoundError": ModuleNotFoundError, "LookupError": LookupError,
    "IndexError": IndexError, "NameError": NameError, "AssertionError": AssertionError,
}
CUSTOM_EXC = {"ExportTooLarge": ExportTooLarge, "AmbiguousJobReference": AmbiguousJobReference,
              "PersistenceError": PersistenceError}


def make_exc(kind, msg):
    cls = CUSTOM_EXC.get(kind) or BASE_EXC.get(kind) or BUILTIN_EXC.get(kind)
    if cls is None:
        cls = type(str(kind), (Exception,), {})
    return cls(msg)


def maybe_raise(r):
    if isinstance(r, dict) and "__raise__" in r:
        raise make_exc(r["__raise__"], r.get("msg", ""))
    return r


def seam(name, args):
    args = {k: v for k, v in args.items() if v is not None}
    scripts = CUR["scripts"]
    if name not in scripts:
        r = {"__raise__": "RuntimeError", "msg": "no script for " + name}
    else:
        sc = scripts[name]
        if isinstance(sc, list):
            if not sc:
                r = {"__raise__": "RuntimeError", "msg": "no script for " + name}
            else:
                i = CUR["idx"].get(name, 0)
                CUR["idx"][name] = i + 1
                r = sc[min(i, len(sc) - 1)]
        else:
            r = sc
    CUR["trace"].append({"ev": name, "args": args, "ret": r})
    return maybe_raise(r)


# ── doubles ──
class NoopMarker:
    pass


class CallableDouble:
    def __init__(self, name):
        self.name = name
        self.__name__ = name

    def __call__(self, *a, **k):
        if self.name == "entry":
            proj_args = {"args": [proj(x) for x in a],
                         "kwargs": {kk: proj(vv) for kk, vv in k.items()}}
            return TopParserDouble(seam(self.name, proj_args))
        elif self.name in ("func", "namespace_update"):
            proj_args = {"namespace": proj(a[0]) if a else None}
        elif self.name == "run":
            proj_args = {"ref": a[0] if a else None}
        else:
            proj_args = {"args": [proj(x) for x in a],
                         "kwargs": {kk: proj(vv) for kk, vv in k.items()}}
        return seam(self.name, proj_args)


class ParserDouble:
    def __init__(self, script):
        self.script = script if isinstance(script, dict) else {}

    def add_subparsers(self, dest=None, **kw):
        r = seam("add_subparsers", {"dest": dest})
        return SubParsersDouble(r)

    def add_argument(self, *flags, **kwargs):
        return seam("add_argument", {"flags": list(flags), "kwargs": kwargs})

    def parse_args(self, args=None, **kw):
        r = seam("parse_args", {"args": list(args) if args is not None else None})
        return NamespaceDouble(r)


class SubParsersDouble:
    def __init__(self, script):
        self.script = script if isinstance(script, dict) else {}

    def add_parser(self, root, **kw):
        r = seam("add_parser", {"root": root})
        return TopParserDouble(r)


class TopParserDouble:
    def __init__(self, script):
        self.script = script if isinstance(script, dict) else {}

    def set_defaults(self, **kw):
        return seam("set_defaults", {"func": proj(kw.get("func"))})


class ModuleDouble:
    def __init__(self, module, script):
        self.module = module
        self.script = script if isinstance(script, dict) else {}

    def __getattr__(self, name):
        if name in ("module", "script"):
            return object.__getattribute__(self, name)
        r = seam("getattr", {"module": self.module, "name": name})
        return mat_callable(r)


class ScopeDouble:
    def __init__(self, script):
        object.__setattr__(self, "script", script if isinstance(script, dict) else {})

    def __getattr__(self, name):
        return object.__getattribute__(self, "script").get(name)


class NamespaceDouble:
    def __init__(self, script):
        object.__setattr__(self, "script", script if isinstance(script, dict) else {})

    def __getattr__(self, name):
        sc = object.__getattribute__(self, "script")
        if name in sc:
            return mat_callable(sc[name])
        raise AttributeError(name)


def mat_callable(r):
    if isinstance(r, dict) and "__callable__" in r:
        return CallableDouble(r["__callable__"])
    return r


def proj(o):
    if isinstance(o, CallableDouble):
        return {"__callable__": o.name}
    if isinstance(o, NoopMarker):
        return {"__callable__": "_noop_console_command"}
    if isinstance(o, (ParserDouble, SubParsersDouble, TopParserDouble, ModuleDouble,
                      ScopeDouble, NamespaceDouble)):
        return o.script
    if isinstance(o, dict):
        return {k: proj(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [proj(v) for v in o]
    return o


# ── extraction ──
def extract():
    src = open(SRC, encoding="utf-8").read()
    ns = {
        "__name__": "hermes_cli.console_engine_flow_extracted",
        "argparse": argparse, "contextlib": contextlib, "difflib": difflib,
        "functools": functools, "importlib": _importlib_real, "io": io, "json": json,
        "sys": sys, "dataclass": dataclass, "Path": Path, "SimpleNamespace": SimpleNamespace,
        "Any": Any, "Callable": Callable, "Iterable": Iterable, "Literal": Literal,
        "NoReturn": NoReturn, "Sequence": Sequence,
    }
    tree = ast.parse(src)

    def ex(node):
        exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)

    # classes first (constants _SEND_SURFACE / _doctor need them)
    for n in tree.body:
        if isinstance(n, ast.ClassDef) and n.name in (
                "ConsoleCommandError", "ConsoleResult", "ConsoleCommand",
                "_ArgumentParser", "_CliSurface"):
            ex(n)
    # top-level wanted funcs (strip decorators: @functools.cache etc.)
    for n in tree.body:
        if isinstance(n, ast.FunctionDef) and n.name in WANTED_FUNCS:
            n.decorator_list = []
            ex(n)
    # _CliSurface.build method + nested closures + lambda makers
    for n in tree.body:
        if isinstance(n, ast.ClassDef) and n.name == "_CliSurface":
            for m in n.body:
                if isinstance(m, ast.FunctionDef) and m.name == "build":
                    ex(m)
                    for x in ast.walk(m):
                        if isinstance(x, ast.IfExp) and isinstance(x.body, ast.Lambda):
                            node = ast.Assign(targets=[ast.Name(id="__mh__", ctx=ast.Store())], value=x)
                            ast.fix_missing_locations(node)
                            COMPILED["mh"] = compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec")
        if isinstance(n, ast.FunctionDef) and n.name in ("_captured", "_simple_command"):
            for x in n.body:
                if isinstance(x, ast.FunctionDef):
                    x.decorator_list = []
                    ex(x)
        if isinstance(n, ast.FunctionDef) and n.name == "_dispatch":
            for x in ast.walk(n):
                if isinstance(x, ast.Lambda) and isinstance(x.body, ast.Call) and \
                        isinstance(x.body.func, ast.Name) and x.body.func.id == "func":
                    node = ast.Assign(targets=[ast.Name(id="__dispatch_lam__", ctx=ast.Store())], value=x)
                    ast.fix_missing_locations(node)
                    COMPILED["dl"] = compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec")
    # constants (Assign) — now classes + funcs exist
    for n in tree.body:
        if isinstance(n, ast.Assign) and all(isinstance(t, ast.Name) for t in n.targets):
            try:
                ex(n)
            except Exception:
                pass
    missing = (WANTED_FUNCS | {"build", "wrapper", "handler", "_SEND_SURFACE", "_doctor",
                               "_SHELL_TOKENS", "_CONFIRMED_KINDS", "_BLOCKED_TOP", "_BLOCKED_PAIRS"}) - set(ns)
    if missing or "mh" not in COMPILED or "dl" not in COMPILED:
        print("FATAL missing defs: %s (compiled: %s)" % (sorted(missing), sorted(COMPILED)))
        sys.exit(2)
    return ns


# ── recording stubs ──
def install_stubs(ns):
    # claimed siblings (black boxes)
    ns["_capture_output"] = lambda fn: seam("_capture_output", {"fn": "<lambda>"})
    ns["_expect_no_args"] = lambda args, usage: seam("_expect_no_args", {"args": args, "usage": usage})
    ns["_format_job"] = lambda job, action: seam("_format_job", {"job": job, "action": action})
    ns["_summaries_from_parser"] = lambda parser: seam("_summaries_from_parser", {"parser": proj(parser)})
    ns["_noop_console_command"] = NoopMarker()
    # foreign collaborators
    ns["_strip_ansi"] = lambda line: seam("strip_ansi", {"line": line})

    def import_module(m):
        r = seam("import_module", {"module": m})
        return ModuleDouble(m, r)

    ns["importlib"] = SimpleNamespace(import_module=import_module)
    ns["_ArgumentParser"] = lambda **kw: ParserDouble(seam("parser_new", kw))

    # in-function import stub modules
    def stub(full, **attrs):
        parts = full.split(".")
        for i in range(1, len(parts) + 1):
            name = ".".join(parts[:i])
            if name not in sys.modules:
                m = types.ModuleType(name)
                m.__path__ = []
                sys.modules[name] = m
        m = sys.modules[full]
        for k, v in attrs.items():
            setattr(m, k, v)
        return m

    def profile_scope():
        return ScopeDouble(seam("profile_scope", {}))

    stub("persistence_assembly",
         profile_scope=profile_scope,
         open_persistence_reader=lambda d: seam("open_persistence_reader", {"profile_dir": d}),
         open_persistence=lambda scope: seam("open_persistence", {"scope": proj(scope)}))
    stub("hermes_cli.session_export",
         assert_exports_safe=lambda store, ids: seam("assert_exports_safe",
                                                     {"store": store, "session_ids": ids}))
    stub("persistence.core.errors", ExportTooLarge=ExportTooLarge, PersistenceError=PersistenceError)
    stub("cron.jobs", AmbiguousJobReference=AmbiguousJobReference)
    CUSTOM_EXC["ConsoleCommandError"] = ns["ConsoleCommandError"]


# ── drive ──
def surface_double(inp, ns):
    s = SimpleNamespace(**(inp.get("surface") or {}))
    s.build = lambda root, *, live: ns["build"](s, root, live=live)
    return s


def drive(ns, func, inp):
    g = inp.get
    if func == "paths":
        return [{"path": list(p), "mutating": m} for p, m in ns["_paths"](g("spec"))]
    if func == "sub":
        s = ns["_sub"](g("module"), g("builder"), g("handler"))
        return {"kind": s.kind, "module": s.module, "builder": s.builder, "handler": s.handler}
    if func == "reg":
        s = ns["_reg"](g("module"), g("handler"))
        return {"kind": s.kind, "module": s.module, "builder": s.builder, "handler": s.handler}
    if func == "send_surface":
        s = ns["_SEND_SURFACE"]
        return {"kind": s.kind, "module": s.module, "builder": s.builder, "handler": s.handler}
    if func == "shell_tokens":
        return g("token") in ns["_SHELL_TOKENS"]
    if func == "confirmed_kinds":
        return g("kind") in ns["_CONFIRMED_KINDS"]
    if func == "blocked_top":
        return g("token") in ns["_BLOCKED_TOP"]
    if func == "blocked_pairs":
        toks = g("tokens") or []
        return ns["_BLOCKED_PAIRS"].get(tuple(toks[:2]), "")
    if func == "drop_trailing_blank":
        lines = list(g("lines") or [])
        ns["_drop_trailing_blank"](lines)
        return lines
    if func == "parse":
        specs = []
        for sp in g("specs") or []:
            if "bare" in sp:
                specs.append(sp["bare"])
            else:
                specs.append((sp.get("flags") or [], sp.get("kwargs") or {}))
        return proj(ns["_parse"](g("prog"), g("args") or [], *specs))
    if func == "captured":
        fn = mat_callable(g("fn"))
        ns["_captured"](fn)
        return {"__captured__": True, "fn": proj(fn)}
    if func == "captured_wrapper":
        ns["fn"] = mat_callable(g("fn"))
        return ns["wrapper"](g("engine"), g("args") or [])
    if func == "simple_command":
        mk = mat_callable(g("make_args")) if g("make_args") is not None else (lambda: ())
        ns["_simple_command"](g("usage"), g("module"), g("name"), make_args=mk, **(g("kwargs") or {}))
        return {"__simple_command__": True, "usage": g("usage"), "module": g("module"), "name": g("name")}
    if func == "simple_command_handler":
        ns["usage"] = g("usage")
        ns["module"] = g("module")
        ns["name"] = g("name")
        ns["make_args"] = mat_callable(g("make_args")) if g("make_args") is not None else (lambda: ())
        ns["kwargs"] = g("kwargs") or {}
        return ns["handler"](None, g("args") or [])
    if func == "doctor":
        return ns["_doctor"](None, g("args") or [])
    if func == "build_main_handler":
        ns["self"] = SimpleNamespace(handler=g("handler"))
        ns["live"] = g("live", False)
        exec(COMPILED["mh"], ns)
        return proj(ns["__mh__"]())
    if func == "surface_build":
        s = SimpleNamespace(**(g("surface") or {}))
        return proj(ns["build"](s, g("root"), live=g("live", False)))
    if func == "surface_summaries":
        return ns["_surface_summaries"](surface_double(inp, ns), g("root"))
    if func == "dispatch_lambda":
        ns["func"] = CallableDouble("func")
        ns["namespace"] = g("namespace")
        exec(COMPILED["dl"], ns)
        return proj(ns["__dispatch_lam__"]())
    if func == "dispatch":
        nu = CallableDouble("namespace_update") if jtruthy(g("namespace_update")) else None
        return ns["_dispatch"](surface_double(inp, ns), g("root"), g("fixed") or [],
                               g("args") or [], nu)
    if func == "session_db":
        return ns["_session_db"](read_only=g("read_only", True))
    if func == "guard_exports":
        ns["_guard_exports"](g("store"), g("session_ids") or [])
        return None
    if func == "cron_job_action":
        run = CallableDouble("run")
        return ns["_cron_job_action"](g("args") or [], g("usage"), g("action"), run)
    raise ValueError("unknown func: %s" % func)


def jtruthy(v):
    if v is None:
        return False
    if isinstance(v, (dict, list, str)):
        return len(v) > 0
    return bool(v)


def _numnorm(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: _numnorm(v) for k, v in o.items()}
    if isinstance(o, list):
        return [_numnorm(v) for v in o]
    return o


def canon(o):
    return json.loads(json.dumps(_numnorm(o), sort_keys=True, ensure_ascii=False, default=str))


def main():
    ns = extract()
    install_stubs(ns)
    REAL = {nm: ns[nm] for nm in REAL_KEYS if nm in ns}
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
        CUR["scripts"] = inp.get("seams", {})
        CUR["idx"] = {}
        CUR["trace"] = []
        for nm in REAL:
            ns[nm] = REAL[nm]
        try:
            ret = drive(ns, func, inp)
        except BaseException as ex:
            ret = {"__raise__": type(ex).__name__, "msg": str(ex)}
        want = {"ret": proj(ret), "trace": CUR["trace"]}
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False))
    if cases == 0:
        print("ORACLE console_engine_flow: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE console_engine_flow: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()