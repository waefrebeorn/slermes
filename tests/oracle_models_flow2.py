#!/usr/bin/env python3
"""Oracle for the hermes_cli/models.py UNCLAIMED-remainder FLOW port
(src/cli/models_flow2.c).

Drives the LIVE models.py defs with SCRIPTED seams: every collaborator (claimed
siblings, foreign-module imports, credential/pool/clock reads, logger) is a
recording stub fed the same scripts the C harness feeds md2_call. Compares the
DECISION SEQUENCE (ordered "trace" of {ev,args,ret} seam events), the canonical
return ("ret") and the post-call out-state ("state"). The ported functions call
each other REAL (AST-extracted into one per-case namespace), so internal
composition (_live_result_entry -> _cache_entry, _model_requires_account_discovery
-> _normalized_cache_slug, _static_catalog -> _chat_catalog_rows,
cached_fetch_api_models -> _cache_entry/_chat_catalog_rows/_store_cache_entry,
get_cached_nous_free_tier -> _pricing_profile_key) is exercised, matching the C
port's internal transcription.

Live defs are AST-extracted from the quarry (annotations kept as strings via the
__future__ flag); only the needed module constants are seeded per case. The heavy
quarry import chain is never imported. Every exec namespace is a registered
sys.modules module (py3.14 annotation resolution wants a real module alias).
Fail-closed: 0 cases => exit 1; any harness/oracle exception per case rides as
{"__raise__": cls} (BaseException, so SystemExit/KeyboardInterrupt propagate).
Summary: 'ORACLE models_flow2: N cases, M mismatches'.

Representation contract (both sides, see src/cli/models_flow2.c):
- tuple/set -> array (sets sorted); None -> null; integral floats collapse to
  ints; Path -> str; CuratedFallbackModels -> {"$curated": [...]};
  _NativePickerModelList -> {"$native": [...]}; uncaught exception ->
  {"__raise__": cls}; an exception passed as a logger arg projects to
  {"__raise__": cls, "msg": str(e)}.
- scripts: absent key -> None; {"$seq": [...]} per-call queue (exhaustion raises
  RuntimeError("script exhausted: <name>")); {"__raise__": cls, "msg": m} raises
  (trace ret is the WHOLE marker); any other value reused. None-valued arg keys
  dropped on both sides.
- except Exception never catches the BaseException trio.
- fixture domain: str()/float() operands are ASCII strings/numbers/bools/null;
  float() tokens exclude underscores; URLs are scheme://host ASCII; cache rows
  carry fp/at/models; base_url_origin results are 3-element arrays.
"""
import __future__
import ast
import builtins
import json
import logging
import math
import os
import sys
import types
import urllib.parse
from pathlib import Path, PurePath, PurePosixPath
from typing import Any, Optional

try:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    if hasattr(sys.stdin, "reconfigure"):
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "hermes_cli" / "models.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "hermes_cli" / "models.py")
STATIC_SRC = str(QUARRY / "hermes_cli" / "models_catalog_static.py")
SWITCH_SRC = str(QUARRY / "hermes_cli" / "model_switch_providers.py")

NAME = "models_flow2"

PORTED = [
    "_zero_priced", "_is_subscription_billed", "_union_with_portal_recommendations",
    "union_with_nous_on_sale_models", "_pricing_profile_key", "get_cached_nous_free_tier",
    "_is_anthropic_frontier_tier", "_anthropic_models_url", "_anthropic_next_cursor",
    "_fast_mode_route_supported", "model_supports_ultrafast", "_cache_entry",
    "_live_result_entry", "_normalized_cache_slug", "_model_requires_account_discovery",
    "_ollama_native_probe_reachable", "_resolve_anthropic_pool_catalog_credentials",
    "_probe_neg_key", "_probe_result", "_deepinfra_env", "_chat_catalog_rows",
    "_codex_catalog", "_nous_catalog", "_anthropic_catalog", "_static_catalog",
    "_store_cache_entry", "cached_fetch_api_models",
]
WANTED = set(PORTED)

logging.disable(logging.CRITICAL)

