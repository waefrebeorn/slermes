#!/usr/bin/env python3
"""Oracle for the hermes_cli/models.py catalog/selection FLOW port
(src/cli/port_models_catalog_flow.c).

Drives the LIVE models.py defs with SCRIPTED seams: every config/profile/
credential/network dependency is a recording stub fed the same scripts the C
harness feeds mcat_call. Compares the DECISION SEQUENCE (ordered "trace" of
{ev,args,ret} seam events) and the canonical return ("out"). Pure claimed
siblings (normalize_provider, _provider_keys, _cache_entry_valid,
_model_dedup_key) stay REAL via AST extraction so nested decision logic is
exercised, matching the C port's internal transcriptions.

Live defs are AST-extracted from the quarry (annotations kept as strings via
the __future__ flag); CuratedFallbackModels comes from models_catalog_static.
The heavy quarry import chain is never imported. Fail-closed: 0 cases =>
exit 1; any harness/oracle exception per case rides as {"__raise__": cls}.
Summary: 'ORACLE models_catalog_flow: N cases, M mismatches'.

Representation contract (both sides, see include/port_models_catalog_flow.h):
- tuple/set/frozenset -> array (sets sorted); CuratedFallbackModels ->
  {"$curated": [...]}; None -> null; uncaught exception -> {"__raise__": cls}.
- scripts: absent key -> None; {"$seq": [...]} consumed per call (exhaustion
  raises RuntimeError("script exhausted: <name>")); {"__raise__": cls, "msg"}
  raises; any other value reused. None-valued arg keys dropped on both sides.
- _BORROWED_MODEL_PROVIDERS iterates in fixture order (live frozenset order is
  hash-randomized); membership-only sets stay unordered.
- str()/strip()/lower() operand fields are ASCII strings/null in fixtures.
"""
import __future__
import ast
import builtins
import json
import logging
import os
import sys
import types
from pathlib import Path

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
if hasattr(sys.stdin, "reconfigure"):
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "hermes_cli" / "models.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "hermes_cli" / "models.py")
STATIC_SRC = str(QUARRY / "hermes_cli" / "models_catalog_static.py")

NAME = "models_catalog_flow"

PORTED = [
    "_merge_unique", "_configured_provider_ids", "_resolve_provider_prefix",
    "static_catalog", "static_catalog_providers", "_static_catalog_matches",
    "_detection_candidates", "_catalog_fetcher", "_api_key_credentials",
    "_api_key_provider_live", "_stepfun_catalog", "_profile_live_catalog",
    "probe_profile_catalog", "merge_profile_catalog",
    "_drop_delisted_opencode_models", "_configured_relay_base_url",
    "_relay_model_catalog", "_disk_serve_tier",
]
PURE = ["normalize_provider", "_provider_keys", "_cache_entry_valid", "_model_dedup_key"]
WANTED = set(PORTED) | set(PURE)

logging.disable(logging.CRITICAL)

# ── AST extraction ──────────────────────────────────────────────────────────

_SRC_TEXT = Path(SRC).read_text(encoding="utf-8")
_TREE = ast.parse(_SRC_TEXT)
_FLAGS = __future__.annotations.compiler_flag
DEF_CODE = {}
for _node in _TREE.body:
    if isinstance(_node, ast.FunctionDef) and _node.name in WANTED:
        DEF_CODE[_node.name] = compile(
            ast.Module(body=[_node], type_ignores=[]), SRC, "exec", flags=_FLAGS, dont_inherit=True)
_missing = WANTED - set(DEF_CODE)
if _missing:
    print(f"ORACLE {NAME}: FATAL missing live defs: {sorted(_missing)}", file=sys.stderr)
    sys.exit(1)

_STATIC_TREE = ast.parse(Path(STATIC_SRC).read_text(encoding="utf-8"))
_CURATED_NODE = next(n for n in _STATIC_TREE.body
                     if isinstance(n, ast.ClassDef) and n.name == "CuratedFallbackModels")


