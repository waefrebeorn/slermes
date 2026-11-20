#!/usr/bin/env python3
"""Oracle for the tui_gateway/server.py SECOND-WAVE FLOW cores / pure leaves
(src/gateway/tui_server_flow2.c).

CRLF-aware PoP audit: 32 names of server.py are claimed by flow1 / port_server
/ port_methods_tools / pet_commands; this oracle covers 13 UNCLAIMED functions:
  _ws_orphan_setting, _resolve_ws_orphan_reap_grace, _configured_cwd_from_cfg,
  _default_session_cwd, _pet_cfg, _pet_config_scale, _pet_cancel_arm,
  _pet_cancel_request, _pet_is_cancelled, _response_profile_name,
  _pet_gen_root, _spawn_trees_root, _spawn_tree_session_dir.

It AST-extracts those defs, seeds faithful doubles for the module globals they
read, and replaces every collaborator / filesystem probe / clock with recording
stubs returning the SAME scripted results the C harness feeds its universal
seam. In-function imports are stubbed on sys.modules so the live
`from X import Y` binds the recorder. The pet cancel set is modeled as an
insertion-ordered dedup list on both sides (only add/discard/membership are
observed). Compares ordered seam trace + return + final state. The C port
carries the FULL builtin exception hierarchy, so scripted Exception-family
raises are swallowed at suppress/except-Exception sites while the
BaseException trio (and CancelledError) propagate.

Input: stdin JSON lines from t_port_tui_server_flow2.
Summary: ORACLE tui_server_flow2: N cases, M mismatches. Fail-closed on 0 cases.
"""
import ast
import asyncio
import builtins
import contextlib
import copy
import json
import os
import sys
import types
from pathlib import Path, PurePath, PurePosixPath
from types import SimpleNamespace

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

# Scrubbed-env runner: derive home from __file__ (repo = parent.parent, home = parent).
_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "ts2_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "tui_gateway", "server.py")

TARGETS = [
    "_ws_orphan_setting", "_resolve_ws_orphan_reap_grace",
    "_configured_cwd_from_cfg", "_default_session_cwd",
    "_pet_cfg", "_pet_config_scale", "_pet_cancel_arm", "_pet_cancel_request",
    "_pet_is_cancelled", "_response_profile_name",
    "_pet_gen_root", "_spawn_trees_root", "_spawn_tree_session_dir",
]

CUR = {"func": None, "in": {}, "trace": [], "io": {}, "pos": {}, "raises": []}


def rec(ev):
    CUR["trace"].append(ev)


# ── exceptions ───────────────────────────────────────────────────────────────
class ConfigBackendUnavailable(SystemExit):
    """Faithful double: a SystemExit subclass (passes `except Exception`)."""


class ProfileUnavailableError(Exception):
    """Faithful double for the custom profile error (an Exception subclass)."""


def make_exc(spec):
    kind = spec.get("__raise__", "Exception")
    msg = spec.get("message", "")
    if kind == "ConfigBackendUnavailable":
        return ConfigBackendUnavailable(msg)
    if kind == "ProfileUnavailableError":
        return ProfileUnavailableError(msg)
    if kind == "CancelledError":
        return asyncio.CancelledError(msg)
    if kind == "KeyError":
        # str(KeyError(k)) == repr(k); the spec message IS repr(k) -> recover k.
        try:
            key = ast.literal_eval(msg)
        except Exception:
            key = msg
        return KeyError(key)
    cls = getattr(builtins, kind, None)
    if isinstance(cls, type) and issubclass(cls, BaseException):
        return cls(msg)
    return type(kind, (Exception,), {})(msg)


class Raise:
    def __init__(self, spec):
        self.spec = spec
        self.exc = make_exc(spec)


# ── script access (mirrors the harness stub semantics, fail-closed) ──────────
def next_script(name):
    io = CUR["io"]
    if name not in io:
        return Raise({"__raise__": "RuntimeError", "message": "no script for " + name})
    v = io[name]
    if isinstance(v, list):
        i = CUR["pos"].get(name, 0)
        if i >= len(v):
            return Raise({"__raise__": "RuntimeError", "message": "script exhausted for " + name})
        CUR["pos"][name] = i + 1
        return v[i]
    if isinstance(v, dict) and v.get("__script__"):
        return v.get("ret")
    return v