# ── AST extraction ──────────────────────────────────────────────────────────
_SRC_TEXT = Path(SRC).read_text(encoding="utf-8")
_TREE = ast.parse(_SRC_TEXT)
_FLAGS = __future__.annotations.compiler_flag


def _compile_node(node, src):
    return compile(ast.Module(body=[node], type_ignores=[]), src, "exec",
                   flags=_FLAGS, dont_inherit=True)


DEF_CODE = {}
for _node in _TREE.body:
    if isinstance(_node, ast.FunctionDef) and _node.name in WANTED:
        DEF_CODE[_node.name] = _compile_node(_node, SRC)
_missing = WANTED - set(DEF_CODE)
if _missing:
    print(f"ORACLE {NAME}: FATAL missing live defs: {sorted(_missing)}", file=sys.stderr)
    sys.exit(1)


def _extract_class(src, name):
    tree = ast.parse(Path(src).read_text(encoding="utf-8"))
    node = next(n for n in tree.body if isinstance(n, ast.ClassDef) and n.name == name)
    ns = {"__builtins__": builtins, "__name__": "md2_live_models"}
    exec(_compile_node(node, src), ns)
    return ns[name]


CuratedFallbackModels = _extract_class(STATIC_SRC, "CuratedFallbackModels")
_NativePickerModelList = _extract_class(SWITCH_SRC, "_NativePickerModelList")

# ── canonical projection ────────────────────────────────────────────────────


def norm(v):
    if isinstance(v, BaseException):
        return {"__raise__": type(v).__name__, "msg": str(v)}
    if isinstance(v, CuratedFallbackModels):
        return {"$curated": [norm(x) for x in v]}
    if isinstance(v, _NativePickerModelList):
        return {"$native": [norm(x) for x in v]}
    if isinstance(v, PurePath):
        return str(v)
    if isinstance(v, (list, tuple)):
        return [norm(x) for x in v]
    if isinstance(v, (set, frozenset)):
        return sorted((norm(x) for x in v), key=lambda s: str(s))
    if isinstance(v, dict):
        return {k: norm(x) for k, x in v.items()}
    if isinstance(v, float) and v.is_integer():
        return int(v)
    return v


def denorm(v):
    if isinstance(v, dict):
        if "$curated" in v:
            return CuratedFallbackModels(denorm(x) for x in v["$curated"])
        if "$native" in v:
            return _NativePickerModelList(denorm(x) for x in v["$native"])
        return {k: denorm(x) for k, x in v.items()}
    if isinstance(v, list):
        return [denorm(x) for x in v]
    return v


def make_exc(marker):
    cls_name = marker.get("__raise__", "RuntimeError")
    msg = marker.get("msg", "")
    cls = getattr(builtins, cls_name, None)
    if not (isinstance(cls, type) and issubclass(cls, BaseException)):
        cls = type(cls_name, (Exception,), {})
    return cls(msg)


# ── scripted seams ──────────────────────────────────────────────────────────
_SCRIPTS = None


class Scripts:
    def __init__(self, io, trace):
        self.io = io or {}
        self.trace = trace
        self.cursors = {}

    def next(self, name):
        script = self.io.get(name)
        if isinstance(script, dict) and "$seq" in script:
            seq = script["$seq"]
            i = self.cursors.get(name, 0)
            if i >= len(seq):
                return {"__raise__": "RuntimeError", "msg": f"script exhausted: {name}"}
            self.cursors[name] = i + 1
            return seq[i]
        return script


