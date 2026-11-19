#!/usr/bin/env python3
"""Oracle for config_backend_flow.c — hermes_cli/config_backend.py FLOW cores +
leaves (LIVE authority: the quarry hermes_cli/config_backend.py).

Drives the LIVE functions with SCRIPTED I/O: the module-level flows, the
FileBackend path-level leaves, the _ExplicitFile override and the contextvar
managers are AST-extracted from the quarry (no heavy hermes_cli.config_backend
import). Module constants (Assign AND AnnAssign, plain-Name targets) and the
Changes / exception / FileBackend / _ExplicitFile classes are seeded. Every
stat/open/read, utils signature+yaml helper, foreign-module import, the CLAIMED
sibling functions (_route, get_config_backend, _backend_for, _bootstrap,
read_config_doc, write_config_keys, FileBackend.write_path) and the selected
backend's method surface are replaced by recording stubs returning the SAME
scripted results the C harness feeds its universal seam. Internal composition
(config_version inside config_cache_version / config_version_or_none;
supports_file_tooling inside require_file_tooling; the FileBackend method ->
path-primitive chains) runs REAL on both sides. Compares the ordered DECISION
SEQUENCE + return value with ordered-dict comparison (insertion order is
semantic). Per-case BaseException catch (SystemExit / ConfigBackendUnavailable
escape except OSError and are compared as raise specs; scripted raises
round-trip bare).

Input: stdin JSON lines from t_port_config_backend_flow:
    {"func":..., "in":{...}, "out":{"ret":..., "trace":[...]}}
Summary: ORACLE config_backend_flow: N cases, M mismatches. Fail-closed (exit 2)
when extraction misses defs or zero cases were compared.

Script semantics (identical on both sides): a seams[name] ARRAY is a per-call
queue (last value repeats); a single value is a constant; a MISSING script
returns None (NULL in C). {"__raise__": cls, "msg": ...} scripts an exception.

Fixture domain (kept identical on both sides): ASCII text; POSIX lexical paths;
integral JSON numbers read as int; a version is a JSON array (tuple); a backend
descriptor is a JSON object with a "name"; a Changes object is
{document,set,unset,extra_content_on_create} (canonical order, defaults filled);
a file token is the open seam's scripted return (a path string); the with-block
file close and preserve_read_base CM enter/exit are no-ops (not seamed);
frozenset()/() normalize to []; contextvar state cv={refuse,pinned}.
"""
import ast
import builtins
import contextlib
import contextvars
import json
import os
import re
import sys
import threading
import types
from dataclasses import dataclass, field
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

# Scrubbed-env runner: derive home from __file__ (repo is parent.parent).
_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "cb_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "hermes_cli", "config_backend.py")

# func key -> live python name (module-level unless noted).
PY = {
    "drop_other_access": "drop_other_access",
    "bootstrap_names": "_bootstrap_names",
    "bootstrap_deployment": "bootstrap_deployment",
    "local_file_layer": "local_file_layer",
    "read_config_doc_readonly": "read_config_doc_readonly",
    "config_version": "config_version",
    "config_cache_version": "config_cache_version",
    "config_version_or_none": "config_version_or_none",
    "probe_config_readable": "probe_config_readable",
    "config_exists": "config_exists",
    "write_config_document": "write_config_document",
    "config_destination": "config_destination",
    "write_config_key": "write_config_key",
    "keeping_read_base": "keeping_read_base",
    "write_changes_flow": "write_changes",
    "deployment_env_names_flow": "deployment_env_names",
    "supports_file_tooling_flow": "supports_file_tooling",
    "require_file_tooling": "require_file_tooling",
    "refusing_locked_omissions": "refusing_locked_omissions",
    "refused_locked_omission_prefixes": "refused_locked_omission_prefixes",
    "pinning_keys": "pinning_keys",
    "pinned_keys": "pinned_keys",
}
# FileBackend method leaves (func key -> method name).
FILE_METHODS = {
    "file_config_path": "config_path",
    "file_version_path": "version_path",
    "file_read_path": "read_path",
    "file_read_path_readonly": "read_path_readonly",
    "file_read_user_doc": "read_user_doc",
    "file_read_user_doc_readonly": "read_user_doc_readonly",
    "file_version": "version",
    "file_cache_version": "cache_version",
    "file_exists": "exists",
    "file_probe_readable": "probe_readable",
    "file_write_changes": "write_changes",
    "file_locked": "locked",
    "file_supports_file_tooling": "supports_file_tooling",
    "file_honors_managed_config": "honors_managed_config",
    "file_boot": "boot",
    "file_protected_env_names": "protected_env_names",
    "file_deployment_env_names": "deployment_env_names",
    "file_bootstrap_env_names": "bootstrap_env_names",
    "file_preserve_read_base": "preserve_read_base",
    "file_doctor_warnings": "doctor_warnings",
    "file_apply_in_memory": "apply_in_memory",
    "file_describe": "describe",
    "file_destination": "destination",
}
WANTED = set(PY.values())

