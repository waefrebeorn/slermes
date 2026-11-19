#!/usr/bin/env python3
"""Oracle for the hermes_cli/config_migrations.py UNCLAIMED-remainder FLOW port
(src/cli/config_migrations_flow2.c).

Drives the LIVE config_migrations.py registry-helper / migration-step defs with
SCRIPTED I/O: every collaborator is a recording stub fed the same scripts the C
harness feeds its seams. Compares the DECISION SEQUENCE (ordered seam names in
"trace"), the return ("ret"), the mutated raw_config mirror ("state", restricted
to the keys the case seeded), the mutated caller "results" dict and captured
"stdout". The claimed sibling _cfg is SEAMED; the ported siblings (_dict_at,
_commit, _rewrite_key, _rewrite_stale_default(s), _local_files_allowed,
_lower_is, _prompt_caching_child_lines, read_raw_config, _persist_migration)
stay REAL so nested decision logic is exercised, matching the C compositions.

Live defs are AST-extracted (FunctionDef pass then Assign/AnnAssign pass, with
__future__.annotations so annotations stay strings). The heavy quarry import
chain is never imported. Fail-closed: a missing seam script raises
RuntimeError("unscripted seam: <name>"); 0 cases => exit 1. Summary:
'ORACLE config_migrations_flow2: N cases, M mismatches'.

Representation contract (both sides): paths are posix strings (FakePath);
set results project to SORTED arrays; an uncaught exception rides as
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
SRC = str(QUARRY / "hermes_cli" / "config_migrations.py")

PY = {
    "local_files_allowed": "_local_files_allowed",
    "read_raw_config": "read_raw_config",
    "persist_migration": "_persist_migration",
    "dict_at": "_dict_at",
    "commit": "_commit",
    "rewrite_key": "_rewrite_key",
    "rewrite_stale_default": "_rewrite_stale_default",
    "rewrite_stale_defaults": "_rewrite_stale_defaults",
    "lower_is": "_lower_is",
    "installed_user_plugins": "_installed_user_plugins",
    "migrate_to_41": "_migrate_to_41",
    "migrate_to_44": "_migrate_to_44",
    "migrate_to_45": "_migrate_to_45",
    "migrate_to_46": "_migrate_to_46",
    "migrate_to_48": "_migrate_to_48",
    "migrate_to_49": "_migrate_to_49",
    "migrate_to_50": "_migrate_to_50",
    "prompt_caching_child_lines": "_prompt_caching_child_lines",
    "migrate_to_54": "_migrate_to_54",
    "disable_suspicious_mcp_servers": "disable_suspicious_mcp_servers",
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


class FileDouble:
    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False


class FakePath:
    """Posix path double: / and .name lexical; filesystem probes are seams."""
    def __init__(self, s, ctx=None):
        self._s = str(s)
        self._ctx = ctx

    def __truediv__(self, other):
        base = self._s.rstrip("/") if self._s != "/" else ""
        return FakePath(base + "/" + str(other), self._ctx)

    @property
    def name(self):
        return self._s.rstrip("/").rsplit("/", 1)[-1]

    def is_dir(self):
        return CURRENT["ctx"].recorder("path.is_dir")(self)

    def is_file(self):
        return CURRENT["ctx"].recorder("path.is_file")(self)

    def exists(self):
        return CURRENT["ctx"].recorder("path.exists")(self)

    def iterdir(self):
        ret = CURRENT["ctx"].recorder("path.iterdir")(self)
        if isinstance(ret, list):
            return [FakePath(p, self._ctx) for p in ret]
        return ret

    def read_text(self, encoding=None, errors=None):
        ret = CURRENT["ctx"].recorder("path.read_text")(self, encoding=encoding)
        if isinstance(ret, str) and ret.startswith("\ufeff"):
            ret = ret[1:]
        return ret

    def write_text(self, text, encoding=None, errors=None):
        return CURRENT["ctx"].recorder("path.write_text")(self, text, encoding=encoding)

    def __lt__(self, other):
        return self._s < other._s

    def __eq__(self, other):
        return isinstance(other, FakePath) and self._s == other._s

    def __hash__(self):
        return hash(self._s)

    def __fspath__(self):
        return self._s

    def __str__(self):
        return self._s

    def __repr__(self):
        return f"FakePath({self._s!r})"


class CaseCtx:
    def __init__(self, inp):
        self.inp = inp
        self.trace = []
        self.stdout = []
        self.io = inp.get("io", {}) or {}
        self.cursors = {}
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


def convert_seam_ret(name, ret):
    ctx = CURRENT["ctx"]
    if name in ("config.get_config_path", "config.get_hermes_home", "get_hermes_home"):
        return FakePath(ret, ctx) if isinstance(ret, str) else ret
    if name in ("_configurable_keys", "_get_plugin_toolset_keys"):
        return set(ret) if isinstance(ret, list) else ret
    if name == "_roster":
        if isinstance(ret, list):
            out = []
            for item in ret:
                if isinstance(item, (list, tuple)):
                    if len(item) == 2:
                        out.append((item[0], FakePath(item[1], ctx) if isinstance(item[1], str) else item[1]))
                    else:
                        out.append(tuple(item))
                else:
                    out.append(item)
            return out
        return ret
    return ret


def _import_guard_module(modname, script_name, call_names, consts=None):
    m = types.ModuleType(modname)
    for k, v in (consts or {}).items():
        setattr(m, k, v)

    def getattr_(name):
        ctx = CURRENT["ctx"]
        entry = ctx.peek(script_name)
        if isinstance(entry, dict) and isinstance(entry.get("import_raise"), dict):
            ctx.consume(script_name)
            raise make_exc(entry["import_raise"])
        if name in call_names:
            return ctx.recorder(name)
        raise AttributeError(name)

    m.__getattr__ = getattr_
    return m


def _ensure_pkg(name):
    if name not in sys.modules:
        m = types.ModuleType(name)
        m.__path__ = []
        sys.modules[name] = m
    return sys.modules[name]


def _config_defaults_consts():
    src = open(str(QUARRY / "hermes_cli" / "config_defaults.py"), encoding="utf-8").read()
    tree = ast.parse(src)
    want = {"DEFAULT_SANDBOX_IMAGE", "LEGACY_SANDBOX_IMAGES",
            "DEFAULT_VERCEL_IMAGE", "LEGACY_VERCEL_RUNTIME"}
    ns = {}
    for node in tree.body:
        if isinstance(node, ast.Assign):
            tgt = node.targets[0]
            if isinstance(tgt, ast.Name) and tgt.id in want:
                exec(compile(ast.Module(body=[node], type_ignores=[]), "config_defaults.py", "exec"), ns)
    return {k: ns[k] for k in want if k in ns}


def install_stubs(ctx, inp):
    ns = NS
    rec = ctx.recorder

    def mod(name, **attrs):
        m = types.ModuleType(name)
        for k, v in attrs.items():
            setattr(m, k, v)
        sys.modules[name] = m
        return m

    _ensure_pkg("hermes_cli")
    _ensure_pkg("tools")
    _ensure_pkg("agent")

    # state-backed config-module seams (AUTO; ret/raise override).
    def read_raw_stub():
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
        ctx.trace.append("config._persist_migration")
        entry = ctx.peek("config._persist_migration")
        if isinstance(entry, dict) and isinstance(entry.get("raise"), dict):
            ctx.consume("config._persist_migration")
            raise make_exc(entry["raise"])
        ctx.consume("config._persist_migration")
        ctx.state["raw_config"] = copy.deepcopy(config)
        return None

    # claimed sibling _cfg: returns the config-module double (ret ignored).
    def cfg_stub():
        ctx.trace.append("cfg")
        entry = ctx.peek("cfg")
        if entry is None:
            raise RuntimeError("unscripted seam: cfg")
        ctx.consume("cfg")
        if isinstance(entry, dict) and isinstance(entry.get("raise"), dict):
            raise make_exc(entry["raise"])
        return MODULE

    MODULE = types.SimpleNamespace(
        read_raw_config=read_raw_stub,
        _persist_migration=persist_stub,
        get_config_path=rec("config.get_config_path"),
        get_hermes_home=rec("config.get_hermes_home"),
        get_env_value_prefer_dotenv=rec("config.get_env_value_prefer_dotenv"),
        remove_env_value=rec("config.remove_env_value"),
        fast_safe_load=rec("config.fast_safe_load"),
    )
    ns["_cfg"] = cfg_stub
    ns["_CONFIG_ONLY"] = types.SimpleNamespace(get=rec("CONFIG_ONLY.get"))

    def print_stub(*a, **k):
        ctx.trace.append("print")
        entry = ctx.peek("print")
        if isinstance(entry, dict) and isinstance(entry.get("raise"), dict):
            ctx.consume("print")
            raise make_exc(entry["raise"])
        ctx.consume("print")
        ctx.stdout.append(a[0] if a else "")
        return None
    ns["print"] = print_stub

    def open_stub(*a, **k):
        ctx.trace.append("open")
        entry = ctx.peek("open")
        if entry is None:
            raise RuntimeError("unscripted seam: open")
        ctx.consume("open")
        if isinstance(entry, dict) and isinstance(entry.get("raise"), dict):
            raise make_exc(entry["raise"])
        return FileDouble()
    ns["open"] = open_stub

    cd = _config_defaults_consts()
    sys.modules["hermes_constants"] = _import_guard_module(
        "hermes_constants", "get_hermes_home", {"get_hermes_home"})
    sys.modules["tools.bot_mode_probe"] = _import_guard_module(
        "tools.bot_mode_probe", "_roster",
        {"_hermes_root", "_roster", "strip_legacy_protocol"},
        consts={"_PROTOCOL_HEADING": "## Messaging other agents"})
    sys.modules["agent.skill_utils"] = _import_guard_module(
        "agent.skill_utils", "parse_config_string_list", {"parse_config_string_list"})
    sys.modules["hermes_cli.tools_config"] = _import_guard_module(
        "hermes_cli.tools_config", "_configurable_keys",
        {"_configurable_keys", "_get_plugin_toolset_keys"})
    sys.modules["hermes_cli.toolset_scope"] = _import_guard_module(
        "hermes_cli.toolset_scope", "toolset_allowed_for_platform",
        {"toolset_allowed_for_platform"})
    sys.modules["tools.mcp_tool_common"] = _import_guard_module(
        "tools.mcp_tool_common", "_parse_boolish", {"_parse_boolish"})
    sys.modules["hermes_cli.config_backend"] = _import_guard_module(
        "hermes_cli.config_backend", "supports_file_tooling", {"supports_file_tooling"})
    sys.modules["hermes_cli.mcp_security"] = _import_guard_module(
        "hermes_cli.mcp_security", "validate_mcp_server_entry",
        {"validate_mcp_server_entry"})
    sys.modules["hermes_cli.config_defaults"] = mod("hermes_cli.config_defaults", **cd)


def make_match(d):
    k = d.get("kind")
    if k == "eq":
        v = d.get("value")
        return lambda cur: cur == v
    if k == "lower_is":
        return NS["_lower_is"](d.get("word", ""))
    if k == "none_or_lower_is":
        li = NS["_lower_is"](d.get("word", ""))
        return lambda cur: cur is None or li(cur)
    if k == "not_none":
        return lambda cur: cur is not None
    if k == "always_true":
        return lambda _cur: True
    raise ValueError("unknown match kind: %r" % k)


def make_guard(d):
    if d is None:
        return lambda _m: True
    k = d.get("kind", "always_true")
    if k == "always_true":
        return lambda _m: True
    if k == "is_true":
        key = d.get("key", "")
        return lambda raw: raw.get(key) is True
    if k == "has_key":
        key = d.get("key", "")
        return lambda raw: key in raw
    if k == "lacks_key":
        key = d.get("key", "")
        return lambda raw: key not in raw
    raise ValueError("unknown guard kind: %r" % k)


def proj(v):
    if isinstance(v, dict):
        return {k: proj(x) for k, x in v.items()}
    if isinstance(v, (tuple, list)):
        return [proj(x) for x in v]
    if isinstance(v, (set, frozenset)):
        return sorted(v)
    if isinstance(v, FakePath):
        return str(v)
    if isinstance(v, Path):
        return v.as_posix()
    if isinstance(v, BaseException):
        return exc_spec(v)
    return v


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
    sys.modules.setdefault("hermes_cli.config_migrations", types.ModuleType("hermes_cli.config_migrations"))
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    # FunctionDefs FIRST so module-level partials (_migrate_to_44) resolve.
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec", flags=ANN_FLAG), ns)
            except Exception:
                pass
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgt = node.targets[0] if isinstance(node, ast.Assign) else node.target
            if not isinstance(tgt, ast.Name):
                continue
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec", flags=ANN_FLAG), ns)
            except Exception:
                pass
    ns["logger"] = logging.getLogger("hermes_cli.config_migrations")
    need = list(PY.values()) + [
        "_CONFIG_ONLY", "_RETIRED_TIRITH_KEYS", "_LOCAL_WHISPER_MODELS",
        "_TEMPLATE_CACHE_TTL_LINE", "_TOP_LEVEL_KEY", "SUPPORT_FLOOR_VERSION",
    ]
    missing = [v for v in need if v not in ns]
    if missing:
        print("FATAL config_migrations_flow2: extraction missing names:", sorted(set(missing)))
        sys.exit(2)
    return ns


NS = extract()


def run_case(func, inp):
    ctx = CaseCtx(inp)
    CURRENT["ctx"] = ctx
    install_stubs(ctx, inp)
    ns = NS
    quiet = inp.get("quiet", False)
    results = copy.deepcopy(inp.get("results") or {})
    out = {"ret": None, "trace": ctx.trace}
    try:
        if func == "local_files_allowed":
            out["ret"] = ns["_local_files_allowed"]()
        elif func == "read_raw_config":
            out["ret"] = ns["read_raw_config"]()
        elif func == "persist_migration":
            ns["_persist_migration"](copy.deepcopy(inp.get("config") or {}))
            out["ret"] = None
        elif func == "dict_at":
            out["ret"] = ns["_dict_at"](copy.deepcopy(inp.get("config") or {}), inp.get("key", ""))
        elif func == "commit":
            ns["_commit"](copy.deepcopy(inp.get("config") or {}), results, quiet,
                          inp.get("added"), inp.get("message"))
            out["ret"] = None
        elif func == "rewrite_key":
            ns["_rewrite_key"](
                results, quiet, section=inp.get("section", ""), key=inp.get("key", ""),
                match=make_match(inp["match"]), new=inp.get("new"),
                added=inp.get("added"), message=inp.get("message"),
                extra_guard=make_guard(inp.get("extra_guard")),
                create_section=inp.get("create_section", False))
            out["ret"] = None
        elif func == "rewrite_stale_default":
            p = ns["_rewrite_stale_default"](old=inp.get("old"), **(inp.get("kw") or {}))
            kw = dict(p.keywords)
            kw["match"] = {"kind": "eq", "value": inp.get("old")}
            out["ret"] = {"__partial__": "_rewrite_key", "kw": kw}
        elif func == "rewrite_stale_defaults":
            rows = [tuple(r) for r in (inp.get("rows") or [])]
            ns["_rewrite_stale_defaults"](results, quiet, section=inp.get("section", ""), rows=rows)
            out["ret"] = None
        elif func == "lower_is":
            out["ret"] = ns["_lower_is"](inp.get("word", ""))(inp.get("cur"))
        elif func == "installed_user_plugins":
            out["ret"] = ns["_installed_user_plugins"](set(inp.get("disabled") or []))
        elif func == "migrate_to_41":
            ns["_migrate_to_41"](results, quiet); out["ret"] = None
        elif func == "migrate_to_44":
            ns["_migrate_to_44"](results, quiet); out["ret"] = None
        elif func == "migrate_to_45":
            ns["_migrate_to_45"](results, quiet); out["ret"] = None
        elif func == "migrate_to_46":
            ns["_migrate_to_46"](results, quiet); out["ret"] = None
        elif func == "migrate_to_48":
            ns["_migrate_to_48"](results, quiet); out["ret"] = None
        elif func == "migrate_to_49":
            ns["_migrate_to_49"](results, quiet); out["ret"] = None
        elif func == "migrate_to_50":
            ns["_migrate_to_50"](results, quiet); out["ret"] = None
        elif func == "prompt_caching_child_lines":
            out["ret"] = ns["_prompt_caching_child_lines"](inp.get("text"))
        elif func == "migrate_to_54":
            ns["_migrate_to_54"](results, quiet); out["ret"] = None
        elif func == "disable_suspicious_mcp_servers":
            ns["disable_suspicious_mcp_servers"](results, quiet); out["ret"] = None
        else:
            raise KeyError(func)
    except BaseException as e:
        out["ret"] = {"__raise__": exc_spec(e)}
    out["ret"] = proj(out["ret"])
    out["state"] = {k: proj(ctx.state[k]) for k in (inp.get("state") or {}) if k in ctx.state}
    out["results"] = proj(results)
    out["stdout"] = ctx.stdout
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
        ok = all(deep_eq(got.get(k), want.get(k)) for k in ("ret", "trace", "state", "results", "stdout"))
        if not ok:
            mism += 1
            print(f"MISMATCH {func}")
            print(f"  C  ={json.dumps(got, sort_keys=True, default=str)}")
            print(f"  PY ={json.dumps(want, sort_keys=True, default=str)}")
    print(f"ORACLE config_migrations_flow2: {cases} cases, {mism} mismatches")
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())