def seam(name, args):
    r = next_script(name)
    if isinstance(r, Raise):
        CUR["raises"].append(r)
        rec({"ev": name, "args": args, "ret": r.spec})
        raise r.exc
    rec({"ev": name, "args": args, "ret": r})
    if isinstance(r, dict) and "__raise__" in r:
        e = make_exc(r)
        CUR["raises"].append(Raise(r))
        raise e
    return copy.deepcopy(r) if isinstance(r, (dict, list)) else r


# ── projections / doubles ────────────────────────────────────────────────────
def proj(x):
    if isinstance(x, (PurePath, PathDouble)):
        return x.as_posix()
    return x


class PathDouble:
    """Minimal Path stand-in: `/` join, `.mkdir`, `.as_posix`, `str`."""

    def __init__(self, s):
        self.s = s

    def _child(self, other):
        base = "" if not self.s else self.s.rstrip("/") or "/"
        child = str(other)
        if base == "/":
            return PathDouble("/" + child)
        return PathDouble(base + "/" + child)

    def __truediv__(self, other):
        return self._child(other)

    def as_posix(self):
        return self.s

    def __str__(self):
        return self.s if self.s is not None else "None"

    def mkdir(self, **kw):
        return seam("mkdir", {"path": self.s, "parents": kw.get("parents"),
                              "exist_ok": kw.get("exist_ok")})


class OrderedSet:
    """Insertion-ordered set double: add/discard/membership, stable echo order."""

    def __init__(self, items=()):
        self.items = []
        for x in items:
            if x not in self.items:
                self.items.append(x)

    def add(self, x):
        if x not in self.items:
            self.items.append(x)

    def discard(self, x):
        if x in self.items:
            self.items.remove(x)

    def __contains__(self, x):
        return x in self.items


class OsDouble:
    def __init__(self):
        self.environ = SimpleNamespace(
            get=lambda name: seam("environ_get", {"name": name}))
        self.path = SimpleNamespace(
            expanduser=lambda path: seam("expanduser", {"raw": path}),
            abspath=lambda path: seam("abspath", {"path": proj(path)}),
            isdir=lambda path: seam("isdir", {"path": proj(path)}))

    def getenv(self, name):
        return seam("getenv", {"name": name})

    def getcwd(self):
        return seam("getcwd", {})


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


# ── extraction ───────────────────────────────────────────────────────────────
def extract():
    src = open(SRC, encoding="utf-8").read()
    mod = types.ModuleType("tui_server_flow2_extracted")
    mod.__package__ = "tui_gateway"
    sys.modules[mod.__name__] = mod
    ns = mod.__dict__
    ns.update({"json": json, "contextlib": contextlib, "os": OsDouble(),
               "Path": Path, "PurePosixPath": PurePosixPath, "copy": copy,
               "logger": RecLogger(), "time": SimpleNamespace(),
               "ConfigBackendUnavailable": ConfigBackendUnavailable,
               "ProfileUnavailableError": ProfileUnavailableError})
    tree = ast.parse(src)
    # module constants (plain-Name Assign / AnnAssign), seeded with contextlib.suppress
    for node in tree.body:
        if isinstance(node, ast.Assign) and all(isinstance(t, ast.Name) for t in node.targets):
            with contextlib.suppress(Exception):
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
        elif isinstance(node, ast.AnnAssign) and node.value and isinstance(node.target, ast.Name):
            with contextlib.suppress(Exception):
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    import __future__
    flags = __future__.annotations.compiler_flag
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in TARGETS:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec", flags=flags), ns)
    for name in TARGETS:
        assert name in ns, "extraction missed %s" % name
    for name in ("_CWD_PLACEHOLDERS", "_MODEL_SWITCH_MARKER_PREFIX"):
        assert name in ns, "constant missed %s" % name
    return ns


# ── collaborator recorders (module-global / bare-global seams) ──────────────
def _path_rec(name):
    def f():
        return PathDouble(seam(name, {}))
    return f


COLLAB = {
    "_ws_orphan_setting": lambda env_var, cfg_key, default: seam(
        "_ws_orphan_setting", {"env_var": env_var, "cfg_key": cfg_key, "default": default}),
    "_launch_configured_cwd": lambda: seam("launch_configured_cwd", {}),
    "_pet_cfg": lambda: seam("pet_cfg", {}),
    "_canonical_profile_request": lambda value: seam("canonical_profile_request", {"value": value}),
    "_current_profile_name": lambda: seam("current_profile_name", {}),
    "_profile_home": lambda name: seam("profile_home", {"name": name}),
    "get_hermes_home": _path_rec("get_hermes_home"),
    "_spawn_trees_root": _path_rec("spawn_trees_root"),
    "is_truthy_value": lambda value, default=False: seam("is_truthy_value", {"value": value, "default": default}),
}
# The C seam name for _launch_configured_cwd is "launch_configured_cwd"; emit under that.
COLLAB["_launch_configured_cwd"] = lambda: seam("launch_configured_cwd", {})


