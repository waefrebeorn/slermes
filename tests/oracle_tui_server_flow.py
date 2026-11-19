#!/usr/bin/env python3
"""Oracle for the tui_gateway/server.py FLOW cores + pure leaves
(src/gateway/tui_server_flow.c).

AST-extracts the 26 target functions and the module constants they read,
seeds faithful doubles for FailedConfigRead / ConfigBackendUnavailable /
Changes / locks, and replaces every collaborator (sibling functions, config
backend reads/writes, server_requests.send, the prompt bridge, logging,
clock) with recording stubs returning the SAME scripted results the C
harness feeds its universal seam. Module-global collaborators are overridden
in the namespace; in-function imports are stubbed on sys.modules so the live
`from X import Y` binds the recorder.

State is injected as a JSON mirror (cfg cache / home / sessions / reprobe /
mcp); the oracle materializes the module globals, lets live mutate them, then
norms the final state back to JSON. Compares ordered seam trace + return +
final state. BaseException is caught per case; scripted raises round-trip
bare (identity-matched to the Raise instance the script handed out).

Input: stdin JSON lines from t_port_tui_server_flow.
Summary: ORACLE tui_server_flow: N cases, M mismatches. Fail-closed on 0 cases.
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
from pathlib import Path
from types import SimpleNamespace

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

# Scrubbed-env runner: derive home from __file__ (repo = parent.parent, home = parent).
_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "ts_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "tui_gateway", "server.py")

TARGETS = [
    "_ok", "_normalize_request", "_coerce_int_config_value", "_display_cfg",
    "_load_interim_assistant_messages", "_load_dashboard_process_isolation_config",
    "_active_config_path", "_load_cfg", "_drop_cfg_cache", "_load_cfg_raw",
    "_save_cfg", "_write_config_changes", "_write_config_key", "_write_config_keys",
    "_is_model_switch_marker", "_is_pivot_marker", "_turn_started_at",
    "_cli_exec_blocked", "_clarify_timeout_seconds", "_ask", "_clarify_block",
    "_tour_request", "_preview_action_request", "_status_update", "_resolve_name",
    "_finish_reload",
]

CUR = {"func": None, "in": {}, "trace": [], "io": {}, "pos": {}, "raises": []}


def rec(ev):
    CUR["trace"].append(ev)


# ── exceptions ───────────────────────────────────────────────────────────────
class ConfigBackendUnavailable(SystemExit):
    """Faithful double: a SystemExit subclass (passes `except Exception`)."""


class FailedConfigRead(dict):
    def __init__(self, data=(), *, error):
        super().__init__(data)
        self.read_error = error


class Changes:
    def __init__(self, document=None, set=None, unset=(), extra_content_on_create=None):
        self.document = document
        self.set = dict(set) if set is not None else {}
        self.unset = tuple(unset)
        self.extra_content_on_create = extra_content_on_create


def make_exc(spec):
    kind = spec.get("__raise__", "Exception")
    msg = spec.get("message", "")
    if kind == "ConfigBackendUnavailable":
        return ConfigBackendUnavailable(msg)
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


def proj_path(p):
    return p.as_posix() if isinstance(p, Path) else p


def mat(x):
    if isinstance(x, dict):
        return SimpleNamespace(**{k: mat(v) for k, v in x.items()})
    if isinstance(x, list):
        return [mat(v) for v in x]
    return x


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


class NoOpLock:
    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False


# ── extraction ───────────────────────────────────────────────────────────────
def extract():
    src = open(SRC, encoding="utf-8").read()
    mod = types.ModuleType("tui_server_flow_extracted")
    mod.__package__ = "tui_gateway"
    sys.modules[mod.__name__] = mod
    ns = mod.__dict__
    ns.update({"json": json, "contextlib": contextlib, "os": os, "Path": Path, "copy": copy,
               "logger": RecLogger(), "ConfigBackendUnavailable": ConfigBackendUnavailable,
               "FailedConfigRead": FailedConfigRead, "Changes": Changes})
    tree = ast.parse(src)
    # module constants (plain-Name Assign / AnnAssign), json seeded for the bridge dumps
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
    for name in ("_MODEL_SWITCH_MARKER_PREFIX", "_CLI_EXEC_BLOCKED", "_CWD_PLACEHOLDERS",
                 "_TOUR_BRIDGE_UNAVAILABLE", "_PREVIEW_ACTION_BRIDGE_UNAVAILABLE"):
        assert name in ns, "constant missed %s" % name
    return ns


# ── collaborator recorders (module-global seams) ─────────────────────────────
def collaborator_recorders():
    R = {}
    R["_load_cfg"] = lambda: seam("_load_cfg", {})
    R["_display_cfg"] = lambda: seam("_display_cfg", {})
    R["_active_config_path"] = lambda: seam("_active_config_path", {})
    R["_load_cfg_raw"] = lambda: seam("_load_cfg_raw", {})
    R["_drop_cfg_cache"] = lambda: seam("_drop_cfg_cache", {})
    R["_is_model_switch_marker"] = lambda entry: seam("_is_model_switch_marker", {"entry": entry})
    R["_clarify_timeout_seconds"] = lambda: seam("_clarify_timeout_seconds", {})
    R["is_truthy_value"] = lambda value, default=False: seam("is_truthy_value", {"value": value, "default": default})
    R["get_hermes_home_override"] = lambda: seam("get_hermes_home_override", {})
    R["supports_file_tooling"] = lambda: seam("supports_file_tooling", {})

    def _coerce(value, default, *, min_value):
        return seam("_coerce_int_config_value", {"value": value, "default": default, "min_value": min_value})
    R["_coerce_int_config_value"] = _coerce

    def _save(cfg):
        return seam("_save_cfg", {"cfg": cfg})
    R["_save_cfg"] = _save

    def _wcc(sets, unsets=()):
        return seam("_write_config_changes", {"sets": sets, "unsets": list(unsets)})
    R["_write_config_changes"] = _wcc

    def _wck(updates):
        return seam("_write_config_keys", {"updates": updates})
    R["_write_config_keys"] = _wck

    def _ask(method, sid, payload, timeout=300):
        return seam("_ask", {"method": method, "sid": sid, "payload": payload, "timeout": timeout})
    R["_ask"] = _ask

    def _emit(event, sid, payload):
        return seam("_emit", {"event": event, "sid": sid, "payload": payload})
    R["_emit"] = _emit

    def _ok(rid, result):
        return seam("_ok", {"rid": rid, "result": result})
    R["_ok"] = _ok

    def _err(rid, code, msg, data=None):
        return seam("_err", {"rid": rid, "code": code, "msg": msg})
    R["_err"] = _err
    return R


# ── in-function import stubs (sys.modules) ───────────────────────────────────
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

    stub("hermes_cli.config_effective",
         load_user_config_effective=lambda path: seam("load_user_config_effective", {"path": proj_path(path)}))
    stub("hermes_cli.config_backend",
         config_cache_version=lambda path: seam("config_cache_version", {"path": proj_path(path)}),
         config_exists=lambda path: seam("config_exists", {"path": proj_path(path)}),
         supports_file_tooling=lambda: seam("supports_file_tooling", {}))
    stub("hermes_cli.config",
         read_user_config_raw=lambda path: seam("read_user_config_raw", {"path": proj_path(path)}),
         atomic_config_replace=lambda path, cfg: seam("atomic_config_replace", {"path": proj_path(path), "cfg": cfg}))
    stub("hermes_cli.config_read_errors", FailedConfigRead=FailedConfigRead)
    stub("hermes_cli.commands",
         resolve_command=lambda name: mat(seam("resolve_command", {"name": name})))
    stub("tools.clarify_gateway", get_clarify_timeout=lambda: seam("get_clarify_timeout", {}))
    stub("tui_gateway.methods_config_set",
         _refuse_managed_keys=lambda *keys: seam("_refuse_managed_keys", {"keys": list(keys)}))
    stub("cli", save_config_value=lambda key, value: seam("save_config_value", {"key": key, "value": value}))

    def send(method, sid, params, timeout=None, qids=None):
        args = {"method": method, "sid": sid, "params": params, "timeout": timeout}
        if qids is not None:
            args["qids"] = qids
        return seam("send", args)
    stub("tui_gateway.server_requests", send=send)


# ── per-case setup ───────────────────────────────────────────────────────────
REAL = {}
COLLAB = {}


def setup_case(ns, func, inp):
    CUR["func"] = func
    CUR["in"] = inp
    CUR["trace"] = []
    CUR["io"] = inp.get("io") or {}
    CUR["pos"] = {}
    CUR["raises"] = []
    # restore every target to its real def, then seam the collaborators this flow uses
    for nm in TARGETS:
        ns[nm] = REAL[nm]
    for nm, factory in COLLAB.items():
        if nm != func:
            ns[nm] = factory
    # state mirror -> module globals
    stt = inp.get("state") or {}
    cfg = stt.get("cfg") or {}
    ns["_cfg_cache"] = copy.deepcopy(cfg.get("cache"))
    ns["_cfg_sig"] = copy.deepcopy(cfg.get("sig"))
    ns["_cfg_path"] = cfg.get("path")
    home = stt.get("home") or {}
    ns["_hermes_home"] = home.get("hermes_home", "")
    ns["_HERMES_HOME_AT_IMPORT"] = home.get("at_import", "")
    ns["_sessions"] = {sid: copy.deepcopy(sess) for sid, sess in (stt.get("sessions") or {}).items()}
    ns["_preview_action_reprobe"] = {sid: object() for sid in (stt.get("reprobe") or {})}
    ns["_mcp_reload_loaded_rev"] = (stt.get("mcp") or {}).get("loaded_rev", "")
    # silent locks + clock + backend object
    ns["_sessions_lock"] = NoOpLock()
    ns["_cfg_lock"] = NoOpLock()
    ns["_preview_action_reprobe_lock"] = NoOpLock()
    ns["time"] = SimpleNamespace(monotonic=lambda: seam("monotonic", {}))
    ns["_config_backend"] = SimpleNamespace(
        write_changes=lambda path, changes: seam("write_changes", {
            "path": proj_path(path),
            "changes": {"set": dict(changes.set), "unset": list(changes.unset)}}))
    ns["logger"] = RecLogger()


def norm(x):
    if isinstance(x, Path):
        return x.as_posix()
    if isinstance(x, SimpleNamespace):
        return {k: norm(v) for k, v in vars(x).items()}
    if isinstance(x, dict):
        return {k: norm(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [norm(v) for v in x]
    return x


def norm_state(ns):
    return {
        "cfg": {"cache": norm(ns["_cfg_cache"]), "sig": norm(ns["_cfg_sig"]), "path": norm(ns["_cfg_path"])},
        "home": {"hermes_home": ns["_hermes_home"], "at_import": ns["_HERMES_HOME_AT_IMPORT"]},
        "sessions": {sid: norm(sess) for sid, sess in ns["_sessions"].items()},
        "reprobe": {sid: True for sid in ns["_preview_action_reprobe"]},
        "mcp": {"loaded_rev": ns["_mcp_reload_loaded_rev"]},
    }


def drive(ns, func, inp):
    f = ns[func]
    if func == "_ok":
        return f(inp.get("rid"), inp.get("result"))
    if func == "_normalize_request":
        return f(inp.get("req"))
    if func == "_coerce_int_config_value":
        return f(inp.get("value"), inp.get("default"), min_value=inp.get("min_value"))
    if func == "_display_cfg":
        return f()
    if func == "_load_interim_assistant_messages":
        return f()
    if func == "_load_dashboard_process_isolation_config":
        return f(inp.get("cfg"))
    if func == "_active_config_path":
        return f()
    if func == "_load_cfg":
        return f()
    if func == "_drop_cfg_cache":
        return f()
    if func == "_load_cfg_raw":
        return f()
    if func == "_save_cfg":
        return f(inp.get("cfg"))
    if func == "_write_config_changes":
        return f(inp.get("sets"), tuple(inp.get("unsets") or ()))
    if func == "_write_config_key":
        return f(inp.get("key_path"), inp.get("value"))
    if func == "_write_config_keys":
        return f(inp.get("updates"))
    if func == "_is_model_switch_marker":
        return f(inp.get("entry"))
    if func == "_is_pivot_marker":
        return f(inp.get("entry"))
    if func == "_turn_started_at":
        return f(inp.get("session"))
    if func == "_cli_exec_blocked":
        return f(inp.get("argv"))
    if func == "_clarify_timeout_seconds":
        return f()
    if func == "_ask":
        return f(inp.get("method"), inp.get("sid"), inp.get("params"), timeout=inp.get("timeout", 300))
    if func == "_clarify_block":
        return f(inp.get("sid"), inp.get("questions"))
    if func == "_tour_request":
        return f(inp.get("sid"), inp.get("payload"))
    if func == "_preview_action_request":
        return f(inp.get("sid"), inp.get("payload"))
    if func == "_status_update":
        return f(inp.get("sid"), inp.get("kind"), inp.get("text"))
    if func == "_resolve_name":
        return f(inp.get("name"))
    if func == "_finish_reload":
        return f(inp.get("rid"), inp.get("params"), coalesced=inp.get("coalesced"))
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


def main():
    ns = extract()
    REAL.update({nm: ns[nm] for nm in TARGETS})
    COLLAB.update(collaborator_recorders())
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
    print("ORACLE tui_server_flow: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())