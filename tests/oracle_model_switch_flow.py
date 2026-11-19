#!/usr/bin/env python3
"""Oracle for the hermes_cli/model_switch.py switching FLOW port
(src/cli/port_model_switch_flow.c).

Drives the LIVE model_switch.py defs with SCRIPTED seams: every provider/
config/credential/network dependency is a recording stub fed the same scripts
the C harness feeds msf_call. Compares the DECISION SEQUENCE (ordered "trace"
of {ev,args,ret} seam events), the canonical return ("ret") and the post-call
_Switch field window ("state"). Pure claimed siblings (_clean,
_declared_model_ids, _model_sort_key, _split_version_suffix,
_ambiguous_alias_message, _bare_custom_provider_def,
_resolve_named_custom_model_id) and internal gap steps stay REAL via AST
extraction so nested decision logic is exercised, matching the C port's
internal transcriptions.

Live defs are AST-extracted from the quarry (annotations kept as strings via
the __future__ flag); module-level Assign/AnnAssign constants are seeded after
the class/def execs; ENDPOINT_*_KEYS / GUEST_MODEL come from the live
route_identity / anon_auth Assigns. The heavy quarry import chain is never
imported. Fail-closed: 0 cases => exit 1; any harness/oracle exception per
case rides as {"__raise__": cls}. Summary:
'ORACLE model_switch_flow: N cases, M mismatches'.

Representation contract (both sides, see include/port_model_switch_flow.h):
- tuple/set/frozenset -> array (sets sorted); DirectAlias -> 5-key dict;
  ProviderDef/profile doubles -> attribute dicts; ModelSwitchResult -> full
  18-field dict; None -> null; uncaught exception -> {"__raise__": cls}.
- scripts: absent key -> None; {"$seq": [...]} consumed per call (exhaustion
  raises RuntimeError("script exhausted: <name>")); {"__raise__": cls, ...}
  raises (trace ret is the WHOLE marker); any other value reused. None-valued
  arg keys dropped on both sides. custom_provider_aliases scripts are SORTED
  arrays (the live frozenset canonical form).
- _Switch / ModelSwitchResult fixture dicts are default-filled identically on
  both sides; st-mutating funcs emit the whole field window as out.state.
- str()/strip()/lower() operand fields are ASCII strings/null in fixtures.
"""
import __future__
import ast
import builtins
import contextlib
import json
import logging
import re
import sys
import types
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, NamedTuple, Optional

try:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    if hasattr(sys.stdin, "reconfigure"):
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except Exception:  # already-read streams (in-process self-test) keep their encoding
    pass

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "hermes_cli" / "model_switch.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "hermes_cli" / "model_switch.py")
RI_SRC = str(QUARRY / "hermes_cli" / "route_identity.py")
AA_SRC = str(QUARRY / "hermes_cli" / "anon_auth.py")

NAME = "model_switch_flow"

PORTED = [
    "_provider_identity", "resolve_alias", "_external_process_catalog",
    "_external_process_match", "_aggregator_catalog_match", "_config_declares_model",
    "_unknown_provider_message", "_aggregator_alias_error", "_apply_direct_alias_endpoint",
    "_route_explicit_provider", "_route_from_model_input", "_route_configured_provider",
    "_validate_switch", "model_selection_config_updates", "selection_route_changed",
    "_merge_selection_updates", "apply_model_selection", "persist_model_selection",
]
PURE = [
    "_clean", "_declared_item_id", "_declared_model_ids", "_model_sort_key",
    "_split_version_suffix", "_ambiguous_alias_message", "_bare_custom_provider_def",
    "_custom_entries", "_entry_aliases", "_current_provider_match", "_route_alias_fallback",
    "_convert_vendor_colon_slug", "_names_known_provider", "_entry_configured_key",
    "_resolve_named_custom_model_id", "_opencode_api_mode", "_nous_api_mode",
]
CLASSES = ["ModelIdentity", "DirectAlias", "ModelSwitchResult", "_Switch", "AmbiguousAliasError"]
WANTED = set(PORTED) | set(PURE)

