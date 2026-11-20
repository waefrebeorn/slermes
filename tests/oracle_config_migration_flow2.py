#!/usr/bin/env python3
"""Oracle for the config-migration registry UNCLAIMED-remainder FLOW port
(src/cli/config_migration_flow2.c).

Drives the LIVE defs of hermes_cli/config_migrations.py (_migrate_to_34 /
_migrate_to_38 / _migrate_to_39) and its registered browser sibling
hermes_cli/config_migrations_browser.py (9 defs, migrate_to_52 et al.) with
SCRIPTED I/O: every collaborator is a recording stub fed the same scripts the
C harness feeds its seams. Compares the DECISION SEQUENCE (ordered seam names
in "trace"), the return ("ret"), the mutated raw_config mirror ("state",
restricted to the keys the case seeded), the mutated caller "results" dict,
captured "stdout", and the mutated in-place arguments ("browser" for
move_agent_browser_env, "config" for adopt_agent_browser_env).

Claimed siblings are SEAMED (read_raw_config / _persist_migration / _commit
from config_migrations.py; record_core_use / defer_startup_notice from
left_core_migration.py). The ported defs stay REAL so nested decision logic is
exercised, matching the C compositions. The "commit" seam models the claimed
_commit's observable effects atomically (persist + config_added append +
conditional stdout), mirroring the C dispatcher's AUTO-effect branch.

Live defs are AST-extracted (FunctionDef pass then Assign/AnnAssign pass, with
__future__.annotations so annotations stay strings). The heavy quarry import
chain is never imported: in-function imports resolve against sys.modules stub
modules whose __getattr__ serves the recording seams and gates import_raise.
Fail-closed: a missing seam script raises RuntimeError("unscripted seam:
<name>"); 0 cases => exit 1; a missing extracted name => exit 2. Summary:
'ORACLE config_migration_flow2: N cases, M mismatches'.

Representation contract (both sides): paths are POSIX strings (BrowserPath
double; expanduser/resolve are seams, is_relative_to lexical); os.path.isabs is
the lexical posix model; move_agent_browser_env's tuple projects to
{"moved":[...],"pinned":[...]}; an uncaught exception rides as
{"__raise__": {type,message,code?,name?}}; SystemExit carries code; a scripted
KeyError message is the RAW key (both sides repr it). Integral JSON numbers read
as int; bool is distinct from int in deep_eq. Fixture input domain is ASCII.
"""
import ast
import builtins
import copy
import json
import os
import sys
import types
import __future__
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "hermes_cli" / "config_migrations.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC_CM = str(QUARRY / "hermes_cli" / "config_migrations.py")
SRC_BR = str(QUARRY / "hermes_cli" / "config_migrations_browser.py")

