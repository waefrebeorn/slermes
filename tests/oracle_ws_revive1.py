#!/usr/bin/env python3
"""Differential oracle for the ws_revive1 revival wave (17 ports).

Reads the harness's JSONL stdout on stdin ({func, in, out, trace, st}),
recomputes the LIVE Python reference for each case from the echoed
"in" (AST-extracted defs + seeded Assign/AnnAssign constants, identical
scripted seams), and diffs out / ordered trace / post-state.

Fail-closed: extraction failures, constant drift vs the C pins, unknown
funcs and corrupt lines exit 2. BaseException (SystemExit!) is caught
per case and compared as {"__raise__": cls, "msg": str(exc)}.

Double conventions (mirrored exactly by src/hermes_cli/ws_revive1.c):
  - PathDouble: POSIX-string Path stand-in (is_dir/exists/read_text seam).
  - ProcDouble: {"pid": int?, "poll": null|int}; poll() == None is live.
  - StoreDouble / PersistenceDouble: scripted value + close() seam.
  - ProviderDouble: dict with get_config_schema=="fn" carries the method.
  - SecretDouble: bearer_token.get_secret_value().
  - SimpleNamespace wraps scripted attribute objects (pydantic bodies,
    registry providers, catalog cards, ModelSwitchResult); jnorm unwraps.
  - httpx double: Client.request -> "httpx_request" seam; the response
    spec {"status_code", "json"|"json_error"} drives raise_for_status/json.
  - CONFIG_SCHEMA / OPTIONAL_ENV_VARS are per-case injected data inputs.

Run:  python tests/oracle_ws_revive1.py < harness.out
Gate: bash tests/run_one_oracle.sh ws_revive1 oracle_ws_revive1.py "" \
          "lib/libjson/json.o"
"""
import ast
import asyncio
import builtins
import copy
import json
import math
import os
import re
import sys
import textwrap
import types
from datetime import datetime, UTC
from pathlib import Path
from typing import Any, Callable, Collection, Optional

_REPO = Path(__file__).resolve().parent.parent
HERMES_ROOT = Path(r"C:/Users/eman5/hermes-agent")
if str(HERMES_ROOT) not in sys.path:
    sys.path.insert(0, str(HERMES_ROOT))