def install_import_stubs():
    def stub(modname, **attrs):
        parts = modname.split(".")
        for i in range(1, len(parts) + 1):
            pn = ".".join(parts[:i])
            if pn not in sys.modules:
                m = types.ModuleType(pn)
                m.__path__ = []
                sys.modules[pn] = m
            if i > 1:
                setattr(sys.modules[".".join(parts[:i - 1])], parts[i - 1], sys.modules[pn])
        m = sys.modules[modname]
        for k, v in attrs.items():
            setattr(m, k, v)
        return m

    stub("hermes_cli.config",
         load_config=lambda: seam("load_config", {}))
    stub("agent.pet",
         constants=SimpleNamespace(DEFAULT_SCALE=0.33))

REAL = {}


def setup_case(ns, func, inp):
    CUR["func"] = func
    CUR["in"] = inp
    CUR["trace"] = []
    CUR["io"] = inp.get("io") or {}
    CUR["pos"] = {}
    CUR["raises"] = []
    for nm in TARGETS:
        ns[nm] = REAL[nm]
    for nm, factory in COLLAB.items():
        if nm != func:
            ns[nm] = factory
    ns["os"] = OsDouble()
    ns["logger"] = RecLogger()
    ns["_pet_cancel_lock"] = contextlib.nullcontext()   # no-op lock for the cancel set
    # in-function imports bind the per-case recorder on the stub module
    sys.modules["hermes_cli.config"].load_config = lambda: seam("load_config", {})
    # materialize the cancel set (dedup list, insertion order)
    stt = inp.get("state") or {}
    arr = stt.get("cancel") or []
    seen = []
    for t in arr:
        if t not in seen:
            seen.append(t)
    ns["_pet_cancelled"] = OrderedSet(seen)


def drive(ns, func, inp):
    f = ns[func]
    if func == "_ws_orphan_setting":
        return f(inp.get("env_var"), inp.get("cfg_key"), inp.get("default"))
    if func == "_resolve_ws_orphan_reap_grace":
        return f()
    if func == "_configured_cwd_from_cfg":
        return f(inp.get("cfg"))
    if func == "_default_session_cwd":
        return f()
    if func == "_pet_cfg":
        return f()
    if func == "_pet_config_scale":
        return f()
    if func == "_pet_cancel_arm":
        return f(inp.get("token"))
    if func == "_pet_cancel_request":
        return f(inp.get("token"))
    if func == "_pet_is_cancelled":
        return f(inp.get("token"))
    if func == "_response_profile_name":
        return f(inp.get("profile"))
    if func == "_pet_gen_root":
        return f()
    if func == "_spawn_trees_root":
        return f()
    if func == "_spawn_tree_session_dir":
        return f(inp.get("session_id"))
    raise ValueError("unknown func: %s" % func)


def run_case(ns, func, inp):
    setup_case(ns, func, inp)
    try:
        ret = drive(ns, func, inp)
        want_ret = norm(ret)
    except BaseException as e:
        want_ret = None
        for r in CUR["raises"]:
            if e is r.exc:
                want_ret = r.spec
                break
        if want_ret is None:
            want_ret = {"__raise__": type(e).__name__, "message": str(e)}
    return {"ret": want_ret, "trace": CUR["trace"], "state": norm_state(ns)}


def norm(x):
    if isinstance(x, (PurePath, PathDouble)):
        return x.as_posix()
    if isinstance(x, BaseException):
        return str(x)
    if isinstance(x, SimpleNamespace):
        return {k: norm(v) for k, v in vars(x).items()}
    if isinstance(x, dict):
        return {k: norm(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [norm(v) for v in x]
    return x


def norm_state(ns):
    return {"cancel": list(ns["_pet_cancelled"].items)}


def canon(x):
    if isinstance(x, BaseException):
        return ("str", str(x))
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


def main():
    ns = extract()
    REAL.update({nm: ns[nm] for nm in TARGETS})
    install_import_stubs()
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
        want = run_case(ns, func, inp)
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("ORACLE tui_server_flow2: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())