# func -> (namespace, live name)
PY = {
    "migrate_to_34": ("NS", "_migrate_to_34"),
    "migrate_to_38": ("NS", "_migrate_to_38"),
    "migrate_to_39": ("NS", "_migrate_to_39"),
    "agent_browser_env": ("NSB", "_agent_browser_env"),
    "inside_pm_store": ("NSB", "_inside_pm_store"),
    "browser_value_from_env": ("NSB", "_browser_value_from_env"),
    "config_decided": ("NSB", "_config_decided"),
    "backend_removed_note": ("NSB", "_backend_removed_note"),
    "move_agent_browser_env": ("NSB", "move_agent_browser_env"),
    "agent_browser_env_pending": ("NSB", "agent_browser_env_pending"),
    "adopt_agent_browser_env": ("NSB", "adopt_agent_browser_env"),
    "migrate_to_52": ("NSB", "migrate_to_52"),
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
            # these builtins reject a single-arg __init__; bypass safely.
            e = cls.__new__(cls)
            Exception.__init__(e, msg)
        else:
            e = cls(msg)
    else:
        e = Exception(msg)
    if t == "ModuleNotFoundError":
        e.name = spec.get("name")
    return e


def exc_spec(e):
    spec = {"type": type(e).__name__, "message": str(e)}
    if isinstance(e, SystemExit):
        spec["code"] = e.code
    if isinstance(e, ModuleNotFoundError):
        spec["name"] = getattr(e, "name", None)
    return spec


class BrowserPath:
    """Posix path double: expanduser/resolve are seams; is_relative_to lexical."""
    def __init__(self, s):
        self._s = str(s)

    def expanduser(self):
        ret = CURRENT["ctx"].recorder("path.expanduser")(self._s)
        return BrowserPath(ret) if isinstance(ret, str) else ret

    def resolve(self):
        ret = CURRENT["ctx"].recorder("path.resolve")(self._s)
        return BrowserPath(ret) if isinstance(ret, str) else ret

    def is_relative_to(self, other):
        a = [p for p in self._s.split("/") if p]
        b = [p for p in str(other).split("/") if p]
        return len(a) >= len(b) and a[:len(b)] == b

    def __str__(self):
        return self._s

    def __fspath__(self):
        return self._s

    def __repr__(self):
        return f"BrowserPath({self._s!r})"

    def __eq__(self, other):
        return isinstance(other, BrowserPath) and self._s == other._s

    def __hash__(self):
        return hash(self._s)


class CaseCtx:
    def __init__(self, inp):
        self.inp = inp
        self.trace = []
        self.stdout = []
        self.io = inp.get("io", {}) or {}
        self.cursors = {}
        self.state = copy.deepcopy(inp.get("state", {}) or {})
        self.results = None   # set by run_case

    def peek(self, name):
        entry = self.io.get(name)
        if entry is None:
            return _UNSCRIPTED
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            return entry[idx] if idx < len(entry) else _UNSCRIPTED
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
            if entry is _UNSCRIPTED:
                raise RuntimeError("unscripted seam: " + name)
            ctx.consume(name)
            if isinstance(entry, dict):
                eff = entry.get("state")
                if isinstance(eff, dict):
                    ctx.state.update(copy.deepcopy(eff))
                if isinstance(entry.get("import_raise"), dict):
                    raise make_exc(entry["import_raise"])
                if isinstance(entry.get("raise"), dict):
                    raise make_exc(entry["raise"])
                ret = entry.get("ret")
            else:
                ret = entry
            return convert_seam_ret(name, ret)
        return call


CURRENT = {"ctx": None}

# Sentinel: a seam with NO script (missing key, scalar null, or exhausted
# queue). Distinct from a scripted JSON null QUEUE ELEMENT, which is a real
# None return (e.g. os.environ.get for an unset var) — the C harness returns
# it as a JSON_NULL node and only raises fail-closed on missing/exhausted.
_UNSCRIPTED = object()


def convert_seam_ret(name, ret):
    if name in ("pm.store_root", "pm.writable_store_root", "path.expanduser", "path.resolve"):
        return BrowserPath(ret) if isinstance(ret, str) else ret
    return ret


def _ensure_pkg(name):
    if name not in sys.modules:
        m = types.ModuleType(name)
        m.__path__ = []
        sys.modules[name] = m
    return sys.modules[name]


def _guard_module(modname, attrs):
    """attrs: {live_name: (seam_name, resolver)}. __getattr__ gates import_raise
    on the seam's own script entry, then serves the resolver's stub."""
    m = types.ModuleType(modname)

    def getattr_(name):
        ctx = CURRENT["ctx"]
        if name not in attrs:
            raise AttributeError(name)
        seam, resolver = attrs[name]
        entry = ctx.peek(seam)
        if isinstance(entry, dict) and isinstance(entry.get("import_raise"), dict):
            ctx.consume(seam)
            raise make_exc(entry["import_raise"])
        return resolver()

    m.__getattr__ = getattr_
    return m


def extract():
    import contextvars
    import functools
    import logging
    import re as _re
    from typing import Any, Callable, Dict, Iterable, List, Optional, Tuple
    from urllib.parse import urlparse

    ns = {
        "__name__": "hermes_cli.config_migrations",
        "contextvars": contextvars, "copy": copy, "functools": functools,
        "logging": logging, "re": _re, "urlparse": urlparse,
        "Any": Any, "Callable": Callable, "Dict": Dict, "Iterable": Iterable,
        "List": List, "Optional": Optional, "Tuple": Tuple,
        "logger": logging.getLogger("hermes_cli.config_migrations"),
    }
    tree = ast.parse(open(SRC_CM, encoding="utf-8").read())
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC_CM, "exec", flags=ANN_FLAG), ns)
            except Exception:
                pass
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgt = node.targets[0] if isinstance(node, ast.Assign) else node.target
            if not isinstance(tgt, ast.Name):
                continue
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC_CM, "exec", flags=ANN_FLAG), ns)
            except Exception:
                pass

    nsb = {
        "__name__": "hermes_cli.config_migrations_browser",
        "os": os, "re": _re, "Any": Any,
    }
    btree = ast.parse(open(SRC_BR, encoding="utf-8").read())
    for node in btree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC_BR, "exec", flags=ANN_FLAG), nsb)
            except Exception:
                pass
    for node in btree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgt = node.targets[0] if isinstance(node, ast.Assign) else node.target
            if not isinstance(tgt, ast.Name):
                continue
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC_BR, "exec", flags=ANN_FLAG), nsb)
            except Exception:
                pass

    need_ns = ["_migrate_to_34", "_migrate_to_38", "_migrate_to_39"]
    need_nsb = [
        "_agent_browser_env", "_inside_pm_store", "_browser_value_from_env",
        "_config_decided", "_backend_removed_note", "move_agent_browser_env",
        "agent_browser_env_pending", "adopt_agent_browser_env", "migrate_to_52",
        "_AGENT_BROWSER_ENV_KEYS", "_BACKEND_REASONS",
    ]
    missing = [v for v in need_ns if v not in ns] + [v for v in need_nsb if v not in nsb]
    if missing:
        print("FATAL config_migration_flow2: extraction missing names:", sorted(set(missing)))
        sys.exit(2)
    return ns, nsb