# ── seam arg projections ────────────────────────────────────────────────────
SEAM_ARGS = {
    # claimed siblings (bare-global ns overrides)
    "strip_vendor_prefix": lambda a, k: {"model_id": a[0]},
    "normalize_provider": lambda a, k: {"provider": a[0]},
    "fetch_nous_recommended_models": lambda a, k: {"portal_base_url": a[0], "force_refresh": k["force_refresh"]},
    "extract_model_name": lambda a, k: {"entry": a[0]},
    "cache_entry_valid": lambda a, k: dict({"entry": a[0], "fp": a[1]},
                                           **({"allow_empty": k["allow_empty"]} if "allow_empty" in k else {})),
    "get_curated_nous_model_ids": lambda a, k: {},
    "get_model_config_dict": lambda a, k: {},
    "fetch_anthropic_models": lambda a, k: {"base_url": k.get("base_url"), "api_key": k.get("api_key")},
    "merge_unique": lambda a, k: {"primary": a[0], "secondary": a[1]},
    "static_catalog": lambda a, k: {"provider": a[0]},
    "drop_delisted_opencode_models": lambda a, k: {"normalized": a[0], "rows": a[1]},
    "merge_with_models_dev": lambda a, k: {"provider": a[0], "curated": a[1]},
    "load_provider_models_cache": lambda a, k: {},
    "save_provider_models_cache": lambda a, k: {"data": a[0]},
    "provider_models_cache_path": lambda a, k: {},
    "custom_endpoint_fingerprint": lambda a, k: {"api_key": a[0], "api_mode": a[1], "headers": a[2]},
    "spawn_swr_refresh": lambda a, k: {"cache_key": a[0], "refresh_fn": a[1] is not None},
    "fetch_api_models": lambda a, k: {"api_key": a[0], "base_url": a[1], "timeout": k["timeout"],
                                      "api_mode": k.get("api_mode"), "headers": k.get("headers")},
    "is_anthropic_fast_model": lambda a, k: {"model_id": a[0]},
    # foreign-module collaborators
    "compute_sale_discount": lambda a, k: {"prompt": a[0], "completion": a[1], "original": a[2]},
    "price_float": lambda a, k: {"raw": a[0], "positive": k["positive"]},
    "hermes_home_key": lambda a, k: {},
    "time.monotonic": lambda a, k: {},
    "time.time": lambda a, k: {},
    "is_grok_46_family": lambda a, k: {"model_id": a[0]},
    "strip_codex_context_variant_suffix": lambda a, k: {"model_id": a[0]},
    "openai_fast_region_allows": lambda a, k: {"base_url": a[0], "model_id": a[1], "tier": a[2]},
    "codex_model_requires_account_discovery": lambda a, k: {"model": a[0]},
    "is_astra_model": lambda a, k: {"model": a[0]},
    "get_ollama_base_url": lambda a, k: {},
    "get_ollama_native_headers": lambda a, k: {"base_url": a[0]},
    "root_for_ollama_native_api": lambda a, k: {"base_url": a[0]},
    "ollama_probe_cache_key": lambda a, k: {"root": a[0], "headers": a[1]},
    "read_credential_pool": lambda a, k: {"provider": a[0]},
    "base_url_origin": lambda a, k: {"base_url": a[0]},
    "get_env_value_prefer_dotenv": lambda a, k: {"key": a[0]},
    "without_generation_models": lambda a, k: {"models": a[0]},
    "get_codex_model_ids": lambda a, k: {"access_token": k.get("access_token"), "base_url": k.get("base_url")},
    "resolve_codex_runtime_credentials": lambda a, k: {"read_only": k["read_only"]},
    "codex_access_token_is_expiring": lambda a, k: {"access_token": a[0], "skew_seconds": a[1]},
    "background_caller": lambda a, k: {},
    "resolve_nous_runtime_credentials": lambda a, k: {},
    "fetch_nous_models": lambda a, k: {"api_key": k.get("api_key"), "inference_base_url": k.get("inference_base_url")},
    "xai_finalize_catalog": lambda a, k: {"ids": a[0]},
    "write_under_cache_lock": lambda a, k: {"path": a[0]},
    "materialize_probe_api_key": lambda a, k: {"api_key": a[0]},
    "fetch_models": lambda a, k: {},
}

PLAIN_TUPLE = {"base_url_origin", "compute_sale_discount"}
# list-returning seams whose live result preserves a list subclass tag
LIST_SEAMS = {
    "static_catalog", "merge_unique", "drop_delisted_opencode_models",
    "merge_with_models_dev", "xai_finalize_catalog", "without_generation_models",
    "fetch_nous_models", "get_codex_model_ids", "fetch_api_models",
    "fetch_anthropic_models", "get_curated_nous_model_ids",
}


