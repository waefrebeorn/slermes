#!/usr/bin/env python3
"""Oracle for the gateway/session_policy.py second-wave FLOW cores
(src/gateway/session_policy_flow2.c).

AST-extracts the LIVE unclaimed defs (no heavy gateway import), seeds the
module constants + the real AuthorityRefusal (persistence/core/errors.py), and
replaces every live I/O dependency with recording stubs driven by the SAME
per-name scripts the C harness feeds its universal seams (in.io). Compares the
ordered seam-call trace plus the return value and the mutated config/overrides
state.

Script shapes (both sides): a bare JSON array is a SEQUENCE (one result per
call, last element repeats after exhaustion); any other value is reused; a
scripted null is None. FAIL-CLOSED: a missing or empty script raises
RuntimeError("no script for <name>"). BaseException kinds (SystemExit /
KeyboardInterrupt / CancelledError / GeneratorExit) pass through every
`except Exception` site; the drive wrapper catches BaseException into a
{"__raise__": kind, "msg": str(exc)} ret spec (AuthorityRefusal also carries
reason/field/detail). Scripted raise specs propagate VERBATIM (identity match).

Modeling: pin_reasoning_effort / unpin_reasoning mutate in.config
(out.state.config); bind/unbind_turn_workspace mutate in.overrides
(out.state.overrides, the tools.terminal_tool._task_env_overrides table).
Claimed siblings are seams (present_sections, profile_target,
terminal_uses_host_cwd); ported siblings compose inline. _follow (foreign
gateway/session_policy_refresh) is seamed and its copy/pop contract applied by
the stub. Paths are PurePosixPath (POSIX separators on Windows). str() positions
are scalar-domain (str/int/bool/null); json.loads positions are strict-valid
JSON strings; .strip()/.lower() are ASCII. `and`/`or`/`return call(...)`
positions return the operand verbatim.

Input: stdin JSON lines from t_port_session_policy_flow2.
Summary: ORACLE session_policy_flow2: N cases, M mismatches. Fail-closed on 0.
"""
import ast
import asyncio
import copy
import json
import os
import sys
import types
import __future__
from dataclasses import MISSING, dataclass, fields
from functools import cached_property
from pathlib import Path, PurePosixPath, PureWindowsPath, PurePath

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "gateway", "session_policy.py")
ERRORS = os.path.join(QUARRY, "persistence", "core", "errors.py")

CUR = {"func": None, "in": {}, "trace": [], "idx": {}, "scripted": []}


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
    if kind == "SystemExit":
        return SystemExit(msg)
    if kind == "KeyboardInterrupt":
        return KeyboardInterrupt(msg)
    if kind == "GeneratorExit":
        return GeneratorExit(msg)
    if kind == "AuthorityRefusal":
        return AuthorityRefusal(msg, field=v.get("field"), detail=v.get("detail"))
    builtins = {
        "TypeError": TypeError, "ValueError": ValueError, "AttributeError": AttributeError,
        "KeyError": KeyError, "OSError": OSError, "FileNotFoundError": FileNotFoundError,
        "RuntimeError": RuntimeError, "TimeoutError": TimeoutError, "ImportError": ImportError,
        "ModuleNotFoundError": ModuleNotFoundError, "OverflowError": OverflowError,
        "IndexError": IndexError, "Exception": Exception, "StopIteration": StopIteration,
        "AssertionError": AssertionError, "LookupError": LookupError, "ArithmeticError": ArithmeticError,
        "SyntaxError": SyntaxError, "SystemError": SystemError, "NotImplementedError": NotImplementedError,
    }
    cls = builtins.get(kind)
    if cls is None:
        cls = type(str(kind), (Exception,), {})
    return cls(msg)


def denorm(x):
    if isinstance(x, (PolicyDouble, AuthorityDouble)):
        return {k: denorm(v) for k, v in object.__getattribute__(x, "_d").items()}
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
    CUR["trace"].append({"ev": name, "args": args, "ret": r})
    if isinstance(r, dict) and "__raise__" in r:
        exc = make_exc(r)
        CUR["scripted"].append((exc, r))
        raise exc
    return r


def norm_nums(x):
    if isinstance(x, float) and x.is_integer():
        return int(x)
    if isinstance(x, dict):
        return {k: norm_nums(v) for k, v in x.items()}
    if isinstance(x, list):
        return [norm_nums(v) for v in x]
    return x