NS, NSB = extract()


def install_stubs(ctx, inp):
    rec = ctx.recorder

    def mod(name, **attrs):
        m = types.ModuleType(name)
        for k, v in attrs.items():
            setattr(m, k, v)
        sys.modules[name] = m
        return m

    _ensure_pkg("hermes_cli")
    _ensure_pkg("pm")

    # AUTO state-backed read_raw_config (config_migrations binding).
    def read_raw_stub():
        ctx.trace.append("read_raw_config")
        entry = ctx.peek("read_raw_config")
        if isinstance(entry, dict) and isinstance(entry.get("raise"), dict):
            ctx.consume("read_raw_config")
            raise make_exc(entry["raise"])
        ctx.consume("read_raw_config")
        if isinstance(entry, dict) and "ret" in entry:
            return copy.deepcopy(entry["ret"])
        return copy.deepcopy(ctx.state.get("raw_config", {}))

    # AUTO state-backed config.read_raw_config (hermes_cli.config binding).
    def config_read_raw_stub():
        ctx.trace.append("config.read_raw_config")
        entry = ctx.peek("config.read_raw_config")
        if isinstance(entry, dict) and isinstance(entry.get("raise"), dict):
            ctx.consume("config.read_raw_config")
            raise make_exc(entry["raise"])
        ctx.consume("config.read_raw_config")
        if isinstance(entry, dict) and "ret" in entry:
            return copy.deepcopy(entry["ret"])
        return copy.deepcopy(ctx.state.get("raw_config", {}))

    def persist_stub(config):
        ctx.trace.append("persist_migration")
        entry = ctx.peek("persist_migration")
        if isinstance(entry, dict) and isinstance(entry.get("raise"), dict):
            ctx.consume("persist_migration")
            raise make_exc(entry["raise"])
        ctx.consume("persist_migration")
        ctx.state["raw_config"] = copy.deepcopy(config)
        return None

    # AUTO effect seam modelling the claimed _commit.
    def commit_stub(config, results, quiet, added, message):
        ctx.trace.append("commit")
        entry = ctx.peek("commit")
        if isinstance(entry, dict) and isinstance(entry.get("raise"), dict):
            ctx.consume("commit")
            raise make_exc(entry["raise"])
        ctx.consume("commit")
        ctx.state["raw_config"] = copy.deepcopy(config)
        if added:
            if "config_added" not in results:
                raise KeyError("config_added")
            arr = results["config_added"]
            if not isinstance(arr, list):
                raise AttributeError(f"'{type(arr).__name__}' object has no attribute 'append'")
            arr.append(added)
        if message and not quiet:
            ctx.stdout.append(message)
        return None

    def print_stub(*a, **k):
        ctx.trace.append("print")
        entry = ctx.peek("print")
        if isinstance(entry, dict) and isinstance(entry.get("raise"), dict):
            ctx.consume("print")
            raise make_exc(entry["raise"])
        ctx.consume("print")
        ctx.stdout.append(a[0] if a else "")
        return None

    # claimed-sibling bare globals in the config_migrations namespace.
    NS["read_raw_config"] = read_raw_stub
    NS["_persist_migration"] = persist_stub
    NS["_commit"] = commit_stub
    NS["print"] = print_stub

    # os double for the browser namespace (environ.get / path.* seams).
    NSB["os"] = types.SimpleNamespace(
        environ=types.SimpleNamespace(get=rec("os.environ.get")),
        path=types.SimpleNamespace(
            isabs=lambda s: str(s).startswith("/"),
            expanduser=rec("os.path.expanduser"),
            abspath=rec("os.path.abspath"),
        ),
    )
    NSB["print"] = print_stub

    # pathlib stub: Path constructs the BrowserPath double (no seam).
    mod("pathlib", Path=BrowserPath)

    # in-function import guards (per-name import_raise on the seam's entry).
    sys.modules["hermes_cli.personality"] = _guard_module("hermes_cli.personality", {
        "available_personalities": ("available_personalities", lambda: rec("available_personalities")),
        "normalize_personality_name": ("normalize_personality_name", lambda: rec("normalize_personality_name")),
        "prompt_text": ("prompt_text", lambda: rec("prompt_text")),
        "render_personality_prompt": ("render_personality_prompt", lambda: rec("render_personality_prompt")),
    })
    sys.modules["hermes_cli.relay_plugin_cutover"] = _guard_module("hermes_cli.relay_plugin_cutover", {
        "legacy_relay_plugin_keys": ("legacy_relay_plugin_keys", lambda: rec("legacy_relay_plugin_keys")),
    })
    sys.modules["hermes_cli.env_loader"] = _guard_module("hermes_cli.env_loader", {
        "resolve_home_dotenv": ("resolve_home_dotenv", lambda: rec("resolve_home_dotenv")),
    })
    sys.modules["hermes_constants"] = _guard_module("hermes_constants", {
        "get_routing_process_hermes_home": ("get_routing_process_hermes_home", lambda: rec("get_routing_process_hermes_home")),
        "hermes_home_key": ("hermes_home_key", lambda: rec("hermes_home_key")),
        "get_hermes_home": ("get_hermes_home", lambda: rec("get_hermes_home")),
    })
    sys.modules["hermes_cli.config_migrations"] = _guard_module("hermes_cli.config_migrations", {
        "_persist_migration": ("persist_migration", lambda: persist_stub),
        "read_raw_config": ("read_raw_config", lambda: read_raw_stub),
    })
    sys.modules["hermes_cli.config"] = _guard_module("hermes_cli.config", {
        "read_raw_config": ("config.read_raw_config", lambda: config_read_raw_stub),
    })
    sys.modules["hermes_cli.left_core_migration"] = _guard_module("hermes_cli.left_core_migration", {
        "record_core_use": ("record_core_use", lambda: rec("record_core_use")),
        "defer_startup_notice": ("defer_startup_notice", lambda: rec("defer_startup_notice")),
    })
    paths_mod = mod("pm.paths",
                    store_root=rec("pm.store_root"),
                    writable_store_root=rec("pm.writable_store_root"))

    def pm_getattr(name):
        ctx2 = CURRENT["ctx"]
        if name != "paths":
            raise AttributeError(name)
        entry = ctx2.peek("pm.paths")
        if isinstance(entry, dict) and isinstance(entry.get("import_raise"), dict):
            ctx2.consume("pm.paths")
            raise make_exc(entry["import_raise"])
        return paths_mod

    pm_pkg = sys.modules["pm"]
    pm_pkg.__getattr__ = pm_getattr
    # NOTE: do NOT set pm_pkg.paths as a real attribute — `from pm import paths`
    # would find it directly and bypass __getattr__'s import_raise gate. Clear
    # any stale real attribute via __dict__ (hasattr would fire __getattr__).
    if "paths" in pm_pkg.__dict__:
        del pm_pkg.__dict__["paths"]