logging.disable(logging.CRITICAL)

# ── AST extraction ──────────────────────────────────────────────────────────

_SRC_TEXT = Path(SRC).read_text(encoding="utf-8")
_TREE = ast.parse(_SRC_TEXT)
_FLAGS = __future__.annotations.compiler_flag


def _compile_node(node, src):
    return compile(ast.Module(body=[node], type_ignores=[]), src, "exec",
                   flags=_FLAGS, dont_inherit=True)


DEF_CODE = {}
CLASS_CODE = {}
ASSIGN_NODES = []
for _node in _TREE.body:
    if isinstance(_node, ast.FunctionDef) and _node.name in WANTED:
        DEF_CODE[_node.name] = _compile_node(_node, SRC)
    elif isinstance(_node, ast.ClassDef) and _node.name in CLASSES:
        CLASS_CODE[_node.name] = _compile_node(_node, SRC)
    elif isinstance(_node, (ast.Assign, ast.AnnAssign)):
        ASSIGN_NODES.append(_node)
_missing = (WANTED - set(DEF_CODE)) | (set(CLASSES) - set(CLASS_CODE))
if _missing:
    print(f"ORACLE {NAME}: FATAL missing live defs: {sorted(_missing)}", file=sys.stderr)
    sys.exit(1)


def _extract_assign(src, name):
    tree = ast.parse(Path(src).read_text(encoding="utf-8"))
    for node in tree.body:
        if isinstance(node, ast.Assign) and any(
                isinstance(t, ast.Name) and t.id == name for t in node.targets):
            ns = {"__builtins__": builtins}
            exec(_compile_node(node, src), ns)
            return ns[name]
    print(f"ORACLE {NAME}: FATAL missing constant {name} in {src}", file=sys.stderr)
    sys.exit(1)


ENDPOINT_CREDENTIAL_KEYS = _extract_assign(RI_SRC, "ENDPOINT_CREDENTIAL_KEYS")
ENDPOINT_KEY_POINTER_KEYS = _extract_assign(RI_SRC, "ENDPOINT_KEY_POINTER_KEYS")
GUEST_MODEL = _extract_assign(AA_SRC, "GUEST_MODEL")


class ProviderDef:
    """Attribute double for providers.ProviderDef (isinstance-free flows)."""
    def __init__(self, **kw):
        self.__dict__.update(kw)


class ModelCapabilities:
    pass


class ModelInfo:
    pass


class ChangesD:
    """config_backend.Changes double (set/unset keyword payload)."""
    def __init__(self, set=None, unset=()):
        self.set = set
        self.unset = unset


_BASE_NS = {
    "__name__": "msf_live_model_switch",
    "__builtins__": builtins,
    "logging": logging,
    "logger": logging.getLogger("msf_live_model_switch"),
    "os": types.SimpleNamespace(environ={}, chmod=lambda *a, **k: None),
    "re": re,
    "suppress": contextlib.suppress,
    "dataclass": dataclass,
    "field": field,
    "Any": Any,
    "NamedTuple": NamedTuple,
    "Optional": Optional,
    "Path": Path,
    "ProviderDef": ProviderDef,
    "ModelCapabilities": ModelCapabilities,
    "ModelInfo": ModelInfo,
}
# py3.14 dataclass/annotation resolution wants a registered module BEFORE
# the dataclass ClassDefs exec — run everything in the module's own dict.
_MOD = types.ModuleType("msf_live_model_switch")
_MOD.__dict__.update(_BASE_NS)
sys.modules["msf_live_model_switch"] = _MOD
_BASE_NS = _MOD.__dict__
# Classes first (constants construct ModelIdentity), then defs, then the
# module-level Assign/AnnAssign pass in source order.
for _nm in CLASSES:
    exec(CLASS_CODE[_nm], _BASE_NS)
for _nm, _code in DEF_CODE.items():
    exec(_code, _BASE_NS)
for _node in ASSIGN_NODES:
    exec(_compile_node(_node, SRC), _BASE_NS)
