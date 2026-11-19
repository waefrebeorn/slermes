#!/usr/bin/env python3
"""Oracle for the main.py CLI-dispatch FLOW-core port (src/cli/port_main_cli_flow.c).

Drives the LIVE hermes_cli/main.py dispatch functions with SCRIPTED I/O: env,
file reads, prompts, prints, sibling calls, dependency imports and the argparse
parser/subparsers doubles are recording stubs fed the same scripts the C
harness feeds its seams. Compares the DECISION SEQUENCE (ordered seam calls
{"name","args"} in "trace"), the final return ("ret", with {"__raise__": ...}
for escaping exceptions incl. SystemExit), captured real stdout/stderr, and —
for stateful ports — the mutated state ("state").

The live defs are AST-extracted from the quarry (module constants seeded from
Assign AND AnnAssign, plain-Name targets only; the extraction namespace is a
registered module so py3.14 dataclasses/annotation resolution works). The
heavy quarry import chain is NEVER imported: in-function imports resolve
against stub sys.modules entries whose attributes are recording seams; the
real InvalidUserConfigError class is AST-extracted from hermes_cli/config.py;
`import fastapi`/`import uvicorn` go through a scripted meta_path finder.
Fail-closed: missing defs or 0 cases => exit 2.
Summary: 'ORACLE main_cli_flow: N cases, M mismatches'.

Representation contract (shared with the C port header):
- Paths are POSIX strings; the live defs run with os.path == posixpath and
  FakePath doubles whose str() is the posix string (as_posix normalization —
  str(Path) flips slashes on Windows).
- args is a SimpleNamespace built from in.args (vars(args) projection).
- SystemExit keeps its code's type ({"__raise__":"SystemExit","code":...});
  the live `exc.code == 0` test distinguishes int 0 from str "0".
- Scripted OSError str(): with errno -> "[Errno N] message"; without ->
  "message" (both sides construct the exception identically).
- read_text seams strip a leading U+FEFF for utf-8-sig on BOTH sides.
- env.get/env.pop are state-resolved (in.env), recorded but not scripted.
- print/stringio.getvalue are echo/effect seams: a script entry is required
  as a marker; the effect (capture-buffer append / speculative buffer read)
  is the result, not the script value.
- Integral floats normalize to ints on both sides before comparison.
"""
import ast
import builtins
import contextlib as _real_contextlib
import importlib.machinery
import io as _real_io
import json
import logging
import math
import os
import posixpath
import re
import sys
import types
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Optional

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

Q = os.path.expanduser("~/hermes-agent")
SRC = os.path.join(Q, "hermes_cli", "main.py")
CONFIG_SRC = os.path.join(Q, "hermes_cli", "config.py")

TARGETS = [
    "_early_interface_config_path", "_resolve_sudo_user_profile_env",
    "_under_gateway_supervisor", "_s6_supervised_gateway_run",
    "explicit_cli_profile", "_read_packed_ref", "_dotenv_has_provider_key",
    "_latest_session_id", "_first_run_setup_guard", "_finalize_update_receipt",
    "_update_preflight_handled", "_dashboard_lifecycle_flags",
    "_dashboard_sanitize_desktop_env", "_require_dashboard_web_deps",
    "_guard_noninteractive_user_config", "_is_tui_chat_launch",
    "_default_to_chat", "_parse_cli_args",
]

import __future__ as _fut
_FLAGS = _fut.annotations.compiler_flag


def seg(src, node):
    start = node.lineno
    for d in getattr(node, "decorator_list", []):
        start = min(start, d.lineno)
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    exec(compile(seg(src, node), path, "exec", flags=_FLAGS, dont_inherit=True), ns)


# Extraction namespace IS a registered module (py3.14 dataclasses need
# sys.modules[cls.__module__]; annotation resolution needs a real module).
_MOD = types.ModuleType("hermes_cli_main_flow_extracted")
_MOD.__file__ = SRC
sys.modules[_MOD.__name__] = _MOD
NS = _MOD.__dict__