def proj(v):
    if isinstance(v, dict):
        return {k: proj(x) for k, x in v.items()}
    if isinstance(v, (tuple, list)):
        return [proj(x) for x in v]
    if isinstance(v, (set, frozenset)):
        return sorted(v)
    if isinstance(v, BrowserPath):
        return str(v)
    if isinstance(v, Path):
        return v.as_posix()
    if isinstance(v, BaseException):
        return exc_spec(v)
    return v


def run_case(func, inp):
    ctx = CaseCtx(inp)
    CURRENT["ctx"] = ctx
    results = copy.deepcopy(inp.get("results") or {})
    ctx.results = results
    install_stubs(ctx, inp)
    quiet = inp.get("quiet", False)
    home = inp.get("home", "")
    ns, live = PY[func]
    target = (NS if ns == "NS" else NSB)[live]
    out = {"ret": None, "trace": ctx.trace}
    try:
        if func in ("migrate_to_34", "migrate_to_38", "migrate_to_39", "migrate_to_52"):
            target(results, quiet)
            out["ret"] = None
        elif func == "agent_browser_env":
            out["ret"] = target(home)
        elif func == "inside_pm_store":
            out["ret"] = target(inp.get("value", ""))
        elif func == "browser_value_from_env":
            out["ret"] = target(inp.get("key", ""), inp.get("value", ""))
        elif func == "config_decided":
            out["ret"] = target(inp.get("key", ""), inp.get("current"))
        elif func == "backend_removed_note":
            out["ret"] = target(inp.get("backend"))
        elif func == "move_agent_browser_env":
            browser = copy.deepcopy(inp.get("browser") or {})
            moved, pinned = target(home, browser)
            out["ret"] = {"moved": moved, "pinned": pinned}
            out["browser"] = browser
        elif func == "agent_browser_env_pending":
            out["ret"] = target()
        elif func == "adopt_agent_browser_env":
            config = copy.deepcopy(inp.get("config") or {})
            out["ret"] = target(home, config)
            out["config"] = config
        else:
            raise KeyError(func)
    except BaseException as e:
        out["ret"] = {"__raise__": exc_spec(e)}
    out["ret"] = proj(out["ret"])
    out["state"] = {k: proj(ctx.state[k]) for k in (inp.get("state") or {}) if k in ctx.state}
    out["results"] = proj(results)
    out["stdout"] = ctx.stdout
    if "browser" in out:
        out["browser"] = proj(out["browser"])
    if "config" in out:
        out["config"] = proj(out["config"])
    return out


def deep_eq(a, b):
    if isinstance(a, bool) or isinstance(b, bool):
        return isinstance(a, bool) and isinstance(b, bool) and a == b
    if isinstance(a, (int, float)) and isinstance(b, (int, float)):
        return a == b
    if isinstance(a, dict) and isinstance(b, dict):
        return a.keys() == b.keys() and all(deep_eq(a[k], b[k]) for k in a)
    if isinstance(a, list) and isinstance(b, list):
        return len(a) == len(b) and all(deep_eq(x, y) for x, y in zip(a, b))
    if type(a) is not type(b):
        return False
    return a == b


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
        ok = all(deep_eq(got.get(k), want.get(k)) for k in want)
        if not ok:
            mism += 1
            print(f"MISMATCH {func}")
            print(f"  C  ={json.dumps(got, sort_keys=True, default=str)}")
            print(f"  PY ={json.dumps(want, sort_keys=True, default=str)}")
    print(f"ORACLE config_migration_flow2: {cases} cases, {mism} mismatches")
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())