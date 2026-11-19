#!/usr/bin/env python3
"""Oracle for the tui_gateway/methods_tools.py FLOW port
(src/gateway/tui_methods_tools_flow.c).

Drives the LIVE methods_tools Plugins-hub bodies, shared dispatch helpers and
pure leaves with SCRIPTED seams: every server global (_ok/_err), deferred
module import (_tools_mod), module method, sibling flow (_plugin_rows /
_ensure_plugin_activation_listener / _portable_server_names) and in-function
import callable is a recording stub fed the same scripts the C harness feeds
its universal seam. Compares the DECISION SEQUENCE (ordered seam events
{"name","args","ret"} in "trace"), the final return ("ret") and the mutated
module-global mirror ("state": _plugin_activations / _plugin_activation_subscribed).

Live defs are AST-extracted from methods_tools.py (module Assign/AnnAssign
constants seeded, plain-Name targets only; UNDECORATED FunctionDefs exec'd with
the future-annotations flag into a registered sys.modules module — py3.14).
The heavy quarry import chain is never imported. _PLUGIN_ID_RE is AST-extracted
from hermes_cli/plugin_events.py and runs REAL (the C port transcribes it).
Ported sibling flows are SEAMED on both sides (each is exercised by its own
cases); the pure leaves (_joined_output, _is_snapshot_restore,
_worker_refused_skill, _refusal_data, _with_activation) stay LIVE so nested
decision logic is exercised, matching the C port's inline transcriptions.

Fail-closed: missing defs or 0 cases => exit 2; a missing/exhausted seam script
is a recorded RuntimeError raise on BOTH sides. Per-case wrapper catches
BaseException (SystemExit included). Summary:
'ORACLE tui_methods_tools_flow: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
  BaseException-only kinds (SystemExit/KeyboardInterrupt/GeneratorExit/
  CancelledError) pass through every except-Exception site.
- Scripted raises of plain kinds carry str(exc) == message verbatim; the
  PluginOperationError class is shared between make_exc and the plugins_cmd
  module double so the except-clause identity matches.
- tools_mod scripts return the module NAME string; it resolves to a
  SimpleNamespace of recording stubs (PluginOperationError / _PLUGIN_ID_RE /
  restart_mcp_servers_with_new_inputs are special attributes).
- Manager/package doubles: scripted dicts become SimpleNamespaces; the
  manager's on_plugin_loaded records the seam AND captures the live closure;
  in.invoke_loaded then drives the captured closure exactly as the C harness
  drives its state-mutating callback model.
- Tuples project to lists; sets to sorted lists; integral floats to ints;
  Paths to POSIX strings (fixture paths are POSIX, no drive letters/UNC).
- str()/strip()/lower()/split() fold the ASCII fixture corpus only; params
  identifier/name/key values are strings or null.
- Seam args drop None-valued keys on both sides (absent-vs-null parity).
"""
import ast
import asyncio
import copy
import json
import os
import re
import sys
import textwrap
import types
import __future__ as _fut
from pathlib import Path

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
SRC = os.path.join(Q, "tui_gateway", "methods_tools.py")
EV_SRC = os.path.join(Q, "hermes_cli", "plugin_events.py")

_FLAGS = _fut.annotations.compiler_flag


def seg(src, node):
    start = node.lineno
    for d in getattr(node, "decorator_list", []):
        start = min(start, d.lineno)
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    text = textwrap.dedent(seg(src, node))
    exec(compile(text, path, "exec", flags=_FLAGS, dont_inherit=True), ns)


def new_module(name):
    mod = types.ModuleType(name)
    sys.modules[name] = mod
    return mod


def seed_constants(src, ns, path):
    for node in ast.parse(src).body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
            if not all(isinstance(t, ast.Name) for t in tgts):
                continue
            try:
                exec_node(src, node, ns, path)
            except Exception:
                pass


