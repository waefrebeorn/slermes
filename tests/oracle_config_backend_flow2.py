#!/usr/bin/env python3
"""Oracle for config_backend_flow2.c — hermes_cli/config_backend.py UNCLAIMED
remainder (LIVE authority: the quarry hermes_cli/config_backend.py).

Drives the LIVE defs with SCRIPTED I/O. The two FLOW cores (_refuse_redacted_sets
and its nested read_old closure), the six constructors (ConfigWriteError /
ConfigLockedError / ConfigValueError / _ExplicitFile / UserLayer / Changes __init__)
and the nineteen ConfigBackend Protocol methods are AST-extracted from the quarry
(no heavy hermes_cli.config_backend import). The CLAIMED siblings keeping_read_base
and read_config_doc, and the imported collaborator refuse_redacted_config_sets, are
replaced by recording stubs returning the SAME scripted results the C harness feeds
its universal seam. Internal composition (ConfigLockedError/ConfigValueError super()
-> ConfigWriteError.__init__) runs REAL on both sides. Compares the ordered DECISION
SEQUENCE + return value with ordered-dict comparison (insertion order is semantic).
Per-case BaseException catch (SystemExit / ConfigBackendUnavailable escape the
FileNotFoundError catch and are compared as raise specs; scripted raises round-trip
bare).

Input: stdin JSON lines from t_port_config_backend_flow2:
    {"func":..., "in":{...}, "out":{"ret":..., "trace":[...]}}
Summary: ORACLE config_backend_flow2: N cases, M mismatches. Fail-closed (exit 2)
when extraction misses defs/classes or zero cases were compared.

Script semantics (identical on both sides, FAIL-CLOSED): a MISSING script returns a
RuntimeError raise spec {"__raise__":"RuntimeError","msg":"no script for seam: <name>"};
a seams[name] ARRAY is a per-call queue (last value repeats, empty array -> None); a
single value is a constant; a scripted null is None (NULL in C);
{"__raise__": cls, "msg": ...} scripts an exception.

Fixture domain (kept identical on both sides): ASCII text; POSIX lexical paths;
integral JSON numbers read as int; ctor kwargs restricted to the declared fields (no
binding-error cases); a PRESENT null passes through verbatim, an ABSENT field takes its
default; the in-function import resolves silently (import failure outside the domain);
the read callback projects to its captured config_path and the seamed collaborator
never invokes it; the Protocol methods ignore self/args and return None.
"""
import ast
import builtins
import contextlib
import contextvars
import importlib
import json
import os
import re
import sys
import threading
import types
from dataclasses import dataclass, field
from pathlib import Path
from typing import Protocol, runtime_checkable

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

# Scrubbed-env runner: derive home from __file__ (repo is parent.parent).
_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "cb2_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "hermes_cli", "config_backend.py")
SRC_TEXT = open(SRC, encoding="utf-8").read()
ANN_FLAG = __import__("__future__").annotations.compiler_flag

# func key -> live name.
FLOW_FUNCS = {
    "refuse_redacted_sets": "_refuse_redacted_sets",
    "read_old": "_refuse_redacted_sets.read_old",
}
CTOR_FUNCS = {
    "config_write_error_init": "ConfigWriteError.__init__",
    "config_locked_error_init": "ConfigLockedError.__init__",
    "config_value_error_init": "ConfigValueError.__init__",
    "explicit_file_init": "_ExplicitFile.__init__",
    "user_layer_init": "UserLayer.__init__",
    "changes_init": "Changes.__init__",
}
# ConfigBackend Protocol methods: live signature (args after self) for binding.
PROTO_SIG = {
    "read_user_doc": ["home"],
    "read_user_doc_readonly": ["home"],
    "version": ["home"],
    "cache_version": ["home", "version"],
    "exists": ["home"],
    "probe_readable": ["home"],
    "write_changes": ["home", "changes"],
    "locked": ["home", "dotted"],
    "supports_file_tooling": [],
    "honors_managed_config": [],
    "boot": ["home"],
    "protected_env_names": [],
    "deployment_env_names": [],
    "bootstrap_env_names": [],
    "preserve_read_base": [],
    "doctor_warnings": ["home"],
    "apply_in_memory": ["home", "doc"],
    "describe": ["home"],
    "destination": ["home"],
}
PROTO_FUNCS = {"proto_" + n: n for n in PROTO_SIG}
ALL_FUNCS = set(FLOW_FUNCS) | set(CTOR_FUNCS) | set(PROTO_FUNCS)
CLASSES = ["UserLayer", "Changes", "ConfigBackend", "ConfigBackendUnavailable",
           "ConfigWriteError", "ConfigLockedError", "ConfigValueError",
           "FileBackend", "_ExplicitFile"]
TOP_DEFS = ["_refuse_redacted_sets"]

CUR = {"in": {}, "trace": [], "scripts": {}, "idx": {}, "raised": []}
EXC_CLASSES = {}


# ── exception construction ──