class RecLogger:
    def _norm_arg(self, a):
        if isinstance(a, BaseException):
            return raise_spec(a)
        return denorm(a)

    def _rec(self, name, fmt, a, k):
        args = {"fmt": fmt}
        if a:
            args["args"] = [self._norm_arg(x) for x in a]
        if k.get("exc_info"):
            args["exc_info"] = True
        return seam(name, args)

    def debug(self, fmt, *a, **k):
        return self._rec("logger.debug", fmt, a, k)

    def info(self, fmt, *a, **k):
        return self._rec("logger.info", fmt, a, k)

    def warning(self, fmt, *a, **k):
        return self._rec("logger.warning", fmt, a, k)

    def error(self, fmt, *a, **k):
        return self._rec("logger.error", fmt, a, k)


class PolicyDouble:
    def __init__(self, d):
        object.__setattr__(self, "_d", dict(d or {}))

    def __getattr__(self, name):
        d = object.__getattribute__(self, "_d")
        if name in d:
            return d[name]
        raise AttributeError("'types.SimpleNamespace' object has no attribute '%s'" % name)

    def config(self, authority=None, *, secrets=True):
        return seam("policy.config", {})


class AuthorityDouble:
    def __init__(self, d):
        object.__setattr__(self, "_d", dict(d or {}))

    def __getattr__(self, name):
        d = object.__getattribute__(self, "_d")
        if name in d:
            return d[name]
        raise AttributeError("'types.SimpleNamespace' object has no attribute '%s'" % name)