# ── methods_tools.py namespace (live defs + seeded constants) ──
def extract():
    mod = new_module("tmt_oracle_methods_tools")
    ns = mod.__dict__
    import contextlib
    import threading
    ns.update({"contextlib": contextlib, "sys": sys, "os": os, "re": re,
               "json": json, "threading": threading, "Path": Path})
    src = open(SRC, encoding="utf-8").read()
    seed_constants(src, ns, SRC)
    for node in ast.parse(src).body:
        if isinstance(node, ast.FunctionDef) and not node.decorator_list:
            exec_node(src, node, ns, SRC)
    required = [
        "_joined_output", "_is_snapshot_restore", "_worker_refused_skill",
        "_refusal_data", "_with_activation", "_exec_out", "_run_action",
        "_cmd_queue", "_cmd_snapshot", "_mcp_reload_confirm_required",
        "_session_key_or_err", "_ensure_plugin_activation_listener",
        "_plugins_list", "_plugins_toggle", "_plugins_install", "_plugins_inspect",
        "_plugins_install_desktop", "_plugins_remove_desktop", "_plugins_update",
        "_plugins_remove", "_portable_server_names", "_plugins_settings",
        "_plugins_update_all", "_plugins_auto_update", "_plugins_onboarding",
        "_plugins_presence", "_SKILL_WORKER_REFUSED",
        "_plugin_activations", "_plugin_activation_subscribed",
    ]
    missing = [k for k in required if k not in ns]
    if missing:
        print(f"FATAL oracle_tui_methods_tools_flow: methods_tools missing: {missing}")
        sys.exit(2)
    return ns


NS = extract()

# ── _PLUGIN_ID_RE (real, from hermes_cli/plugin_events.py) ──
def extract_plugin_id_re():
    ns = {"re": re}
    src = open(EV_SRC, encoding="utf-8").read()
    for node in ast.parse(src).body:
        if isinstance(node, ast.Assign) and any(
                isinstance(t, ast.Name) and t.id == "_PLUGIN_ID_RE" for t in node.targets):
            exec_node(src, node, ns, EV_SRC)
            return ns["_PLUGIN_ID_RE"]
    print("FATAL oracle_tui_methods_tools_flow: _PLUGIN_ID_RE not found")
    sys.exit(2)


PLUGIN_ID_RE = extract_plugin_id_re()

# ── exception materialization / projection ──
PluginOperationError = type("PluginOperationError", (Exception,), {})

EXC_CLASSES = {
    "ValueError": ValueError, "TypeError": TypeError, "RuntimeError": RuntimeError,
    "KeyError": KeyError, "AttributeError": AttributeError, "ImportError": ImportError,
    "PermissionError": PermissionError, "OSError": OSError, "IndexError": IndexError,
    "Exception": Exception, "SystemExit": SystemExit,
    "KeyboardInterrupt": KeyboardInterrupt, "GeneratorExit": GeneratorExit,
    "CancelledError": asyncio.CancelledError,
    "PluginOperationError": PluginOperationError,
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    return cls(spec.get("message", ""))


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# ── canonical projection (compare parsed JSON, never raw strings) ──
def canon(v):
    def conv(x):
        if isinstance(x, types.SimpleNamespace):
            if hasattr(x, "__tmt_module__"):
                return x.__tmt_module__  # module double -> its name marker
            return conv(vars(x))
        if isinstance(x, Path):
            return x.as_posix()
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, (set, frozenset)):
            return sorted(conv(i) for i in x)
        if isinstance(x, float) and x.is_integer():
            return int(x)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


# ── per-case script backend (mirrors the C tmt_call exactly) ──
_MISSING = object()


class Ctx:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.trace = []

    def _record_raise(self, name, args, exc):
        self.trace.append({"name": name, "args": args, "ret": proj_exc(exc)})
        raise exc

    def call(self, name, args):
        args = {k: v for k, v in (args or {}).items() if v is not None}
        entry = self.io.get(name, _MISSING)
        used = entry
        if entry is _MISSING:
            self._record_raise(name, args, RuntimeError(f"no script for {name}"))
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                self._record_raise(name, args, RuntimeError(f"script exhausted for {name}"))
            self.cursors[name] = idx + 1
            used = entry[idx]
        if isinstance(used, dict) and used.get("__script__"):
            if isinstance(used.get("raise"), dict):
                self._record_raise(name, args, make_exc(used["raise"]))
            ret = used.get("ret")
        else:
            ret = used
        self.trace.append({"name": name, "args": canon(args), "ret": canon(ret)})
        # C returns a fresh copy per call; live flows mutate seam results in
        # place (_with_activation), so the script entry must stay pristine.
        return copy.deepcopy(ret) if isinstance(ret, (dict, list)) else ret