class _InertCM:
    def __enter__(self):
        return self

    def __exit__(self, *exc):
        return False


def materialize(name, script):
    if name == "background_caller":
        return _InertCM()
    if script is None:
        return None
    if name in PLAIN_TUPLE:
        return tuple(script) if isinstance(script, (list, tuple)) else script
    return script


def record(name, args):
    scripts = _SCRIPTS
    args = {k: norm(v) for k, v in args.items() if v is not None}
    script = scripts.next(name)
    if isinstance(script, dict) and isinstance(script.get("__raise__"), str):
        marker = dict(script)
        scripts.trace.append({"ev": name, "args": args, "ret": marker})
        raise make_exc(marker)
    value = materialize(name, denorm(script))
    scripts.trace.append({"ev": name, "args": args, "ret": norm(script)})
    return value


class SeamFn:
    def __init__(self, name):
        self.name = name

    def __call__(self, *a, **kw):
        return record(self.name, SEAM_ARGS[self.name](a, kw))


class LoggerDouble:
    def _log(self, level, msg, args, kw):
        d = {"msg": msg, "args": list(args)}
        if kw.get("exc_info"):
            d["exc_info"] = True
        return record("logger." + level, d)

    def debug(self, msg, *args, **kw):
        return self._log("debug", msg, args, kw)

    def warning(self, msg, *args, **kw):
        return self._log("warning", msg, args, kw)

    def info(self, msg, *args, **kw):
        return self._log("info", msg, args, kw)


_LOGGER_SINGLETON = LoggerDouble()


class TimeDouble:
    def time(self):
        return record("time.time", {})

    def monotonic(self):
        return record("time.monotonic", {})


_TIME_SINGLETON = TimeDouble()


def _write_under_cache_lock(path, fn):
    record("write_under_cache_lock", {"path": path})
    return fn()


# ── stub modules for lazy imports (singletons dispatch to current scripts) ──


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


def install_stubs():
    agent = _mod("agent")
    agent.__path__ = []
    mm = _mod("agent.model_metadata",
              is_grok_46_family=SeamFn("is_grok_46_family"),
              strip_codex_context_variant_suffix=SeamFn("strip_codex_context_variant_suffix"))
    re_mod = _mod("agent.reasoning_effort", is_astra_model=SeamFn("is_astra_model"))
    cp = _mod("agent.credential_pool", AUTH_TYPE_API_KEY="api_key")
    cts = _mod("agent.command_token_source", materialize_probe_api_key=SeamFn("materialize_probe_api_key"))
    agent.model_metadata = mm
    agent.reasoning_effort = re_mod
    agent.credential_pool = cp
    agent.command_token_source = cts

    hc = _mod("hermes_cli")
    hc.__path__ = []
    mp = _mod("hermes_cli.models_pricing",
              _price_float=SeamFn("price_float"),
              compute_sale_discount=SeamFn("compute_sale_discount"))
    mfr = _mod("hermes_cli.models_fast_regions", openai_fast_region_allows=SeamFn("openai_fast_region_allows"))
    cm = _mod("hermes_cli.codex_models",
              get_codex_model_ids=SeamFn("get_codex_model_ids"),
              codex_model_requires_account_discovery=SeamFn("codex_model_requires_account_discovery"))
    auth = _mod("hermes_cli.auth",
                resolve_codex_runtime_credentials=SeamFn("resolve_codex_runtime_credentials"),
                _codex_access_token_is_expiring=SeamFn("codex_access_token_is_expiring"),
                resolve_nous_runtime_credentials=SeamFn("resolve_nous_runtime_credentials"),
                fetch_nous_models=SeamFn("fetch_nous_models"),
                read_credential_pool=SeamFn("read_credential_pool"))
    ac = _mod("hermes_cli.anon_challenge", background_caller=SeamFn("background_caller"))
    cfg = _mod("hermes_cli.config", get_env_value_prefer_dotenv=SeamFn("get_env_value_prefer_dotenv"))
    cc = _mod("hermes_cli.chat_catalog", without_generation_models=SeamFn("without_generation_models"))
    mcl = _mod("hermes_cli.models_cache_lock", write_under_cache_lock=_write_under_cache_lock)
    msp = _mod("hermes_cli.model_switch_providers", _NativePickerModelList=_NativePickerModelList)
    hc.models_pricing = mp
    hc.models_fast_regions = mfr
    hc.codex_models = cm
    hc.auth = auth
    hc.anon_challenge = ac
    hc.config = cfg
    hc.chat_catalog = cc
    hc.models_cache_lock = mcl
    hc.model_switch_providers = msp

    _mod("hermes_constants", hermes_home_key=SeamFn("hermes_home_key"))
    _mod("utils", base_url_origin=SeamFn("base_url_origin"))


