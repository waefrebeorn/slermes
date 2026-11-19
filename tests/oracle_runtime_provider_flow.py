#!/usr/bin/env python3
"""Oracle for the runtime-provider FLOW-core port (src/cli/port_runtime_provider_flow.c).

Drives the LIVE hermes_cli/runtime_provider.py provider-resolution defs with
SCRIPTED I/O: every external call (env secrets, provider-registry lookups,
config loads, credential resolvers, URL utilities, pool refresh, logging) is a
recording stub fed the same scripts the C harness feeds its seams. Compares the
DECISION SEQUENCE (ordered seam calls {"name","args"} in "trace"), the final
return ("ret") and — for the _tag flow — the mutated runtime state ("state").

The live defs are AST-extracted (module constants seeded, Assign AND AnnAssign,
plain-Name targets only; PoolKey/_OAuthRuntimeSpec dataclasses seeded) so the
heavy quarry import chain is never imported. The extraction namespace IS a
registered sys.modules module ("hermes_cli.runtime_provider", the __name__
alias) because py3.14 dataclasses/annotation resolution needs cls.__module__
resolvable. Pure siblings (_cfg_provider, _same_registered_provider, _runtime,
_azure_anthropic_env_key, openai_env_key_authoritative_for, _tag,
_pool_entry_api_key, _nous_entry_key_usable, _nous_min_key_ttl,
_openrouter_fallback) stay REAL so nested decision logic is exercised, matching
the C port. AuthError is the REAL class extracted from hermes_cli/auth_constants.py.
Fail-closed: missing defs or 0 cases => exit 2.
Summary line: 'ORACLE runtime_provider_flow: N cases, M mismatches'.

Representation contract (shared with the C port header):
- Registry / provider / pool-entry descriptors are JSON objects; the oracle
  wraps them in SimpleNamespace for getattr/attribute access. Registry seam
  results are null (missing) or a NON-EMPTY object (fixtures never return an
  empty object, whose SimpleNamespace truthiness would diverge from JSON).
- str()/repr() of CONTAINERS is out of fixture scope; fixtures carry scalars.
- strip()/rstrip()/lower()/repr() are ASCII-scoped in the port; fixtures stay
  ASCII (Python's are unicode-wide). Embedded number literals avoid
  integral-float forms ("1.0") and leading zeros (libjson double parse).
- Non-finite floats -> {"__float__": "inf"|"-inf"|"nan"}; integral floats
  normalize to ints on both sides before comparison.
- `except Exception` does NOT catch SystemExit/KeyboardInterrupt/GeneratorExit;
  the top-level catch is BaseException so those project to a raise sentinel.
- BaseException objects passed as logger.debug args project to str(exc) (the
  %s rendering); the C port records the same string.
- _runtime call-site binding: an extra key duplicating a positional parameter
  (provider/api_mode/base_url/api_key, checked in signature order) raises
  TypeError "_runtime() got multiple values for argument '<name>'" before any
  seam event.
"""
import ast
import contextlib
import copy
import json
import logging
import math
import os
import re
import sys
import types
from dataclasses import dataclass
from typing import Any, Callable, Dict, Optional

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

Q = os.path.expanduser("~/hermes-agent")
SRC = os.path.join(Q, "hermes_cli", "runtime_provider.py")
AUTH_CONST_SRC = os.path.join(Q, "hermes_cli", "auth_constants.py")
HERMES_CONST_SRC = os.path.join(Q, "hermes_constants.py")

TARGETS = [
    "_resolves_to_custom", "_cfg_provider", "_same_registered_provider",
    "_config_base_url_for_provider", "_on_declared_anthropic_endpoint",
    "_azure_inferred_api_mode", "_runtime", "_creds_fallback",
    "_azure_anthropic_env_key", "openai_env_key_authoritative_for",
    "_host_gated_env_key_candidates", "_openrouter_should_use_pool",
    "_raise_if_provider_disabled", "_raise_for_credentialless_bare_custom",
    "_tag", "_local_endpoint_bypass", "_resolve_requested_shortcuts",
    "_refresh_nous_pool_entry",
]
STATEFUL = {"_tag"}

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
# sys.modules[cls.__module__]; the __name__ alias resolves annotations).
for _pkg in ("hermes_cli",):
    if _pkg not in sys.modules:
        _m = types.ModuleType(_pkg)
        _m.__path__ = []
        sys.modules[_pkg] = _m