CUR = {"in": {}, "trace": [], "scripts": {}, "idx": {}, "raised": []}
EXC_CLASSES = {}
EXPLICIT_CLS = None
FILE_CLS = None


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
    sc = CUR["scripts"].get(name)
    if sc is None:
        r = None
    elif isinstance(sc, list):
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


def posix(p):
    if p is None:
        return None
    if isinstance(p, (PathDouble, Path)):
        return p.as_posix()
    return str(p).replace("\\", "/")


def changes_proj(ch):
    return {"document": ch.document, "set": dict(ch.set),
            "unset": list(ch.unset), "extra_content_on_create": ch.extra_content_on_create}


# ── normalization / canonical comparison ──

def norm(o):
    if o is None or isinstance(o, (bool, int, float, str)):
        return o
    if isinstance(o, (Path, PathDouble)):
        return o.as_posix()
    if EXPLICIT_CLS is not None and isinstance(o, EXPLICIT_CLS):
        return {"explicit": o._path.as_posix(), "name": "file"}
    if isinstance(o, contextlib.nullcontext):
        return {"__cm__": "nullcontext"}
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

def _parent_lex(s):
    while len(s) > 1 and s.endswith("/"):
        s = s[:-1]
    idx = s.rfind("/")
    if idx < 0:
        return "."
    if idx == 0:
        return "/"
    return s[:idx]


def _join_lex(a, b):
    if not b:
        return a
    if b.startswith("/"):
        return b
    if not a:
        return b
    if a.endswith("/"):
        return a + b
    return a + "/" + b


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

    def stat(self):
        r = seam("path_stat", {"path": self.s})
        return types.SimpleNamespace(**(r if isinstance(r, dict) else {}))

    def exists(self):
        return seam("path_exists", {"path": self.s})

    @property
    def parent(self):
        return PathDouble(_parent_lex(self.s))

    @property
    def name(self):
        idx = self.s.rfind("/")
        return self.s[idx + 1:] if idx >= 0 else self.s

    def __truediv__(self, other):
        return PathDouble(_join_lex(self.s, str(other)))

    def as_posix(self):
        return self.s

    def __fspath__(self):
        return self.s

    def __str__(self):
        return self.s


class FileDouble:
    def __init__(self, token):
        self.token = token

    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False

    def read(self, size=-1):
        return seam("file_read", {"file": self.token, "size": size})


def _file_token(f):
    return f.token if isinstance(f, FileDouble) else f