def extract():
    src = open(SRC, encoding="utf-8").read()
    NS.update({
        "os": os, "sys": sys, "re": re, "json": json, "math": math,
        "types": types, "logging": logging, "posixpath": posixpath,
        "contextlib": _real_contextlib, "itertools": __import__("itertools"),
        "threading": __import__("threading"), "functools": __import__("functools"),
        "subprocess": __import__("subprocess"),
        "Path": Path, "Any": Any, "Optional": Optional,
        "dataclass": dataclass, "field": field,
        "SimpleNamespace": types.SimpleNamespace,
        "contextmanager": _real_contextlib.contextmanager,
        "suppress": _real_contextlib.suppress,
        "cache": getattr(__import__("functools"), "cache", lambda f: f),
        "__file__": SRC,
    })
    tree = ast.parse(src)

    def seed_constants():
        for node in tree.body:
            if isinstance(node, (ast.Assign, ast.AnnAssign)):
                tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
                if not all(isinstance(t, ast.Name) for t in tgts):
                    continue   # never exec Subscript targets (real env mutation)
                try:
                    exec_node(src, node, NS, SRC)
                except Exception:
                    pass

    seed_constants()
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            try:
                exec_node(src, node, NS, SRC)
            except Exception:
                pass
    seed_constants()   # re-seed after defs (forwarder aliases etc.)

    # Real exception class from hermes_cli/config.py (simple RuntimeError
    # subclass; extracted alone so the heavy config import chain never runs).
    cfg_src = open(CONFIG_SRC, encoding="utf-8").read()
    cfg_tree = ast.parse(cfg_src)
    for node in cfg_tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "InvalidUserConfigError":
            exec_node(cfg_src, node, NS, CONFIG_SRC)

    missing = [t for t in TARGETS if t not in NS]
    if "InvalidUserConfigError" not in NS:
        missing.append("InvalidUserConfigError")
    if missing:
        print(f"FATAL oracle_main_cli_flow: missing defs: {missing}")
        sys.exit(2)


extract()

CURRENT_CTX = None