def build_exc(spec):
    cls = spec.get("__raise__", "Exception")
    msg = spec.get("msg", "")
    if cls in EXC_CLASSES:
        C = EXC_CLASSES[cls]
        if cls == "ConfigLockedError":
            return C(spec.get("path", ""), spec.get("locked_by", ""),
                     spec.get("message") or msg or None)
        if cls == "ConfigWriteError":
            return C(msg, code=spec.get("code", "config_write_failed"))
        if cls == "ConfigValueError":
            return C(msg, code=spec.get("code", "config_value_invalid"))
        return C(msg)  # ConfigBackendUnavailable (SystemExit subclass)
    b = getattr(builtins, cls, None)
    if isinstance(b, type) and issubclass(b, BaseException):
        return b(msg)
    return type(cls, (Exception,), {})(msg)


def seam(name, args):
    args = {k: v for k, v in args.items() if v is not None}
    scripts = CUR["scripts"]
    if name not in scripts:
        # FAIL-CLOSED: missing script -> RuntimeError raise spec (presence test,
        # NOT .get() is None — a scripted null is a valid None result).
        r = {"__raise__": "RuntimeError", "msg": "no script for seam: %s" % name}
    else:
        sc = scripts[name]
        if isinstance(sc, list):
            i = CUR["idx"].get(name, 0)
            CUR["idx"][name] = i + 1
            r = sc[min(i, len(sc) - 1)] if sc else None
        else:
            r = sc
    r = json.loads(json.dumps(r))  # deep copy: live mutation cannot corrupt script/trace
    CUR["trace"].append({"ev": name, "args": args, "ret": r})
    if isinstance(r, dict) and "__raise__" in r:
        e = build_exc(r)
        CUR["raised"].append((e, r))
        raise e
    return r


# ── normalization / canonical comparison ──

def norm(o):
    if o is None or isinstance(o, (bool, int, float, str)):
        return o
    if isinstance(o, PathDouble):
        return o.as_posix()
    if isinstance(o, (frozenset, set)):
        return sorted(norm(v) for v in o)
    if isinstance(o, dict):
        return {str(k): norm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [norm(v) for v in o]
    return str(o)


def _numnorm(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: _numnorm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [_numnorm(v) for v in o]
    return o


def canon(o):
    o = _numnorm(o)
    if isinstance(o, dict):
        return ("obj", [(k, canon(v)) for k, v in o.items()])
    if isinstance(o, (list, tuple)):
        return ("arr", [canon(v) for v in o])
    if o is None:
        return ("null",)
    if isinstance(o, bool):
        return ("bool", o)
    if isinstance(o, (int, float)):
        return ("num", float(o))
    return ("str", o)


# ── doubles ──

class PathDouble:
    def __init__(self, p):
        if isinstance(p, PathDouble):
            s = p.s
        elif p is None:
            s = ""
        else:
            s = str(p)
        s = s.replace("\\", "/")
        while len(s) > 1 and s.endswith("/"):
            s = s[:-1]
        self.s = s

    def as_posix(self):
        return self.s

    def __fspath__(self):
        return self.s

    def __str__(self):
        return self.s


# ── ctor projections (key order matches the C projection builders) ──

def proj_write_exc(e):
    return {"message": str(e), "code": norm(e.code), "args": norm(list(e.args))}


def proj_locked_exc(e):
    return {"message": str(e), "code": norm(e.code), "args": norm(list(e.args)),
            "path": norm(e.path), "locked_by": norm(e.locked_by)}


# ── extraction ──

def extract():
    mod = types.ModuleType("config_backend_flow2_extracted")
    sys.modules["config_backend_flow2_extracted"] = mod
    ns = mod.__dict__
    ns.update({
        "__name__": "config_backend_flow2_extracted",
        "json": json, "os": os, "sys": sys, "re": re, "threading": threading,
        "contextlib": contextlib, "contextvars": contextvars, "importlib": importlib,
        "Path": PathDouble, "dataclass": dataclass, "field": field,
        "Any": object, "Mapping": dict, "Optional": object, "Union": object,
        "Protocol": Protocol, "runtime_checkable": runtime_checkable,
        "PathLike": object,
    })
    tree = ast.parse(SRC_TEXT)

    def exec_node(node):
        exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec",
                     flags=ANN_FLAG), ns)

    def seed_consts():
        for node in tree.body:
            if isinstance(node, ast.Assign):
                if not all(isinstance(t, ast.Name) for t in node.targets):
                    continue
            elif isinstance(node, ast.AnnAssign):
                if not (node.value and isinstance(node.target, ast.Name)):
                    continue
            else:
                continue
            try:
                exec_node(node)
            except Exception:
                pass

    seed_consts()
    # Classes in source order (bases before subclasses: ConfigWriteError before
    # ConfigLockedError/ConfigValueError; FileBackend before _ExplicitFile).
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name in CLASSES:
            exec_node(node)
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in TOP_DEFS:
            exec_node(node)
    seed_consts()  # constants that reference classes (e.g. _FILE_BACKEND)

    missing = [n for n in TOP_DEFS + CLASSES if n not in ns]
    if missing:
        print("ORACLE config_backend_flow2: FATAL missing defs: %s" % sorted(missing),
              file=sys.stderr)
        sys.exit(2)
    for k in ("CONFIG_FILENAME", "BACKEND_ENV"):
        if k not in ns:
            print("ORACLE config_backend_flow2: FATAL missing constant: %s" % k,
                  file=sys.stderr)
            sys.exit(2)
    for cname in ("ConfigBackendUnavailable", "ConfigWriteError", "ConfigLockedError",
                  "ConfigValueError"):
        EXC_CLASSES[cname] = ns[cname]
    return ns