# The logger Assign rebinds the seeded logger; restore a disabled real one.
_BASE_NS["logger"] = logging.getLogger("msf_live_model_switch")
AmbiguousAliasError = _BASE_NS["AmbiguousAliasError"]
DirectAlias = _BASE_NS["DirectAlias"]

# ── canonical representation ────────────────────────────────────────────────


def norm(v):
    if isinstance(v, DirectAlias):
        return {"model": v.model, "provider": v.provider, "base_url": v.base_url,
                "api_key": v.api_key, "key_env": v.key_env}
    if isinstance(v, (list, tuple)):
        return [norm(x) for x in v]
    if isinstance(v, (set, frozenset)):
        return sorted((norm(x) for x in v), key=lambda s: str(s))
    if isinstance(v, dict):
        return {k: norm(x) for k, x in v.items()}
    if isinstance(v, (ProviderDef, types.SimpleNamespace)):
        return {k: norm(x) for k, x in vars(v).items() if not callable(x)}
    if isinstance(v, (_BASE_NS["ModelSwitchResult"], _BASE_NS["_Switch"])):
        return {k: norm(x) for k, x in vars(v).items()}
    if isinstance(v, Path):
        return v.as_posix()
    if isinstance(v, float) and v.is_integer():
        return int(v)
    return v


def make_exc(marker):
    cls_name = marker.get("__raise__", "RuntimeError")
    if cls_name == "AmbiguousAliasError":
        return AmbiguousAliasError(marker.get("alias", ""), marker.get("provider", ""),
                                   marker.get("candidates", []))
    msg = marker.get("msg", "")
    cls = getattr(builtins, cls_name, None)
    if not (isinstance(cls, type) and issubclass(cls, BaseException)):
        cls = type(cls_name, (Exception,), {})
    return cls(msg)


# ── scripted seams ──────────────────────────────────────────────────────────

ARGMAP = {
    "resolve_provider_full": lambda a, k: {"name": a[0], "user_providers": a[1],
                                           "custom_providers": a[2]},
    "normalize_provider": lambda a, k: {"name": a[0]},
    "get_provider_profile": lambda a, k: {"provider": a[0]},
    "get_provider": lambda a, k: {"name": a[0], "allow_network": k.get("allow_network")},
    "list_provider_models": lambda a, k: {"provider": a[0]},
    "static_catalog": lambda a, k: {"provider": a[0]},
    "is_aggregator": lambda a, k: {"provider": a[0]},
    "ensure_direct_aliases": lambda a, k: {},
    "direct_alias_api_key": lambda a, k: {"alias": norm(a[0])},
    "direct_alias_runtime_request": lambda a, k: {"alias": norm(a[0])},
    "may_reuse_session_credential": lambda a, k: {"session_base_url": a[0], "alias_base_url": a[1]},
    "resolve_runtime_provider": lambda a, k: {"requested": k.get("requested"),
                                              "explicit_api_key": k.get("explicit_api_key"),
                                              "explicit_base_url": k.get("explicit_base_url"),
                                              "target_model": k.get("target_model")},
    "auto_detect_local_model": lambda a, k: {"base_url": a[0]},
    "moa_default_preset": lambda a, k: {},
    "provider_install_hint": lambda a, k: {"provider": a[0]},
    "validate_config_structure": lambda a, k: {},
    "get_authenticated_provider_slugs": lambda a, k: {"current_provider": k.get("current_provider"),
                                                      "user_providers": k.get("user_providers"),
                                                      "custom_providers": k.get("custom_providers")},
    "get_label": lambda a, k: {"provider": a[0]},
    "resolve_alias_fallback": lambda a, k: {"raw_input": a[0], "authenticated_providers": a[1],
                                            "user_providers": a[2], "custom_providers": a[3]},
    "configured_provider_matches": lambda a, k: {"model_name": a[0], "user_providers": a[1],
                                                 "custom_providers": a[2]},
    "persisted_provider_id": lambda a, k: {"target_provider": a[0], "user_providers": a[1]},
    "custom_provider_aliases": lambda a, k: {"name": a[0], "provider_key": a[1]},
    "base_url_hostname": lambda a, k: {"url": a[0]},
    "route_is_welcome_host": lambda a, k: {"base_url": a[0]},
    "detect_provider_for_model": lambda a, k: {"model_name": a[0], "current_provider": a[1]},
    "load_config": lambda a, k: {},
    "normalize_moa_config": lambda a, k: {"cfg": a[0]},
    "exact_moa_preset_name": lambda a, k: {"normalized": a[0], "raw_input": a[1]},
    "normalize_model_for_provider": lambda a, k: {"model": a[0], "provider": a[1]},
    "is_known_non_chat_model": lambda a, k: {"model": a[0]},
    "get_ollama_request_headers": lambda a, k: {},
    "extra_headers_from_config": lambda a, k: {"cfg": a[0]},
    "validate_requested_model": lambda a, k: {"model": a[0], "provider": a[1],
                                              "api_key": k.get("api_key"), "base_url": k.get("base_url"),
                                              "api_mode": k.get("api_mode"), "headers": k.get("headers")},
    "is_provider_enabled": lambda a, k: {"cfg": a[0]},
    "same_ollama_native_root": lambda a, k: {"base_url": a[0], "configured_base": a[1]},
    "ollama_configured_base": lambda a, k: {},
    "read_env": lambda a, k: {"name": a[0]},
    "should_clear_context_pin": lambda a, k: {"configured_model": a[0], "model": a[1],
                                              "configured_base_url": a[2], "base_url": a[3],
                                              "configured_provider": a[4], "provider": a[5]},
    "normalize_route_base_url": lambda a, k: {"base_url": a[0]},
    "get_config_path": lambda a, k: {},
    "read_user_config_raw": lambda a, k: {"path": str(a[0])},
    "supports_file_tooling": lambda a, k: {},
    "write_changes": lambda a, k: {"path": str(a[0]), "set": a[1].set, "unset": list(a[1].unset)},
    "atomic_config_replace": lambda a, k: {"path": str(a[0]), "config": a[1]},
    "chmod": lambda a, k: {"path": str(a[0]), "mode": a[1]},
}


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