def rec(ctx, name, args_fn=None, build=None):
    def f(*a, **k):
        args = args_fn(*a, **k) if args_fn else {}
        raw = ctx.call(name, args)
        return build(raw) if build else raw
    return f


# ── module doubles (tools_mod resolves the scripted module name) ──
def restart_mcp_servers_with_new_inputs(*a, **k):
    raise AssertionError("restart_mcp_servers_with_new_inputs must not be called")


MODULE_ATTRS = {
    "hermes_cli.config": {"load_config": "config.load_config"},
    "hermes_cli.plugins_cmd": {
        "dashboard_set_agent_plugin_enabled": "plugins_cmd.dashboard_set_agent_plugin_enabled",
        "dashboard_install_plugin": "plugins_cmd.dashboard_install_plugin",
        "dashboard_update_user_plugin": "plugins_cmd.dashboard_update_user_plugin",
        "dashboard_remove_user_plugin": "plugins_cmd.dashboard_remove_user_plugin",
        "_find_plugin_entry": "plugins_cmd._find_plugin_entry",
        "_is_portable_plugin_dir": "plugins_cmd._is_portable_plugin_dir",
        "PluginOperationError": None,
    },
    "hermes_cli.plugins_cmd_inspect": {
        "inspect_plugin_source": "plugins_cmd_inspect.inspect_plugin_source"},
    "hermes_cli.desktop_plugin_install": {
        "install_desktop_plugin": "desktop_plugin_install.install_desktop_plugin",
        "remove_desktop_plugin": "desktop_plugin_install.remove_desktop_plugin"},
    "hermes_cli.plugins_settings": {"save_plugin_settings": "plugins_settings.save_plugin_settings"},
    "hermes_cli.plugin_events": {
        "broadcast_plugin_event": "plugin_events.broadcast_plugin_event",
        "_PLUGIN_ID_RE": None,
    },
    "hermes_cli.plugins_auto_update": {
        "auto_update_state": "plugins_auto_update.auto_update_state",
        "set_auto_update": "plugins_auto_update.set_auto_update",
        "update_all_unattended": "plugins_auto_update.update_all_unattended"},
    "hermes_cli.plugin_catalog_presence": {
        "onboarding_entries": "plugin_catalog_presence.onboarding_entries",
        "presence_for": "plugin_catalog_presence.presence_for"},
    "hermes_cli.agent_plugins": {"load_agent_plugin": "agent_plugins.load_agent_plugin"},
    "hermes_cli.plugins_manifest": {
        "portable_mcp_server_name": "plugins_manifest.portable_mcp_server_name"},
    "tools.mcp_tool_discovery": {"restart_mcp_servers_with_new_inputs": None},
}