# ── base namespace ──────────────────────────────────────────────────────────
_BASE_NS = {
    "__name__": "md2_live_models",
    "__builtins__": builtins,
    "json": json,
    "math": math,
    "logging": logging,
    "urllib": urllib,
    "Any": Any, "Optional": Optional,
    "CuratedFallbackModels": CuratedFallbackModels,
    "_NativePickerModelList": _NativePickerModelList,
}
_MOD = types.ModuleType("md2_live_models")
_MOD.__dict__.update(_BASE_NS)
sys.modules["md2_live_models"] = _MOD
_BASE_NS = _MOD.__dict__

# bare-global module-top imports -> seam recorders (live name -> seam name)
_BARE_GLOBALS = {
    "is_astra_model": "is_astra_model",
    "_xai_finalize_catalog": "xai_finalize_catalog",
    "_get_ollama_base_url": "get_ollama_base_url",
    "_get_ollama_native_headers": "get_ollama_native_headers",
    "_ollama_probe_cache_key": "ollama_probe_cache_key",
    "_root_for_ollama_native_api": "root_for_ollama_native_api",
}
_CLAIMED_SIBLINGS = {
    "_strip_vendor_prefix": "strip_vendor_prefix",
    "normalize_provider": "normalize_provider",
    "fetch_nous_recommended_models": "fetch_nous_recommended_models",
    "_extract_model_name": "extract_model_name",
    "_cache_entry_valid": "cache_entry_valid",
    "get_curated_nous_model_ids": "get_curated_nous_model_ids",
    "_get_model_config_dict": "get_model_config_dict",
    "_fetch_anthropic_models": "fetch_anthropic_models",
    "_merge_unique": "merge_unique",
    "static_catalog": "static_catalog",
    "_drop_delisted_opencode_models": "drop_delisted_opencode_models",
    "_merge_with_models_dev": "merge_with_models_dev",
    "_load_provider_models_cache": "load_provider_models_cache",
    "_save_provider_models_cache": "save_provider_models_cache",
    "_provider_models_cache_path": "provider_models_cache_path",
    "_custom_endpoint_fingerprint": "custom_endpoint_fingerprint",
    "_spawn_swr_refresh": "spawn_swr_refresh",
    "fetch_api_models": "fetch_api_models",
    "_is_anthropic_fast_model": "is_anthropic_fast_model",
}
for _live_nm, _seam_nm in _BARE_GLOBALS.items():
    _BASE_NS[_live_nm] = SeamFn(_seam_nm)
for _live_nm, _seam_nm in _CLAIMED_SIBLINGS.items():
    _BASE_NS[_live_nm] = SeamFn(_seam_nm)
install_stubs()