class ProviderProfile:
    """Stub of providers.base.ProviderProfile for isinstance gates."""


class ProfileD(ProviderProfile):
    pass


_BASE_NS = {
    "__name__": "mcat_live_models",
    "__builtins__": builtins,
    "logging": logging,
    "logger": logging.getLogger("mcat_live_models"),
    "os": os,
    "Path": Path,
    "ProviderProfile": ProviderProfile,
}
exec(compile(ast.Module(body=[_CURATED_NODE], type_ignores=[]), STATIC_SRC, "exec",
             flags=_FLAGS, dont_inherit=True), _BASE_NS)
CuratedFallbackModels = _BASE_NS["CuratedFallbackModels"]
# py3.14 dataclass/annotation resolution wants a registered module.
_MOD = types.ModuleType("mcat_live_models")
_MOD.__dict__.update(_BASE_NS)
sys.modules.setdefault("mcat_live_models", _MOD)

# ── canonical representation ────────────────────────────────────────────────


def norm(v):
    if isinstance(v, CuratedFallbackModels):
        return {"$curated": [norm(x) for x in v]}
    if isinstance(v, (list, tuple)):
        return [norm(x) for x in v]
    if isinstance(v, (set, frozenset)):
        return sorted((norm(x) for x in v), key=lambda s: str(s))
    if isinstance(v, dict):
        return {k: norm(x) for k, x in v.items()}
    if isinstance(v, (ProfileD, types.SimpleNamespace)):
        return {k: norm(x) for k, x in vars(v).items() if not callable(x)}
    if isinstance(v, Path):
        return v.as_posix()
    if isinstance(v, float) and v.is_integer():
        return int(v)
    return v


def denorm(v):
    if isinstance(v, dict):
        if "$curated" in v:
            return CuratedFallbackModels(denorm(x) for x in v["$curated"])
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