ARGS_FN = {
    "config.load_config": lambda: {},
    "plugins_cmd.dashboard_set_agent_plugin_enabled":
        lambda ident, enabled: {"ident": ident, "enabled": enabled},
    "plugins_cmd.dashboard_install_plugin":
        lambda ident, force, enable, catalog_name, ref, inspected_sha:
        {"ident": ident, "force": force, "enable": enable, "catalog_name": catalog_name,
         "ref": ref, "inspected_sha": inspected_sha},
    "plugins_cmd.dashboard_update_user_plugin":
        lambda name, accept_capabilities, force:
        {"name": name, "accept_capabilities": accept_capabilities, "force": force},
    "plugins_cmd.dashboard_remove_user_plugin": lambda name: {"name": name},
    "plugins_cmd._find_plugin_entry": lambda key: {"key": key},
    "plugins_cmd._is_portable_plugin_dir": lambda plugin_dir: {"plugin_dir": plugin_dir},
    "plugins_cmd_inspect.inspect_plugin_source":
        lambda ident, catalog_name: {"ident": ident, "catalog_name": catalog_name},
    "desktop_plugin_install.install_desktop_plugin":
        lambda ident, ref, catalog_name, force:
        {"ident": ident, "ref": ref, "catalog_name": catalog_name, "force": force},
    "desktop_plugin_install.remove_desktop_plugin":
        lambda name, force: {"name": name, "force": force},
    "plugins_settings.save_plugin_settings":
        lambda canonical, plugin_dir, values:
        {"canonical": canonical,
         "plugin_dir": plugin_dir.as_posix() if plugin_dir is not None else None,
         "values": values},
    "plugin_events.broadcast_plugin_event":
        lambda canonical, event, data: {"canonical": canonical, "event": event, "data": data},
    "plugins_auto_update.auto_update_state": lambda: {},
    "plugins_auto_update.set_auto_update": lambda enable: {"enable": enable},
    "plugins_auto_update.update_all_unattended": lambda: {},
    "plugin_catalog_presence.onboarding_entries": lambda: {},
    "plugin_catalog_presence.presence_for": lambda names: {"names": names},
    "agent_plugins.load_agent_plugin":
        lambda plugin_dir, root: {"plugin_dir": plugin_dir.as_posix(), "root": root.as_posix()},
    "plugins_manifest.portable_mcp_server_name": lambda key, server: {"key": key, "server": server},
}


def ns_build(raw):
    return types.SimpleNamespace(**raw) if isinstance(raw, dict) else raw


def module_double(name, ctx, captured):
    spec = MODULE_ATTRS.get(name)
    if spec is None:
        return types.SimpleNamespace(__tmt_module__=name)
    attrs = {}
    for attr, seam in spec.items():
        if attr == "PluginOperationError":
            attrs[attr] = PluginOperationError
        elif attr == "_PLUGIN_ID_RE":
            attrs[attr] = PLUGIN_ID_RE
        elif attr == "restart_mcp_servers_with_new_inputs":
            attrs[attr] = restart_mcp_servers_with_new_inputs
        else:
            build = ns_build if seam == "agent_plugins.load_agent_plugin" else None
            attrs[attr] = rec(ctx, seam, ARGS_FN[seam], build)
    return types.SimpleNamespace(__tmt_module__=name, **attrs)


def manager_build(raw, ctx, captured):
    if not isinstance(raw, dict):
        return raw

    def on_plugin_loaded(cb):
        captured.append(cb)
        return ctx.call("on_plugin_loaded", {"callback": "_on_loaded"})

    return types.SimpleNamespace(on_plugin_loaded=on_plugin_loaded, **raw)


def spawn_build(raw, ctx):
    return types.SimpleNamespace(start=rec(ctx, "thread.start", lambda: {}))


def spawn_args(fn, args, name):
    servers = args[0] if args else set()
    return {"fn": fn.__name__, "args": sorted(servers), "name": name}


# ── in-function import stub modules (registered once, rebound per case) ──
for _pkg in ("hermes_cli", "agent"):
    if _pkg not in sys.modules:
        new_module(_pkg)
PLUGINS_MOD = new_module("hermes_cli.plugins")
MEMPROV_MOD = new_module("agent.memory_provider")
sys.modules["hermes_cli"].plugins = PLUGINS_MOD
sys.modules["agent"].memory_provider = MEMPROV_MOD

# ── real defs restored before every case (seam slots are per-case) ──
SEAMED_SIBLINGS = ("_ok", "_err", "_tools_mod", "_plugin_rows",
                   "_ensure_plugin_activation_listener", "_portable_server_names")
REAL = {nm: NS[nm] for nm in SEAMED_SIBLINGS if nm in NS}
# _ok/_err are server globals, not methods_tools defs: the live bodies read
# them as free variables; the per-case recorders below are the ONLY binding.
REAL.setdefault("_ok", None)
REAL.setdefault("_err", None)

STATE_FUNCS = {"_with_activation", "_ensure_plugin_activation_listener",
               "_plugins_toggle", "_plugins_install", "_plugins_update"}