_MOD = types.ModuleType("hermes_cli.runtime_provider")
_MOD.__file__ = SRC
sys.modules[_MOD.__name__] = _MOD
NS = _MOD.__dict__


def _seed_assigns_from(path, names, ns):
    """Extract real module-level Assign/AnnAssign constants from a sibling file."""
    src = open(path, encoding="utf-8").read()
    tree = ast.parse(src)
    seeded = set()
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
            for t in tgts:
                if isinstance(t, ast.Name) and t.id in names:
                    try:
                        exec_node(src, node, ns, path)
                        seeded.add(t.id)
                    except Exception:
                        pass
    return seeded


def extract():
    NS.update({
        "re": re, "json": json, "os": os, "sys": sys, "types": types,
        "contextlib": contextlib, "math": math, "copy": copy, "logging": logging,
        "Any": Any, "Optional": Optional, "Callable": Callable, "Dict": Dict,
        "dataclass": dataclass, "contextmanager": contextlib.contextmanager,
        "ContextVar": __import__("contextvars").ContextVar,
        "SimpleNamespace": types.SimpleNamespace,
        "__file__": SRC,
    })
    # Real imported constants (auth_constants / hermes_constants) so the
    # module's constant Assigns seed faithfully.
    _seed_assigns_from(AUTH_CONST_SRC, {
        "DEFAULT_CODEX_BASE_URL", "DEFAULT_XAI_OAUTH_BASE_URL", "DEFAULT_QWEN_BASE_URL",
        "ACTUAL_LOCAL_NOAUTH_PLACEHOLDER", "LMSTUDIO_NOAUTH_PLACEHOLDER",
    }, NS)
    _seed_assigns_from(HERMES_CONST_SRC, {"OPENROUTER_BASE_URL"}, NS)
    # Imported names referenced by constant Assigns / non-target defs: harmless
    # dummies (the seam-modeled ones are overwritten per case by install_recorders).
    _dummy_names = [
        "resolve_provider", "nous_api_mode", "determine_api_mode", "get_provider",
        "is_actual_route", "is_official_openai_host", "resolve_nous_runtime_credentials",
        "resolve_codex_runtime_credentials", "resolve_xai_oauth_runtime_credentials",
        "resolve_qwen_runtime_credentials", "resolve_api_key_provider_credentials",
        "resolve_external_process_provider_credentials", "has_usable_secret",
        "is_actual_local_base_url", "looks_like_openrouter_key", "normalize_actual_base_url",
        "format_auth_error", "_agent_key_is_usable", "_nous_inference_env_override",
        "_qwen_base_url", "get_secret_str", "base_url_host_matches", "base_url_hostname",
        "base_url_path", "env_int", "same_base_url_origin", "load_pool",
        "credential_pool_matches_provider", "custom_provider_pool_key_candidates",
        "_is_manual_source", "AUTH_TYPE_OAUTH", "CredentialPool", "PooledCredential",
        "_resolve_openrouter_runtime", "_resolve_azure_foundry_runtime",
        "_resolve_bedrock_runtime", "_resolve_external_process_runtime",
        "_is_external_process_provider", "codex_model_provider_id", "expand_direct_api_alias",
        "_resolve_named_custom_runtime", "_get_named_custom_provider", "_LLAMACPP_ALIASES",
        "owned_config_url_credential", "has_named_custom_provider", "is_routable_provider",
    ]
    for nm in _dummy_names:
        NS.setdefault(nm, lambda *a, **k: None)
    NS.setdefault("auth_mod", types.SimpleNamespace())
    NS.setdefault("_config_mod", types.SimpleNamespace())
    NS.setdefault("_models", types.SimpleNamespace())
    NS.setdefault("PROVIDER_REGISTRY", {})
    NS.setdefault("logger", logging.getLogger("hermes_cli.runtime_provider"))

    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)

    def seed_constants():
        for node in tree.body:
            if isinstance(node, (ast.Assign, ast.AnnAssign)):
                tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
                if not all(isinstance(t, ast.Name) for t in tgts):
                    continue
                try:
                    exec_node(src, node, NS, SRC)
                except Exception:
                    pass

    seed_constants()
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name in ("PoolKey", "_OAuthRuntimeSpec"):
            try:
                exec_node(src, node, NS, SRC)
            except Exception:
                pass
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            try:
                exec_node(src, node, NS, SRC)
            except Exception:
                pass
    seed_constants()  # re-seed after defs (lambda/resolver-valued constants)

    # REAL AuthError class from auth_constants (simple RuntimeError subclass).
    ac_src = open(AUTH_CONST_SRC, encoding="utf-8").read()
    ac_tree = ast.parse(ac_src)
    for node in ac_tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "AuthError":
            exec_node(ac_src, node, NS, AUTH_CONST_SRC)

    missing = [t for t in TARGETS if t not in NS]
    for cname in ("AuthError", "_VERTEX_NAMES", "_LOCAL_BYPASS_CLOUD_HOSTS", "_NO_AZURE_KEY"):
        if cname not in NS:
            missing.append(cname)
    if missing:
        print(f"FATAL oracle_runtime_provider_flow: missing defs: {missing}")
        sys.exit(2)
    return NS


