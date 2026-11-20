#!/usr/bin/env python3
"""Oracle for the gateway/session_policy.py fourth-wave FLOW cores
(src/gateway/session_policy_flow4.c).

AST-extracts the LIVE unclaimed defs (no heavy gateway import), seeds module
constants, the real AuthorityRefusal, the real LocalSessionPolicy dataclass and
real ContextVars, then replaces every live I/O dependency with recording stubs
driven by the SAME per-name scripts the C harness feeds its universal seams
(in.io). Compares the ordered seam-call trace plus the return value and the
mutated working config.

Script shapes (both sides): a bare JSON array is a SEQUENCE (one result per
call, last element repeats after exhaustion); any other value is reused; a
scripted null is None. FAIL-CLOSED: missing/empty script raises
RuntimeError("no script for <name>"). Full builtin exception hierarchy:
make_exc resolves kinds through the real builtins module (no hand-rolled
parent table), so `except Exception`/`except ValueError` sites agree with the
C port's complete parent-table walk. Scripted raise specs propagate VERBATIM.

Context-manager funcs (profile_read_scope, _serving_local_desktop_within) are
driven through the real generator-CM protocol: enter, a scripted `body` seam,
then exit; a scripted raise spec on body or on any exit seam propagates or is
suppressed exactly as live `with` semantics do. local_desktop_turn returns a CM
marker (the CM is created but not entered), denormed identically on both sides.

Input: stdin JSON lines from t_port_session_policy_flow4.
Summary: ORACLE session_policy_flow4: N cases, M mismatches. Fail-closed on 0.
"""
import ast
import asyncio
import builtins
import contextlib
import contextvars
import copy
import json
import os
import posixpath
import sys
import types
import __future__
import pathlib
from dataclasses import MISSING, dataclass, fields, replace
from functools import cached_property
from pathlib import Path, PurePosixPath, PureWindowsPath, PurePath

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "gateway", "session_policy.py")
ERRORS = os.path.join(QUARRY, "persistence", "core", "errors.py")

CUR = {"func": None, "in": {}, "trace": [], "idx": {}, "scripted": [], "home": None}


def raise_spec(e):
    if isinstance(e, AuthorityRefusal):
        return {"__raise__": "AuthorityRefusal", "msg": str(e),
                "reason": e.reason, "field": e.field, "detail": e.detail}
    return {"__raise__": type(e).__name__, "msg": str(e)}


def make_exc(v):
    kind = v.get("__raise__")
    msg = v.get("msg", "")
    if kind == "CancelledError":
        return asyncio.CancelledError(msg)
    if kind == "AuthorityRefusal":
        return AuthorityRefusal(msg, field=v.get("field"), detail=v.get("detail"))
    cls = getattr(builtins, kind, None)
    if isinstance(cls, type) and issubclass(cls, BaseException):
        if kind in ("UnicodeDecodeError", "UnicodeEncodeError", "UnicodeTranslateError"):
            e = cls.__new__(cls)
            Exception.__init__(e, msg)
            return e
        if kind in ("BaseExceptionGroup", "ExceptionGroup"):
            return cls(msg, [ValueError(msg)])
        return cls(msg)
    return type(str(kind), (Exception,), {})(msg)


def spec_of(e):
    for exc, s in CUR["scripted"]:
        if e is exc:
            return s
    return raise_spec(e)


def compact_json(s):
    """Normalize a json.dumps string to libjson's compact, ordered form."""
    if not isinstance(s, str):
        return s
    try:
        return json.dumps(json.loads(s), separators=(",", ":"), ensure_ascii=False)
    except Exception:
        return s


CM_FIELDS = {"config_json", "request_json", "terminal_json",
             "client_env_json", "client_env_rules_json"}


