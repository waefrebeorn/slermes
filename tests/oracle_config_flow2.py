#!/usr/bin/env python3
"""Oracle for the hermes_cli/config.py UNCLAIMED-remainder FLOW port
(src/cli/config_flow2.c).

Drives the LIVE config.py command-surface / env-write-guard / manifest-secret /
offer-prompt defs with SCRIPTED I/O: every collaborator is a recording stub fed
the same scripts the C harness feeds its seams. Compares the DECISION SEQUENCE
(ordered seam names in "trace"), the return ("ret"), the mutated module-global
mirror ("state", restricted to the keys the case seeded) and captured print
output ("stdout" / "stderr"). Pure siblings (_split_key_path,
_platform_manifest_env_entries, _is_plugin_dir_name) and the composed ported
siblings (_managed_source, _exit_invalid, _usage_exit, _run_write_command,
_ask_yes_no, _manifest_secret_envs) stay REAL so nested decision logic is
exercised, matching the C port's transcriptions / compositions.

Live defs are AST-extracted (module Assign/AnnAssign + FunctionDef exec). The
heavy quarry import chain is never imported. Fail-closed: a missing seam script
raises RuntimeError("unscripted seam: <name>"); 0 cases => exit 1. Summary:
'ORACLE config_flow2: N cases, M mismatches'.

Representation contract (both sides): paths are posix strings (FakePath);
frozenset/set results project to SORTED arrays; an uncaught exception rides as
{"__raise__": {type,message,code?,...}}; SystemExit carries code. Two cosmetic
seams are AUTO (print/color); every other seam is scripted + fail-closed.
"""
import ast
import builtins
import copy
import json
import os
import sys
import types
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "hermes_cli" / "config.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "hermes_cli" / "config.py")

PY = {
    "format_docker_update_message": "format_docker_update_message",
    "module_getattr": "__getattr__",
    "managed_source": "_managed_source",
    "env_write_refusal": "env_write_refusal",
    "env_write_blocked": "_env_write_blocked",
    "require_env_writable": "require_env_writable",
    "write_anthropic_slots": "_write_anthropic_slots",
    "scoped_environ_get": "_scoped_environ_get",
    "legacy_gateway_platforms_key": "_legacy_gateway_platforms_key",
    "exit_if_key_managed": "_exit_if_key_managed",
    "touch_skin_file": "_touch_skin_file",
    "exit_invalid": "_exit_invalid",
    "write_user_config": "_write_user_config",
    "print_unknown_key_notice": "_print_unknown_key_notice",
    "dropped_shadow_notes": "_dropped_shadow_notes",
    "usage_exit": "_usage_exit",
    "run_write_command": "_run_write_command",
    "cmd_config_get": "_cmd_config_get",
    "cmd_config_set": "_cmd_config_set",
    "cmd_config_unset": "_cmd_config_unset",
    "tools_suffix": "_tools_suffix",
    "print_banner": "_print_banner",
    "cmd_config_show": "_cmd_config_show",
    "is_plugin_dir_name": "_is_plugin_dir_name",
    "platform_manifest_env_entries": "_platform_manifest_env_entries",
    "manifest_secret_envs": "_manifest_secret_envs",
    "platform_manifest_secret_envs": "platform_manifest_secret_envs",
    "platform_manifest_secret_scan": "platform_manifest_secret_scan",
    "ask_yes_no": "_ask_yes_no",
    "offer_list": "_offer_list",
    "prompt_and_save_env": "_prompt_and_save_env",
}


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


class FakePath:
    """Posix path double: exists()/touch() are scripted seams; / and .parent real."""
    def __init__(self, s, ctx=None):
        self._s = str(s)
        self._ctx = ctx

    def __truediv__(self, other):
        base = self._s.rstrip("/") if self._s != "/" else ""
        return FakePath(base + "/" + str(other), self._ctx)

    @property
    def parent(self):
        s = self._s.rstrip("/")
        if "/" not in s:
            return FakePath(".", self._ctx)
        head = s.rsplit("/", 1)[0]
        return FakePath(head if head else "/", self._ctx)

    @property
    def name(self):
        return self._s.rstrip("/").rsplit("/", 1)[-1]

    def exists(self):
        return CURRENT["ctx"].recorder("path_exists")(self)

    def touch(self):
        return CURRENT["ctx"].recorder("touch")(self)

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
        self.stderr = []
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
            if entry is None:   # missing OR queue exhausted -> fail closed
                raise RuntimeError("unscripted seam: " + name)
            ctx.consume(name)
            if isinstance(entry, dict):
                eff = entry.get("state")
                if isinstance(eff, dict):
                    ctx.state.update(copy.deepcopy(eff))
                if isinstance(entry.get("raise"), dict):
                    raise make_exc(entry["raise"])
                ret = entry.get("ret")
            else:
                ret = entry
            return convert_seam_ret(name, ret)
        return call