class BackendDouble:
    def __init__(self, desc):
        self._desc = desc if isinstance(desc, dict) else {}
        self.name = self._desc.get("name")

    def read_user_doc_readonly(self, home):
        return seam("backend_read_user_doc_readonly", {"home": posix(home)})

    def version(self, home):
        return seam("backend_version", {"home": posix(home)})

    def cache_version(self, home, version):
        return seam("backend_cache_version", {"home": posix(home), "version": version})

    def probe_readable(self, home):
        return seam("backend_probe_readable", {"home": posix(home)})

    def exists(self, home):
        return seam("backend_exists", {"home": posix(home)})

    def write_changes(self, home, changes):
        return seam("backend_write_changes", {"home": posix(home), "changes": changes_proj(changes)})

    def destination(self, home):
        return seam("backend_destination", {"home": posix(home)})

    def deployment_env_names(self):
        return seam("backend_deployment_env_names", {})

    def supports_file_tooling(self):
        return seam("backend_supports_file_tooling", {})

    def bootstrap_env_names(self):
        return seam("backend_bootstrap_env_names", {})

    def preserve_read_base(self):
        seam("backend_preserve_read_base", {})
        return contextlib.nullcontext()


def _backend(r):
    return BackendDouble(r if isinstance(r, dict) else {})


# ── extraction ──

def extract():
    import __future__
    global EXPLICIT_CLS, FILE_CLS
    src = open(SRC, encoding="utf-8").read()
    mod = types.ModuleType("config_backend_extracted")
    sys.modules["config_backend_extracted"] = mod
    ns = mod.__dict__
    ns.update({
        "__name__": "config_backend_extracted",
        "json": json, "os": os, "sys": sys, "re": re, "threading": threading,
        "contextlib": contextlib, "contextvars": contextvars, "importlib": __import__("importlib"),
        "Path": PathDouble, "dataclass": dataclass, "field": field,
        "Any": object, "Mapping": dict, "Optional": object, "Union": object,
    })
    tree = ast.parse(src)
    # Pass 1: module constants (plain-Name Assign/AnnAssign targets only).
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
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
        except Exception:
            pass
    # Pass 2: classes (Changes, exceptions, FileBackend, _ExplicitFile). Skip the
    # ConfigBackend Protocol (heavy, unused by the ported funcs).
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name != "ConfigBackend":
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec",
                         flags=__future__.annotations.compiler_flag), ns)
    # Pass 3: module-level target defs (future-annotations keeps annotations str).
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec",
                         flags=__future__.annotations.compiler_flag), ns)
    missing = WANTED - set(ns)
    if missing:
        print("FATAL missing defs: %s" % sorted(missing))
        sys.exit(2)
    for cname in ("ConfigBackendUnavailable", "ConfigWriteError", "ConfigLockedError",
                  "ConfigValueError", "Changes", "FileBackend", "_ExplicitFile"):
        if cname not in ns:
            print("FATAL missing class: %s" % cname)
            sys.exit(2)
    for k in ("CONFIG_FILENAME", "BACKEND_ENV", "_REFUSE_LOCKED_OMISSIONS", "_PINNED_KEYS"):
        if k not in ns:
            print("FATAL missing constant: %s" % k)
            sys.exit(2)
    # The @contextlib.contextmanager decorator is part of the FunctionDef node's
    # decorator_list, so compiling the node already applies it — do NOT re-wrap.
    for cname in ("ConfigBackendUnavailable", "ConfigWriteError", "ConfigLockedError", "ConfigValueError"):
        EXC_CLASSES[cname] = ns[cname]
    EXPLICIT_CLS = ns["_ExplicitFile"]
    FILE_CLS = ns["FileBackend"]
    return ns


def _pkg(name):
    m = sys.modules.get(name)
    if m is None:
        m = types.ModuleType(name)
        m.__path__ = []
        sys.modules[name] = m
    return m


def _submod(pkg, name):
    m = types.ModuleType(name)
    sys.modules[name] = m
    setattr(pkg, name.rsplit(".", 1)[-1], m)
    return m