def denorm(x):
    if isinstance(x, contextlib._GeneratorContextManager):
        return {"__cm__": "serving_local_desktop"}
    if isinstance(x, (PolicyDouble, AttrDouble, AuthorityDouble)):
        return {k: denorm(v) for k, v in object.__getattribute__(x, "_d").items()}
    if isinstance(x, LocalSessionPolicyCls):
        d = {f.name: denorm(getattr(x, f.name)) for f in fields(x)}
        for k in CM_FIELDS:
            if k in d:
                d[k] = compact_json(d[k])
        return d
    if isinstance(x, PurePath):
        return x.as_posix()
    if isinstance(x, BaseException):
        return raise_spec(x)
    if isinstance(x, dict):
        return {k: denorm(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [denorm(v) for v in x]
    if isinstance(x, (set, frozenset)):
        return sorted(denorm(v) for v in x)
    return x


def seam(name, args):
    args = {k: denorm(v) for k, v in args.items() if v is not None}
    io = CUR["in"].get("io") or {}
    if name not in io:
        r = {"__raise__": "RuntimeError", "msg": "no script for " + name}
    else:
        sc = io[name]
        if isinstance(sc, list):
            if not sc:
                r = {"__raise__": "RuntimeError", "msg": "no script for " + name}
            else:
                i = CUR["idx"].get(name, 0)
                CUR["idx"][name] = i + 1
                r = sc[min(i, len(sc) - 1)]
        else:
            r = sc
    if isinstance(r, dict) and set(r) == {"__list__"}:
        r = copy.deepcopy(r["__list__"])
    CUR["trace"].append({"ev": name, "args": args, "ret": copy.deepcopy(r)})
    if isinstance(r, dict) and "__raise__" in r:
        exc = make_exc(r)
        CUR["scripted"].append((exc, r))
        raise exc
    return copy.deepcopy(r) if isinstance(r, (dict, list)) else r


def norm_nums(x):
    if isinstance(x, float) and x.is_integer():
        return int(x)
    if isinstance(x, dict):
        return {k: norm_nums(v) for k, v in x.items()}
    if isinstance(x, list):
        return [norm_nums(v) for v in x]
    return x


class RecLogger:
    def _rec(self, name, fmt, a, k):
        args = {"fmt": fmt}
        if a:
            args["args"] = [raise_spec(x) if isinstance(x, BaseException) else denorm(x) for x in a]
        if k.get("exc_info"):
            args["exc_info"] = True
        return seam(name, args)

    def debug(self, fmt, *a, **k): return self._rec("logger.debug", fmt, a, k)
    def info(self, fmt, *a, **k): return self._rec("logger.info", fmt, a, k)
    def warning(self, fmt, *a, **k): return self._rec("logger.warning", fmt, a, k)
    def error(self, fmt, *a, **k): return self._rec("logger.error", fmt, a, k)


class AttrDouble:
    def __init__(self, d):
        object.__setattr__(self, "_d", dict(d or {}))

    def __getattr__(self, name):
        d = object.__getattribute__(self, "_d")
        if name in d:
            return d[name]
        raise AttributeError("'types.SimpleNamespace' object has no attribute '%s'" % name)


class AuthorityDouble(AttrDouble):
    def __setattr__(self, name, value):
        object.__getattribute__(self, "_d")[name] = value


class PolicyDouble(AttrDouble):
    def read_profile(self, authority, for_turn=False):
        return seam("policy.read_profile", {"policy": self._d, "authority": authority, "for_turn": for_turn})

    def current_model(self, authority):
        return seam("policy.current_model", {"policy": self._d, "authority": authority})


class AdapterDouble:
    def __init__(self, d):
        self._d = d or {}
        self.policies = {k: (PolicyDouble(p) if isinstance(p, dict) else p)
                         for k, p in (self._d.get("policies") or {}).items()}


class RunnerDouble:
    def __init__(self, d):
        self._d = dict(d or {})

    def _adapter_for_source(self, source):
        r = seam("runner._adapter_for_source", {"source": source})
        if isinstance(r, dict):
            return AdapterDouble(r)
        return r


class SeamCM:
    """A context manager whose enter/exit call seams (models `with` doubles)."""
    def __init__(self, name, enter_args=None):
        self.name = name
        self.enter_args = dict(enter_args or {})

    def __enter__(self):
        return seam(self.name + ".__enter__", dict(self.enter_args))

    def __exit__(self, et, ev, tb):
        spec = spec_of(ev) if ev is not None else None
        r = seam(self.name + ".__exit__", {"exc": spec})
        if isinstance(r, dict) and "__raise__" in r:
            raise make_exc(r)
        return bool(r) if r is not None else False


def run_cm(cm):
    try:
        cm.__enter__()
    except BaseException as e:
        return {"ret": spec_of(e), "state": None}
    pending = None
    try:
        seam("body", {})
    except BaseException as e:
        pending = e
    suppressed = False
    try:
        if pending is not None:
            suppressed = cm.__exit__(type(pending), pending, None)
        else:
            suppressed = cm.__exit__(None, None, None)
    except BaseException as e:
        pending = e
    if pending is not None and not suppressed:
        return {"ret": spec_of(pending), "state": None}
    return {"ret": None, "state": None}


class SavedResolver:
    def __call__(self, name):
        return seam("known", {"name": name, "mode": "saved"})


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


# -- live-def extraction --
WANTED = (
    "active_policy", "turn_model", "profile_read_scope", "_serving_local_desktop_within",
    "local_desktop_turn", "_profile_workspace_cwd", "_unnamed_extension_toolsets",
    "_resolve_toolsets", "_apply_launch_overrides", "build_policy",
)


def seed_errors(ns):
    tree = ast.parse(open(ERRORS, encoding="utf-8").read())
    by_name = {}
    for node in tree.body:
        if isinstance(node, (ast.ClassDef, ast.FunctionDef)):
            by_name[node.name] = node
    for name in ("PersistenceError", "AuthorityRefusal", "_one_line"):
        exec(compile(ast.Module(body=[by_name[name]], type_ignores=[]), ERRORS, "exec"), ns)


def extract():
    src = open(SRC, encoding="utf-8").read()
    modname = "sp4_extracted_session_policy"
    mod = types.ModuleType(modname)
    sys.modules[modname] = mod
    ns = mod.__dict__
    ns.update({
        "__name__": modname, "__file__": SRC,
        "json": json, "os": os, "Path": Path,
        "PurePosixPath": PurePosixPath, "PureWindowsPath": PureWindowsPath,
        "contextlib": contextlib, "contextvars": contextvars,
        "ContextVar": contextvars.ContextVar, "contextmanager": contextlib.contextmanager,
        "dataclass": dataclass, "fields": fields, "MISSING": MISSING, "replace": replace,
        "cached_property": cached_property,
        "logging": types.SimpleNamespace(getLogger=lambda name: RecLogger()),
        "logger": RecLogger(),
    })
    seed_errors(ns)
    tree = ast.parse(src)
    flags = __future__.annotations.compiler_flag
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
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec", flags=flags), ns)
        elif isinstance(node, ast.ClassDef) and node.name == "LocalSessionPolicy":
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec", flags=flags), ns)
    return ns


NS = extract()
AuthorityRefusal = NS["AuthorityRefusal"]
LocalSessionPolicyCls = NS["LocalSessionPolicy"]
NS["logger"] = RecLogger()


def mod(name, **attrs):
    m = types.ModuleType(name)
    m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    if "." in name:
        parent, child = name.rsplit(".", 1)
        if parent in sys.modules:
            setattr(sys.modules[parent], child, m)
    return m


# -- claimed-sibling seams (NS-level) --
NS["launch_skills"] = lambda params: seam("launch_skills", {"params": params})
def present_sections_stub(config):
    r = seam("present_sections", {"config": config})
    if isinstance(r, dict):
        config.clear()
        config.update(r)
        return config
    return r


NS["present_sections"] = present_sections_stub
NS["_without_auto_pick"] = lambda params: seam("without_auto_pick", {"params": params})
NS["profile_target"] = lambda config: seam("profile_target", {"config": config})
NS["_in_coding_workspace"] = lambda config, source, cwd: seam(
    "_in_coding_workspace", {"config": config, "source": source, "cwd": cwd})
NS["terminal_uses_host_cwd"] = lambda terminal: seam("terminal_uses_host_cwd", {"terminal": terminal})
NS["_with_entry_pick"] = lambda params, config: seam("with_entry_pick", {"params": params, "config": config})
NS["_keeps_profile_route"] = lambda picked, config: seam("keeps_profile_route", {"picked": picked, "config": config})
NS["_refuse"] = lambda field, detail=None: (_ for _ in ()).throw(
    AuthorityRefusal("invalid_params", field=field, detail=detail))


def pin_stub(config, effort, session=False):
    config.setdefault("agent", {})["reasoning_effort"] = effort
    config["agent"].pop("reasoning_overrides", None)
    if session:
        config["session_pins"] = {**(config.get("session_pins") or {}), "reasoning": True}


NS["pin_reasoning_effort"] = pin_stub


def extract_secrets_stub(value, private, *paths):
    if paths and tuple(paths[0]) == (None,):
        r = seam("extract_config_secrets_terminal", {"value": value, "private": private, "tuple_path": True})
    else:
        r = seam("extract_config_secrets_config", {"value": value, "private": private})
    if isinstance(r, dict):
        value.clear()
        value.update(r)
    return None


NS["_extract_config_secrets"] = extract_secrets_stub

# -- foreign-module stub packages --
mod("gateway")
mod("hermes_cli")
mod("agent")
mod("tools")

mod("gateway.session_a2a",
    build_forward_policy=lambda params, config, private_secrets=None: seam(
        "build_forward_policy", {"params": params, "config": config, "private_secrets": private_secrets}))
mod("agent.runtime_cwd", resolve_agent_cwd=lambda: seam("resolve_agent_cwd", {}))
mod("tools.terminal_scope",
    build_profile_terminal_scope=lambda home: seam("build_profile_terminal_scope", {"home": home}),
    default_terminal_scope=lambda: seam("default_terminal_scope", {}))
mod("gateway.session_local_editor", validate_editor=lambda source, editor: seam(
    "validate_editor", {"source": source, "editor": editor}))
mod("gateway.session_client", client_problem=lambda client: seam("client_problem", {"client": client}))


def private_editor_request_stub(request, private_secrets):
    r = seam("private_editor_request", {"request": request, "private_secrets": private_secrets})
    if isinstance(r, dict):
        request.clear()
        request.update(r)
    return None


mod("gateway.session_local_mcp", private_editor_request=private_editor_request_stub)

mod("agent.session_client_env",
    client_env_rules=lambda config, local=False: seam("client_env_rules", {"config": config, "local": local}),
    normalise_client_env=lambda client_env, rules: seam(
        "normalise_client_env", {"client_env": client_env, "rules": rules}))
mod("hermes_constants",
    get_hermes_home=lambda: CUR["home"],
    profile_name_for_home=lambda home: seam("profile_name_for_home", {"home": home}),
    parse_reasoning_effort=lambda value: seam("parse_reasoning_effort", {"value": value}))
mod("hermes_cli.config",
    read_user_config_raw=lambda path: seam("read_user_config_raw", {"path": str(path)}),
    resolve_turn_limit=lambda value, default=0: seam("resolve_turn_limit", {"value": value, "default": default}))
mod("agent.fast_mode",
    parse_exact_service_tier=lambda value: seam("parse_exact_service_tier", {"value": value}))
class SeamKeys:
    def __iter__(self):
        return iter(seam("route_bound_keys", {}))


mod("gateway.session_route_identity", ROUTE_BOUND_KEYS=SeamKeys())
mod("gateway.config", Platform=types.SimpleNamespace(LOCAL="local"))
mod("hermes_cli.process_identity", serving_local_desktop=lambda on: SeamCM("serving_local_desktop", {"on": on}))
mod("agent.coding_context", focus_toolsets=lambda config: set(seam("focus_toolsets", {"config": config}) or []))
mod("agent.skill_utils",
    parse_config_string_list=lambda value: set(seam("parse_config_string_list", {"value": value}) or []))


def resolve_toolset_stub(name, include_registry=True):
    return seam("resolve_toolset", {"name": name, "include_registry": include_registry})


mod("toolsets", TOOLSETS=set(), CLIENT_SURFACE_TOOLSETS=set(),
    resolve_toolset=resolve_toolset_stub,
    validate_toolset=lambda name: seam("known", {"name": name, "mode": "builtin"}))
def saved_resolver_stub(config):
    seam("saved_toolset_resolver", {"config": config})
    return SavedResolver()


mod("hermes_cli.toolset_validation", saved_toolset_resolver=saved_resolver_stub)
def enabled_servers_stub(config):
    return set(seam("enabled_mcp_server_names", {"config": config}) or [])


def plugin_keys_stub():
    return set(seam("get_plugin_toolset_keys", {}) or [])


mod("hermes_cli.tools_config",
    enabled_mcp_server_names=enabled_servers_stub,
    _get_platform_tools=lambda config, surface: set(seam("get_platform_tools", {"config": config, "surface": surface}) or []),
    _get_plugin_toolset_keys=plugin_keys_stub)

mod("logging", getLogger=lambda name: RecLogger())


def run_flow(fn, state_fn):
    try:
        ret = fn()
        return {"ret": denorm(ret), "state": state_fn()}
    except BaseException as e:
        return {"ret": spec_of(e), "state": state_fn()}


def drive(func, inp):
    nostate = lambda: None
    if func == "active_policy":
        NS["_ACTIVE_POLICY"].set(inp.get("active"))
        return run_flow(lambda: NS["active_policy"](), nostate)
    if func == "turn_model":
        return run_flow(lambda: NS["turn_model"](
            PolicyDouble(inp.get("policy")), inp.get("authority")), nostate)
    if func == "profile_read_scope":
        return run_cm(NS["profile_read_scope"](
            PolicyDouble(inp.get("policy")), inp.get("authority")))
    if func == "_serving_local_desktop_within":
        return run_cm(NS["_serving_local_desktop_within"](SeamCM("scope", {})))
    if func == "local_desktop_turn":
        return run_flow(lambda: NS["local_desktop_turn"](
            RunnerDouble(inp.get("runner")),
            AttrDouble(inp.get("source")) if inp.get("source") is not None else None,
            inp.get("scope")), nostate)
    if func == "_profile_workspace_cwd":
        return run_flow(lambda: NS["_profile_workspace_cwd"](inp.get("home")), nostate)
    if func == "_unnamed_extension_toolsets":
        sys.modules["toolsets"].TOOLSETS = set(inp.get("toolsets") or [])
        return run_flow(lambda: NS["_unnamed_extension_toolsets"](
            inp.get("explicit"), set(inp.get("requested") or []), inp.get("config")), nostate)
    if func == "_resolve_toolsets":
        sys.modules["toolsets"].TOOLSETS = set(inp.get("toolsets") or [])
        sys.modules["toolsets"].CLIENT_SURFACE_TOOLSETS = set(inp.get("surface_toolsets") or [])
        config = copy.deepcopy(inp.get("config") or {})
        return run_flow(lambda: NS["_resolve_toolsets"](
            inp.get("params"), config, inp.get("source"),
            bool(inp.get("safe_mode", False)), bool(inp.get("coding_posture", False)),
            bypass=bool(inp.get("bypass", False))), lambda: {"config": denorm(config)})
    if func == "_apply_launch_overrides":
        config = copy.deepcopy(inp.get("config") or {})
        return run_flow(lambda: NS["_apply_launch_overrides"](
            inp.get("params"), config), lambda: {"config": denorm(config)})
    if func == "build_policy":
        CUR["home"] = inp.get("home")
        sys.modules["toolsets"].TOOLSETS = set(inp.get("toolsets") or [])
        sys.modules["toolsets"].CLIENT_SURFACE_TOOLSETS = set(inp.get("surface_toolsets") or [])
        config = copy.deepcopy(inp.get("config") or {})
        box = {}
        def run():
            r = NS["build_policy"](
                inp.get("params"), config,
                private_secrets=inp.get("private_secrets"),
                profile_terminal=bool(inp.get("profile_terminal", True)),
                workspace_posture=bool(inp.get("workspace_posture", False)),
                client_local=bool(inp.get("client_local", False)))
            box["r"] = r
            return r
        def final_config():
            pol = box.get("r")
            if isinstance(pol, LocalSessionPolicyCls):
                try:
                    return {"config": json.loads(pol.config_json)}
                except Exception:
                    pass
            return {"config": denorm(config)}
        return run_flow(run, final_config)
    raise ValueError("unknown func: %s" % func)


class SeamPath(PurePosixPath):
    def is_dir(self):
        return seam("os.path.isdir", {"path": str(self)})


class _SeamPath:
    def __getattr__(self, name):
        if name == "isdir":
            return lambda p: seam("os.path.isdir", {"path": str(p)})
        return getattr(posixpath, name)


def main():
    os.path = _SeamPath()
    pathlib.Path = PurePosixPath
    NS["Path"] = SeamPath
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
        inp = norm_nums(record.get("in") or {})
        got = record.get("out") or {}
        cases += 1
        CUR["func"] = func
        CUR["in"] = inp
        CUR["trace"] = []
        CUR["idx"] = {}
        CUR["scripted"] = []
        CUR["home"] = None
        NS["logger"] = RecLogger()
        try:
            want = drive(func, inp)
        except BaseException as e:
            want = {"ret": spec_of(e), "state": None}
        want["trace"] = CUR["trace"]
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("ORACLE session_policy_flow4: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())