CURRENT = {"ctx": None}


def convert_seam_ret(name, ret):
    if name in ("get_managed_dir", "get_config_path", "get_hermes_home", "get_project_root"):
        return FakePath(ret, CURRENT["ctx"]) if isinstance(ret, str) else ret
    if name == "core_declared_env_names":
        return frozenset(ret) if isinstance(ret, list) else ret
    if name == "get_config_backend":
        return types.SimpleNamespace(locked=CURRENT["ctx"].recorder("backend_locked"))
    return ret


def _import_guard_module(modname, script_name, call_names):
    m = types.ModuleType(modname)

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


def install_stubs(ctx, inp):
    ns = NS
    rec = ctx.recorder

    def mod(name, **attrs):
        m = types.ModuleType(name)
        for k, v in attrs.items():
            setattr(m, k, v)
        sys.modules[name] = m
        return m

    _ensure_pkg("agent")
    _ensure_pkg("hermes_cli")
    _ensure_pkg("hermes_cli.observability")

    sys.modules["agent.secret_scope"] = _import_guard_module(
        "agent.secret_scope", "get_secret", {"get_secret"})
    sys.modules["hermes_cli.observability.shared_metrics_disabled"] = _import_guard_module(
        "hermes_cli.observability.shared_metrics_disabled",
        "recording_raw_config_write", {"recording_raw_config_write"})
    sys.modules["hermes_cli.config_show"] = _import_guard_module(
        "hermes_cli.config_show", "show_config", {"show_config"})

    ns["managed_scope"] = types.SimpleNamespace(
        is_key_managed=rec("is_key_managed"),
        is_env_managed=rec("is_env_managed"),
        get_managed_dir=rec("get_managed_dir"))
    ns["os"] = types.SimpleNamespace(environ=types.SimpleNamespace(get=rec("os.environ.get")))

    # AUTO cosmetic seams (no script, never fail closed).
    def print_stub(*a, file=None, **k):
        ctx.trace.append("print")
        text = a[0] if a else ""
        if file is sys.stderr:
            ctx.stderr.append(text)
        else:
            ctx.stdout.append(text)
        return None
    ns["print"] = print_stub

    def color_stub(*a, **k):
        ctx.trace.append("color")
        return a[0] if a else ""
    ns["color"] = color_stub

    ns["input"] = rec("input")

    for name in ("is_managed", "format_managed_message", "get_config_backend",
                 "get_config_path", "get_hermes_home", "ensure_hermes_home",
                 "get_config_value", "set_config_value", "unset_config_value",
                 "save_env_value", "core_declared_env_names",
                 "masked_secret_prompt", "line_input"):
        ns[name] = rec(name)
    ns["atomic_config_replace"] = lambda *a, **k: None   # passed as a callback marker

    # claimed navigation siblings: scripted seams (mutation echoed via state).
    def unset_nested_stub(config, dotted_key):
        ctx.trace.append("unset_nested")
        entry = ctx.peek("unset_nested")
        if entry is None:
            raise RuntimeError("unscripted seam: unset_nested")
        ctx.consume("unset_nested")
        if isinstance(entry, dict):
            if isinstance(entry.get("raise"), dict):
                raise make_exc(entry["raise"])
            eff = entry.get("state")
            if isinstance(eff, dict) and "user_config" in eff:
                config.clear()
                config.update(copy.deepcopy(eff["user_config"]))
            return entry.get("ret")
        return entry
    ns["_unset_nested"] = unset_nested_stub
    ns["_locate_nested"] = rec("locate_nested")

    # unclaimed heavy generator sibling: scripted (dir, manifest) pairs + skipped.
    def ppm_stub(home=None, source="all", *, strict=False, skipped=None):
        ctx.trace.append("platform_plugin_manifests")
        entry = ctx.peek("platform_plugin_manifests")
        if entry is None:
            raise RuntimeError("unscripted seam: platform_plugin_manifests")
        ctx.consume("platform_plugin_manifests")
        ret = []
        if isinstance(entry, dict):
            if isinstance(entry.get("raise"), dict):
                raise make_exc(entry["raise"])
            ret = entry.get("ret") or []
            eff = entry.get("state")
            if isinstance(eff, dict):
                for k, v in eff.items():
                    if k == "skipped" and skipped is not None:
                        skipped.clear()
                        skipped.extend(copy.deepcopy(v))
                    else:
                        ctx.state[k] = copy.deepcopy(v)
        else:
            ret = entry or []
        return iter([(d, m) for d, m in ret])
    ns["_platform_plugin_manifests"] = ppm_stub