# ── exception materialization ─────────────────────────────────────────────
EXC_CLASSES = {
    "OSError": OSError, "PermissionError": PermissionError,
    "FileNotFoundError": FileNotFoundError, "FileExistsError": FileExistsError,
    "BlockingIOError": BlockingIOError, "IsADirectoryError": IsADirectoryError,
    "NotADirectoryError": NotADirectoryError, "InterruptedError": InterruptedError,
    "TimeoutError": TimeoutError, "ProcessLookupError": ProcessLookupError,
    "ValueError": ValueError, "TypeError": TypeError, "RuntimeError": RuntimeError,
    "KeyError": KeyError, "AttributeError": AttributeError, "Exception": Exception,
    "ImportError": ImportError, "ModuleNotFoundError": ModuleNotFoundError,
    "EOFError": EOFError, "KeyboardInterrupt": KeyboardInterrupt,
    "InvalidUserConfigError": NS["InvalidUserConfigError"],
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    if kind == "SystemExit":
        return SystemExit(spec.get("code"))
    msg = spec.get("message", "")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    en = spec.get("errno")
    if en is not None and issubclass(cls, OSError):
        return cls(en, msg)
    return cls(msg)


def proj_exc(exc):
    if isinstance(exc, SystemExit):
        return {"__raise__": "SystemExit", "code": exc.code}
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# ── per-case script backend ───────────────────────────────────────────────
class Ctx:
    def __init__(self, inp):
        self.io = inp.get("io") or {}
        self.cursors = {}
        self.trace = []

    def next_entry(self, name):
        if name not in self.io:
            raise RuntimeError(f"no script for {name}")
        entry = self.io[name]
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                raise RuntimeError(f"script exhausted for {name}")
            self.cursors[name] = idx + 1
            return entry[idx]
        return entry

    def call(self, name, args):
        self.trace.append({"name": name, "args": args})
        entry = self.next_entry(name)
        if isinstance(entry, dict) and entry.get("__script__"):
            if isinstance(entry.get("raise"), dict):
                raise make_exc(entry["raise"])
            return entry.get("ret")
        return entry

    def state_call(self, name, args, fn):
        self.trace.append({"name": name, "args": args})
        return fn()


# ── data model doubles ────────────────────────────────────────────────────
class FakePath:
    """Path double: posix str, recording read_text/exists, posix joining."""
    def __init__(self, s, ctx):
        self.s = s
        self.ctx = ctx

    def __str__(self):
        return self.s

    def __fspath__(self):
        return self.s

    def __truediv__(self, other):
        return FakePath(posixpath.join(self.s, str(other)), self.ctx)

    def read_text(self, encoding=None, errors=None):
        args = {"path": self.s, "encoding": encoding}
        if errors is not None:
            args["errors"] = errors
        r = self.ctx.call("path.read_text", args)
        if encoding == "utf-8-sig" and isinstance(r, str) and r.startswith("\ufeff"):
            r = r[1:]   # the utf-8-sig codec strips the BOM live
        return r

    def exists(self):
        return self.ctx.call("path.exists", {"path": self.s})


class EnvDouble:
    def __init__(self, ctx, env):
        self.ctx = ctx
        self.env = dict(env)

    def get(self, name, default=None):
        return self.ctx.state_call("env.get", {"name": name, "default": default},
                                   lambda: self.env.get(name, default))

    def pop(self, name, default=None):
        def do():
            if name in self.env:
                return self.env.pop(name)
            return default
        return self.ctx.state_call("env.pop", {"name": name, "default": default}, do)


class _RecStringIO(_real_io.StringIO):
    """StringIO whose getvalue() records the echo seam (speculative buffer)."""
    def getvalue(self):
        CURRENT_CTX.call("stringio.getvalue", {})
        return super().getvalue()


_IO_DOUBLE = types.ModuleType("io")
_IO_DOUBLE.StringIO = _RecStringIO


class _NullLoader:
    def create_module(self, spec):
        return types.ModuleType(spec.name)

    def exec_module(self, module):
        pass


class _SeamFinder:
    """Scripted `import fastapi` / `import uvicorn` (recorded seams)."""
    def find_spec(self, fullname, path=None, target=None):
        if CURRENT_CTX is None or fullname not in ("fastapi", "uvicorn"):
            return None
        CURRENT_CTX.call("import." + fullname, {})   # may raise the scripted ImportError
        return importlib.machinery.ModuleSpec(fullname, _NullLoader())


sys.meta_path.insert(0, _SeamFinder())


# ── stub modules for in-function imports (recording seams) ────────────────
def _stub_module(name, **attrs):
    m = types.ModuleType(name)
    m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    parent, _, leaf = name.rpartition(".")
    if parent:
        pmod = sys.modules.get(parent)
        if pmod is None:
            pmod = _stub_module(parent)
        setattr(pmod, leaf, m)
    return m


def _ctx():
    if CURRENT_CTX is None:
        raise RuntimeError("seam called outside a case")
    return CURRENT_CTX


def _fakepath_or_none(r):
    return None if r is None else FakePath(r, _ctx())


def _refusal(r):
    return None if r is None else types.SimpleNamespace(message=r.get("message", ""))


_stub_module("hermes_constants",
    named_profile_is_live=lambda candidate: _ctx().call("named_profile_is_live", {"path": str(candidate)}),
    sudo_invoker_default_home=lambda: _fakepath_or_none(_ctx().call("sudo_invoker_default_home", {})),
    get_hermes_home=lambda: FakePath(_ctx().call("get_hermes_home", {}), _ctx()),
)
_stub_module("hermes_cli.service_manager",
    _s6_running=lambda: _ctx().call("_s6_running", {}),
)
_stub_module("hermes_cli._parser",
    gateway_run_no_supervise=lambda argv: _ctx().call("gateway_run_no_supervise", {"argv": list(argv)}),
)
_stub_module("hermes_cli.setup",
    is_interactive_stdin=lambda: _ctx().call("is_interactive_stdin", {}),
    print_noninteractive_setup_guidance=lambda message: _ctx().call(
        "print_noninteractive_setup_guidance", {"message": message}),
)
_stub_module("hermes_cli.update_receipt",
    finalize_pending_update_receipt=lambda code, reason: _ctx().call(
        "finalize_pending_update_receipt", {"code": code, "reason": reason}),
)
_stub_module("hermes_cli.config",
    InvalidUserConfigError=NS["InvalidUserConfigError"],
    require_parseable_user_config=lambda *, ignore_user_config=False: _ctx().call(
        "require_parseable_user_config", {"ignore_user_config": ignore_user_config}),
    is_managed=lambda: _ctx().call("is_managed", {}),
    managed_error=lambda action: _ctx().call("managed_error", {"action": action}),
)
_stub_module("hermes_cli.update_channel",
    handle_metadata_args=lambda args, project_root: _ctx().call(
        "handle_metadata_args", {"args": vars(args), "project_root": str(project_root)}),
)
_stub_module("hermes_cli.update_cmd_common",
    _record_stop=lambda reason, *, without_receipt=None: _ctx().call(
        "_record_stop", {"reason": reason, "without_receipt": without_receipt}),
)
_stub_module("hermes_cli.update_inventory",
    collect_runtime_inventory=lambda: _ctx().call("collect_runtime_inventory", {}),
    print_update_plan=lambda inventory: _ctx().call("print_update_plan", {"inventory": inventory}),
)
_stub_module("hermes_cli.update_cmd_windows",
    VENV_HOLDERS_EXIT=3,
    list_venv_holders=lambda: _ctx().call("list_venv_holders", {}),
)
_stub_module("hermes_cli.update_contract",
    evaluate_update_admission=lambda root: _refusal(_ctx().call(
        "evaluate_update_admission", {"project_root": str(root)})),
    record_refusal_receipt=lambda refusal: _ctx().call(
        "record_refusal_receipt", {"refusal": {"message": refusal.message}}),
)
_stub_module("hermes_cli.update_cmd",
    _cmd_update_check=lambda **kw: _ctx().call("_cmd_update_check", dict(kw)),
)
_stub_module("hermes_cli.dashboard_procs",
    _kill_stale_dashboard_processes=lambda *, reason, scope_home: _ctx().call(
        "_kill_stale_dashboard_processes", {"reason": reason, "scope_home": scope_home}),
)
_stub_module("hermes_cli.main_dep_hints",
    smart_app_control_block_message=lambda error: _ctx().call(
        "smart_app_control_block_message", {"error": str(error)}),
    missing_optional_deps_message=lambda surface, what, extra: _ctx().call(
        "missing_optional_deps_message", {"surface": surface, "what": what, "extra": extra}),
)

# Module-global siblings of the targets: recording overrides (the AST pass
# exec'd the real defs; the flow comparison seams them like the C port does).
NS["_resolve_last_session"] = lambda source="cli": _ctx().call("_resolve_last_session", {"source": source})
NS["cmd_setup"] = lambda args: _ctx().call("cmd_setup", {"args": vars(args)})
NS["_resolve_update_branch"] = lambda args: _ctx().call("_resolve_update_branch", {"args": vars(args)})
NS["_report_dashboard_status"] = lambda: _ctx().call("_report_dashboard_status", {})
NS["_find_stale_dashboard_pids"] = lambda **kw: _ctx().call("_find_stale_dashboard_pids", dict(kw))
NS["_is_desktop_owned_backend"] = lambda: _ctx().call("_is_desktop_owned_backend", {})
NS["_is_electron_packaged_web_dist"] = lambda value: _ctx().call("_is_electron_packaged_web_dist", {"value": value})
NS["_bypass_chat_launch"] = lambda args: _ctx().call("_bypass_chat_launch", {"args": vars(args)})
NS["_resolve_use_tui"] = lambda args: _ctx().call("_resolve_use_tui", {"args": vars(args)})
NS["_promote_top_level_resume"] = lambda args: _ctx().call("_promote_top_level_resume", {"args": vars(args)})
NS["_set_chat_arg_defaults"] = lambda args: _ctx().call("_set_chat_arg_defaults", {"args": vars(args)})
NS["cmd_chat"] = lambda args: _ctx().call("cmd_chat", {"args": vars(args)})
NS["_coalesce_session_name_args"] = lambda argv: _ctx().call("_coalesce_session_name_args", {"argv": list(argv)})


# ── per-case installation ─────────────────────────────────────────────────
def make_parser_doubles(inp, ctx):
    sp_spec = inp.get("subparsers") or {}
    subparsers = types.SimpleNamespace(required=bool(sp_spec.get("required", False)))
    if sp_spec.get("has_choices"):
        subparsers.choices = {k: None for k in (sp_spec.get("choices") or [])}

    def parse_args(argv):
        speculative = isinstance(sys.stdout, _RecStringIO)
        ctx.trace.append({"name": "parser.parse_args",
                          "args": {"argv": list(argv), "required": subparsers.required,
                                   "speculative": speculative}})
        entry = ctx.next_entry("parser.parse_args")
        if isinstance(entry, dict) and entry.get("__script__"):
            # argparse writes diagnostics/help BEFORE exiting
            if entry.get("stdout"):
                sys.stdout.write(entry["stdout"])
            if entry.get("stderr"):
                sys.stderr.write(entry["stderr"])
            if isinstance(entry.get("raise"), dict):
                raise make_exc(entry["raise"])
            return entry.get("ret")
        return entry

    parser = types.SimpleNamespace(parse_args=parse_args)
    return parser, subparsers


def install_case(func, inp, ctx, env_double):
    saved = {"print": builtins.print, "input": builtins.input,
             "os": NS.get("os"), "dumps": json.dumps,
             "io": sys.modules.get("io"),
             "profile": NS.get("_explicit_cli_profile"),
             "root": NS.get("PROJECT_ROOT")}

    def patched_print(*a, sep=" ", end="\n", file=None, flush=False):
        text = sep.join(str(x) for x in a) + end
        fname = "stderr" if file is sys.stderr else "stdout"
        ctx.call("print", {"text": text, "end": end, "file": fname})
        (sys.stderr if fname == "stderr" else sys.stdout).write(text)

    builtins.print = patched_print
    builtins.input = lambda prompt="": ctx.call("input", {"prompt": prompt})
    NS["os"] = types.SimpleNamespace(
        path=types.SimpleNamespace(
            join=posixpath.join,
            expanduser=lambda p: ctx.call("expanduser", {"path": p}),
        ),
        environ=env_double,
    )
    NS["_explicit_cli_profile"] = inp.get("explicit_cli_profile")
    NS["PROJECT_ROOT"] = FakePath(inp.get("project_root", "/repo"), ctx)
    if func == "_update_preflight_handled":
        json.dumps = lambda value, indent=None, **kw: ctx.call(
            "json.dumps", {"value": value, "indent": indent})
    if func == "_parse_cli_args":
        sys.modules["io"] = _IO_DOUBLE
    if func == "_require_dashboard_web_deps":
        sys.modules.pop("fastapi", None)
        sys.modules.pop("uvicorn", None)
    return saved


def restore_case(saved):
    builtins.print = saved["print"]
    builtins.input = saved["input"]
    json.dumps = saved["dumps"]
    NS["os"] = saved["os"]
    NS["_explicit_cli_profile"] = saved["profile"]
    NS["PROJECT_ROOT"] = saved["root"]
    if saved["io"] is not None:
        sys.modules["io"] = saved["io"]
    else:
        sys.modules.pop("io", None)


def state_for(func, args, env_double, subparsers):
    if func == "_dashboard_sanitize_desktop_env":
        return {"env": dict(env_double.env)}
    if func == "_guard_noninteractive_user_config":
        return {"args": vars(args)}
    if func == "_parse_cli_args":
        return {"subparsers_required": subparsers.required}
    return None


def drive(func, inp, ctx, args, env_double, parser, subparsers):
    if func == "_early_interface_config_path":
        ret = NS[func]()
    elif func == "_resolve_sudo_user_profile_env":
        ret = NS[func](inp.get("name", ""))
    elif func in ("_under_gateway_supervisor", "_s6_supervised_gateway_run"):
        ret = NS[func](list(inp.get("argv") or []))
    elif func == "explicit_cli_profile":
        ret = NS[func]()
    elif func == "_read_packed_ref":
        ret = NS[func](FakePath(inp["common_dir"], ctx), inp.get("ref", ""))
    elif func == "_dotenv_has_provider_key":
        ret = NS[func](FakePath(inp["env_file"], ctx), set(inp.get("provider_env_vars") or []))
    elif func == "_latest_session_id":
        ret = NS[func](bool(inp.get("use_tui", False)))
    elif func == "_finalize_update_receipt":
        ret = NS[func](inp.get("code", 0), inp.get("reason", ""))
    elif func == "_dashboard_lifecycle_flags":
        ret = NS[func](args, inp.get("token_file"))
    elif func == "_dashboard_sanitize_desktop_env":
        ret = NS[func](bool(inp.get("headless_backend", False)))
    elif func == "_require_dashboard_web_deps":
        ret = NS[func]()
    elif func == "_parse_cli_args":
        ret = NS[func](parser, subparsers, list(inp.get("argv") or []))
    elif func in ("_first_run_setup_guard", "_update_preflight_handled",
                  "_guard_noninteractive_user_config", "_is_tui_chat_launch",
                  "_default_to_chat"):
        ret = NS[func](args)
    else:
        raise RuntimeError(f"unknown func {func}")
    return ret, state_for(func, args, env_double, subparsers)


def canon(v):
    if isinstance(v, float):
        if math.isnan(v):
            return {"__float__": "nan"}
        if math.isinf(v):
            return {"__float__": "-inf" if v < 0 else "inf"}
        if v == int(v) and abs(v) < 1e16:
            return int(v)
        return v
    if isinstance(v, dict):
        return {k: canon(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [canon(x) for x in v]
    return v


def run_case(func, inp):
    global CURRENT_CTX
    ctx = Ctx(inp)
    CURRENT_CTX = ctx
    cap_out = _real_io.StringIO()
    cap_err = _real_io.StringIO()
    args = types.SimpleNamespace(**(inp.get("args") or {}))
    env_double = EnvDouble(ctx, inp.get("env") or {})
    saved = install_case(func, inp, ctx, env_double)
    parser, subparsers = make_parser_doubles(inp, ctx)
    ret = None
    state = None
    try:
        with _real_contextlib.redirect_stdout(cap_out), _real_contextlib.redirect_stderr(cap_err):
            ret, state = drive(func, inp, ctx, args, env_double, parser, subparsers)
    except BaseException as exc:   # noqa: BLE001 - SystemExit included
        ret = proj_exc(exc)
        state = state_for(func, args, env_double, subparsers)
    finally:
        restore_case(saved)
        CURRENT_CTX = None
    out = {"ret": ret, "trace": ctx.trace,
           "stdout": cap_out.getvalue(), "stderr": cap_err.getvalue()}
    if state is not None:
        out["state"] = state
    return out


def deep_eq(a, b):
    return canon(a) == canon(b)


def main():
    cases = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line or not line.startswith("{"):
            continue
        try:
            rec = json.loads(line)
        except json.JSONDecodeError:
            continue
        func = rec.get("func")
        inp = rec.get("in", {}) or {}
        c_out = rec.get("out")
        cases += 1
        if func not in TARGETS:
            mism += 1
            print(f"MISMATCH {func}: unknown func (fail-closed)")
            continue
        try:
            py_out = run_case(func, inp)
        except Exception as exc:   # noqa: BLE001
            mism += 1
            print(f"PYERR {func}: {type(exc).__name__}: {exc}")
            continue
        if c_out is None:
            mism += 1
            print(f"MISMATCH {func}: missing C out")
            continue
        bad = []
        for key, want in py_out.items():
            got = c_out.get(key)
            if not deep_eq(got, want):
                bad.append(key)
        if bad:
            mism += 1
            print(f"MISMATCH {func} keys={bad}")
            for key in bad:
                print(f"  C.{key}={json.dumps(canon(c_out.get(key)), default=str)[:600]}")
                print(f"  PY.{key}={json.dumps(canon(py_out.get(key)), default=str)[:600]}")
    if cases == 0:
        print("FATAL oracle_main_cli_flow: 0 cases")
        print("ORACLE main_cli_flow: 0 cases, 0 mismatches")
        return 2
    print(f"ORACLE main_cli_flow: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