def build_ns(rec):
    global _SCRIPTS
    inp = rec["in"]
    env = inp.get("env") or {}
    trace = []
    scripts = Scripts(inp.get("io") or {}, trace)
    _SCRIPTS = scripts
    ns = dict(_BASE_NS)
    ns["logger"] = _LOGGER_SINGLETON
    ns["time"] = _TIME_SINGLETON
    # per-case constants (seeded BEFORE def-exec: defaults read them)
    ns["_OPENAI_ULTRAFAST_MODELS"] = frozenset(env.get("ultrafast_models") or [])
    ns["_MODELS_DEV_PREFERRED"] = frozenset(env.get("models_dev_preferred") or [])
    ns["_PROVIDER_MODELS"] = {k: list(v) for k, v in (env.get("provider_models") or {}).items()}
    ns["_OLLAMA_LOCAL_PROBE_REACHABLE"] = dict(env.get("probe_reachable") or {})
    ns["_free_tier_cache"] = {k: tuple(v) for k, v in (env.get("free_tier_cache") or {}).items()}
    ns["_FREE_TIER_CACHE_TTL"] = env.get("free_tier_ttl", 180)
    ns["_PROVIDER_MODELS_CACHE_TTL"] = env.get("cache_ttl", 3600)
    ns["_PROVIDER_MODELS_STALE_SERVE_MAX"] = env.get("stale_max", 604800)
    sys.modules["agent.credential_pool"].AUTH_TYPE_API_KEY = env.get("auth_type_api_key", "api_key")
    for _nm, _code in DEF_CODE.items():
        exec(_code, ns)
    # claimed siblings stay seamed (override any exec'd binding)
    for _live_nm, _seam_nm in _CLAIMED_SIBLINGS.items():
        ns[_live_nm] = SeamFn(_seam_nm)
    for _live_nm, _seam_nm in _BARE_GLOBALS.items():
        ns[_live_nm] = SeamFn(_seam_nm)
    ns["logger"] = _LOGGER_SINGLETON
    ns["time"] = _TIME_SINGLETON
    return ns, trace, scripts


# ── live driver ─────────────────────────────────────────────────────────────


def drive(rec, ns, scripts, holder):
    func = rec["func"]
    inp = rec["in"]
    if func == "_zero_priced":
        return ns[func](inp.get("pricing"), tuple(inp.get("keys") or ()), inp.get("default", "1"))
    if func == "_is_subscription_billed":
        return ns[func](inp.get("entry"))
    if func == "_union_with_portal_recommendations":
        return ns[func](inp["tier_key"], inp["curated_ids"], inp["pricing"], inp["portal_base_url"],
                        force_refresh=inp.get("force_refresh", False),
                        synthesize_free_pricing=inp.get("synthesize_free_pricing", False))
    if func == "union_with_nous_on_sale_models":
        return ns[func](inp["curated_ids"], inp.get("pricing"))
    if func == "_pricing_profile_key":
        return ns[func]()
    if func == "get_cached_nous_free_tier":
        return ns[func]()
    if func == "_is_anthropic_frontier_tier":
        return ns[func](inp.get("model_id"))
    if func == "_anthropic_models_url":
        return ns[func](inp.get("base_url"), after_id=inp.get("after_id"))
    if func == "_anthropic_next_cursor":
        seen = set(inp.get("seen") or [])
        ret = ns[func](inp.get("page"), seen)
        holder["state"] = {"seen": norm(seen)}
        return ret
    if func == "_fast_mode_route_supported":
        return ns[func](inp.get("model_id"), inp.get("provider"), inp.get("base_url"), inp.get("tier"))
    if func == "model_supports_ultrafast":
        return ns[func](inp.get("model_id"))
    if func == "_cache_entry":
        return ns[func](inp["fp"], denorm(inp.get("models")), inp.get("at"))
    if func == "_live_result_entry":
        return ns[func](inp["fp"], denorm(inp.get("live")), inp.get("existing"), inp.get("at"))
    if func == "_normalized_cache_slug":
        return ns[func](inp.get("provider"))
    if func == "_model_requires_account_discovery":
        return ns[func](inp.get("provider"), inp["model"])
    if func == "_ollama_native_probe_reachable":
        return ns[func]()
    if func == "_resolve_anthropic_pool_catalog_credentials":
        return ns[func]()
    if func == "_probe_neg_key":
        return ns[func](inp["base_url"])
    if func == "_probe_result":
        return ns[func](inp.get("models"), inp.get("probed_url"), inp.get("resolved_base_url"),
                        inp.get("suggested_base_url"), inp.get("used_fallback", False))
    if func == "_deepinfra_env":
        return ns[func](inp["key"])
    if func == "_chat_catalog_rows":
        return ns[func](denorm(inp.get("models")))
    if func in ("_codex_catalog", "_nous_catalog", "_anthropic_catalog"):
        return ns[func](inp["normalized"], inp.get("force_refresh", False))
    if func == "_static_catalog":
        return ns[func](inp["normalized"], inp.get("fetcher"))
    if func == "_store_cache_entry":
        return ns[func](inp["cache_key"], inp["entry"])
    if func == "cached_fetch_api_models":
        fm = None
        if inp.get("fetch_models") is not None:
            fm = SeamFn("fetch_models")
        return ns[func](inp.get("api_key"), inp.get("base_url"),
                        timeout=inp.get("timeout", 5.0), api_mode=inp.get("api_mode"),
                        headers=inp.get("headers"), force_refresh=inp.get("force_refresh", False),
                        cache_only=inp.get("cache_only", False), fetch_models=fm,
                        ttl_seconds=inp.get("ttl_seconds", ns["_PROVIDER_MODELS_CACHE_TTL"]))
    raise KeyError(f"unknown func {func}")