def install_stubs(ns):
    # ── claimed siblings (seamed, never re-ported) ──
    def route_stub(config_path):
        r = seam("route", {"config_path": config_path})
        if not isinstance(r, dict):
            r = {}
        home = r.get("home")
        return (_backend(r.get("backend")), PathDouble(home if home is not None else ""))
    ns["_route"] = route_stub
    ns["get_config_backend"] = lambda: _backend(seam("get_config_backend", {}))
    ns["_backend_for"] = lambda kind: _backend(seam("backend_for", {"kind": kind}))
    ns["_bootstrap"] = lambda home, project_env, *, once: seam(
        "bootstrap", {"home": posix(home), "project_env": posix(project_env), "once": once})
    ns["_ensure_bootstrap_env"] = lambda: seam("ensure_bootstrap_env", {})
    ns["read_config_doc"] = lambda config_path: seam("read_config_doc", {"config_path": config_path})
    ns["write_config_keys"] = lambda config_path, updates: seam(
        "write_config_keys", {"config_path": config_path, "updates": updates})
    ns["version_token"] = lambda version: seam("version_token", {"version": version})
    ns["config_cache_version_or_none"] = lambda config_path: seam(
        "config_cache_version_or_none", {"config_path": config_path})
    # unported sibling _refuse_redacted_sets -> seam inside write_changes
    ns["_refuse_redacted_sets"] = lambda config_path, sets: seam(
        "refuse_redacted_sets", {"config_path": config_path, "sets": sets})
    # FileBackend.write_path is CLAIMED -> seam stub on the class.
    def write_path_stub(path, changes):
        return seam("write_path", {"path": posix(path), "changes": changes_proj(changes)})
    ns["FileBackend"].write_path = staticmethod(write_path_stub)

    # ── builtin open + os.chmod (seamed) ──
    def open_stub(file, mode="r", *a, **kw):
        encoding = kw.get("encoding")
        args = {"path": posix(file), "mode": mode}
        if encoding is not None:
            args["encoding"] = encoding
        tok = seam("open", args)
        return FileDouble(tok)
    ns["open"] = open_stub
    ns["os"] = types.SimpleNamespace(
        chmod=lambda path, mode: seam("os_chmod", {"path": posix(path), "mode": mode}))

    # ── in-function imports via sys.modules stubs ──
    def _stat_proj(st):
        return dict(st.__dict__) if isinstance(st, types.SimpleNamespace) else st
    utils = types.ModuleType("utils")
    utils.file_signature = lambda st: seam("file_signature", {"st": _stat_proj(st)})
    utils.fast_safe_load = lambda f: seam("fast_safe_load", {"file": _file_token(f)})
    utils.load_yaml_file_readonly = lambda path: seam("load_yaml_file_readonly", {"path": posix(path)})
    utils.racy_content_digest = lambda path, version: seam(
        "racy_content_digest", {"path": posix(path), "version": version})
    sys.modules["utils"] = utils

    agent = _pkg("agent")
    swp = _submod(agent, "agent.safe_worker_policy")
    swp.worker_config_snapshot = lambda: seam("worker_config_snapshot", {})

    hc = types.ModuleType("hermes_constants")
    hc.get_managed_system = lambda home=None: seam("get_managed_system", {})
    hc._container_or_chmod_skipped = lambda: seam("container_or_chmod_skipped", {})
    sys.modules["hermes_constants"] = hc


# ── drive ──

