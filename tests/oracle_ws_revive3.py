#!/usr/bin/env python3
"""oracle_ws_revive3.py — differential oracle for the web-server group-3 revival.

Reads the harness JSONL on stdin ({"func","in","out"}), AST-extracts the LIVE
quarry defs (Assign + AnnAssign constants seeded, py3.14 dataclass module
alias registered), drives each live function under the SAME scripted seam
results the C harness consumed, and compares ordered trace + ret + post-state.
BaseException is caught per case (SystemExit included, bare spec round-trip);
missing/exhausted scripts fail closed on both sides.

Seam conventions (identical on both sides):
- In-function imports resolve to stub modules whose attributes are recording
  seams named in the drivers (get_env_value, has_direct_modal_credentials,
  _is_provider_active, PairingStore, list_memory_provider_names,
  normalize_profile_name, resp.json, redacted_credential_preview, time.time,
  _resolve_profile_dir, _category_providers, _call_cron_for_profile,
  _messaging_platform_catalog).
- Integral JSON numbers model Python ints, fractional ones floats; fixtures
  never carry integral float literals. JSON arrays model lists/tuples;
  objects model dicts or SimpleNamespace attribute carriers where noted.
- frozenset results normalize to SORTED arrays; tuples to lists.
- HTTPException rides {"__raise__":"HTTPException","status_code","detail"};
  every other exception {"__raise__":cls,"message":str(exc)}.
- str(value) fixture scope: scalars only; .lower()/\\D scope: ASCII only;
  sys.platform pinned to "win32" on both sides.

Gate:
  bash tests/run_one_oracle.sh ws_revive3 oracle_ws_revive3.py "" "lib/libjson/json.o"
"""
import ast
import builtins
import copy
import __future__
import hashlib
import json
import math
import os
import re
import subprocess
import sys
import types
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
os.environ.setdefault("HOME", str(REPO.parent))
os.environ.setdefault("USERPROFILE", str(REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(REPO / ".testbin" / "dbg_home_w3"))

FUT = __future__.annotations.compiler_flag
MISSING = object()


def norm(x):
    """Integral floats -> ints; tuples/sets -> lists (sets sorted); Path -> posix."""
    if isinstance(x, float):
        if math.isfinite(x) and x == int(x) and abs(x) < 1e16:
            return int(x)
        return x
    if isinstance(x, dict):
        return {k: norm(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [norm(v) for v in x]
    if isinstance(x, (set, frozenset)):
        return sorted(norm(v) for v in x)
    if isinstance(x, Path):
        return x.as_posix()
    return x


class NoopLock:
    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False


class HTTPException(Exception):
    """fastapi.HTTPException double — carries the fields the port emits."""

    def __init__(self, status_code=None, detail=None, headers=None):
        super().__init__(detail)
        self.status_code = status_code
        self.detail = detail
        self.headers = headers


class Raise:
    def __init__(self, spec):
        ty = spec.get("type", "Exception")
        msg = spec.get("message", "")
        self.spec = {"__raise__": ty, "message": msg}
        cls = getattr(builtins, ty, None)
        if not (isinstance(cls, type) and issubclass(cls, BaseException)):
            cls = type(ty, (Exception,), {})
        # Real builtins for the BaseException trio so except Exception
        # sites propagate them exactly like the C port does.
        self.exc = cls(msg)


class Script:
    def __init__(self, io_map):
        self.io = io_map or {}
        self.cursors = {}

    def next(self, name):
        entry = self.io.get(name, MISSING)
        if entry is MISSING:
            raise RuntimeError(f"no script for {name}")
        if isinstance(entry, list):
            i = self.cursors.get(name, 0)
            if i >= len(entry):
                raise RuntimeError(f"script exhausted for {name}")
            self.cursors[name] = i + 1
            entry = entry[i]
        if isinstance(entry, dict) and entry.get("__script__"):
            if "raise" in entry:
                return Raise(entry["raise"])
            return entry.get("ret")
        return entry


# ── AST extraction ──────────────────────────────────────────────────────────

def compile_node(node, filename):
    mod = ast.Module(body=[node], type_ignores=[])
    ast.fix_missing_locations(mod)
    return compile(mod, filename, "exec", flags=FUT, dont_inherit=True)


def load_ns(relpath, seeds, want_funcs=(), strict_consts=False):
    src = (QUARRY / relpath).read_text(encoding="utf-8")
    tree = ast.parse(src)
    modname = "quarry_w3_" + re.sub(r"\W", "_", relpath)
    mod = types.ModuleType(modname)
    sys.modules[modname] = mod
    ns = mod.__dict__
    ns.update(seeds)
    ns["__name__"] = modname
    consts = [n for n in tree.body
              if isinstance(n, (ast.Assign, ast.AnnAssign))
              and n.value is not None
              and all(isinstance(t, ast.Name) for t in
                      (n.targets if isinstance(n, ast.Assign) else [n.target]))]

    def seed_consts(strict):
        for n in consts:
            try:
                exec(compile_node(n, relpath), ns)
            except Exception:
                if strict:
                    raise

    seed_consts(False)
    for n in ast.walk(tree):
        if isinstance(n, ast.FunctionDef) and n.name in want_funcs:
            n2 = copy.copy(n)
            n2.decorator_list = []
            exec(compile_node(n2, relpath), ns)
    seed_consts(strict_consts)
    for name in want_funcs:
        assert name in ns, f"seed failure: {relpath}:{name}"
    return ns


def base_seeds(modname):
    logging_double = types.SimpleNamespace(
        getLogger=lambda name: types.SimpleNamespace(
            warning=lambda *a, **k: None, debug=lambda *a, **k: None,
            info=lambda *a, **k: None, error=lambda *a, **k: None,
            exception=lambda *a, **k: None))
    return {
        "__name__": modname,
        "Any": object, "Callable": object, "Optional": object, "Dict": object,
        "Literal": object, "Mapping": object, "Protocol": object, "Tuple": object,
        "List": object, "Awaitable": object, "Iterable": object, "Set": object,
        "dataclass": __import__("dataclasses").dataclass,
        "field": __import__("dataclasses").field,
        "contextlib": __import__("contextlib"),
        "functools": __import__("functools"),
        "threading": types.SimpleNamespace(Lock=NoopLock),
        "logging": logging_double,
        "time": types.SimpleNamespace(monotonic=lambda: 0.0, time=lambda: 0.0),
        "math": math, "re": re, "json": json, "hashlib": hashlib,
        "subprocess": subprocess,
        "os": types.SimpleNamespace(environ={}),
        "sys": sys,
        "HTTPException": HTTPException,
        "Path": Path,
    }


def pkg(name):
    if name not in sys.modules:
        m = types.ModuleType(name)
        m.__path__ = []
        sys.modules[name] = m
    return sys.modules[name]


def stub_mod(modname, **attrs):
    mod = types.ModuleType(modname)
    for k, v in attrs.items():
        setattr(mod, k, v)
    sys.modules[modname] = mod
    parent, _, child = modname.rpartition(".")
    if parent:
        setattr(pkg(parent), child, mod)
    return mod


# ── module namespaces (built once) ──────────────────────────────────────────

NS_MSG = load_ns(
    "hermes_cli/web_routers/messaging.py", base_seeds("quarry_w3_msg"),
    want_funcs=("_normalize_whatsapp_allowed_users",
                "_whatsapp_phone_from_identifier",
                "_normalize_whatsapp_onboarding_mode",
                "_prune_telegram_onboarding_pairings"))

NS_WSMEM = load_ns(
    "hermes_cli/web_server_memory.py", base_seeds("quarry_w3_wsmem"),
    want_funcs=("_string_list",))

NS_TOOLS = load_ns(
    "hermes_cli/web_routers/tools.py", base_seeds("quarry_w3_tools"),
    want_funcs=("_resolve_toolset_model_plugin", "_terminal_cfg_value",
                "_probe_ssh_backend", "_probe_modal_backend",
                "_find_toolset_provider_row", "_env_value"))

NS_FILES = load_ns(
    "hermes_cli/web_routers/files.py", base_seeds("quarry_w3_files"),
    want_funcs=("_chat_image_extension",))

NS_CFGENV = load_ns(
    "hermes_cli/web_routers/config_env.py", base_seeds("quarry_w3_cfgenv"),
    want_funcs=("_api_key_display",))

_oauth_seeds = base_seeds("quarry_w3_oauth")
_oauth_seeds["sys"] = types.SimpleNamespace(platform="win32")  # pinned
_oauth_seeds["_oauth_sessions_lock"] = NoopLock()
NS_OAUTH = load_ns(
    "hermes_cli/web_routers/oauth.py", _oauth_seeds,
    want_funcs=("_oauth_provider_disconnect_hint",
                "_oauth_provider_disconnect_command",
                "_claude_code_disconnect_command",
                "_codex_device_code_start_error",
                "_http_response_error_detail", "_gc_oauth_sessions"))

NS_MP = load_ns(
    "hermes_cli/web_routers/memory_providers.py", base_seeds("quarry_w3_mp"),
    want_funcs=("_read_field", "_declared_field_is_set",
                "_serialize_field_value", "_command_result",
                "_trim_setup_output"))

# Real utils.is_truthy_value for _serialize_field_value's in-function import.
NS_UTILS = load_ns(
    "utils/env.py", base_seeds("quarry_w3_utils"),
    want_funcs=("is_truthy_value",))
stub_mod("utils", is_truthy_value=NS_UTILS["is_truthy_value"])

_mcp_seeds = base_seeds("quarry_w3_mcp")
_mcp_seeds["_mcp_oauth_flows_lock"] = NoopLock()
NS_MCP = load_ns(
    "hermes_cli/web_routers/mcp.py", _mcp_seeds,
    want_funcs=("_gc_mcp_oauth_flows", "_mcp_install_action_name"))

NS_WSPROF = load_ns(
    "hermes_cli/web_server_profiles.py", base_seeds("quarry_w3_wsprof"),
    want_funcs=("_hub_action_name", "_profile_cli_args", "_resolve_profile_dir"))

NS_OPS = load_ns(
    "hermes_cli/web_routers/ops.py", base_seeds("quarry_w3_ops"),
    want_funcs=("_pairing_store",))

NS_WSCFG = load_ns(
    "hermes_cli/web_server_config.py", base_seeds("quarry_w3_wscfg"),
    want_funcs=("_memory_provider_options",))

NS_WSCRON = load_ns(
    "hermes_cli/web_server_cron.py", base_seeds("quarry_w3_wscron"),
    want_funcs=("_validate_dashboard_cron_context_from",))

NS_WSMSG = load_ns(
    "hermes_cli/web_server_messaging.py", base_seeds("quarry_w3_wsmsg"),
    want_funcs=("_channel_managed_env_keys",))

# Fail-closed seeding audit: every name a driver reads must be bound
# (a missing name would otherwise surface as a uniform KeyError "ret").
for _ns, _names in (
    (NS_MSG, ("_normalize_whatsapp_allowed_users", "_whatsapp_phone_from_identifier",
              "_normalize_whatsapp_onboarding_mode", "_prune_telegram_onboarding_pairings")),
    (NS_WSMEM, ("_string_list",)),
    (NS_TOOLS, ("_resolve_toolset_model_plugin", "_terminal_cfg_value", "_probe_ssh_backend",
                "_probe_modal_backend", "_find_toolset_provider_row", "_env_value")),
    (NS_FILES, ("_chat_image_extension",)),
    (NS_CFGENV, ("_api_key_display",)),
    (NS_OAUTH, ("_oauth_provider_disconnect_hint", "_oauth_provider_disconnect_command",
                "_claude_code_disconnect_command", "_codex_device_code_start_error",
                "_http_response_error_detail", "_gc_oauth_sessions")),
    (NS_MP, ("_read_field", "_declared_field_is_set", "_serialize_field_value",
             "_command_result", "_trim_setup_output")),
    (NS_MCP, ("_gc_mcp_oauth_flows", "_mcp_install_action_name")),
    (NS_WSPROF, ("_hub_action_name", "_profile_cli_args")),
    (NS_OPS, ("_pairing_store",)),
    (NS_WSCFG, ("_memory_provider_options",)),
    (NS_WSCRON, ("_validate_dashboard_cron_context_from",)),
    (NS_WSMSG, ("_channel_managed_env_keys",)),
    (NS_UTILS, ("is_truthy_value",)),
):
    for _nm in _names:
        assert _nm in _ns, f"unbound driver target: {_nm}"
assert NS_OAUTH["_OAUTH_SESSION_TTL_SECONDS"] == 900
assert NS_MCP["_MCP_DASHBOARD_OAUTH_TTL"] == 900
assert NS_FILES["_CHAT_IMAGE_MAGIC"][0][0] == b"\x89PNG\r\n\x1a\n"


# ── recording seam factory ──────────────────────────────────────────────────

def make_rec(script, trace):
    def rec(name, argb, make=None):
        def call(*a, **k):
            r = script.next(name)
            ev = {"ev": name, "args": argb(a, k), "ret": None}
            if isinstance(r, Raise):
                ev["ret"] = r.spec
                trace.append(ev)
                raise r.exc
            ev["ret"] = norm(r)
            trace.append(ev)
            return make(r) if make else r
        return call
    return rec


def _field_double(work):
    f = work.get("field") or {}
    return types.SimpleNamespace(
        key=f.get("key", ""), kind=f.get("kind", "text"),
        default=f.get("default", ""), env_key=f.get("env_key"),
        aliases=f.get("aliases", []), env_fallbacks=f.get("env_fallbacks", []))


def _install_env_value(rec):
    stub_mod("hermes_cli.config",
             get_env_value=rec("get_env_value", lambda a, k: {"name": a[0]}))


# ── per-function drivers ────────────────────────────────────────────────────

def d_normalize_whatsapp_allowed_users(work, trace, script):
    return NS_MSG["_normalize_whatsapp_allowed_users"](work.get("value"))


def d_whatsapp_phone_from_identifier(work, trace, script):
    return NS_MSG["_whatsapp_phone_from_identifier"](work.get("value"))


def d_normalize_whatsapp_onboarding_mode(work, trace, script):
    return NS_MSG["_normalize_whatsapp_onboarding_mode"](work.get("value"))


def d_string_list(work, trace, script):
    return NS_WSMEM["_string_list"](work.get("value"))


def d_resolve_toolset_model_plugin(work, trace, script):
    return NS_TOOLS["_resolve_toolset_model_plugin"](
        work.get("ts_key"), work.get("provider_row"))


def d_chat_image_extension(work, trace, script):
    return NS_FILES["_chat_image_extension"](bytes.fromhex(work.get("data_hex", "")))


def d_api_key_display(work, trace, script):
    rec = make_rec(script, trace)
    NS_CFGENV["redacted_credential_preview"] = rec(
        "redacted_credential_preview", lambda a, k: {"value": a[0]})
    return NS_CFGENV["_api_key_display"](work.get("entry", {}))


def d_oauth_provider_disconnect_hint(work, trace, script):
    return NS_OAUTH["_oauth_provider_disconnect_hint"](
        work.get("provider", {}), work.get("status", {}))


def d_codex_device_code_start_error(work, trace, script):
    rec = make_rec(script, trace)
    resp = types.SimpleNamespace(**(work.get("resp") or {}))
    resp.json = rec("resp.json", lambda a, k: {})
    return NS_OAUTH["_codex_device_code_start_error"](resp)


def d_read_field(work, trace, script):
    return NS_MP["_read_field"](
        _field_double(work), work.get("sources", []), work.get("env", {}))


def d_declared_field_is_set(work, trace, script):
    return NS_MP["_declared_field_is_set"](
        _field_double(work), work.get("sources", []), work.get("env", {}))


def d_serialize_field_value(work, trace, script):
    return NS_MP["_serialize_field_value"](_field_double(work), work.get("value"))


def d_command_result(work, trace, script):
    c = work.get("completed")
    completed = types.SimpleNamespace(**c) if isinstance(c, dict) else None
    return NS_MP["_command_result"](
        kind=work["kind"], name=work["name"], status=work["status"],
        command=work.get("command", ""), completed=completed,
        error=work.get("error"))


def d_terminal_cfg_value(work, trace, script):
    rec = make_rec(script, trace)
    _install_env_value(rec)
    return NS_TOOLS["_terminal_cfg_value"](
        work.get("terminal_cfg", {}), work["key"], work["env_var"])


def d_probe_ssh_backend(work, trace, script):
    rec = make_rec(script, trace)
    _install_env_value(rec)
    return NS_TOOLS["_probe_ssh_backend"](work.get("terminal_cfg", {}))


def d_probe_modal_backend(work, trace, script):
    rec = make_rec(script, trace)
    stub_mod("tools.tool_backend_helpers",
             has_direct_modal_credentials=rec(
                 "has_direct_modal_credentials", lambda a, k: {}))
    _install_env_value(rec)
    return NS_TOOLS["_probe_modal_backend"](work.get("cfg"))


def d_find_toolset_provider_row(work, trace, script):
    rec = make_rec(script, trace)
    NS_TOOLS["_category_providers"] = rec(
        "_category_providers", lambda a, k: {"ts_key": a[0], "config": a[1]})
    stub_mod("hermes_cli.tools_config",
             _is_provider_active=rec(
                 "_is_provider_active",
                 lambda a, k: {"p": a[0], "config": a[1],
                              "force_fresh": k.get("force_fresh")}))
    return NS_TOOLS["_find_toolset_provider_row"](
        work["ts_key"], work.get("config", {}), work.get("provider"))


def d_gc_oauth_sessions(work, trace, script):
    rec = make_rec(script, trace)
    NS_OAUTH["time"] = types.SimpleNamespace(time=rec("time.time", lambda a, k: {}))
    NS_OAUTH["_oauth_sessions"] = work.setdefault("oauth_sessions", {})
    NS_OAUTH["_oauth_sessions_lock"] = NoopLock()
    return NS_OAUTH["_gc_oauth_sessions"]()


def d_gc_mcp_oauth_flows(work, trace, script):
    rec = make_rec(script, trace)
    NS_MCP["time"] = types.SimpleNamespace(time=rec("time.time", lambda a, k: {}))
    flows_raw = work.setdefault("mcp_oauth_flows", {})
    NS_MCP["_mcp_oauth_flows"] = {
        fid: types.SimpleNamespace(**f) if isinstance(f, dict) else f
        for fid, f in flows_raw.items()}
    NS_MCP["_mcp_oauth_flows_lock"] = NoopLock()
    try:
        return NS_MCP["_gc_mcp_oauth_flows"]()
    finally:
        work["mcp_oauth_flows"] = {
            fid: (vars(f) if isinstance(f, types.SimpleNamespace) else f)
            for fid, f in NS_MCP["_mcp_oauth_flows"].items()}


def d_prune_telegram_onboarding_pairings(work, trace, script):
    rec = make_rec(script, trace)
    NS_MSG["time"] = types.SimpleNamespace(time=rec("time.time", lambda a, k: {}))
    raw = work.setdefault("telegram_onboarding_pairings", {})
    NS_MSG["_telegram_onboarding_pairings"] = {
        pid: types.SimpleNamespace(**r) if isinstance(r, dict) else r
        for pid, r in raw.items()}
    try:
        return NS_MSG["_prune_telegram_onboarding_pairings"]()
    finally:
        work["telegram_onboarding_pairings"] = {
            pid: (vars(r) if isinstance(r, types.SimpleNamespace) else r)
            for pid, r in NS_MSG["_telegram_onboarding_pairings"].items()}


def d_mcp_install_action_name(work, trace, script):
    alf = work.setdefault("action_log_files", {})
    stub_mod("hermes_cli.web_server_gateway", _ACTION_LOG_FILES=alf)
    return NS_MCP["_mcp_install_action_name"](work["name"])


def d_hub_action_name(work, trace, script):
    alf = work.setdefault("action_log_files", {})
    NS_WSPROF["_ACTION_LOG_FILES"] = alf
    return NS_WSPROF["_hub_action_name"](work["verb"], work["key"], work.get("profile"))


def d_profile_cli_args(work, trace, script):
    rec = make_rec(script, trace)
    NS_WSPROF["_resolve_profile_dir"] = rec(
        "_resolve_profile_dir", lambda a, k: {"name": a[0]})
    stub_mod("hermes_cli.profiles",
             normalize_profile_name=rec(
                 "normalize_profile_name", lambda a, k: {"name": a[0]}))
    return NS_WSPROF["_profile_cli_args"](work.get("profile"))


def d_pairing_store(work, trace, script):
    rec = make_rec(script, trace)
    NS_OPS["_resolve_profile_dir"] = rec(
        "_resolve_profile_dir", lambda a, k: {"name": a[0]})
    stub_mod("gateway.pairing",
             PairingStore=rec("PairingStore", lambda a, k: dict(k)))
    return NS_OPS["_pairing_store"](work.get("profile"))


def d_memory_provider_options(work, trace, script):
    rec = make_rec(script, trace)
    stub_mod("plugins.memory",
             list_memory_provider_names=rec(
                 "list_memory_provider_names", lambda a, k: {}))
    return NS_WSCFG["_memory_provider_options"]()


def d_validate_dashboard_cron_context_from(work, trace, script):
    rec = make_rec(script, trace)
    NS_WSCRON["_call_cron_for_profile"] = rec(
        "_call_cron_for_profile",
        lambda a, k: {"profile_name": a[0], "method": a[1],
                      "ref": a[2] if len(a) > 2 else None})
    return NS_WSCRON["_validate_dashboard_cron_context_from"](
        work.get("refs"), work.get("profile_name"))


def d_channel_managed_env_keys(work, trace, script):
    rec = make_rec(script, trace)
    NS_WSMSG["_messaging_platform_catalog"] = rec(
        "_messaging_platform_catalog", lambda a, k: {})
    return NS_WSMSG["_channel_managed_env_keys"]()


DRIVERS = {
    "ws_u_normalize_whatsapp_allowed_users": d_normalize_whatsapp_allowed_users,
    "ws_u_whatsapp_phone_from_identifier": d_whatsapp_phone_from_identifier,
    "ws_u_normalize_whatsapp_onboarding_mode": d_normalize_whatsapp_onboarding_mode,
    "ws_u_string_list": d_string_list,
    "ws_u_resolve_toolset_model_plugin": d_resolve_toolset_model_plugin,
    "ws_u_chat_image_extension": d_chat_image_extension,
    "ws_u_api_key_display": d_api_key_display,
    "ws_u_oauth_provider_disconnect_hint": d_oauth_provider_disconnect_hint,
    "ws_u_codex_device_code_start_error": d_codex_device_code_start_error,
    "ws_u_read_field": d_read_field,
    "ws_u_declared_field_is_set": d_declared_field_is_set,
    "ws_u_serialize_field_value": d_serialize_field_value,
    "ws_u_command_result": d_command_result,
    "ws_u_terminal_cfg_value": d_terminal_cfg_value,
    "ws_u_probe_ssh_backend": d_probe_ssh_backend,
    "ws_u_probe_modal_backend": d_probe_modal_backend,
    "ws_u_find_toolset_provider_row": d_find_toolset_provider_row,
    "ws_u_gc_oauth_sessions": d_gc_oauth_sessions,
    "ws_u_gc_mcp_oauth_flows": d_gc_mcp_oauth_flows,
    "ws_u_prune_telegram_onboarding_pairings": d_prune_telegram_onboarding_pairings,
    "ws_u_mcp_install_action_name": d_mcp_install_action_name,
    "ws_u_hub_action_name": d_hub_action_name,
    "ws_u_profile_cli_args": d_profile_cli_args,
    "ws_u_pairing_store": d_pairing_store,
    "ws_u_memory_provider_options": d_memory_provider_options,
    "ws_u_validate_dashboard_cron_context_from": d_validate_dashboard_cron_context_from,
    "ws_u_channel_managed_env_keys": d_channel_managed_env_keys,
}


def run_case(driver, inp):
    trace = []
    work = copy.deepcopy(inp)
    script = Script(work.get("io"))
    try:
        ret = driver(work, trace, script)
        work.pop("io", None)
        return {"ret": norm(ret), "trace": norm(trace), "state": norm(work)}
    except BaseException as e:  # SystemExit included — fail closed per case
        if isinstance(e, HTTPException):
            spec = {"__raise__": "HTTPException",
                    "status_code": e.status_code, "detail": e.detail}
        else:
            spec = {"__raise__": type(e).__name__, "message": str(e)}
        work.pop("io", None)
        return {"ret": spec, "trace": norm(trace), "state": norm(work)}


def main():
    n = 0
    mismatches = 0
    shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec = json.loads(line)
        except Exception as e:
            mismatches += 1
            print(f"BAD JSON from harness: {e}")
            continue
        n += 1
        func = rec.get("func", "")
        inp = rec.get("in") or {}
        got = rec.get("out")
        driver = DRIVERS.get(func)
        if driver is None:
            mismatches += 1
            print(f"UNKNOWN func {func!r}")
            continue
        if not isinstance(got, dict) or "error" in got:
            mismatches += 1
            print(f"HARNESS ERROR {func}: {got}")
            continue
        try:
            want = run_case(driver, inp)
        except BaseException as e:
            mismatches += 1
            print(f"ORACLE DRIVER FAILURE {func}: {type(e).__name__}: {e}")
            continue
        if got != want:
            mismatches += 1
            if shown < 8:
                shown += 1
                print(f"MISMATCH {func} case#{n}")
                for key in ("ret", "trace", "state"):
                    if got.get(key) != want.get(key):
                        print(f"  {key} C  : {json.dumps(got.get(key), ensure_ascii=False)[:600]}")
                        print(f"  {key} PY : {json.dumps(want.get(key), ensure_ascii=False)[:600]}")
    print(f"ORACLE ws_revive3: {n} cases, {mismatches} mismatches")
    sys.exit(1 if mismatches else 0)


if __name__ == "__main__":
    main()