def drive(func, inp):
    args = inp.get("args") or {}
    ctx = Ctx(inp.get("io"))
    captured = []
    state_in = copy.deepcopy(inp.get("state") or {})
    for nm, real in REAL.items():
        if real is not None:
            NS[nm] = real
    NS["_ok"] = rec(ctx, "ok", lambda rid, result: {"rid": rid, "result": result})
    NS["_err"] = rec(ctx, "err",
                     lambda rid, code, msg, data=None:
                     {"rid": rid, "code": code, "message": msg, "data": data})
    NS["_tools_mod"] = rec(ctx, "tools_mod", lambda module: {"module": module},
                           build=lambda raw: module_double(raw, ctx, captured))
    NS["_plugin_rows"] = rec(ctx, "plugin_rows", lambda: {})
    NS["_ensure_plugin_activation_listener"] = rec(
        ctx, "ensure_plugin_activation_listener", lambda: {})
    NS["_portable_server_names"] = rec(
        ctx, "portable_server_names",
        lambda plugin_dir, key: {"plugin_dir": plugin_dir.as_posix(), "key": key},
        build=lambda raw: set(raw) if isinstance(raw, list) else raw)
    PLUGINS_MOD.get_plugin_manager = rec(
        ctx, "get_plugin_manager", lambda: {},
        build=lambda raw: manager_build(raw, ctx, captured))
    MEMPROV_MOD.spawn_context_thread = rec(
        ctx, "spawn_context_thread", spawn_args, build=lambda raw: spawn_build(raw, ctx))
    # The case TARGET must run REAL even when it is also a seamed sibling of
    # other flows (the recorder installed above would otherwise call itself).
    if func in SEAMED_SIBLINGS and REAL.get(func) is not None:
        NS[func] = REAL[func]
    if func in STATE_FUNCS:
        NS["_plugin_activations"] = copy.deepcopy(state_in.get("_plugin_activations") or {})
        NS["_plugin_activation_subscribed"] = set(state_in.get("_plugin_activation_subscribed") or [])
    rid = args.get("rid")
    params = args.get("params")
    try:
        if func == "_joined_output":
            r = args.get("r")
            ret = NS[func](types.SimpleNamespace(**r) if isinstance(r, dict) else r)
        elif func == "_is_snapshot_restore":
            ret = NS[func](args.get("arg"))
        elif func == "_worker_refused_skill":
            ret = NS[func](make_exc(args["exc"]))
        elif func == "_refusal_data":
            ret = NS[func](args.get("result"))
        elif func == "_with_activation":
            ret = NS[func](copy.deepcopy(args.get("result")), args.get("name"))
        elif func == "_exec_out":
            ret = NS[func](rid, args.get("output"))
        elif func == "_run_action":
            table = {a: rec(ctx, seam,
                            lambda r, p, *extra: {"rid": r, "params": p, "extra": list(extra)})
                     for a, seam in (args.get("table") or {}).items()}
            ret = NS[func](rid, params, table, args.get("label", ""),
                           *(args.get("extra") or []))
        elif func == "_cmd_queue":
            ret = NS[func](rid, {}, None, "queue", args.get("arg"))
        elif func == "_cmd_snapshot":
            ret = NS[func](rid, {}, None, "snapshot", args.get("arg"))
        elif func == "_mcp_reload_confirm_required":
            ret = NS[func]()
        elif func == "_session_key_or_err":
            ret = NS[func](rid, args.get("session"), args.get("module", ""),
                           args.get("label", ""))
        elif func == "_ensure_plugin_activation_listener":
            ret = NS[func]()
            for summaries in (inp.get("invoke_loaded") or []):
                for cb in captured:
                    cb(summaries)
        elif func == "_portable_server_names":
            ret = NS[func](Path(args["plugin_dir"]), args.get("key"))
        elif func in ("_plugins_list", "_plugins_toggle", "_plugins_install",
                      "_plugins_inspect", "_plugins_install_desktop",
                      "_plugins_remove_desktop", "_plugins_update", "_plugins_remove",
                      "_plugins_settings", "_plugins_update_all", "_plugins_auto_update",
                      "_plugins_onboarding", "_plugins_presence"):
            ret = NS[func](rid, params)
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:  # SystemExit included
        ret = proj_exc(exc)
    state_out = dict(state_in)
    if func in STATE_FUNCS:
        state_out.update({
            "_plugin_activations": NS["_plugin_activations"],
            "_plugin_activation_subscribed": sorted(NS["_plugin_activation_subscribed"]),
        })
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state_out)}