def convert(name, value):
    if name in ("resolve_provider_full", "get_provider_profile", "get_provider"):
        return ProviderDef(**value) if isinstance(value, dict) else value
    if name == "custom_provider_aliases":
        return frozenset(value) if isinstance(value, list) else value
    if name in ("detect_provider_for_model", "resolve_alias_fallback",
                "direct_alias_runtime_request", "ollama_configured_base"):
        return tuple(value) if isinstance(value, list) else value
    if name == "validate_config_structure":
        return [types.SimpleNamespace(**d) if isinstance(d, dict) else d for d in value] \
            if isinstance(value, list) else value
    return value


def recorder(name, scripts):
    def rec(*a, **kw):
        args = {k: v for k, v in ARGMAP[name](a, kw).items() if v is not None}
        script = scripts.next(name)
        if isinstance(script, dict) and isinstance(script.get("__raise__"), str):
            marker = dict(script)
            scripts.trace.append({"ev": name, "args": args, "ret": marker})
            raise make_exc(marker)
        value = convert(name, script)
        scripts.trace.append({"ev": name, "args": args, "ret": norm(value)})
        return value
    return rec


class _EnvD:
    """os.environ double: get() records read_env and honors the "" default."""
    def __init__(self, scripts):
        self._scripts = scripts

    def get(self, name, default=""):
        v = recorder("read_env", self._scripts)(name)
        return v if v is not None else default


class _OsD:
    def __init__(self, scripts):
        self.environ = _EnvD(scripts)
        self.chmod = recorder("chmod", scripts)


def _dummy(*a, **k):
    raise AssertionError("unscripted dependency called")