def drive(ns, func, inp):
    fb = ns["FileBackend"]()
    try:
        if func in PY:
            name = PY[func]
            if func == "drop_other_access":
                ret = ns[name](PathDouble(inp.get("path")))
            elif func == "bootstrap_names":
                ret = ns[name](inp.get("kind", ""))
            elif func == "bootstrap_deployment":
                ret = ns[name](inp.get("home"), inp.get("project_env"))
            elif func == "local_file_layer":
                ret = ns[name](inp.get("config_path"))
            elif func in ("read_config_doc_readonly", "config_version", "config_cache_version",
                          "config_version_or_none", "probe_config_readable", "config_exists",
                          "config_destination"):
                ret = ns[name](inp.get("config_path"))
            elif func == "keeping_read_base":
                cp_ = inp.get("config_path")
                ret = ns[name](lambda: ns["read_config_doc"](cp_))
            elif func == "write_config_document":
                ret = ns[name](inp.get("config_path"), inp.get("document"),
                               extra_content_on_create=inp.get("extra_content_on_create"))
            elif func == "write_config_key":
                ret = ns[name](inp.get("config_path"), inp.get("key_path"), inp.get("value"))
            elif func == "write_changes_flow":
                ret = ns[name](inp.get("config_path"), ns["Changes"](**(inp.get("changes") or {})))
            elif func in ("deployment_env_names_flow", "supports_file_tooling_flow"):
                ret = ns[name]()
            elif func == "require_file_tooling":
                ret = ns[name](inp.get("what", ""))
            elif func in ("refusing_locked_omissions", "pinning_keys"):
                cv = inp.get("cv") or {}
                ns["_REFUSE_LOCKED_OMISSIONS"].set(tuple(cv.get("refuse", [])))
                ns["_PINNED_KEYS"].set(tuple(cv.get("pinned", [])))
                args = inp.get("prefixes" if func == "refusing_locked_omissions" else "keys", [])
                with ns[name](*args):
                    seam("cm_body", {"refused": list(ns["refused_locked_omission_prefixes"]()),
                                     "pinned": list(ns["pinned_keys"]())})
                seam("cm_after", {"refused": list(ns["refused_locked_omission_prefixes"]()),
                                  "pinned": list(ns["pinned_keys"]())})
                ret = None
            elif func in ("refused_locked_omission_prefixes", "pinned_keys"):
                cv = inp.get("cv") or {}
                ns["_REFUSE_LOCKED_OMISSIONS"].set(tuple(cv.get("refuse", [])))
                ns["_PINNED_KEYS"].set(tuple(cv.get("pinned", [])))
                ret = ns[name]()
            else:
                raise ValueError("unhandled module func: %s" % func)
        elif func in FILE_METHODS:
            m = FILE_METHODS[func]
            home = inp.get("home")
            if func == "file_config_path":
                ret = getattr(fb, m)(PathDouble(home))
            elif func == "file_version_path":
                ret = getattr(fb, m)(PathDouble(inp.get("path")))
            elif func in ("file_read_path", "file_read_path_readonly"):
                ret = getattr(fb, m)(PathDouble(inp.get("path")))
            elif func in ("file_read_user_doc", "file_read_user_doc_readonly", "file_version",
                          "file_exists", "file_probe_readable", "file_boot", "file_doctor_warnings",
                          "file_describe", "file_destination"):
                ret = getattr(fb, m)(PathDouble(home))
            elif func == "file_cache_version":
                ret = getattr(fb, m)(PathDouble(home), tuple(inp.get("version") or []))
            elif func == "file_write_changes":
                ret = getattr(fb, m)(PathDouble(home), ns["Changes"](**(inp.get("changes") or {})))
            elif func == "file_locked":
                ret = getattr(fb, m)(PathDouble(home), inp.get("dotted", ""))
            elif func in ("file_supports_file_tooling", "file_honors_managed_config",
                          "file_protected_env_names", "file_deployment_env_names",
                          "file_bootstrap_env_names", "file_preserve_read_base"):
                ret = getattr(fb, m)()
            elif func == "file_apply_in_memory":
                ret = getattr(fb, m)(PathDouble(home), inp.get("doc"))
            else:
                raise ValueError("unhandled file method: %s" % func)
        elif func == "explicit_config_path":
            ef = ns["_ExplicitFile"](PathDouble(inp.get("path")))
            ret = ef.config_path(PathDouble(inp.get("home")))
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
        if func not in PY and func not in FILE_METHODS and func != "explicit_config_path":
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
        print("ORACLE config_backend_flow: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE config_backend_flow: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()