ARGMAP = {
    "load_config": lambda a, k: {},
    "get_model_config_dict": lambda a, k: {},
    "get_provider_profile": lambda a, k: {"provider": a[0]},
    "list_providers": lambda a, k: {},
    "resolve_api_key_provider_credentials": lambda a, k: {"provider": a[0]},
    "registry_entry": lambda a, k: {"provider": a[0]},
    "relay_probe_api_key": lambda a, k: {"provider": a[0], "relay": a[1], "api_key": a[2], "base_url": a[3]},
    "fetch_api_models": lambda a, k: {"api_key": a[0], "base_url": a[1]},
    "fetch_models": lambda a, k: dict(k),
    "model_alias_canonical": lambda a, k: {"key": a[0]},
    "detect_static_provider_for_model": lambda a, k: {"model_name": a[0], "current_provider": a[1]},
    "model_in_provider_catalog": lambda a, k: {"name_lower": a[0], "providers": sorted(a[1])},
    "find_openrouter_slug": lambda a, k: {"model_name": a[0]},
    "normalize_route_base_url": lambda a, k: {"base_url": a[0]},
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


def make_profile(d, scripts):
    if not isinstance(d, dict):
        return d
    if d.get("is_provider_profile", True):
        p = ProfileD()
        p.__dict__.update(d)

        def fetch_models(self, **kw):
            return recorder("fetch_models", scripts)(**kw)

        p.fetch_models = types.MethodType(fetch_models, p)
        return p
    return types.SimpleNamespace(**d)


def convert(name, value, scripts):
    if name == "get_provider_profile":
        return make_profile(value, scripts)
    if name == "list_providers":
        return [make_profile(p, scripts) for p in value] if isinstance(value, list) else value
    if name == "registry_entry":
        return types.SimpleNamespace(**value) if isinstance(value, dict) else value
    if name == "detect_static_provider_for_model":
        return tuple(value) if isinstance(value, list) else value
    return value


def recorder(name, scripts):
    def rec(*a, **kw):
        args = {k: v for k, v in ARGMAP[name](a, kw).items() if v is not None}
        script = scripts.next(name)
        if isinstance(script, dict) and isinstance(script.get("__raise__"), str):
            marker = {"__raise__": script["__raise__"], "msg": script.get("msg", "")}
            scripts.trace.append({"ev": name, "args": args, "ret": marker})
            raise make_exc(marker)
        value = convert(name, denorm(script), scripts)
        scripts.trace.append({"ev": name, "args": args, "ret": norm(value)})
        return value
    return rec


def install_module_stubs(scripts):
    hc = types.ModuleType("hermes_cli")
    hc.__path__ = []
    cfg = types.ModuleType("hermes_cli.config")
    cfg.load_config = recorder("load_config", scripts)
    auth = types.ModuleType("hermes_cli.auth")
    auth.resolve_api_key_provider_credentials = recorder("resolve_api_key_provider_credentials", scripts)

    class _Registry:
        def get(self, provider):
            return recorder("registry_entry", scripts)(provider)

    auth.PROVIDER_REGISTRY = _Registry()
    adp = types.ModuleType("hermes_cli.auth_declared_providers")
    adp.relay_probe_api_key = recorder("relay_probe_api_key", scripts)
    ms = types.ModuleType("hermes_cli.model_search")
    ms.model_alias_canonical = recorder("model_alias_canonical", scripts)
    prov = types.ModuleType("providers")
    prov.__path__ = []
    prov.get_provider_profile = recorder("get_provider_profile", scripts)
    prov.list_providers = recorder("list_providers", scripts)
    pb = types.ModuleType("providers.base")
    pb.ProviderProfile = ProviderProfile
    hc.config, hc.auth, hc.auth_declared_providers, hc.model_search = cfg, auth, adp, ms
    prov.base = pb
    sys.modules.update({
        "hermes_cli": hc, "hermes_cli.config": cfg, "hermes_cli.auth": auth,
        "hermes_cli.auth_declared_providers": adp, "hermes_cli.model_search": ms,
        "providers": prov, "providers.base": pb,
    })


# ── per-case namespace ──────────────────────────────────────────────────────


def build_ns(rec):
    env = rec["in"].get("env") or {}
    ns = dict(_BASE_NS)
    ns["_PROVIDER_ALIASES"] = dict(env.get("aliases") or {})
    ns["_PROVIDER_MODELS"] = {k: list(v) for k, v in (env.get("provider_models") or {}).items()}
    ns["_AGGREGATOR_PROVIDERS"] = set(env.get("aggregators") or [])
    # ordered model of the live frozenset iteration (hash order is random)
    ns["_BORROWED_MODEL_PROVIDERS"] = tuple(env.get("borrowed") or [])
    ns["_LIVE_FIRST_PICKER_PROVIDERS"] = frozenset(env.get("live_first") or [])
    ns["_OPENCODE_FREE_EXCLUDED_MODELS"] = frozenset(env.get("opencode_excluded") or [])
    ns["_OLLAMA_LOCAL_MODELS_CACHE_TTL"] = env.get("ollama_ttl", 300)
    ns["_PROVIDER_MODELS_CACHE_TTL"] = env.get("cache_ttl", 3600)
    ns["_PROVIDER_MODELS_STALE_SERVE_MAX"] = env.get("stale_max", 604800)
    ns["_PROVIDER_MODELS_FALLBACK_TTL"] = env.get("fallback_ttl", 60)
    ns["_PROVIDER_CATALOG_FETCHERS"] = {
        k: (lambda normalized, force_refresh, _k=k: None) for k in (env.get("fetchers") or [])}
    trace = []
    scripts = Scripts(rec["in"].get("io") or {}, trace)
    install_module_stubs(scripts)
    # constants BEFORE def-exec: _disk_serve_tier's default ttl reads one
    for nm, code in DEF_CODE.items():
        exec(code, ns)
    # seamed claimed siblings override as recorders (never extracted real);
    # the seam name is the C/harness name (no leading underscore)
    for live_nm, seam_nm in (
            ("detect_static_provider_for_model", "detect_static_provider_for_model"),
            ("_model_in_provider_catalog", "model_in_provider_catalog"),
            ("_find_openrouter_slug", "find_openrouter_slug"),
            ("_get_model_config_dict", "get_model_config_dict"),
            ("normalize_route_base_url", "normalize_route_base_url"),
            ("fetch_api_models", "fetch_api_models")):
        ns[live_nm] = recorder(seam_nm, scripts)
    return ns, trace, scripts


# ── live driver ─────────────────────────────────────────────────────────────


def drive(rec, ns, scripts):
    func = rec["func"]
    inp = rec["in"]
    if func == "_merge_unique":
        kw = {"key": ns["_model_dedup_key"]} if inp.get("dedup_key") else {}
        return ns[func](inp["primary"], inp["secondary"], **kw)
    if func == "_configured_provider_ids":
        return ns[func]()
    if func == "_resolve_provider_prefix":
        return ns[func](inp["model_name"])
    if func == "static_catalog":
        return ns[func](inp["provider"])
    if func == "static_catalog_providers":
        return ns[func]()
    if func in ("_static_catalog_matches", "_detection_candidates"):
        return list(ns[func](inp["name"], inp["current_provider"]))
    if func == "_catalog_fetcher":
        f = ns[func](inp["normalized"])
        if f is None:
            return None
        for k, v in ns["_PROVIDER_CATALOG_FETCHERS"].items():
            if v is f:
                return k
        return "profile-hook"
    if func == "_api_key_credentials":
        return ns[func](inp["normalized"])
    if func in ("_api_key_provider_live", "_stepfun_catalog"):
        return ns[func](inp["normalized"], inp.get("force_refresh", False))
    if func == "_profile_live_catalog":
        return ns[func](inp["normalized"])
    if func == "probe_profile_catalog":
        return ns[func](inp["normalized"], make_profile(inp.get("profile"), scripts),
                        inp.get("api_key"), inp.get("base_url"))
    if func == "merge_profile_catalog":
        return ns[func](inp["normalized"], make_profile(inp.get("profile"), scripts),
                        denorm(inp.get("live")))
    if func == "_drop_delisted_opencode_models":
        return ns[func](inp["normalized"], denorm(inp.get("rows")))
    if func == "_configured_relay_base_url":
        return ns[func](inp["provider"])
    if func == "_relay_model_catalog":
        return ns[func](inp["normalized"], inp["relay"])
    if func == "_disk_serve_tier":
        return ns[func](denorm(inp.get("entry")), inp["fp"], inp["now"],
                        is_ollama=inp.get("is_ollama", False),
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
        trace = []
        try:
            ns, trace, scripts = build_ns(rec)
            try:
                out = norm(drive(rec, ns, scripts))
            except BaseException as e:  # noqa: BLE001 - fail-closed per case
                out = {"__raise__": type(e).__name__}
        except BaseException as e:  # noqa: BLE001 - seeding failure is a mismatch
            out = {"__raise__": type(e).__name__}
        got_out = rec.get("out")
        got_trace = rec.get("trace")
        if got_out != out or got_trace != trace:
            mism += 1
            if shown < 10:
                shown += 1
                print(f"MISMATCH {rec.get('func')} case {n}:")
                if got_out != out:
                    print(f"  out  C={json.dumps(got_out, ensure_ascii=False)[:400]}")
                    print(f"  out  P={json.dumps(out, ensure_ascii=False)[:400]}")
                if got_trace != trace:
                    print(f"  trace C={json.dumps(got_trace, ensure_ascii=False)[:600]}")
                    print(f"  trace P={json.dumps(trace, ensure_ascii=False)[:600]}")
    print(f"ORACLE {NAME}: {n} cases, {mism} mismatches")
    sys.exit(1 if (mism or n == 0) else 0)


if __name__ == "__main__":
    main()