def install_module_stubs(scripts, env):
    def mod(name, **attrs):
        m = types.ModuleType(name)
        for k, v in attrs.items():
            setattr(m, k, v)
        sys.modules[name] = m
        return m

    hc = mod("hermes_cli")
    hc.__path__ = []
    cfg = mod("hermes_cli.config",
              load_config=recorder("load_config", scripts),
              validate_config_structure=recorder("validate_config_structure", scripts),
              is_provider_enabled=recorder("is_provider_enabled", scripts),
              _CONFIG_LOCK=contextlib.nullcontext(),
              atomic_config_replace=recorder("atomic_config_replace", scripts),
              get_config_path=recorder("get_config_path", scripts),
              read_user_config_raw=recorder("read_user_config_raw", scripts))
    cb = mod("hermes_cli.config_backend",
             supports_file_tooling=recorder("supports_file_tooling", scripts),
             write_config_keys=_dummy,
             write_changes=recorder("write_changes", scripts),
             Changes=ChangesD)
    prov = mod("hermes_cli.providers",
               resolve_provider_full=recorder("resolve_provider_full", scripts),
               normalize_provider=recorder("normalize_provider", scripts),
               is_aggregator=recorder("is_aggregator", scripts),
               get_label=recorder("get_label", scripts),
               custom_provider_aliases=recorder("custom_provider_aliases", scripts),
               get_provider=recorder("get_provider", scripts),
               ALIASES=dict(env.get("provider_aliases") or {}),
               determine_api_mode=_dummy, host_mandated_api_mode=_dummy,
               LLAMACPP_ALIASES=set(), resolve_user_provider=_dummy,
               is_actual_route=_dummy, nous_api_mode=_dummy)
    models = mod("hermes_cli.models",
                 static_catalog=recorder("static_catalog", scripts),
                 _AGGREGATOR_PROVIDERS=set(env.get("aggregators") or []),
                 detect_provider_for_model=recorder("detect_provider_for_model", scripts),
                 opencode_model_api_mode=_dummy, opencode_provider_family=_dummy,
                 normalize_opencode_base_url=_dummy, _get_provider_config_dict=_dummy,
                 parse_model_input=_dummy, _find_openrouter_slug=_dummy,
                 normalize_provider=_dummy)
    ml = mod("hermes_cli.models_local",
             _same_ollama_native_root=recorder("same_ollama_native_root", scripts),
             _get_ollama_request_headers=recorder("get_ollama_request_headers", scripts),
             should_use_ollama_native_catalog=_dummy)
    mv = mod("hermes_cli.models_validate",
             validate_requested_model=recorder("validate_requested_model", scripts))
    cc = mod("hermes_cli.chat_catalog",
             is_known_non_chat_model=recorder("is_known_non_chat_model", scripts))
    rp = mod("hermes_cli.runtime_provider",
             resolve_runtime_provider=recorder("resolve_runtime_provider", scripts),
             _auto_detect_local_model=recorder("auto_detect_local_model", scripts),
             _resolves_to_custom=_dummy, _config_base_url_trustworthy_for_bare_custom=_dummy,
             _get_model_config=_dummy, _get_named_custom_provider=_dummy,
             _custom_provider_request_overrides=_dummy)
    rpc = mod("hermes_cli.runtime_provider_custom",
              persisted_provider_id=recorder("persisted_provider_id", scripts))
    ri = mod("hermes_cli.route_identity",
             should_clear_context_pin=recorder("should_clear_context_pin", scripts),
             normalize_route_base_url=recorder("normalize_route_base_url", scripts),
             ENDPOINT_CREDENTIAL_KEYS=ENDPOINT_CREDENTIAL_KEYS,
             ENDPOINT_KEY_POINTER_KEYS=ENDPOINT_KEY_POINTER_KEYS)
    moa = mod("hermes_cli.moa_config",
              normalize_moa_config=recorder("normalize_moa_config", scripts),
              exact_moa_preset_name=recorder("exact_moa_preset_name", scripts))
    lcm = mod("hermes_cli.left_core_migration",
              provider_install_hint=recorder("provider_install_hint", scripts))
    aa = mod("hermes_cli.anon_auth",
             GUEST_MODEL=GUEST_MODEL,
             route_is_welcome_host=recorder("route_is_welcome_host", scripts))
    mn = mod("hermes_cli.model_normalize",
             normalize_model_for_provider=recorder("normalize_model_for_provider", scripts))
    msp = mod("hermes_cli.model_switch_providers",
              _extra_headers_from_config=recorder("extra_headers_from_config", scripts),
              _saved_entry_on_other_origin=_dummy, list_authenticated_providers=_dummy)
    agent = mod("agent")
    agent.__path__ = []
    md = mod("agent.models_dev",
             list_provider_models=recorder("list_provider_models", scripts),
             ModelCapabilities=ModelCapabilities, ModelInfo=ModelInfo,
             get_model_capabilities=_dummy, get_model_info=_dummy)
    providers = mod("providers", get_provider_profile=recorder("get_provider_profile", scripts))
    providers.__path__ = []
    utils = mod("utils",
                base_url_hostname=recorder("base_url_hostname", scripts),
                base_url_host_matches=_dummy, base_url_origin=_dummy)
    hc.config, hc.config_backend, hc.providers, hc.models = cfg, cb, prov, models
    hc.models_local, hc.models_validate, hc.chat_catalog = ml, mv, cc
    hc.runtime_provider, hc.runtime_provider_custom, hc.route_identity = rp, rpc, ri
    hc.moa_config, hc.left_core_migration, hc.anon_auth = moa, lcm, aa
    hc.model_normalize, hc.model_switch_providers = mn, msp
    agent.models_dev = md