# ── three-way seam-name cross-check (C port <-> oracle stubs <-> fixture io) ──
SEAM_NAMES = [
    "ok", "err", "tools_mod", "get_plugin_manager", "on_plugin_loaded",
    "ensure_plugin_activation_listener", "plugin_rows", "portable_server_names",
    "config.load_config",
    "plugins_cmd.dashboard_set_agent_plugin_enabled",
    "plugins_cmd.dashboard_install_plugin",
    "plugins_cmd.dashboard_update_user_plugin",
    "plugins_cmd.dashboard_remove_user_plugin",
    "plugins_cmd._find_plugin_entry", "plugins_cmd._is_portable_plugin_dir",
    "plugins_cmd_inspect.inspect_plugin_source",
    "desktop_plugin_install.install_desktop_plugin",
    "desktop_plugin_install.remove_desktop_plugin",
    "plugins_settings.save_plugin_settings",
    "plugin_events.broadcast_plugin_event",
    "plugins_auto_update.auto_update_state", "plugins_auto_update.set_auto_update",
    "plugins_auto_update.update_all_unattended",
    "plugin_catalog_presence.onboarding_entries", "plugin_catalog_presence.presence_for",
    "agent_plugins.load_agent_plugin", "plugins_manifest.portable_mcp_server_name",
    "spawn_context_thread", "thread.start",
]