os.environ.setdefault("HOME", str(_REPO.parent.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

BASE_EXC = {
    "SystemExit": SystemExit,
    "KeyboardInterrupt": KeyboardInterrupt,
    "GeneratorExit": GeneratorExit,
    "CancelledError": asyncio.CancelledError,
}


def fatal(msg):
    print(f"ORACLE ws_revive1 FATAL: {msg}", file=sys.stderr)
    sys.exit(2)


# ---------------------------------------------------------------- extraction

def extract(modpath, funcs=(), consts=(), seed=None):
    """Exec selected live nodes into a registered module namespace."""
    path = HERMES_ROOT / modpath
    src = path.read_text(encoding="utf-8")
    tree = ast.parse(src)
    modname = "ws1live_" + modpath.replace("/", "_").replace(".", "_")
    mod = types.ModuleType(modname)
    mod.__file__ = str(path)
    sys.modules[modname] = mod
    ns = mod.__dict__
    ns.update(seed or {})

    def exec_node(node):
        seg = ast.get_source_segment(src, node)
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.col_offset > 0:
            seg = textwrap.dedent(seg)
        exec(compile(seg, str(path), "exec"), ns)

    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            targets = node.targets if isinstance(node, ast.Assign) else [node.target]
            if any(isinstance(t, ast.Name) and t.id in consts for t in targets):
                exec_node(node)
    for node in ast.walk(tree):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in funcs:
            exec_node(node)
    # re-seed constants that reference extracted defs. Only re-exec a const
    # whose value actually names an extracted def: re-executing an identity
    # sentinel (_OMIT = object()) mints a NEW object, so `is` checks against
    # defaults bound at def time (_status_card's last_refresh=_OMIT) fail and
    # the bare object leaks into the card (TypeError on json.dumps).
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            targets = node.targets if isinstance(node, ast.Assign) else [node.target]
            if any(isinstance(t, ast.Name) and t.id in consts for t in targets):
                val = node.value
                if val is None or not any(
                        isinstance(n, ast.Name) and n.id in funcs
                        for n in ast.walk(val)):
                    continue
                try:
                    exec_node(node)
                except Exception:
                    pass
    missing = [n for n in funcs if n not in ns] + [n for n in consts if n not in ns]
    if missing:
        fatal(f"{modpath}: failed to bind {missing}")
    return ns


def stub_package(name):
    if name not in sys.modules:
        m = types.ModuleType(name)
        m.__path__ = []
        sys.modules[name] = m
    return sys.modules[name]


def stub_module(name):
    if name not in sys.modules:
        m = types.ModuleType(name)
        sys.modules[name] = m
        if "." in name:
            parent, _, leaf = name.rpartition(".")
            setattr(stub_package(parent), leaf, m)
    return sys.modules[name]


# ---------------------------------------------------------------- doubles

class HTTPException(Exception):
    """starlette-shaped double: str(exc) == f"{status_code}: {detail}"."""

    def __init__(self, status_code=500, detail=None, headers=None):
        self.status_code = status_code
        self.detail = detail if detail is not None else ""
        self.headers = headers
        super().__init__(detail)

    def __str__(self):
        return f"{self.status_code}: {self.detail}"


class TransientStoreError(Exception):
    pass


EXC_CLASSES = {"HTTPException": HTTPException, "TransientStoreError": TransientStoreError}


def make_exc(spec):
    kind = spec.get("__raise__", "Exception")
    msg = spec.get("msg", "")
    if kind == "HTTPException":
        code, _, detail = msg.partition(": ")
        try:
            return HTTPException(int(code), detail)
        except ValueError:
            return HTTPException(500, msg)
    cls = EXC_CLASSES.get(kind) or BASE_EXC.get(kind)
    if cls is None:
        cand = getattr(builtins, kind, None)
        if isinstance(cand, type) and issubclass(cand, BaseException):
            cls = cand
        else:
            cls = type(kind, (Exception,), {})
    if kind == "KeyError":
        return cls(msg)  # str(KeyError(k)) == repr(k)
    return cls(msg)


class PathDouble:
    """POSIX-string Path stand-in; filesystem ops cross the seam."""

    def __init__(self, s):
        s = str(s)
        while len(s) > 1 and s.endswith("/"):
            s = s[:-1]
        self.s = s

    def __str__(self):
        return self.s

    def __truediv__(self, other):
        o = str(other)
        if o.startswith("/"):
            return PathDouble(o)
        return PathDouble(self.s + "/" + o)

    @property
    def name(self):
        return self.s.rsplit("/", 1)[-1]

    @property
    def parent(self):
        if "/" not in self.s:
            return PathDouble(".")
        head = self.s.rsplit("/", 1)[0]
        return PathDouble(head or "/")

    def is_dir(self):
        return _CTX.recorder("path_is_dir")(self.s)

    def exists(self):
        return _CTX.recorder("path_exists")(self.s)

    def read_text(self, encoding=None):
        raw = _CTX.recorder("read_text")(self.s, encoding)
        if encoding == "utf-8-sig" and isinstance(raw, str) and raw.startswith("﻿"):
            raw = raw[1:]
        return raw


class ProcDouble:
    def __init__(self, spec):
        self.spec = spec

    def poll(self):
        return self.spec.get("poll")

    def __getattr__(self, name):
        spec = object.__getattribute__(self, "spec")
        if name in spec:
            return spec[name]
        raise AttributeError(name)


class StoreDouble:
    def __init__(self, ctx, value):
        self.ctx = ctx
        self.value = value

    def close(self):
        self.ctx.recorder("store_close")(self.value)


class ProviderDouble:
    def __init__(self, ctx, raw):
        self.ctx = ctx
        self.raw = raw

    def get_config_schema(self):
        return self.ctx.recorder("provider_get_config_schema")()


class SecretDouble:
    def __init__(self, v):
        self.v = v

    def get_secret_value(self):
        return self.v


class AnalyticsDouble:
    def __init__(self, ctx):
        self.ctx = ctx

    def call_usage_by_route(self, started_after):
        return self.ctx.recorder("call_usage_by_route")(started_after)

    def session_usage_by_route(self, started_after):
        return self.ctx.recorder("session_usage_by_route")(started_after)

    def session_usage_totals(self, started_after, model_rows_only):
        return self.ctx.recorder("session_usage_totals")(started_after, model_rows_only)


class PersistenceDouble:
    def __init__(self, ctx, value):
        self.ctx = ctx
        self.value = value
        self.usage_analytics = AnalyticsDouble(ctx)

    def close(self):
        self.ctx.recorder("persistence_close")(self.value)


class ScopeCM:
    def __init__(self, ctx, profile):
        self.ctx = ctx
        self.profile = profile

    def __enter__(self):
        self.ctx.recorder("profile_scope_enter")(self.profile)
        return self

    def __exit__(self, *a):
        self.ctx.recorder("profile_scope_exit")(self.profile)
        return False


class LoggerDouble:
    def __init__(self, ctx):
        self.ctx = ctx

    def _log(self, level, msg, *a, **k):
        formatted = msg % a if a else msg
        self.ctx.trace.append({"ev": f"log_{level}", "args": [formatted], "ret": None})

    def debug(self, msg, *a, **k):
        self._log("debug", msg, *a, **k)

    def info(self, msg, *a, **k):
        self._log("info", msg, *a, **k)

    def warning(self, msg, *a, **k):
        self._log("warning", msg, *a, **k)

    def exception(self, msg, *a, **k):
        self._log("exception", msg, *a, **k)


class TimeDouble:
    def __init__(self, ctx):
        self.ctx = ctx

    def monotonic(self):
        return self.ctx.recorder("time_monotonic")()

    def time(self):
        return self.ctx.recorder("time_time")()


class EntryDouble:
    def __init__(self, path):
        self.path = path


class ScanCM:
    def __init__(self, entries):
        self.entries = entries

    def __enter__(self):
        return [EntryDouble(p) for p in self.entries]

    def __exit__(self, *a):
        return False


class OsDouble:
    def __init__(self, ctx):
        self.ctx = ctx

    def scandir(self, root):
        raw = self.ctx.recorder("scandir")(str(root))
        return ScanCM(raw)


class JsonDouble:
    def __init__(self, ctx):
        self.ctx = ctx

    def loads(self, text):
        return self.ctx.recorder("json_loads")(text)


class HttpStatusErrorDouble(Exception):
    def __init__(self, message="", response=None):
        self.response = response
        super().__init__(message)


class HttpResponseDouble:
    def __init__(self, ctx, spec):
        self.ctx = ctx
        self.spec = spec if isinstance(spec, dict) else {}
        self.status_code = self.spec.get("status_code", 200)

    def raise_for_status(self):
        if 400 <= self.status_code < 600:
            raise HttpStatusErrorDouble("status error", response=self)

    def json(self):
        if "json_error" in self.spec:
            raise make_exc(self.spec["json_error"])
        if "json" not in self.spec:
            raise ValueError("no json body")
        return copy.deepcopy(self.spec["json"])


class HttpClientDouble:
    def __init__(self, ctx, timeout=None, headers=None):
        self.ctx = ctx

    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False

    def request(self, method, url, headers=None, **kw):
        spec = self.ctx.recorder("httpx_request")(method, url, headers, kw.get("json"))
        return HttpResponseDouble(self.ctx, spec)


class HttpxDouble(types.ModuleType):
    pass


HTTPX = HttpxDouble("httpx")
HTTPX.Timeout = lambda *a, **k: None
HTTPX.HTTPStatusError = HttpStatusErrorDouble
HTTPX.Client = lambda timeout=None, headers=None: HttpClientDouble(_CTX, timeout, headers)
sys.modules["httpx"] = HTTPX


def jnorm(x):
    """Unwrap doubles for trace/state comparison (C sees plain JSON)."""
    if isinstance(x, PathDouble):
        return str(x)
    if isinstance(x, ProcDouble):
        return jnorm(x.spec)
    if isinstance(x, StoreDouble):
        return jnorm(x.value)
    if isinstance(x, ProviderDouble):
        return jnorm(x.raw)
    if isinstance(x, SecretDouble):
        return jnorm(x.v)
    if isinstance(x, PersistenceDouble):
        return jnorm(x.value)
    if isinstance(x, types.SimpleNamespace):
        return {k: jnorm(v) for k, v in vars(x).items()}
    if isinstance(x, dict):
        return {k: jnorm(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [jnorm(v) for v in x]
    return x


def materialize_provider(ctx, x):
    if isinstance(x, dict) and x.get("get_config_schema") == "fn":
        return ProviderDouble(ctx, x)
    return x


def materialize_proc(x):
    return ProcDouble(x) if isinstance(x, dict) else x


def materialize_restart(x):
    if x is None:
        return None
    ts, proc, cmd = x
    return (ts, materialize_proc(proc), tuple(cmd))


def materialize_body(x):
    if not isinstance(x, dict):
        return x
    bt = x.get("bearer_token")
    return types.SimpleNamespace(
        name=x.get("name"), url=x.get("url"), command=x.get("command"),
        auth=x.get("auth"),
        bearer_token=SecretDouble(bt) if bt is not None else None,
        args=x.get("args"), env=x.get("env"))


# ---------------------------------------------------------------- seam scripts

class CaseCtx:
    def __init__(self, io):
        self.io = io or {}
        self.trace = []
        self.counts = {}

    def script_value(self, name):
        if name not in self.io:
            raise RuntimeError(f"unscripted seam: {name}")
        script = self.io[name]
        if isinstance(script, list):
            idx = self.counts.get(name, 0)
            if idx >= len(script):
                raise RuntimeError(f"script exhausted: {name}")
            self.counts[name] = idx + 1
            return script[idx]
        if isinstance(script, dict) and script.get("__script__"):
            return script.get("ret")
        return script

    def recorder(self, name):
        def call(*args):
            try:
                raw = self.script_value(name)
            except RuntimeError as e:
                self.trace.append({"ev": name, "args": [jnorm(a) for a in args],
                                   "ret": {"__raise__": "RuntimeError", "msg": str(e)}})
                raise
            self.trace.append({"ev": name, "args": [jnorm(a) for a in args],
                               "ret": copy.deepcopy(raw)})
            if isinstance(raw, dict) and "__raise__" in raw:
                raise make_exc(raw)
            return copy.deepcopy(raw)
        return call


_CTX = None   # per-case context, read by the module-level doubles


def norm(x):
    """Integral floats -> int; tuples -> lists; sets -> sorted lists."""
    if isinstance(x, bool):
        return x
    if isinstance(x, float):
        if math.isfinite(x) and x == int(x):
            return int(x)
        return x
    if isinstance(x, dict):
        return {k: norm(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [norm(v) for v in x]
    if isinstance(x, set):
        return sorted(norm(v) for v in x)
    return x


# ---------------------------------------------------------------- stub modules

HC = stub_package("hermes_cli")
CFG_STUB = stub_module("hermes_cli.config")
AUTH_STUB = stub_module("hermes_cli.auth")
HC.auth = AUTH_STUB
WSP_STUB = stub_module("hermes_cli.web_server_profiles")
MCPCFG_STUB = stub_module("hermes_cli.mcp_config")
MCPSEC_STUB = stub_module("hermes_cli.mcp_security")
MA_STUB = stub_module("hermes_cli.model_assignment")
PROF_STUB = stub_module("hermes_cli.profiles")
INV_STUB = stub_module("hermes_cli.inventory")
MOD_STUB = stub_module("hermes_cli.models")
PC_STUB = stub_module("hermes_cli.provider_catalog")
HCONST = stub_module("hermes_constants")
PASM = stub_module("persistence_assembly")
SRET = stub_module("session_retention")
PLUG = stub_package("plugins")
PMEM = stub_module("plugins.memory")
PLUG.memory = PMEM
TOOLS = stub_package("tools")
TTS_STUB = stub_module("tools.tts_tool")
STT_STUB = stub_module("tools.transcription_common")
AGENT = stub_package("agent")
TTSREG = stub_module("agent.tts_registry")
STTREG = stub_module("agent.transcription_registry")
AMEM = stub_module("agent.memory_provider")
PERS = stub_package("persistence")
PCORE = stub_package("persistence.core")
PERR = stub_module("persistence.core.errors")
PERR.TransientStoreError = TransientStoreError

# ---------------------------------------------------------------- live extraction

try:
    NS_TTS = extract("tools/tts_command_provider.py", consts=("BUILTIN_TTS_PROVIDERS",))
    NS_STT = extract("tools/transcription_common.py", consts=("BUILTIN_STT_PROVIDERS",))
    TTS_STUB.BUILTIN_TTS_PROVIDERS = NS_TTS["BUILTIN_TTS_PROVIDERS"]
    STT_STUB.BUILTIN_STT_PROVIDERS = NS_STT["BUILTIN_STT_PROVIDERS"]

    NS_CFGMOD = extract("hermes_cli/config.py", funcs=("cfg_get",),
                        seed={"Any": Any, "Optional": Optional})
    NS_CPROV = extract("hermes_cli/config_providers.py",
                       funcs=("coerce_provider_id", "_canonical_api_mode"),
                       consts=("_API_MODE_ALIASES",))
    NS_WSOAUTH = extract("hermes_cli/web_server_oauth.py", funcs=("_truncate_token",),
                         seed={"Optional": Optional})

    NS_MEM = extract("hermes_cli/web_server_memory.py",
                     funcs=("_normalize_memory_provider_schema", "_schema_field_kind",
                            "_normalize_memory_provider_name", "_memory_provider_status",
                            "_read_memory_provider_existing_values",
                            "_discover_memory_provider_statuses"),
                     seed={"Any": Any, "Optional": Optional})
    NS_CFG = extract("hermes_cli/web_server_config.py",
                     funcs=("_custom_provider_options", "_is_command_provider_block",
                            "_schema_select_options", "_denormalize_config_from_web",
                            "_schema_with_dynamic_provider_options"),
                     seed={"Any": Any, "Optional": Optional,
                           "cfg_get": NS_CFGMOD["cfg_get"], "CONFIG_SCHEMA": {}})
    NS_GW = extract("hermes_cli/web_server_gateway.py",
                    funcs=("_profile_platform_ports",),
                    consts=("_PORT_BINDING_PLATFORM_PORTS", "_PLATFORM_DEAD_STATES"),
                    seed={"Optional": Optional, "Path": Path})
    NS_MCP = extract("hermes_cli/web_server_mcp.py",
                     funcs=("normalize_mcp_server_create",),
                     seed={"Any": Any, "Optional": Optional,
                           "MCPServerCreate": type("MCPServerCreate", (), {})})
    NS_OAUTH = extract("hermes_cli/web_routers/oauth.py",
                       funcs=("_resolve_provider_status", "_status_card", "_epoch_ms_to_iso"),
                       consts=("_OMIT", "_PROVIDER_STATUS"),
                       seed={"Any": Any, "Optional": Optional, "Callable": Callable,
                             "datetime": datetime, "UTC": UTC,
                             "_truncate_token": NS_WSOAUTH["_truncate_token"]})
    NS_SESS = extract("hermes_cli/web_server_sessions.py",
                      funcs=("_maybe_auto_archive_for_profile",),
                      consts=("_AUTO_ARCHIVE_CHECK_INTERVAL_S",),
                      seed={"Optional": Optional, "_last_auto_archive_check": {}})
    NS_WS = extract("hermes_cli/web_server.py",
                    funcs=("_spawn_gateway_restart",),
                    consts=("GATEWAY_RESTART_COOLDOWN_SECONDS", "_LAST_GATEWAY_RESTART"),
                    seed={"Optional": Optional})
    NS_CE = extract("hermes_cli/web_routers/config_env.py",
                    funcs=("_custom_endpoint_response", "_catalog_provider_env_metadata",
                           "_models_from_custom_endpoint_entry", "_endpoint_row",
                           "_api_key_display", "_endpoint_api_mode",
                           "_model_names_provider", "_custom_endpoint_id", "_provider_card"),
                    consts=("_DESKTOP_API_MODES", "_AUTH_TYPE_ENV_VARS"),
                    seed={"Any": Any, "Optional": Optional, "re": re,
                          "coerce_provider_id": NS_CPROV["coerce_provider_id"],
                          "_canonical_api_mode": NS_CPROV["_canonical_api_mode"],
                          "_API_MODE_ALIASES": NS_CPROV["_API_MODE_ALIASES"],
                          "OPTIONAL_ENV_VARS": {}})
    NS_AN = extract("hermes_cli/web_routers/analytics.py",
                    funcs=("_get_models_analytics", "_fold_session_only_rows",
                           "_merge_aux_into_rows", "_has_usage"),
                    consts=("_MODEL_CARD_KEYS", "_AUX_SUMMED_KEYS", "_USAGE_KEYS"),
                    seed={"Any": Any, "Optional": Optional})
    NS_MSG = extract("hermes_cli/web_server_messaging.py",
                     funcs=("_telegram_onboarding_request_sync",
                            "_telegram_onboarding_error_message"),
                     consts=("_TELEGRAM_UNAVAILABLE", "_TELEGRAM_INVALID"),
                     seed={"Any": Any, "HTTPException": HTTPException, "httpx": HTTPX})
    NS_MODELS = extract("hermes_cli/web_routers/models.py",
                        funcs=("get_recommended_default_model",),
                        seed={"Optional": Optional, "HTTPException": HTTPException})
    NS_DASH = extract("hermes_cli/web_server_dashboard.py",
                      funcs=("_discover_dashboard_plugins", "_dashboard_plugin_entry"),
                      seed={"Optional": Optional, "Path": PathDouble})
except SystemExit:
    raise
except BaseException as e:
    fatal(f"extraction failed: {type(e).__name__}: {e}")

# _strip_bearer_prefix is a ported C helper: the oracle binds the LIVE def
# into the mcp_config stub so both sides run the same logic.
try:
    _ns_mcp_cfg = extract("hermes_cli/mcp_config.py", funcs=("_strip_bearer_prefix",),
                          seed={"Any": Any})
    MCPCFG_STUB._strip_bearer_prefix = _ns_mcp_cfg["_strip_bearer_prefix"]
except SystemExit:
    raise
except BaseException as e:
    fatal(f"mcp_config extraction failed: {type(e).__name__}: {e}")

# ---------------------------------------------------------------- C-pin checks

if frozenset(NS_TTS["BUILTIN_TTS_PROVIDERS"]) != frozenset({
        "edge", "elevenlabs", "openai", "minimax", "xai", "mistral", "gemini",
        "neutts", "kittentts", "piper", "deepinfra"}):
    fatal("BUILTIN_TTS_PROVIDERS drifted from C pin")
if frozenset(NS_STT["BUILTIN_STT_PROVIDERS"]) != frozenset({
        "local", "local_command", "groq", "openai", "mistral", "xai",
        "elevenlabs", "deepinfra"}):
    fatal("BUILTIN_STT_PROVIDERS drifted from C pin")
if NS_GW["_PORT_BINDING_PLATFORM_PORTS"] != {
        "webhook": ("port", 8644), "api_server": ("port", 8642),
        "msgraph_webhook": ("port", 8646), "feishu": ("webhook_port", 8765),
        "wecom_callback": ("port", 8645), "bluebubbles": ("webhook_port", 8645),
        "sms": ("webhook_port", 8080), "whatsapp_cloud": ("webhook_port", 8090),
        "line": ("port", 8646), "teams": ("port", 3978)}:
    fatal("_PORT_BINDING_PLATFORM_PORTS drifted from C pin")
if set(NS_GW["_PLATFORM_DEAD_STATES"]) != {"fatal", "disconnected", "stopped"}:
    fatal("_PLATFORM_DEAD_STATES drifted from C pin")
if NS_CPROV["_API_MODE_ALIASES"] != {
        "openai": "chat_completions", "openai_chat": "chat_completions",
        "openai-chat": "chat_completions", "chat-completions": "chat_completions",
        "chatcompletions": "chat_completions", "responses": "codex_responses",
        "openai_responses": "codex_responses", "openai-responses": "codex_responses",
        "anthropic": "anthropic_messages", "anthropic-messages": "anthropic_messages",
        "messages": "anthropic_messages", "bedrock": "bedrock_converse",
        "bedrock-converse": "bedrock_converse"}:
    fatal("_API_MODE_ALIASES drifted from C pin")
if set(NS_CE["_DESKTOP_API_MODES"]) != {"chat_completions", "codex_responses",
                                       "anthropic_messages"}:
    fatal("_DESKTOP_API_MODES drifted from C pin")
if tuple(NS_AN["_MODEL_CARD_KEYS"]) != (
        "input_tokens", "output_tokens", "cache_read_tokens", "reasoning_tokens",
        "estimated_cost", "actual_cost", "sessions", "api_calls", "tool_calls",
        "last_used_at", "avg_tokens_per_session"):
    fatal("_MODEL_CARD_KEYS drifted from C pin")
if tuple(NS_AN["_AUX_SUMMED_KEYS"]) != (
        "input_tokens", "output_tokens", "cache_read_tokens", "reasoning_tokens",
        "estimated_cost", "api_calls"):
    fatal("_AUX_SUMMED_KEYS drifted from C pin")
if tuple(NS_AN["_USAGE_KEYS"]) != (
        "input_tokens", "output_tokens", "cache_read_tokens", "reasoning_tokens",
        "estimated_cost", "actual_cost", "api_calls", "tool_calls"):
    fatal("_USAGE_KEYS drifted from C pin")
if NS_SESS["_AUTO_ARCHIVE_CHECK_INTERVAL_S"] != 300.0:
    fatal("_AUTO_ARCHIVE_CHECK_INTERVAL_S drifted from C pin 300.0")
if NS_WS["GATEWAY_RESTART_COOLDOWN_SECONDS"] != 10.0:
    fatal("GATEWAY_RESTART_COOLDOWN_SECONDS drifted from C pin 10.0")
if NS_MSG["_TELEGRAM_UNAVAILABLE"] != "Telegram setup service is unavailable. Try again shortly.":
    fatal("_TELEGRAM_UNAVAILABLE drifted from C pin")
if NS_MSG["_TELEGRAM_INVALID"] != "Telegram setup service returned an invalid response.":
    fatal("_TELEGRAM_INVALID drifted from C pin")
for _k, _v in {
        "not_found": "Telegram pairing was not found. Start a new setup.",
        "expired": "Telegram setup expired. Start a new setup.",
        "claimed": "Telegram setup was already claimed. Start a new setup.",
        "unauthorized": "Telegram setup service rejected this request.",
        "telegram_manager_bot_token_not_configured": "Telegram setup service is not configured.",
        "telegram_token_fetch_failed": "Telegram could not finish bot setup. Try again."}.items():
    if NS_MSG["_telegram_onboarding_error_message"](_k, "FB") != _v:
        fatal(f"telegram error message {_k!r} drifted from C pin")
if list(NS_OAUTH["_PROVIDER_STATUS"].keys()) != [
        "nous", "openai-codex", "qwen-oauth", "minimax-oauth", "xai-oauth"]:
    fatal("_PROVIDER_STATUS keys drifted from C pin")
if [v[0] for v in NS_OAUTH["_PROVIDER_STATUS"].values()] != [
        "get_nous_auth_status_local", "get_codex_auth_status", "get_qwen_auth_status",
        "get_minimax_oauth_auth_status", "get_xai_oauth_auth_status"]:
    fatal("_PROVIDER_STATUS getters drifted from C pin")
if {k: [v[0] for v in vs] for k, vs in NS_CE["_AUTH_TYPE_ENV_VARS"].items()} != {
        "aws_sdk": ["AWS_REGION", "AWS_PROFILE"],
        "vertex": ["VERTEX_CREDENTIALS_PATH"]}:
    fatal("_AUTH_TYPE_ENV_VARS drifted from C pin")


# ---------------------------------------------------------------- live drive

def drive(rec):
    global _CTX
    func = rec["func"]
    inp = rec.get("in") or {}
    ctx = CaseCtx(inp.get("io"))
    _CTX = ctx
    state = inp.get("state") or {}
    st = {}
    try:
        # per-case seam installs
        CFG_STUB.get_hermes_home = lambda: (
            lambda raw: PathDouble(raw) if raw is not None else None
        )(ctx.recorder("get_hermes_home")())
        CFG_STUB.load_config = ctx.recorder("load_config")
        CFG_STUB.read_user_config_raw = lambda p: ctx.recorder("read_user_config_raw")(str(p))
        CFG_STUB.get_compatible_custom_providers = ctx.recorder("get_compatible_custom_providers")
        AUTH_STUB.get_auth_status = ctx.recorder("get_auth_status")
        for _g in ("get_nous_auth_status_local", "get_codex_auth_status",
                   "get_qwen_auth_status", "get_minimax_oauth_auth_status",
                   "get_xai_oauth_auth_status"):
            setattr(AUTH_STUB, _g, ctx.recorder(_g))
        WSP_STUB._plugin_terminal_backend_rows = ctx.recorder("plugin_terminal_backend_rows")
        MCPCFG_STUB._bearer_auth_headers = ctx.recorder("bearer_auth_headers")
        MCPSEC_STUB.validate_mcp_server_entry = ctx.recorder("validate_mcp_server_entry")
        MA_STUB.apply_main_model_assignment = ctx.recorder("apply_main_model_assignment")
        MA_STUB.normalize_main_model_assignment = ctx.recorder("normalize_main_model_assignment")
        PROF_STUB._check_gateway_running = lambda p: ctx.recorder("check_gateway_running")(str(p))
        INV_STUB.build_models_payload = ctx.recorder("build_models_payload")
        INV_STUB.load_picker_context = ctx.recorder("load_picker_context")
        MOD_STUB.pick_silent_default_model = lambda models, provider: ctx.recorder(
            "pick_silent_default_model")(models, provider)
        if inp.get("catalog_import_fails"):
            if hasattr(PC_STUB, "provider_catalog"):
                delattr(PC_STUB, "provider_catalog")
        else:
            PC_STUB.provider_catalog = lambda: [
                types.SimpleNamespace(**c) if isinstance(c, dict) else c
                for c in ctx.recorder("provider_catalog")()]
        HCONST.set_hermes_home_override = ctx.recorder("set_hermes_home_override")
        HCONST.reset_hermes_home_override = ctx.recorder("reset_hermes_home_override")
        PASM.profile_scope = lambda *a, **k: ctx.recorder("profile_scope")(k.get("home"))
        PASM.open_persistence = lambda scope: StoreDouble(
            ctx, ctx.recorder("open_persistence")(scope))
        SRET.run_auto_archive = lambda store, idle_days, min_interval_hours: ctx.recorder(
            "run_auto_archive")(store.value if isinstance(store, StoreDouble) else store,
                                idle_days, min_interval_hours)
        PMEM.discover_memory_providers = ctx.recorder("discover_memory_providers")
        PMEM.parked_provider_notice = ctx.recorder("parked_provider_notice")
        # Registry entries are attribute objects live (getattr(_p, "name"));
        # materialize scripted dicts so .name resolves instead of silently
        # dropping every plugin name.
        def _voice_providers():
            return [types.SimpleNamespace(**c) if isinstance(c, dict) else c
                    for c in ctx.recorder("list_voice_providers")()]
        TTSREG.list_providers = _voice_providers
        STTREG.list_providers = _voice_providers
        AMEM.is_core_memory_provider = ctx.recorder("is_core_memory_provider")
        HTTPX.Client = lambda timeout=None, headers=None: HttpClientDouble(ctx, timeout, headers)

        for _ns in (NS_MEM, NS_SESS, NS_WS, NS_MODELS, NS_DASH):
            _ns["_log"] = LoggerDouble(ctx)
        NS_SESS["time"] = TimeDouble(ctx)
        NS_WS["time"] = TimeDouble(ctx)
        NS_AN["time"] = TimeDouble(ctx)
        NS_CFG["_memory_provider_schema_options"] = ctx.recorder("memory_provider_schema_options")
        NS_CFG["_infer_provider_on_model_change"] = ctx.recorder("infer_provider_on_model_change")
        NS_CFG["_validated_main_model_selection"] = lambda cfg, p, m: types.SimpleNamespace(
            **ctx.recorder("validated_main_model_selection")(cfg, p, m))
        NS_MEM["_load_memory_provider"] = lambda name: materialize_provider(
            ctx, ctx.recorder("load_memory_provider")(name))
        NS_MEM["_read_json_file"] = lambda p: ctx.recorder("read_json_file")(str(p))
        NS_MEM["_memory_provider_setup_info"] = ctx.recorder("memory_provider_setup_info")
        NS_MEM["_memory_provider_is_configured"] = ctx.recorder("memory_provider_is_configured")
        NS_SESS["_session_db_path_for_profile"] = lambda profile: (
            lambda raw: PathDouble(raw) if raw is not None else None
        )(ctx.recorder("session_db_path_for_profile")(profile))
        NS_AN["_open_session_db_for_profile"] = lambda profile, read_only: PersistenceDouble(
            ctx, ctx.recorder("open_session_db")(profile, read_only))
        NS_AN["_attach_tool_calls"] = ctx.recorder("attach_tool_calls")
        NS_AN["_aux_usage_rows"] = ctx.recorder("aux_usage_rows")
        NS_AN["_model_capabilities"] = ctx.recorder("model_capabilities")
        NS_MSG["hermes_user_agent"] = ctx.recorder("hermes_user_agent")
        NS_MSG["_telegram_onboarding_base_url"] = ctx.recorder("telegram_onboarding_base_url")
        NS_MODELS["_config_profile_scope"] = lambda profile: ScopeCM(ctx, profile)
        NS_MODELS["_nous_recommended_default"] = ctx.recorder("nous_recommended_default")
        NS_DASH["_dashboard_plugin_search_dirs"] = lambda: [
            (PathDouble(root), source)
            for root, source in ctx.recorder("dashboard_plugin_search_dirs")()]
        NS_DASH["_safe_plugin_api_relpath"] = lambda api, dashboard_dir: ctx.recorder(
            "safe_plugin_api_relpath")(api, str(dashboard_dir))
        NS_DASH["os"] = OsDouble(ctx)
        NS_DASH["json"] = JsonDouble(ctx)
        NS_CE["redacted_credential_preview"] = ctx.recorder("redacted_credential_preview")
        NS_CE["get_compatible_custom_providers"] = ctx.recorder("get_compatible_custom_providers")
        NS_CE["OPTIONAL_ENV_VARS"] = inp.get("optional_env_vars") or {}

        if func == "ws_u_normalize_memory_provider_schema":
            out = NS_MEM["_normalize_memory_provider_schema"](
                inp["name"], materialize_provider(ctx, inp.get("provider")))
        elif func == "ws_u_read_memory_provider_existing_values":
            out = NS_MEM["_read_memory_provider_existing_values"](inp["name"])
        elif func == "ws_u_discover_memory_provider_statuses":
            out = NS_MEM["_discover_memory_provider_statuses"]()
        elif func == "ws_u_profile_platform_ports":
            out = NS_GW["_profile_platform_ports"](
                PathDouble(inp["profile_home"]), inp.get("runtime"))
        elif func == "ws_u_custom_provider_options":
            out = NS_CFG["_custom_provider_options"](
                inp["kind"], inp["builtin_names"], inp["cfg"])
        elif func == "ws_u_schema_with_dynamic_provider_options":
            NS_CFG["CONFIG_SCHEMA"] = inp.get("schema") or {}
            out = NS_CFG["_schema_with_dynamic_provider_options"]()
        elif func == "ws_u_normalize_mcp_server_create":
            out = NS_MCP["normalize_mcp_server_create"](materialize_body(inp.get("body")))
        elif func == "ws_u_denormalize_config_from_web":
            out = NS_CFG["_denormalize_config_from_web"](inp["config"])
        elif func == "ws_u_resolve_provider_status":
            sf = inp.get("status_fn")
            status_fn = ctx.recorder("status_fn") if sf == "fn" else sf
            out = NS_OAUTH["_resolve_provider_status"](inp.get("provider_id", ""), status_fn)
        elif func == "ws_u_maybe_auto_archive_for_profile":
            NS_SESS["_last_auto_archive_check"] = dict(state.get("last_auto_archive_check") or {})
            # Capture post-state even when the call raises: the check stamp is
            # written BEFORE load_config, and the C harness emits st on raises.
            try:
                out = NS_SESS["_maybe_auto_archive_for_profile"](inp.get("profile"))
            finally:
                st = {"last_auto_archive_check": NS_SESS["_last_auto_archive_check"]}
        elif func == "ws_u_spawn_gateway_restart":
            gm = types.SimpleNamespace()
            gm._gateway_subcommand = ctx.recorder("gateway_subcommand")
            gm._spawn_hermes_action = ctx.recorder("spawn_hermes_action")
            gm._ACTION_PROCS = {k: materialize_proc(v)
                                for k, v in (state.get("action_procs") or {}).items()}
            gm._ACTION_COMMANDS = {k: tuple(v)
                                   for k, v in (state.get("action_commands") or {}).items()}
            NS_WS["_gateway_mod"] = gm
            NS_WS["_LAST_GATEWAY_RESTART"] = materialize_restart(state.get("last_gateway_restart"))
            # Seed st with the pre-call state, then refresh in finally: the C
            # harness emits st even when the call raises (e.g. the in-flight
            # restart guard), with whatever state survived.
            st = {"last_gateway_restart": state.get("last_gateway_restart"),
                  "action_procs": state.get("action_procs") or {},
                  "action_commands": state.get("action_commands") or {}}
            try:
                out = NS_WS["_spawn_gateway_restart"](inp.get("profile"),
                                                      interrupt=inp.get("interrupt", False))
            finally:
                st = {"last_gateway_restart": jnorm(NS_WS["_LAST_GATEWAY_RESTART"]),
                      "action_procs": {k: jnorm(v) for k, v in gm._ACTION_PROCS.items()},
                      "action_commands": {k: list(v) for k, v in gm._ACTION_COMMANDS.items()}}
        elif func == "ws_u_custom_endpoint_response":
            out = NS_CE["_custom_endpoint_response"](inp["cfg"])
        elif func == "ws_u_get_models_analytics":
            out = NS_AN["_get_models_analytics"](inp.get("days", 30), inp.get("profile"))
        elif func == "ws_u_telegram_onboarding_request_sync":
            kw = {"body": inp.get("body"), "bearer_token": inp.get("bearer_token")}
            out = NS_MSG["_telegram_onboarding_request_sync"](
                inp["method"], inp["path"], **kw)
        elif func == "ws_get_recommended_default_model":
            out = NS_MODELS["get_recommended_default_model"](
                inp.get("provider", ""), inp.get("profile"))
        elif func == "ws_u_discover_dashboard_plugins":
            sd = inp.get("search_dirs")
            if sd is not None:
                sd = [(PathDouble(root), source) for root, source in sd]
            out = NS_DASH["_discover_dashboard_plugins"](sd)
        elif func == "ws_u_catalog_provider_env_metadata":
            out = NS_CE["_catalog_provider_env_metadata"]()
        else:
            fatal(f"unknown func {func}")
    except BaseException as e:  # SystemExit included — fail-closed per case
        out = {"__raise__": type(e).__name__, "msg": str(e)}
    return {"func": func, "in": inp, "out": norm(jnorm(out)),
            "trace": norm(jnorm(ctx.trace)), "st": norm(jnorm(st))}


# ---------------------------------------------------------------- comparison

def main():
    n = 0
    mismatches = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        n += 1
        try:
            got = json.loads(line)
        except ValueError as e:
            print(f"CASE {n}: harness line is not valid JSON: {e}")
            mismatches += 1
            continue
        func = got.get("func", "?")
        try:
            want = drive(got)
        except SystemExit:
            raise
        except BaseException as e:
            print(f"CASE {n} ({func}): oracle drive crashed: {type(e).__name__}: {e}")
            mismatches += 1
            continue
        diffs = []
        if got.get("func") != want["func"]:
            diffs.append(("func", got.get("func"), want["func"]))
        if norm(got.get("in")) != want["in"]:
            diffs.append(("in-echo", norm(got.get("in")), want["in"]))
        if norm(got.get("out")) != want["out"]:
            diffs.append(("out", norm(got.get("out")), want["out"]))
        if norm(got.get("trace")) != want["trace"]:
            diffs.append(("trace", norm(got.get("trace")), want["trace"]))
        if norm(got.get("st")) != want["st"]:
            diffs.append(("st", norm(got.get("st")), want["st"]))
        if diffs:
            mismatches += 1
            if mismatches <= 20:
                print(f"MISMATCH case {n} func={func}")
                for field_, g, w in diffs:
                    print(f"  {field_}:\n    C : {json.dumps(g, sort_keys=True)}"
                          f"\n    PY: {json.dumps(w, sort_keys=True)}")
    print(f"ORACLE ws_revive1: {n} cases, {mismatches} mismatches")
    sys.exit(1 if mismatches else 0)


if __name__ == "__main__":
    main()