# ── per-case namespace ──────────────────────────────────────────────────────

# Seamed claimed/gap siblings: (live name, seam name). The recorder override
# is installed under the LIVE name so extracted defs call the script.
SEAMED = [
    ("_ensure_direct_aliases", "ensure_direct_aliases"),
    ("direct_alias_api_key", "direct_alias_api_key"),
    ("direct_alias_runtime_request", "direct_alias_runtime_request"),
    ("_may_reuse_session_credential", "may_reuse_session_credential"),
    ("get_authenticated_provider_slugs", "get_authenticated_provider_slugs"),
    ("_resolve_alias_fallback", "resolve_alias_fallback"),
    ("_configured_provider_matches", "configured_provider_matches"),
    ("_moa_default_preset", "moa_default_preset"),
    ("_ollama_configured_base", "ollama_configured_base"),
    ("_extra_headers_from_config", "extra_headers_from_config"),
    ("resolve_provider_full", "resolve_provider_full"),
    ("normalize_provider", "normalize_provider"),
    ("is_aggregator", "is_aggregator"),
    ("get_label", "get_label"),
    ("custom_provider_aliases", "custom_provider_aliases"),
    ("list_provider_models", "list_provider_models"),
    ("normalize_model_for_provider", "normalize_model_for_provider"),
    ("base_url_hostname", "base_url_hostname"),
]

SWITCH_DEFAULTS = {
    "raw_input": "", "current_provider": "", "current_model": "", "current_base_url": "",
    "current_api_key": "", "is_global": False, "explicit_provider": "",
    "user_providers": None, "custom_providers": None, "new_model": "", "target_provider": "",
    "resolved_alias": "", "alias_endpoint_applied": False, "provider_label": "",
    "api_key": "", "base_url": "", "api_mode": "", "validation_headers": {},
    "suppress_ollama_headers": False, "validation": {},
}
RESULT_DEFAULTS = {
    "success": False, "new_model": "", "target_provider": "", "provider_changed": False,
    "api_key": "", "base_url": "", "api_mode": "", "request_overrides": None,
    "error_message": "", "warning_message": "", "usage_notice": "", "provider_label": "",
    "resolved_via_alias": "", "capabilities": None, "runtime_capabilities": None,
    "model_info": None, "is_global": False, "alias_endpoint_applied": False,
}
ST_FUNCS = {"_apply_direct_alias_endpoint", "_route_explicit_provider",
            "_route_from_model_input", "_route_configured_provider", "_validate_switch"}