def seed_state(ctx, inp):
    ns = NS
    st = ctx.state
    if "_DOCKER_UPDATE_MESSAGE" in st:
        ns["_DOCKER_UPDATE_MESSAGE"] = st["_DOCKER_UPDATE_MESSAGE"]
    bundled = st.get("BUNDLED_PLATFORM_SECRET_ENVS")
    ns["BUNDLED_PLATFORM_SECRET_ENVS"] = frozenset(bundled) if isinstance(bundled, list) else None
    if not isinstance(st.get("results"), dict):
        st["results"] = {"env_added": [], "config_added": [], "warnings": []}
    if not isinstance(st.get("user_config"), dict):
        st["user_config"] = {}


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
    import re as _re
    import logging as _logging
    import threading as _threading
    import difflib as _difflib
    import shlex as _shlex
    import shutil as _shutil
    import stat as _stat
    import subprocess as _subprocess
    import tempfile as _tempfile
    import contextlib as _contextlib
    from dataclasses import dataclass, field
    from typing import Dict, Any, Literal, Optional, List, Tuple, Set
    ns = {
        "__name__": "hermes_cli.config",
        "re": _re, "os": os, "sys": sys, "json": json, "copy": copy,
        "logging": _logging, "logger": _logging.getLogger("hermes_cli.config"),
        "threading": _threading, "difflib": _difflib, "shlex": _shlex,
        "shutil": _shutil, "stat": _stat, "subprocess": _subprocess,
        "tempfile": _tempfile, "contextlib": _contextlib,
        "dataclass": dataclass, "field": field, "Path": Path,
        "Dict": Dict, "Any": Any, "Literal": Literal, "Optional": Optional,
        "List": List, "Tuple": Tuple, "Set": Set,
    }
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgt = node.targets[0] if isinstance(node, ast.Assign) else node.target
            if not isinstance(tgt, ast.Name):
                continue
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            except Exception:
                pass
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            except Exception:
                pass
    # Colors is an ImportFrom (hermes_cli.colors), not an Assign — exec that
    # module to bind the real ANSI constant class (color() itself is seamed).
    col_src = open(str(QUARRY / "hermes_cli" / "colors.py"), encoding="utf-8").read()
    col_ns = {"os": os, "sys": sys}
    exec(compile(col_src, "colors.py", "exec"), col_ns)
    ns["Colors"] = col_ns["Colors"]
    missing = [v for v in PY.values() if v not in ns]
    for sib in ("_split_key_path", "_managed_source", "_exit_invalid", "_usage_exit",
                "_run_write_command", "_ask_yes_no", "_platform_manifest_env_entries",
                "_manifest_secret_envs", "Colors", "_USAGE_GET", "_USAGE_SET", "_USAGE_UNSET"):
        if sib not in ns:
            missing.append(sib)
    if missing:
        print("FATAL config_flow2: extraction missing names:", sorted(set(missing)))
        sys.exit(2)
    return ns


NS = extract()