def extract_nested(parent_name, nested_name, globals_dict):
    """Exec a nested closure def with its free variables bound as globals."""
    tree = ast.parse(SRC_TEXT)
    parent = next(n for n in tree.body
                  if isinstance(n, ast.FunctionDef) and n.name == parent_name)
    node = next(n for n in parent.body
                if isinstance(n, ast.FunctionDef) and n.name == nested_name)
    ns = {"__builtins__": builtins.__dict__}
    ns.update(globals_dict)
    exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec",
                 flags=ANN_FLAG), ns)
    return ns[nested_name]


def install_stubs(ns):
    # ── CLAIMED siblings (seamed, never re-ported) ──
    def krb_stub(read):
        # read is `lambda: read_config_doc(config_path)`; project to its captured
        # config_path (the case input). The stub does NOT invoke the callback.
        cp = CUR["in"].get("config_path")
        return seam("keeping_read_base", {"config_path": cp})
    ns["keeping_read_base"] = krb_stub
    ns["read_config_doc"] = lambda cp: seam("read_config_doc", {"config_path": cp})

    # ── in-function import via sys.modules stubs (every intermediate package) ──
    hc = types.ModuleType("hermes_cli")
    hc.__path__ = []
    ccv = types.ModuleType("hermes_cli.config_credential_values")

    def rrcs(sets, read_old):
        return seam("refuse_redacted_config_sets", {"sets": sets, "read_old": "<callback>"})
    ccv.refuse_redacted_config_sets = rrcs
    hc.config_credential_values = ccv
    sys.modules["hermes_cli"] = hc
    sys.modules["hermes_cli.config_credential_values"] = ccv


# ── drive ──

def drive(ns, func, inp):
    try:
        if func == "refuse_redacted_sets":
            ret = ns["_refuse_redacted_sets"](inp.get("config_path"), inp.get("sets"))
        elif func == "read_old":
            f = extract_nested("_refuse_redacted_sets", "read_old", {
                "config_path": inp.get("config_path"),
                "keeping_read_base": ns["keeping_read_base"],
                "read_config_doc": ns["read_config_doc"],
            })
            ret = f()
        elif func == "config_write_error_init":
            kw = {"code": inp["code"]} if "code" in inp else {}
            e = ns["ConfigWriteError"](inp.get("message", ""), **kw)
            ret = proj_write_exc(e)
        elif func == "config_locked_error_init":
            e = ns["ConfigLockedError"](inp.get("path", ""), inp.get("locked_by", ""),
                                        inp.get("message"))
            ret = proj_locked_exc(e)
        elif func == "config_value_error_init":
            kw = {"code": inp["code"]} if "code" in inp else {}
            e = ns["ConfigValueError"](inp.get("message", ""), **kw)
            ret = proj_write_exc(e)
        elif func == "explicit_file_init":
            ef = ns["_ExplicitFile"](PathDouble(inp.get("path")))
            ret = {"explicit": ef._path.as_posix(), "name": ef.name}
        elif func == "user_layer_init":
            ul = ns["UserLayer"](**(inp.get("fields") or {}))
            ret = {"doc": norm(ul.doc), "version": norm(ul.version),
                   "locks": norm(ul.locks), "provenance": norm(ul.provenance)}
        elif func == "changes_init":
            ch = ns["Changes"](**(inp.get("fields") or {}))
            ret = {"document": norm(ch.document), "set": norm(ch.set),
                   "unset": norm(ch.unset),
                   "extra_content_on_create": norm(ch.extra_content_on_create)}
        elif func in PROTO_FUNCS:
            name = PROTO_FUNCS[func]
            args = [inp.get(k) for k in PROTO_SIG[name]]
            ret = getattr(ns["ConfigBackend"], name)(object(), *args)
        else:
            raise ValueError("unknown func: %s" % func)
    except BaseException as e:  # SystemExit included — fail-closed per case
        ret = None
        for exc, spec in CUR["raised"]:
            if e is exc:
                ret = spec  # scripted raise round-trips bare
                break
        if ret is None:
            ret = {"__raise__": type(e).__name__, "msg": str(e)}
    return ret


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
        CUR["in"] = inp
        CUR["trace"] = []
        CUR["scripts"] = inp.get("seams", {})
        CUR["idx"] = {}
        CUR["raised"] = []
        if func not in ALL_FUNCS:
            mism += 1
            print("PYERR unknown func: %s" % func)
            continue
        ret = drive(ns, func, inp)
        want = {"ret": norm(ret), "trace": CUR["trace"]}
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    if cases == 0:
        print("ORACLE config_backend_flow2: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE config_backend_flow2: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
