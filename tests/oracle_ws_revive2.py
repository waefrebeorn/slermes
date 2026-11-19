#!/usr/bin/env python3
"""oracle_ws_revive2.py — differential oracle for the web_server_wrappers
group-2 deep-stub revival (src/hermes_cli/ws_revive2.c).

Reads the harness JSONL on stdin ({"func","in","out"}), AST-extracts the LIVE
quarry defs (plus their pure siblings), seeds module constants (Assign +
AnnAssign), drives each live function under the SAME scripted seam results
the C harness consumed, and compares ordered trace + ret + post-state.
BaseException is caught per case (SystemExit included, with its code);
missing/exhausted scripts fail closed on both sides.

Seam ownership mirrors the C contract: the C dispatcher adopts args into
the trace; the recording stubs here snapshot args/ret at call time (norm
rebuilds containers), so later live mutation never rewrites a recorded
event. read_text strips one leading BOM on BOTH sides before recording.
HTTPException raise specs render str(exc) == "<status_code>: <detail>"
(default 400) on both sides.

Gate:
  bash tests/run_one_oracle.sh ws_revive2 oracle_ws_revive2.py "" "lib/libjson/json.o"
"""
import ast
import builtins
import copy
import dataclasses
import __future__
import functools
import json
import math
import os
import re
import sys
import textwrap
import types
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
os.environ.setdefault("HOME", str(REPO.parent))
os.environ.setdefault("USERPROFILE", str(REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(REPO / ".testbin" / "dbg_home_ws2"))

FUT = __future__.annotations.compiler_flag
MISSING = object()

from fastapi import HTTPException  # noqa: E402  (real class: str == "code: detail")
import asyncio  # noqa: E402


def norm(x):
    """Integral floats -> ints (libjson emits %.0f); tuples -> lists;
    Path-likes -> posix; dataclass instances -> field dicts."""
    if isinstance(x, float):
        if math.isfinite(x) and x == int(x) and abs(x) < 1e16:
            return int(x)
        return x
    if isinstance(x, dict):
        return {k: norm(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [norm(v) for v in x]
    if hasattr(x, "as_posix"):
        return x.as_posix()
    if dataclasses.is_dataclass(x) and not isinstance(x, type):
        return {f.name: norm(getattr(x, f.name)) for f in dataclasses.fields(x)}
    return x


class NoopLock:
    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False


class Raise:
    """Scripted exception. HTTPException renders the starlette str() shape
    the C dispatcher builds; SystemExit/KeyboardInterrupt/GeneratorExit/
    CancelledError are the REAL BaseException kinds so `except Exception`
    sites pass them through exactly as live does."""

    def __init__(self, spec):
        ty = spec.get("type", "Exception")
        msg = spec.get("message", "")
        assert ty != "KeyError", "scripted KeyError would need repr(msg) on the C side"
        if ty == "HTTPException":
            self.exc = HTTPException(status_code=spec.get("status_code", 400), detail=msg)
            self.spec = {"__raise__": ty, "message": str(self.exc)}
            return
        self.spec = {"__raise__": ty, "message": msg}
        if ty == "CancelledError":
            self.exc = asyncio.CancelledError(msg)
            return
        cls = getattr(builtins, ty, None)
        if not (isinstance(cls, type) and issubclass(cls, BaseException)):
            cls = type(ty, (Exception,), {})
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
    node = copy.copy(node)
    if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
        node.decorator_list = []  # extract plain defs (validators etc.)
    mod = ast.Module(body=[node], type_ignores=[])
    ast.fix_missing_locations(mod)
    return compile(mod, filename, "exec", flags=FUT, dont_inherit=True)


def load_ns(relpath, seeds, want_funcs=(), want_classes=(), strict_consts=True):
    src = (QUARRY / relpath).read_text(encoding="utf-8")
    tree = ast.parse(src)
    modname = "quarry_ws2_" + re.sub(r"\W", "_", relpath)
    mod = types.ModuleType(modname)
    sys.modules[modname] = mod
    ns = mod.__dict__
    ns.update(seeds)
    alias = ns.get("__name__", modname)
    if alias != modname:
        sys.modules[alias] = mod
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
    for n in tree.body:
        if isinstance(n, ast.ClassDef) and n.name in want_classes:
            exec(compile_node(n, relpath), ns)
        if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef)) and n.name in want_funcs:
            exec(compile_node(n, relpath), ns)
    seed_consts(strict_consts)
    for name in list(want_funcs) + list(want_classes):
        assert name in ns, f"seed failure: {relpath}:{name}"
    return ns


def extract_method(relpath, classname, methodname, seeds):
    src = (QUARRY / relpath).read_text(encoding="utf-8")
    tree = ast.parse(src)
    for n in tree.body:
        if isinstance(n, ast.ClassDef) and n.name == classname:
            for sub in n.body:
                if isinstance(sub, ast.FunctionDef) and sub.name == methodname:
                    seg = textwrap.dedent(ast.get_source_segment(src, sub))
                    node = ast.parse(seg).body[0]
                    modname = "quarry_ws2_m_" + re.sub(r"\W", "_", relpath)
                    mod = types.ModuleType(modname)
                    sys.modules[modname] = mod
                    ns = mod.__dict__
                    ns.update(seeds)
                    ns["__name__"] = modname
                    exec(compile_node(node, relpath), ns)
                    return ns
    raise AssertionError(f"method not found: {relpath}:{classname}.{methodname}")


def base_seeds(modname):
    logging_double = types.SimpleNamespace(
        getLogger=lambda name: types.SimpleNamespace(
            warning=lambda *a, **k: None, debug=lambda *a, **k: None,
            info=lambda *a, **k: None, error=lambda *a, **k: None))
    return {
        "__name__": modname,
        "Any": object, "Callable": object, "Optional": object, "Dict": object,
        "Literal": object, "Mapping": object, "Protocol": object, "Tuple": object,
        "Awaitable": object, "List": object,
        "dataclass": dataclasses.dataclass,
        "field": dataclasses.field,
        "contextlib": __import__("contextlib"),
        "functools": functools,
        "threading": types.SimpleNamespace(Lock=NoopLock),
        "logging": logging_double,
        "time": types.SimpleNamespace(monotonic=lambda: 0.0, time=lambda: 0.0),
        "asyncio": types.SimpleNamespace(Event=object, Lock=NoopLock,
                                         CancelledError=asyncio.CancelledError),
        "math": math, "re": re, "json": json,
        "os": types.SimpleNamespace(environ={}),
        "sys": sys,
        "HTTPException": HTTPException,
        "late": lambda name, mod: (lambda *a, **k: None),
        "APIRouter": lambda *a, **k: types.SimpleNamespace(
            get=lambda *a, **k: (lambda f: f), post=lambda *a, **k: (lambda f: f),
            put=lambda *a, **k: (lambda f: f), delete=lambda *a, **k: (lambda f: f)),
    }


def pkg(name):
    if name not in sys.modules:
        m = types.ModuleType(name)
        m.__path__ = []
        sys.modules[name] = m
    return sys.modules[name]


def install_mod(dotted, **attrs):
    parent, _, leaf = dotted.rpartition(".")
    m = types.ModuleType(dotted)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[dotted] = m
    if parent:
        setattr(pkg(parent), leaf, m)
    return m


def uninstall_mod(dotted):
    sys.modules.pop(dotted, None)
    parent, _, leaf = dotted.rpartition(".")
    if parent and parent in sys.modules:
        p = sys.modules[parent]
        if hasattr(p, leaf):
            delattr(p, leaf)


# ── module namespaces (built once) ──────────────────────────────────────────

NS_WMEM = load_ns(
    "hermes_cli/web_server_memory.py", base_seeds("quarry_ws2_wmem"),
    want_funcs=("_field_value", "_field_visible", "_coerce_bool",
                "_field_default", "_env_lookup", "_require_memory_provider_ready"),
    strict_consts=False)

_mprov_seeds = base_seeds("quarry_ws2_mprov")
_mprov_seeds.update(
    _coerce_bool=NS_WMEM["_coerce_bool"], _field_default=NS_WMEM["_field_default"],
    STORAGE_FLAT_JSON="flat_json", STORAGE_PROVIDER="provider",
    _write_provider_flat=lambda *a, **k: None,
    _write_provider_own=lambda *a, **k: None,
    CONFIG_MUTATION_LOCK=NoopLock())
NS_MPROV = load_ns(
    "hermes_cli/web_routers/memory_providers.py", _mprov_seeds,
    want_funcs=("_coerce_schema_field", "_coerce_schema_number",
                "_memory_section", "_update_memory_provider_config"),
    strict_consts=False)

NS_WCFG = load_ns(
    "hermes_cli/web_server_config.py", base_seeds("quarry_ws2_wcfg"),
    want_funcs=("_is_command_provider_block", "_infer_provider_on_model_change"),
    strict_consts=False)

NS_CENV = load_ns(
    "hermes_cli/web_routers/config_env.py", base_seeds("quarry_ws2_cenv"),
    want_funcs=("_models_from_custom_endpoint_entry",), strict_consts=False)

NS_OPS = load_ns(
    "hermes_cli/web_routers/ops.py", base_seeds("quarry_ws2_ops"),
    want_funcs=("_webhook_route_summary",), strict_consts=False)

NS_WPROF = load_ns(
    "hermes_cli/web_server_profiles.py", base_seeds("quarry_ws2_wprof"),
    want_funcs=("_aux_task_summary", "_merge_aux_into_by_model", "_token_volume"),
    strict_consts=False)
assert NS_WPROF["_AUX_COUNTERS"] == ("input_tokens", "output_tokens",
                                     "estimated_cost", "api_calls")

NS_OAUTH = load_ns(
    "hermes_cli/web_routers/oauth.py", base_seeds("quarry_ws2_oauth"),
    want_funcs=("_http_response_error_detail",), strict_consts=False)

NS_PROF = load_ns(
    "hermes_cli/web_routers/profiles.py", base_seeds("quarry_ws2_prof"),
    want_funcs=("_profile_to_dict",), strict_consts=False)


class _SeedPath:
    """Path double used at SEED time (constants like _HOSTED_MANAGED_FILES_ROOT).
    Per-case drivers install recorder-backed subclasses."""

    def __init__(self, s=""):
        self.s = str(s)

    def as_posix(self):
        return self.s

    def __str__(self):
        return self.s

    def __eq__(self, o):
        return isinstance(o, _SeedPath) and self.s == o.s

    def __hash__(self):
        return hash(self.s)


_wfiles_seeds = base_seeds("quarry_ws2_wfiles")
_wfiles_seeds["Path"] = _SeedPath
NS_WFILES = load_ns(
    "hermes_cli/web_server_files.py", _wfiles_seeds,
    want_funcs=("_managed_files_policy",), want_classes=("ManagedFilesPolicy",),
    strict_consts=False)
assert NS_WFILES["_MANAGED_FILES_ROOT_ENV"] == "HERMES_DASHBOARD_FILES_ROOT"
assert NS_WFILES["_HOSTED_MANAGED_FILES_ROOT"].as_posix() == "/opt/data"

NS_MSG = load_ns(
    "hermes_cli/web_routers/messaging.py", base_seeds("quarry_ws2_msg"),
    want_funcs=("_multiplex_port_binding_conflict",
                "_whatsapp_linked_account_from_session",
                "_first_str", "_whatsapp_phone_from_identifier"),
    strict_consts=False)

NS_WGW = load_ns(
    "hermes_cli/web_server_gateway.py", base_seeds("quarry_ws2_wgw"),
    want_funcs=("_split_text_for_speak_stream",), strict_consts=False)

# SENTENCE_BOUNDARY_RE — the real compiled regex from tools.tts_streaming.
_ts_src = (QUARRY / "tools/tts_streaming.py").read_text(encoding="utf-8")
_ts_ns = {"re": re}
for _n in ast.parse(_ts_src).body:
    if isinstance(_n, ast.Assign) and any(
            isinstance(t, ast.Name) and t.id == "SENTENCE_BOUNDARY_RE" for t in _n.targets):
        exec(compile_node(_n, "tools/tts_streaming.py"), _ts_ns)
assert "SENTENCE_BOUNDARY_RE" in _ts_ns
SENTENCE_BOUNDARY_RE = _ts_ns["SENTENCE_BOUNDARY_RE"]

NS_WS = load_ns(
    "hermes_cli/web_server.py", base_seeds("quarry_ws2_ws"),
    want_funcs=("_get_dashboard_plugins",), strict_consts=False)
NS_WS["_dashboard_plugins_cache"] = None
NS_WS["_discover_dashboard_plugins"] = lambda *a, **k: None

NS_WMODELS = extract_method(
    "hermes_cli/web_models.py", "_MoaReferenceControls", "_validate_reference_timeout",
    {"math": math, "Any": object, "Optional": object})

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


def minus_io(work):
    work.pop("io", None)
    return work


def path_arg(x):
    return x.as_posix() if hasattr(x, "as_posix") else x
# ── per-function drivers ────────────────────────────────────────────────────

def d_validate_timeout(work, trace, script):
    return NS_WMODELS["_validate_reference_timeout"](None, work.get("value"))


def d_command_block(work, trace, script):
    return NS_WCFG["_is_command_provider_block"](work.get("value"))


def d_models_from_entry(work, trace, script):
    return NS_CENV["_models_from_custom_endpoint_entry"](work.get("entry"))


def d_webhook_summary(work, trace, script):
    return NS_OPS["_webhook_route_summary"](
        work.get("name"), work.get("route"), work.get("base_url"))


def d_field_value(work, trace, script):
    rec = make_rec(script, trace)
    NS_WMEM["_env_lookup"] = rec("env_lookup", lambda a, k: {"env_key": a[0]})
    return NS_WMEM["_field_value"](work["field"], work["data"])


def d_field_visible(work, trace, script):
    rec = make_rec(script, trace)
    NS_WMEM["_env_lookup"] = rec("env_lookup", lambda a, k: {"env_key": a[0]})
    return NS_WMEM["_field_visible"](
        work["field"], work["data"], fields_by_key=work.get("fields_by_key"))


def d_coerce_field(work, trace, script):
    return NS_MPROV["_coerce_schema_field"](work["field"], work.get("raw"))


def d_aux_summary(work, trace, script):
    return NS_WPROF["_aux_task_summary"](work.get("aux_rows"))


def d_merge_aux(work, trace, script):
    return NS_WPROF["_merge_aux_into_by_model"](work.get("by_model"), work.get("aux_rows"))


def d_error_detail(work, trace, script):
    rec = make_rec(script, trace)
    raw = work.get("resp")
    if isinstance(raw, dict):
        resp = types.SimpleNamespace(**raw)
        resp.json = rec("resp.json", lambda a, k: {})
    else:
        resp = raw
    return NS_OAUTH["_http_response_error_detail"](resp)


def d_provider_ready(work, trace, script):
    rec = make_rec(script, trace)
    NS_WMEM["_discover_memory_provider_statuses"] = rec(
        "discover_memory_provider_statuses", lambda a, k: {})
    return NS_WMEM["_require_memory_provider_ready"](work.get("name"))


def d_profile_to_dict(work, trace, script):
    info = work.get("info")
    if isinstance(info, dict):
        info = types.SimpleNamespace(**info)
    return NS_PROF["_profile_to_dict"](info)


def d_managed_policy(work, trace, script):
    rec = make_rec(script, trace)

    class CasePath(_SeedPath):
        @classmethod
        def home(cls):
            return rec("path_home", lambda a, k: {},
                       make=lambda r: cls(r) if isinstance(r, str) else r)()

    class EnvDouble:
        def get(self, key, default=None):
            return rec("environ.get",
                       lambda a, k: {"key": a[0], "default": a[1] if len(a) > 1 else None})(
                           key, default)

    NS_WFILES["Path"] = CasePath
    NS_WFILES["os"] = types.SimpleNamespace(environ=EnvDouble())
    NS_WFILES["_ensure_managed_root"] = rec(
        "ensure_managed_root", lambda a, k: {"raw_path": path_arg(a[0])},
        make=lambda r: CasePath(r) if isinstance(r, str) else r)
    NS_WFILES["_canonical_path"] = rec(
        "canonical_path", lambda a, k: {"path": path_arg(a[0])},
        make=lambda r: CasePath(r) if isinstance(r, str) else r)
    NS_WFILES["_default_hermes_root_is_opt_data"] = rec(
        "default_hermes_root_is_opt_data", lambda a, k: {})
    return NS_WFILES["_managed_files_policy"](None, create_root=work.get("create_root", True))


def d_multiplex_conflict(work, trace, script):
    rec = make_rec(script, trace)
    install_mod("gateway.config",
                SHARED_LISTENER_MIRROR_PLATFORMS=frozenset({"api_server", "webhook"}))
    install_mod("hermes_cli.profiles",
                get_active_profile_name=rec("get_active_profile_name", lambda a, k: {}))
    install_mod("hermes_cli.gateway_multiplex_mode",
                default_gateway_multiplexes=rec("default_gateway_multiplexes", lambda a, k: {}))
    NS_MSG["_resolve_profile_dir"] = rec(
        "resolve_profile_dir", lambda a, k: {"profile": a[0]})
    return NS_MSG["_multiplex_port_binding_conflict"](
        work.get("platform_id"), work.get("requested_profile"))


def d_infer_provider(work, trace, script):
    rec = make_rec(script, trace)
    if work.get("models_import", True):
        install_mod(
            "hermes_cli.models",
            _AGGREGATOR_PROVIDERS={"nous", "openrouter", "ai-gateway", "kilocode"},
            detect_provider_for_model=rec(
                "detect_provider_for_model",
                lambda a, k: {"model": a[0], "prev_provider": a[1]},
                make=lambda r: tuple(r) if isinstance(r, list) else r),
            normalize_provider=rec("normalize_provider", lambda a, k: {"provider": a[0]}))
    else:
        uninstall_mod("hermes_cli.models")
    if work.get("detect_import", True):
        install_mod(
            "hermes_cli.models_detect",
            provider_has_credentials=rec("provider_has_credentials",
                                         lambda a, k: {"provider": a[0]}))
    else:
        uninstall_mod("hermes_cli.models_detect")
    return NS_WCFG["_infer_provider_on_model_change"](
        work.get("model_val"), work.get("prev_provider"))


def d_split_speak(work, trace, script):
    install_mod("tools.tts_streaming", SENTENCE_BOUNDARY_RE=SENTENCE_BOUNDARY_RE)
    return NS_WGW["_split_text_for_speak_stream"](work.get("text"), work.get("cap"))


def d_dashboard_plugins(work, trace, script):
    rec = make_rec(script, trace)

    class CasePath:
        def __init__(self, s):
            if not isinstance(s, str):
                raise TypeError(
                    "argument should be a str or an os.PathLike object where "
                    f"__fspath__ returns a str, not '{type(s).__name__}'")
            self.s = s

        def is_dir(self):
            return rec("path_is_dir", lambda a, k: {"path": self.s})()

        def as_posix(self):
            return self.s

    install_mod(
        "hermes_cli.web_server_plugin_profiles",
        secondary_profile_home=rec(
            "secondary_profile_home", lambda a, k: {},
            make=lambda r: CasePath(r) if isinstance(r, str) else r),
        profile_dashboard_plugins=rec(
            "profile_dashboard_plugins",
            lambda a, k: {"home": path_arg(a[0]), "force_rescan": a[1]}))
    NS_WS["Path"] = CasePath
    NS_WS["_discover_dashboard_plugins"] = rec("discover_dashboard_plugins", lambda a, k: {})
    NS_WS["_dashboard_plugins_cache"] = work.get("cache")
    ret = NS_WS["_get_dashboard_plugins"](work.get("force_rescan", False))
    # The home branch returns before touching the global cache; mirror the
    # C port, which only writes req.cache on the cache branch.
    home_branch = (trace and trace[0]["ev"] == "secondary_profile_home"
                   and trace[0]["ret"] is not None)
    if not home_branch:
        work["cache"] = NS_WS["_dashboard_plugins_cache"]
    return ret


def d_update_provider_config(work, trace, script):
    rec = make_rec(script, trace)
    holder = {}

    def load_config_call(*a, **k):
        r = script.next("load_config")
        ev = {"ev": "load_config", "args": {}, "ret": None}
        if isinstance(r, Raise):
            ev["ret"] = r.spec
            trace.append(ev)
            raise r.exc
        ev["ret"] = norm(r)
        trace.append(ev)
        obj = copy.deepcopy(r)
        holder["config"] = obj
        return obj

    def writer_args(a, k):
        p = a[0]
        return {"provider": vars(p) if isinstance(p, types.SimpleNamespace) else p,
                "values": a[1]}

    NS_MPROV["_STORAGE_WRITERS"] = {
        "flat_json": rec("write_provider_flat", writer_args),
        "provider": rec("write_provider_own", writer_args),
    }
    NS_MPROV["load_config"] = load_config_call
    NS_MPROV["save_config"] = rec("save_config", lambda a, k: {"config": a[0]})
    NS_MPROV["CONFIG_MUTATION_LOCK"] = NoopLock()
    prov = work.get("provider")
    if isinstance(prov, dict):
        prov = types.SimpleNamespace(**prov)
    ret = NS_MPROV["_update_memory_provider_config"](prov, work.get("values"))
    if "config" in holder:
        work["config"] = holder["config"]
    return ret


def d_whatsapp_linked(work, trace, script):
    class CredPath:
        def __init__(self, s):
            if not isinstance(s, str):
                raise TypeError(
                    "argument should be a str or an os.PathLike object where "
                    f"__fspath__ returns a str, not '{type(s).__name__}'")
            self.s = s

        def __truediv__(self, other):
            base = self.s.rstrip("/")
            return CredPath(base + "/" + str(other) if base else str(other))

        def read_text(self, encoding="utf-8"):
            # Dedicated recorder: the utf-8-sig BOM strip happens BEFORE
            # the event is recorded, exactly as the C dispatcher does.
            r = script.next("read_text")
            args = {"path": self.s, "encoding": encoding}
            if isinstance(r, Raise):
                trace.append({"ev": "read_text", "args": args, "ret": r.spec})
                raise r.exc
            if isinstance(r, str) and r.startswith("\ufeff"):
                r = r[1:]
            trace.append({"ev": "read_text", "args": args, "ret": norm(r)})
            return r

    return NS_MSG["_whatsapp_linked_account_from_session"](CredPath(work["session_path"]))


DRIVERS = {
    "ws_u_validate_reference_timeout": d_validate_timeout,
    "ws_u_is_command_provider_block": d_command_block,
    "ws_u_models_from_custom_endpoint_entry": d_models_from_entry,
    "ws_u_webhook_route_summary": d_webhook_summary,
    "ws_u_field_value": d_field_value,
    "ws_u_field_visible": d_field_visible,
    "ws_u_coerce_schema_field": d_coerce_field,
    "ws_u_aux_task_summary": d_aux_summary,
    "ws_u_merge_aux_into_by_model": d_merge_aux,
    "ws_u_http_response_error_detail": d_error_detail,
    "ws_u_require_memory_provider_ready": d_provider_ready,
    "ws_u_profile_to_dict": d_profile_to_dict,
    "ws_u_managed_files_policy": d_managed_policy,
    "ws_u_multiplex_port_binding_conflict": d_multiplex_conflict,
    "ws_u_infer_provider_on_model_change": d_infer_provider,
    "ws_u_split_text_for_speak_stream": d_split_speak,
    "ws_u_get_dashboard_plugins": d_dashboard_plugins,
    "ws_u_update_memory_provider_config": d_update_provider_config,
    "ws_u_whatsapp_linked_account_from_session": d_whatsapp_linked,
}


def run_case(driver, inp):
    trace = []
    work = copy.deepcopy(inp)
    script = Script(work.get("io"))
    try:
        ret = driver(work, trace, script)
        minus_io(work)
        return {"ret": norm(ret), "trace": norm(trace), "state": norm(work)}
    except BaseException as e:  # SystemExit included — fail closed per case
        spec = {"__raise__": type(e).__name__, "message": str(e)}
        if isinstance(e, SystemExit):
            spec["code"] = e.code
        minus_io(work)
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
    print(f"ORACLE ws_revive2: {n} cases, {mismatches} mismatches")
    sys.exit(1 if mismatches else 0)


if __name__ == "__main__":
    main()