def run_case(func, inp):
    ctx = CaseCtx(inp)
    CURRENT["ctx"] = ctx
    install_stubs(ctx, inp)
    seed_state(ctx, inp)
    ns = NS
    out = {"ret": None, "trace": ctx.trace}
    try:
        if func == "format_docker_update_message":
            out["ret"] = ns["format_docker_update_message"]()
        elif func == "module_getattr":
            out["ret"] = ns["__getattr__"](inp.get("name", ""))
        elif func == "managed_source":
            out["ret"] = ns["_managed_source"](inp.get("filename", ""))
        elif func == "env_write_refusal":
            out["ret"] = ns["env_write_refusal"](inp.get("key", ""), inp.get("action", ""))
        elif func == "env_write_blocked":
            out["ret"] = ns["_env_write_blocked"](inp.get("key", ""), inp.get("action", ""))
        elif func == "require_env_writable":
            out["ret"] = ns["require_env_writable"](inp.get("key", ""), inp.get("action", ""))
        elif func == "write_anthropic_slots":
            save_fn = ctx.recorder("save_fn") if inp.get("save_fn") else None
            out["ret"] = ns["_write_anthropic_slots"](
                inp.get("token", ""), inp.get("api_key", ""), save_fn,
                token_first=inp.get("token_first", True))
        elif func == "scoped_environ_get":
            out["ret"] = ns["_scoped_environ_get"](inp.get("key", ""))
        elif func == "legacy_gateway_platforms_key":
            out["ret"] = ns["_legacy_gateway_platforms_key"](inp.get("requested_key", ""))
        elif func == "exit_if_key_managed":
            out["ret"] = ns["_exit_if_key_managed"](inp.get("key", ""), inp.get("action", ""))
        elif func == "touch_skin_file":
            out["ret"] = ns["_touch_skin_file"](inp.get("key", ""), inp.get("value"))
        elif func == "exit_invalid":
            out["ret"] = ns["_exit_invalid"](inp.get("msg", ""))
        elif func == "write_user_config":
            out["ret"] = ns["_write_user_config"](
                FakePath(inp.get("config_path", ""), ctx), copy.deepcopy(inp.get("user_config")))
        elif func == "print_unknown_key_notice":
            out["ret"] = ns["_print_unknown_key_notice"](inp.get("key", ""), inp.get("suggestion"))
        elif func == "dropped_shadow_notes":
            out["ret"] = ns["_dropped_shadow_notes"](
                ctx.state["user_config"], inp.get("key", ""), inp.get("legacy_key"),
                ctx.recorder("drop_legacy_disabled"))
        elif func == "usage_exit":
            out["ret"] = ns["_usage_exit"](inp.get("usage", ""), inp.get("examples", []),
                                           inp.get("extra"))
        elif func == "run_write_command":
            fn = ctx.recorder(inp.get("fn_name", ""))
            out["ret"] = ns["_run_write_command"](fn, *inp.get("args", []))
        elif func == "cmd_config_get":
            out["ret"] = ns["_cmd_config_get"](types.SimpleNamespace(**(inp.get("args") or {})))
        elif func == "cmd_config_set":
            out["ret"] = ns["_cmd_config_set"](types.SimpleNamespace(**(inp.get("args") or {})))
        elif func == "cmd_config_unset":
            out["ret"] = ns["_cmd_config_unset"](types.SimpleNamespace(**(inp.get("args") or {})))
        elif func == "tools_suffix":
            out["ret"] = ns["_tools_suffix"](inp.get("info") or {}, inp.get("fmt", ""))
        elif func == "print_banner":
            out["ret"] = ns["_print_banner"](inp.get("text", ""))
        elif func == "cmd_config_show":
            out["ret"] = ns["_cmd_config_show"]()
        elif func == "is_plugin_dir_name":
            out["ret"] = ns["_is_plugin_dir_name"](inp.get("name", ""))
        elif func == "platform_manifest_env_entries":
            out["ret"] = list(ns["_platform_manifest_env_entries"](
                inp.get("manifest") or {}, optional=inp.get("optional", True)))
        elif func == "manifest_secret_envs":
            manifests = [(d, m) for d, m in (inp.get("manifests") or [])]
            core = frozenset(inp["core"]) if inp.get("core") is not None else None
            out["ret"] = ns["_manifest_secret_envs"](manifests, core)
        elif func == "platform_manifest_secret_envs":
            home = inp.get("home")
            out["ret"] = ns["platform_manifest_secret_envs"](
                FakePath(home, ctx) if home else None, inp.get("source", "user"),
                strict=inp.get("strict", False))
        elif func == "platform_manifest_secret_scan":
            ctx.state["skipped"] = []
            home = inp.get("home")
            out["ret"] = ns["platform_manifest_secret_scan"](FakePath(home, ctx) if home else None)
        elif func == "ask_yes_no":
            out["ret"] = ns["_ask_yes_no"](inp.get("prompt", ""))
        elif func == "offer_list":
            out["ret"] = ns["_offer_list"](inp.get("heading", ""), inp.get("items", []),
                                           inp.get("question", ""))
        elif func == "prompt_and_save_env":
            out["ret"] = ns["_prompt_and_save_env"](
                inp.get("name", ""), inp.get("info") or {}, inp.get("prompt", ""),
                ctx.state["results"])
        else:
            raise KeyError(func)
    except BaseException as e:   # SystemExit / KeyboardInterrupt are BaseException
        out["ret"] = {"__raise__": exc_spec(e)}
    out["ret"] = proj(out["ret"])
    out["state"] = {k: proj(ctx.state[k]) for k in (inp.get("state") or {}) if k in ctx.state}
    out["stdout"] = ctx.stdout
    out["stderr"] = ctx.stderr
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
        ok = (got.get("trace") == want.get("trace")
              and got.get("ret") == want.get("ret")
              and got.get("state") == want.get("state")
              and got.get("stdout") == want.get("stdout")
              and got.get("stderr") == want.get("stderr"))
        if not ok:
            mism += 1
            print(f"MISMATCH {func}")
            print(f"  C  ={json.dumps(got, sort_keys=True, default=str)}")
            print(f"  PY ={json.dumps(want, sort_keys=True, default=str)}")
    print(f"ORACLE config_flow2: {cases} cases, {mism} mismatches")
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())