def cross_check():
    csrc = open(_REPO / "src" / "gateway" / "tui_methods_tools_flow.c",
                encoding="utf-8").read()
    registry = set(SEAM_NAMES)
    missing_c = [n for n in SEAM_NAMES
                 if f'"{n}"' not in csrc and n != "tools_mod"]
    if missing_c:
        print(f"FATAL oracle_tui_methods_tools_flow: seams absent from C port: {missing_c}")
        sys.exit(2)
    c_seams = set(re.findall(r'tmt_call\(ctx,\s*"([^"]+)"', csrc))
    unknown_c = c_seams - registry
    if unknown_c:
        print(f"FATAL oracle_tui_methods_tools_flow: C seams outside registry: {sorted(unknown_c)}")
        sys.exit(2)
    osrc = open(__file__, encoding="utf-8").read()
    used = set(re.findall(r'rec\(\s*ctx,\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'rec\(\s*\n?\s*ctx,\s*"([^"]+)"', osrc))
    used |= set(v for v in ARGS_FN)
    unknown = used - registry
    if unknown:
        print(f"FATAL oracle_tui_methods_tools_flow: oracle seams outside registry: {sorted(unknown)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "tui_methods_tools_flow.jsonl"
    if fix.exists():
        fix_keys = set()
        table_names = set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            case = json.loads(line)
            inp = case.get("in") or {}
            fix_keys |= set(inp.get("io") or {})
            table_names |= set((inp.get("args") or {}).get("table", {}).values())
        bad = fix_keys - registry - table_names
        if bad:
            print(f"FATAL oracle_tui_methods_tools_flow: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)


def first_trace_diff(want, got):
    wt, gt = want.get("trace") or [], got.get("trace") or []
    for i in range(max(len(wt), len(gt))):
        w = wt[i] if i < len(wt) else None
        g = gt[i] if i < len(gt) else None
        if w != g:
            return i, w, g
    return None, None, None


def report_mismatch(func, want, got, shown):
    if shown >= 12:
        return
    print(f"MISMATCH {func}:")
    if want.get("ret") != got.get("ret"):
        print(f"  ret want={json.dumps(want.get('ret'), ensure_ascii=False)[:400]}")
        print(f"  ret got ={json.dumps(got.get('ret'), ensure_ascii=False)[:400]}")
    i, w, g = first_trace_diff(want, got)
    if i is not None:
        print(f"  trace[{i}] want={json.dumps(w, ensure_ascii=False)[:400]}")
        print(f"  trace[{i}] got ={json.dumps(g, ensure_ascii=False)[:400]}")
    ws, gs = want.get("state") or {}, got.get("state") or {}
    if ws != gs:
        keys = [k for k in set(ws) | set(gs) if ws.get(k) != gs.get(k)]
        print(f"  state diff keys: {sorted(keys)[:10]}")
        for k in sorted(keys)[:3]:
            print(f"    {k} want={json.dumps(ws.get(k), ensure_ascii=False)[:200]}")
            print(f"    {k} got ={json.dumps(gs.get(k), ensure_ascii=False)[:200]}")


def main():
    cross_check()
    n = m = shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec_in = json.loads(line)
        except Exception as exc:
            m += 1
            print(f"MISMATCH <unparsable harness line>: {exc}")
            continue
        n += 1
        func = rec_in.get("func", "")
        inp = rec_in.get("in") or {}
        got = rec_in.get("out")
        if not isinstance(got, dict) or "error" in got:
            m += 1
            print(f"MISMATCH {func}: harness emitted no result: {got}")
            continue
        try:
            want = drive(func, inp)
        except BaseException as exc:
            m += 1
            print(f"MISMATCH {func}: oracle drive raised {type(exc).__name__}: {exc}")
            continue
        g = canon(got)
        if g != want:
            m += 1
            report_mismatch(func, want, g, shown)
            shown += 1
    print(f"ORACLE tui_methods_tools_flow: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    """Python-only drive() self-test over every fixture case (no C involved)."""
    fix = _REPO / "tests" / "fixtures" / "tui_methods_tools_flow.jsonl"
    n = bad = 0
    counts = {}
    for line in open(fix, encoding="utf-8"):
        line = line.strip()
        if not line:
            continue
        case = json.loads(line)
        n += 1
        func = case["func"]
        counts[func] = counts.get(func, 0) + 1
        try:
            out = drive(func, case.get("in") or {})
        except BaseException as exc:
            bad += 1
            print(f"SELFTEST DRIVE RAISED {func} case {n}: {type(exc).__name__}: {exc}")
            continue
        r = out["ret"]
        if isinstance(r, dict) and r.get("__raise__") == "RuntimeError" and \
                str(r.get("message", "")).startswith(("no script", "script exhausted")):
            if not case.get("fail_closed"):
                bad += 1
                print(f"SELFTEST SCRIPT GAP {func} case {n}: {r['message']}")
    # hand-computed spot checks (guard against uniform wrong answers)
    spot = drive("_refusal_data", {"args": {"result": {"source_moved": True}}})
    assert spot["ret"] == {"reason": "source_moved"}, spot
    spot = drive("_refusal_data", {"args": {"result": {"exists": 1}}})
    assert spot["ret"] == {"reason": "exists"}, spot
    spot = drive("_refusal_data", {"args": {"result": {}}})
    assert spot["ret"] is None, spot
    spot = drive("_is_snapshot_restore", {"args": {"arg": "  Restore now"}})
    assert spot["ret"] is True, spot
    spot = drive("_joined_output", {"args": {"r": {"stdout": " a ", "stderr": "b"}}})
    assert spot["ret"] == "a \nb", spot
    spot = drive("_cmd_queue", {"args": {"rid": "r1", "arg": ""},
                                "io": {"err": {"jsonrpc": "2.0", "id": "r1",
                                               "error": {"code": 4004, "message": "usage: /queue <prompt>"}}}})
    assert spot["trace"][0]["name"] == "err", spot
    assert spot["trace"][0]["args"]["code"] == 4004, spot
    target_funcs = [f for f in counts]
    thin = [f for f in target_funcs if counts[f] < 5]
    if thin:
        bad += 1
        print(f"SELFTEST THIN COVERAGE (<5 cases): {thin}")
    print(f"SELFTEST tui_methods_tools_flow: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()