class FakePath:
    """Path double for workspace_ready: is_absolute/is_dir are seams.
    Construction mirrors Path(non-str) -> TypeError before any probe."""
    def __init__(self, s):
        if not isinstance(s, str):
            raise TypeError(
                "argument should be a str or an os.PathLike object where "
                "__fspath__ returns a str, not '%s'" % type(s).__name__)
        self._s = s

    def is_absolute(self):
        return seam("Path.is_absolute", {"path": self._s})

    def is_dir(self):
        return seam("Path.is_dir", {"path": self._s})


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
    "model_route", "_held_pick", "entry_serves", "bound_route_holds",
    "_without_auto_pick", "_keeps_profile_route", "_with_entry_pick",
    "pin_reasoning_effort", "unpin_reasoning", "profile_config",
    "_key_slot", "launch_key", "route_launch_key",
    "workspace_ready", "bind_turn_workspace", "unbind_turn_workspace",
    "accept_launch_hooks", "refresh_profile_hooks", "register_worker_hooks",
    "_in_coding_workspace",
)
HELPERS = ("_refuse",)


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
    import contextlib
    import contextvars
    modname = "sp2_extracted_session_policy"
    mod = types.ModuleType(modname)
    sys.modules[modname] = mod
    ns = mod.__dict__
    ns.update({
        "__name__": modname, "__file__": SRC,
        "json": json, "os": os, "Path": Path,
        "PurePosixPath": PurePosixPath, "PureWindowsPath": PureWindowsPath,
        "contextlib": contextlib, "contextvars": contextvars,
        "ContextVar": contextvars.ContextVar,
        "dataclass": dataclass, "fields": fields, "MISSING": MISSING,
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
        elif isinstance(node, ast.FunctionDef) and node.name in WANTED + HELPERS:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec", flags=flags), ns)
        elif isinstance(node, ast.ClassDef) and node.name == "LocalSessionPolicy":
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    return ns


NS = extract()
AuthorityRefusal = NS["AuthorityRefusal"]
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
def present_sections_stub(config):
    return seam("present_sections", {"config": config})


def profile_target_stub(config):
    return seam("profile_target", {"config": config})


def terminal_uses_host_cwd_stub(terminal):
    return seam("terminal_uses_host_cwd", {"terminal": terminal})


NS["present_sections"] = present_sections_stub
NS["profile_target"] = profile_target_stub
NS["terminal_uses_host_cwd"] = terminal_uses_host_cwd_stub
NS["Path"] = FakePath

# -- foreign-module stub packages --
mod("gateway")
mod("hermes_cli")
mod("agent")
mod("tools")


def normalize_route_base_url(base_url=None):
    return seam("normalize_route_base_url", {"base_url": base_url})


mod("hermes_cli.route_identity", normalize_route_base_url=normalize_route_base_url)


def _route_key(url=None):
    return seam("_route_key", {"url": url})


def routed_custom_identity(url=None, model="", config=None, held=None):
    return seam("routed_custom_identity",
                {"url": url, "model": model, "config": config, "held": held})


mod("hermes_cli.runtime_provider_custom",
    _route_key=_route_key, routed_custom_identity=routed_custom_identity)


def resolved_route(block=None, config=None):
    return seam("resolved_route", {"block": block, "config": config})


def same_route(bound=None, block=None, config=None):
    return seam("same_route", {"bound": bound, "block": block, "config": config})


def _named_entry(provider=None, config=None):
    return seam("_named_entry", {"provider": provider, "config": config})


def profile_route_identity(config=None):
    return seam("profile_route_identity", {"config": config})


def configured_entry_key(identity=None, config=None):
    return seam("configured_entry_key", {"identity": identity, "config": config})


mod("gateway.session_route_identity",
    resolved_route=resolved_route, same_route=same_route, _named_entry=_named_entry,
    profile_route_identity=profile_route_identity, configured_entry_key=configured_entry_key)


def _follow(target, live, key):
    r = seam("_follow", {"agent": target, "followed": live, "key": key})
    # transcribed copy/pop contract (mutates the passed agent dict in place)
    if key in live:
        target[key] = live[key]
    else:
        target.pop(key, None)
    return r


mod("gateway.session_policy_refresh", _follow=_follow)


def _load_gateway_config(path=None):
    return seam("_load_gateway_config", {"path": path})


_load_gateway_config.__name__ = "_load_gateway_config"
mod("gateway.run", _load_gateway_config=_load_gateway_config)


def authority_persistence(authority):
    r = seam("authority_persistence", {"authority": authority})
    if isinstance(r, dict):
        return types.SimpleNamespace(
            scope=types.SimpleNamespace(profile_dir=PurePosixPath(r["profile_dir"])))
    return r


mod("gateway.session_persistence", authority_persistence=authority_persistence)


def in_focus_workspace(platform=None, cwd=None, config=None):
    return seam("in_focus_workspace", {"platform": platform, "cwd": cwd, "config": config})


mod("agent.coding_context", in_focus_workspace=in_focus_workspace)


def accept_for_launch(config=None):
    return seam("accept_for_launch", {"config": config})


def sync_from_config(loader=None):
    return seam("sync_from_config", {"loader": getattr(loader, "__name__", "loader")})


def shell_register_from_config(config=None, accept_hooks=False):
    return seam("shell_hooks.register_from_config",
                {"config": config, "accept_hooks": accept_hooks})


mod("agent.shell_hooks", accept_for_launch=accept_for_launch,
    sync_from_config=sync_from_config, register_from_config=shell_register_from_config)


def outbound_register_from_config(config=None):
    return seam("outbound_webhooks.register_from_config", {"config": config})


mod("agent.outbound_webhooks", register_from_config=outbound_register_from_config)


def register_task_env_overrides(task_id=None, overrides=None):
    return seam("register_task_env_overrides", {"task_id": task_id, "overrides": overrides})


def resolve_task_overrides(task_id=None):
    return seam("resolve_task_overrides", {"task_id": task_id})


def workspace_key_for(cwd=None):
    return seam("workspace_key_for", {"cwd": cwd})


def _qualify_task_key(task_id=None):
    return seam("_qualify_task_key", {"task_id": task_id})


def get_session_cwd(task_id=None):
    return seam("get_session_cwd", {"task_id": task_id})


def clear_session_cwd(task_id=None):
    return seam("clear_session_cwd", {"task_id": task_id})


TT = mod("tools.terminal_tool",
         register_task_env_overrides=register_task_env_overrides,
         resolve_task_overrides=resolve_task_overrides,
         workspace_key_for=workspace_key_for, _qualify_task_key=_qualify_task_key,
         get_session_cwd=get_session_cwd, clear_session_cwd=clear_session_cwd,
         _task_env_overrides={})


def install_case():
    CUR["idx"] = {}
    CUR["scripted"] = []
    NS["logger"] = RecLogger()
    NS["Path"] = FakePath


CFG_FUNCS = {"pin_reasoning_effort", "unpin_reasoning"}
OV_FUNCS = {"bind_turn_workspace", "unbind_turn_workspace"}


def run_flow(fn, state_fn):
    try:
        ret = fn()
        return {"ret": denorm(ret), "state": state_fn()}
    except BaseException as e:
        spec = None
        for exc, s in CUR["scripted"]:
            if e is exc:
                spec = s
                break
        if spec is None:
            spec = raise_spec(e)
        return {"ret": spec, "state": state_fn()}


def drive(func, inp):
    nostate = lambda: None
    if func in CFG_FUNCS:
        config = copy.deepcopy(inp.get("config") or {})
        state_fn = lambda: {"config": config}
    elif func in OV_FUNCS:
        overrides = dict(inp.get("overrides") or {})
        TT._task_env_overrides = overrides
        state_fn = lambda: {"overrides": overrides}
    else:
        state_fn = nostate

    if func == "model_route":
        return run_flow(lambda: NS["model_route"](inp.get("block")), state_fn)
    if func == "_held_pick":
        return run_flow(lambda: NS["_held_pick"](inp.get("frozen"), inp.get("live")), state_fn)
    if func == "entry_serves":
        return run_flow(lambda: NS["entry_serves"](inp.get("block"), inp.get("config")), state_fn)
    if func == "bound_route_holds":
        return run_flow(lambda: NS["bound_route_holds"](
            inp.get("bound"), inp.get("bound_config"), inp.get("block"), inp.get("config")), state_fn)
    if func == "_without_auto_pick":
        return run_flow(lambda: NS["_without_auto_pick"](inp.get("params")), state_fn)
    if func == "_keeps_profile_route":
        return run_flow(lambda: NS["_keeps_profile_route"](inp.get("picked"), inp.get("config")), state_fn)
    if func == "_with_entry_pick":
        return run_flow(lambda: NS["_with_entry_pick"](inp.get("params"), inp.get("config")), state_fn)
    if func == "pin_reasoning_effort":
        return run_flow(lambda: NS["pin_reasoning_effort"](
            config, inp.get("effort"), session=inp.get("session", False)), state_fn)
    if func == "unpin_reasoning":
        return run_flow(lambda: NS["unpin_reasoning"](config, inp.get("profile")), state_fn)
    if func == "profile_config":
        return run_flow(lambda: NS["profile_config"](inp.get("authority")), state_fn)
    if func == "_key_slot":
        return run_flow(lambda: NS["_key_slot"](inp.get("credential_ref")), state_fn)
    if func == "launch_key":
        return run_flow(lambda: NS["launch_key"](
            AuthorityDouble(inp.get("authority")), PolicyDouble(inp.get("policy"))), state_fn)
    if func == "route_launch_key":
        return run_flow(lambda: NS["route_launch_key"](
            AuthorityDouble(inp.get("authority")), PolicyDouble(inp.get("policy")),
            inp.get("config")), state_fn)
    if func == "workspace_ready":
        return run_flow(lambda: NS["workspace_ready"](PolicyDouble(inp.get("policy"))), state_fn)
    if func == "bind_turn_workspace":
        return run_flow(lambda: NS["bind_turn_workspace"](
            PolicyDouble(inp.get("policy")) if inp.get("policy") is not None else None,
            inp.get("task_id")), state_fn)
    if func == "unbind_turn_workspace":
        return run_flow(lambda: NS["unbind_turn_workspace"](inp.get("task_id")), state_fn)
    if func == "accept_launch_hooks":
        return run_flow(lambda: NS["accept_launch_hooks"](PolicyDouble(inp.get("policy"))), state_fn)
    if func == "refresh_profile_hooks":
        return run_flow(lambda: NS["refresh_profile_hooks"](
            PolicyDouble(inp.get("policy")) if inp.get("policy") is not None else None), state_fn)
    if func == "register_worker_hooks":
        return run_flow(lambda: NS["register_worker_hooks"](PolicyDouble(inp.get("policy"))), state_fn)
    if func == "_in_coding_workspace":
        return run_flow(lambda: NS["_in_coding_workspace"](
            inp.get("config"), inp.get("source"), inp.get("cwd")), state_fn)
    raise ValueError("unknown func: %s" % func)


def main():
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
        install_case()
        try:
            want = drive(func, inp)
        except BaseException as e:
            want = {"ret": raise_spec(e), "state": None}
        want["trace"] = CUR["trace"]
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("ORACLE session_policy_flow2: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())