extract()


# ── exception materialization ─────────────────────────────────────────────
EXC_CLASSES = {
    "OSError": OSError, "PermissionError": PermissionError,
    "FileNotFoundError": FileNotFoundError, "FileExistsError": FileExistsError,
    "BlockingIOError": BlockingIOError, "ValueError": ValueError,
    "TypeError": TypeError, "RuntimeError": RuntimeError, "KeyError": KeyError,
    "AttributeError": AttributeError, "Exception": Exception,
    "ImportError": ImportError, "ModuleNotFoundError": ModuleNotFoundError,
    "AuthError": NS["AuthError"],
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    if kind == "SystemExit":
        return SystemExit(spec.get("code"))
    if kind == "KeyboardInterrupt":
        return KeyboardInterrupt(spec.get("message", ""))
    msg = spec.get("message", "")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    errno = spec.get("errno")
    if errno is not None and issubclass(cls, OSError):
        return cls(errno, msg)
    return cls(msg)


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


def canon(v):
    """Integral floats -> int; non-finite floats -> tagged; recurse."""
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
        # record BEFORE resolving the script (the C dispatcher records first);
        # deep-copy args so later mutation cannot rewrite the trace.
        self.trace.append({"name": name, "args": copy.deepcopy(args)})
        entry = self.next_entry(name)
        if isinstance(entry, dict) and entry.get("__script__"):
            if isinstance(entry.get("raise"), dict):
                raise make_exc(entry["raise"])
            return copy.deepcopy(entry.get("ret"))
        return copy.deepcopy(entry)


def _as_descriptor(v):
    """Project a scripted registry/provider/entry object to SimpleNamespace."""
    if isinstance(v, dict):
        return types.SimpleNamespace(**v)
    return v


class _Registry:
    def __init__(self, ctx):
        self._ctx = ctx

    def get(self, key, default=None):
        r = self._ctx.call("PROVIDER_REGISTRY.get", {"key": key})
        return _as_descriptor(r) if r is not None else default

    def values(self):
        r = self._ctx.call("PROVIDER_REGISTRY.values", {})
        return [_as_descriptor(x) for x in (r or [])]


def install_recorders(ctx):
    """Bind every seam-modeled name in NS to the current case's recorder.

    Overwrites the extracted real defs for load_config / _get_model_config /
    _host_derived_api_key / _resolve_vertex_runtime (I/O or already-ported
    siblings modeled as seams). Pure siblings stay real.
    """
    NS["auth_mod"] = types.SimpleNamespace(
        resolve_provider=lambda name: ctx.call("auth.resolve_provider", {"name": name}))
    NS["PROVIDER_REGISTRY"] = _Registry(ctx)
    NS["_models"] = types.SimpleNamespace(
        normalize_provider=lambda p: ctx.call("_models.normalize_provider", {"provider": p}),
        azure_foundry_model_api_mode=lambda m: ctx.call("_models.azure_foundry_model_api_mode", {"model": m}))
    NS["_config_mod"] = types.SimpleNamespace(
        is_provider_enabled=lambda block: ctx.call("_config_mod.is_provider_enabled", {"block": block}))
    NS["get_provider"] = lambda provider, allow_network=False: _as_descriptor(
        ctx.call("get_provider", {"provider": provider, "allow_network": allow_network}))
    NS["base_url_path"] = lambda url: ctx.call("base_url_path", {"url": url})
    NS["base_url_host_matches"] = lambda url, host: ctx.call("base_url_host_matches", {"url": url, "host": host})
    NS["base_url_hostname"] = lambda url: ctx.call("base_url_hostname", {"url": url})
    NS["is_actual_route"] = lambda provider=None, base_url=None: ctx.call(
        "is_actual_route", {"provider": provider, "base_url": base_url})
    NS["normalize_actual_base_url"] = lambda base_url: ctx.call("normalize_actual_base_url", {"base_url": base_url})
    NS["get_secret_str"] = lambda name, default="": ctx.call("get_secret_str", {"name": name, "default": default})
    NS["_host_derived_api_key"] = lambda base_url: ctx.call("_host_derived_api_key", {"base_url": base_url})
    NS["env_int"] = lambda name, default=0: ctx.call("env_int", {"name": name, "default": default})
    NS["_agent_key_is_usable"] = lambda state, min_ttl: ctx.call("_agent_key_is_usable", {"state": state, "min_ttl": min_ttl})
    NS["load_config"] = lambda: ctx.call("load_config", {})
    NS["_get_model_config"] = lambda: ctx.call("_get_model_config", {})
    NS["_resolve_openrouter_runtime"] = lambda **kw: ctx.call("_resolve_openrouter_runtime", dict(kw))
    NS["_resolve_azure_foundry_runtime"] = lambda **kw: ctx.call("_resolve_azure_foundry_runtime", dict(kw))
    NS["_resolve_vertex_runtime"] = lambda requested_provider: ctx.call(
        "_resolve_vertex_runtime", {"requested_provider": requested_provider})
    NS["logger"] = types.SimpleNamespace(
        debug=lambda fmt, *a: ctx.call("logger.debug", {
            "fmt": fmt,
            # BaseException log args project to str(exc) (the %s rendering),
            # matching the C port and the sibling flow oracles' RecLogger.
            "args": [str(x) if isinstance(x, BaseException) else x for x in a]}),
        info=lambda *a, **k: None, warning=lambda *a, **k: None, error=lambda *a, **k: None)


def _mc(inp):
    return dict(inp.get("model_cfg") or {})


def run_case(func, inp):
    ctx = Ctx(inp)
    install_recorders(ctx)
    ns = NS
    out = {}
    ret = None
    try:
        if func == "_resolves_to_custom":
            ret = ns["_resolves_to_custom"](inp.get("name", ""))
        elif func == "_cfg_provider":
            ret = ns["_cfg_provider"](_mc(inp))
        elif func == "_same_registered_provider":
            ret = ns["_same_registered_provider"](inp.get("provider", ""), inp.get("configured_provider", ""))
        elif func == "_config_base_url_for_provider":
            ret = ns["_config_base_url_for_provider"](_mc(inp), inp.get("provider", ""))
        elif func == "_on_declared_anthropic_endpoint":
            ret = ns["_on_declared_anthropic_endpoint"](inp.get("provider", ""), inp.get("base_url"))
        elif func == "_azure_inferred_api_mode":
            ret = ns["_azure_inferred_api_mode"](inp.get("effective_model", ""), inp.get("api_mode", ""))
        elif func == "_runtime":
            ret = ns["_runtime"](inp.get("provider", ""), inp.get("api_mode", ""),
                                 inp.get("base_url"), inp.get("api_key"), **(inp.get("extra") or {}))
        elif func == "_creds_fallback":
            resolve_name = inp.get("resolve_name", "resolve")
            resolve = lambda: ctx.call(resolve_name, {})
            r = ns["_creds_fallback"](inp.get("api_key"), inp.get("explicit_base_url"),
                                      inp.get("base_url"), inp.get("expiry"),
                                      inp.get("expiry_key", ""), resolve)
            ret = {"api_key": r[0], "base_url": r[1], "expiry": r[2]}
        elif func == "_azure_anthropic_env_key":
            ret = ns["_azure_anthropic_env_key"](_mc(inp))
        elif func == "openai_env_key_authoritative_for":
            ret = ns["openai_env_key_authoritative_for"](inp.get("base_url"), inp.get("env_openai_base"))
        elif func == "_host_gated_env_key_candidates":
            ret = ns["_host_gated_env_key_candidates"](inp.get("base_url"), ollama=inp.get("ollama", False))
        elif func == "_openrouter_should_use_pool":
            ret = ns["_openrouter_should_use_pool"](inp.get("requested_provider", ""), _mc(inp),
                                                    inp.get("explicit_api_key"), inp.get("explicit_base_url"))
        elif func == "_raise_if_provider_disabled":
            ret = ns["_raise_if_provider_disabled"](inp.get("requested_provider", ""))
        elif func == "_raise_for_credentialless_bare_custom":
            ret = ns["_raise_for_credentialless_bare_custom"](inp.get("requested_provider", ""),
                                                              dict(inp.get("runtime") or {}))
        elif func == "_tag":
            runtime = inp.get("runtime")
            runtime = dict(runtime) if isinstance(runtime, dict) else runtime
            ret = ns["_tag"](runtime, inp.get("requested_provider", ""))
            out["state"] = {"runtime": runtime}
        elif func == "_local_endpoint_bypass":
            ret = ns["_local_endpoint_bypass"](inp.get("requested_provider", ""),
                                               inp.get("explicit_api_key"), inp.get("explicit_base_url"))
        elif func == "_resolve_requested_shortcuts":
            ret = ns["_resolve_requested_shortcuts"](inp.get("requested_provider", ""),
                                                     inp.get("explicit_api_key"),
                                                     inp.get("explicit_base_url"), inp.get("target_model"))
        elif func == "_refresh_nous_pool_entry":
            pool = types.SimpleNamespace(
                try_refresh_current=lambda: _as_descriptor(ctx.call("pool.try_refresh_current", {})))
            entry = _as_descriptor(dict(inp.get("entry") or {}))
            r = ns["_refresh_nous_pool_entry"](pool, entry, inp.get("pool_api_key", ""))
            e, k = r
            ret = {"entry": vars(e) if isinstance(e, types.SimpleNamespace) else e, "pool_api_key": k}
        else:
            out["error"] = "unknown func"
            return out
    except BaseException as exc:  # noqa: BLE001 - raise sentinel parity (SystemExit!)
        ret = proj_exc(exc)
    out["ret"] = ret
    out["trace"] = ctx.trace
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
        try:
            py_out = run_case(func, inp)
        except Exception as exc:  # noqa: BLE001
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
        print("FATAL oracle_runtime_provider_flow: 0 cases")
        print("ORACLE runtime_provider_flow: 0 cases, 0 mismatches")
        return 2
    print(f"ORACLE runtime_provider_flow: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())