def main():
    n = mism = 0
    shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        n += 1
        holder = {}
        try:
            ns, trace, scripts = build_ns(rec)
            try:
                ret = drive(rec, ns, scripts, holder)
                out = {"ret": norm(ret), "trace": trace, "state": holder.get("state", {})}
            except BaseException as e:  # noqa: BLE001 - fail-closed per case
                out = {"ret": {"__raise__": type(e).__name__}, "trace": trace,
                       "state": holder.get("state", {})}
        except BaseException as e:  # noqa: BLE001 - seeding failure is a mismatch
            out = {"ret": {"__raise__": type(e).__name__}, "trace": [], "state": {}}
        got = rec.get("out")
        if got != out:
            mism += 1
            if shown < 10:
                shown += 1
                print(f"MISMATCH {rec.get('func')} case {n}:")
                print(f"  C={json.dumps(got, ensure_ascii=False)[:800]}")
                print(f"  P={json.dumps(out, ensure_ascii=False)[:800]}")
    print(f"ORACLE {NAME}: {n} cases, {mism} mismatches")
    sys.exit(1 if (mism or n == 0) else 0)


# ── self-test: drive every fixture, flag drive failures / suspicious raises ──


def selftest(fixtures):
    import faulthandler
    faulthandler.dump_traceback_later(60, exit=True)
    bad = 0
    for i, rec in enumerate(fixtures):
        holder = {}
        try:
            ns, trace, scripts = build_ns(rec)
            try:
                ret = drive(rec, ns, scripts, holder)
                spec = norm(ret)
            except BaseException as e:  # noqa: BLE001
                spec = {"__raise__": type(e).__name__, "msg": str(e)}
            if isinstance(spec, dict) and spec.get("__raise__") == "AttributeError":
                # Only a binding bug when the message names a ported method or
                # seam stub; a 'NoneType'/.get raise is a deliberate domain case.
                msg = spec.get("msg", "")
                names = set(PORTED) | set(SEAM_ARGS) | set(_CLAIMED_SIBLINGS)
                if any(f"'{nm}'" in msg for nm in names):
                    bad += 1
                    print(f"SELFTEST case {i} {rec['func']}: AttributeError names a method: {msg}")
        except BaseException as e:  # noqa: BLE001
            bad += 1
            print(f"SELFTEST case {i} {rec['func']}: DRIVE FAILURE {type(e).__name__}: {e}")
    faulthandler.cancel_dump_traceback_later()
    print(f"SELFTEST {NAME}: {len(fixtures)} cases, {bad} suspicious")
    return bad


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        fx = Path(__file__).resolve().parent / "fixtures" / "models_flow2.jsonl"
        recs = [json.loads(l) for l in fx.read_text(encoding="utf-8").splitlines() if l.strip()]
        sys.exit(1 if selftest(recs) else 0)
    main()