def build_ns(rec):
    inp = rec["in"]
    env = inp.get("env") or {}
    ns = dict(_BASE_NS)
    ns["DIRECT_ALIASES"] = {k: DirectAlias(**v) for k, v in (env.get("direct_aliases") or {}).items()}
    trace = []
    scripts = Scripts(inp.get("io") or {}, trace)
    ns["os"] = _OsD(scripts)
    install_module_stubs(scripts, env)
    # Re-exec the defs into the PER-CASE namespace so their __globals__ see
    # the case's DIRECT_ALIASES / recorder overrides (a shallow dict copy
    # alone would leave them bound to the shared module dict).
    for _nm, _code in DEF_CODE.items():
        exec(_code, ns)
    for live_nm, seam_nm in SEAMED:
        ns[live_nm] = recorder(seam_nm, scripts)
    return ns, trace, scripts


def build_switch(fields, ns):
    merged = dict(SWITCH_DEFAULTS)
    merged["validation_headers"] = {}
    merged["validation"] = {}
    merged.update(fields or {})
    return ns["_Switch"](**merged)


def build_result(fields, ns):
    merged = dict(RESULT_DEFAULTS)
    merged.update(fields or {})
    return ns["ModelSwitchResult"](**merged)


# ── live driver ─────────────────────────────────────────────────────────────


def drive(rec, ns, scripts, holder):
    func = rec["func"]
    inp = rec["in"]
    if func in ST_FUNCS:
        st = build_switch(inp.get("st"), ns)
        holder["st"] = st  # partial mutations stay visible if the call raises
        if func == "_apply_direct_alias_endpoint":
            da = DirectAlias(**(inp.get("da") or {}))
            ret = ns[func](st, da)
        else:
            ret = ns[func](st)
        holder["state"] = {k: norm(v) for k, v in vars(st).items()}
        return ret
    if func == "_provider_identity":
        return ns[func](inp.get("name", ""), inp.get("user_providers"),
                        inp.get("custom_providers"))
    if func == "resolve_alias":
        return ns[func](inp.get("raw_input", ""), inp.get("current_provider", ""),
                        inp.get("user_providers"), inp.get("custom_providers"))
    if func == "_external_process_catalog":
        return ns[func](inp.get("provider", ""))
    if func == "_external_process_match":
        return ns[func](inp.get("catalog"), inp.get("aliases"), inp.get("typed", ""),
                        provider=inp.get("provider", ""))
    if func == "_aggregator_catalog_match":
        return ns[func](inp.get("new_model", ""), inp.get("catalog"))
    if func == "_config_declares_model":
        return ns[func](inp.get("new_model", ""), inp.get("target_provider", ""),
                        inp.get("base_url", ""), inp.get("user_providers"),
                        inp.get("custom_providers"))
    if func == "_unknown_provider_message":
        return ns[func](inp.get("explicit_provider", ""))
    if func == "_aggregator_alias_error":
        return ns[func](inp.get("explicit_provider", ""), inp.get("target_provider", ""),
                        inp.get("current_provider", ""), inp.get("user_providers"),
                        inp.get("custom_providers"))
    if func == "model_selection_config_updates":
        return ns[func](build_result(inp.get("result"), ns), inp.get("current_model_cfg"))
    if func == "selection_route_changed":
        return ns[func](inp.get("model_cfg"), build_result(inp.get("result"), ns))
    if func == "_merge_selection_updates":
        return ns[func](inp.get("model_cfg"), inp.get("updates"),
                        mask_cleared=inp.get("mask_cleared", False))
    if func == "apply_model_selection":
        return ns[func](inp.get("model_cfg"), build_result(inp.get("result"), ns),
                        mask_cleared=inp.get("mask_cleared", False))
    if func == "persist_model_selection":
        return ns[func](build_result(inp.get("result"), ns), inp.get("config_path"))
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
                state = {}
                if "st" in holder:
                    state = {k: norm(v) for k, v in vars(holder["st"]).items()}
                out = {"ret": {"__raise__": type(e).__name__}, "trace": trace, "state": state}
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


if __name__ == "__main__":
    main()
