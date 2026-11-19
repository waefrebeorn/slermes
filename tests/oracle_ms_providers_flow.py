#!/usr/bin/env python3
"""Oracle for the hermes_cli/model_switch_providers.py FLOW port
(src/cli/port_ms_providers_flow.c).

Drives the LIVE model_switch_providers.py defs with SCRIPTED seams: every
config/credential/network dependency is a recording stub fed the same scripts
the C harness feeds msp_call. Compares the DECISION SEQUENCE (ordered "trace"
of {ev,args,ret} seam events), the canonical return ("ret") and the post-call
results argument window ("state", _finalize_picker_rows only). The internal
gap step _first_curated stays REAL via AST extraction so nested resolution
logic is exercised, matching the C port's internal transcription. Claimed
siblings (_credential_pool_is_usable, _fetch_picker_live_models) are SEAMED.

Live defs are AST-extracted from the quarry (annotations kept as strings via
the __future__ flag); module-level Assign/AnnAssign constants are seeded after
the class/def execs; ENDPOINT_ADDRESS_FIELDS / GUEST_MODEL / FREE_TIER_LABEL
come from the live config_providers / anon_auth Assigns. The heavy quarry
import chain is never imported. Fail-closed: 0 cases => exit 1; any
harness/oracle exception per case rides as {"__raise__": cls} (BaseException,
so SystemExit propagates). Summary:
'ORACLE ms_providers_flow: N cases, M mismatches'.

Representation contract (both sides, see include/port_ms_providers_flow.h):
- tuple/set/frozenset -> array (sets sorted); _NativePickerModelList -> plain
  array in returns (the {"__native__": [...]} script marker is internal only);
  overlay / PROVIDER_REGISTRY / usage doubles -> attribute dicts; None -> null;
  uncaught exception -> {"__raise__": cls}.
- scripts: absent key -> None; {"$seq": [...]} consumed per call (exhaustion
  raises RuntimeError("script exhausted: <name>")); {"__raise__": cls, ...}
  raises (trace ret is the WHOLE marker); any other value reused. None-valued
  arg keys dropped on both sides. The trace ret is norm(raw script) so the
  native marker / usage / tuple conversions stay internal to the live call.
- read_env rides a singleton os.environ double whose get() dispatches to the
  current case's scripts (default-arg read_env=os.environ.get captures it).
- str()/strip()/lower() operand fields are ASCII strings/null in fixtures;
  raise markers use exact builtin class names (HTTPException maps to
  http.client.HTTPException).
"""
import __future__
import ast
import builtins
import http.client
import json
import logging
import sys
import threading
import types
from pathlib import Path
from typing import Any, List, Optional

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
if hasattr(sys.stdin, "reconfigure"):
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "hermes_cli" / "model_switch_providers.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "hermes_cli" / "model_switch_providers.py")
CP_SRC = str(QUARRY / "hermes_cli" / "config_providers.py")
AA_SRC = str(QUARRY / "hermes_cli" / "anon_auth.py")

NAME = "ms_providers_flow"

PORTED = [
    "_discovered_catalog_stale", "_any_env", "_skip", "_auth_store_has_provider",
    "_raw_pool_usable", "_pool_usable", "_overlay_has_env_creds", "_azure_entra_configured",
    "_has_fast_aws_sdk_signal", "_has_aws_sdk_creds_for_listing", "_live_or_curated_ids",
    "_aws_live_or_curated_ids", "_nous_picker_model_ids", "_free_tier_nous_row", "_cap_models",
    "_discover_endpoint_models", "_finalize_picker_rows", "_saved_entry_on_other_origin",
]
PURE = ["_first_curated"]
CLASSES = ["_NativePickerModelList"]
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


def _extract_assigns(src, names):
    """Exec matching Assign/AnnAssign nodes in source order into one namespace
    (later nodes may reference earlier ones, e.g. ENDPOINT_ADDRESS_FIELDS)."""
    tree = ast.parse(Path(src).read_text(encoding="utf-8"))
    ns = {"__builtins__": builtins}
    found = set()
    for node in tree.body:
        if isinstance(node, ast.Assign):
            tgts = [t.id for t in node.targets if isinstance(t, ast.Name)]
        elif isinstance(node, ast.AnnAssign) and isinstance(node.target, ast.Name):
            tgts = [node.target.id]
        else:
            continue
        if any(t in names for t in tgts):
            exec(_compile_node(node, src), ns)
            found.update(tgts)
    miss = set(names) - found
    if miss:
        print(f"ORACLE {NAME}: FATAL missing constants {sorted(miss)} in {src}", file=sys.stderr)
        sys.exit(1)
    return ns


_CP = _extract_assigns(CP_SRC, {"_CAMEL_ALIASES", "ENDPOINT_ADDRESS_FIELDS"})
ENDPOINT_ADDRESS_FIELDS = _CP["ENDPOINT_ADDRESS_FIELDS"]
_AA = _extract_assigns(AA_SRC, {"GUEST_MODEL", "FREE_TIER_LABEL"})
GUEST_MODEL = _AA["GUEST_MODEL"]
FREE_TIER_LABEL = _AA["FREE_TIER_LABEL"]

# ── scripted seams ──────────────────────────────────────────────────────────

_SCRIPTS = None  # current case's Scripts (singleton os.environ dispatches here)


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


def norm(v):
    if isinstance(v, (list, tuple)):
        return [norm(x) for x in v]
    if isinstance(v, (set, frozenset)):
        return sorted((norm(x) for x in v), key=lambda s: str(s))
    if isinstance(v, dict):
        return {k: norm(x) for k, x in v.items()}
    if isinstance(v, types.SimpleNamespace):
        return {k: norm(x) for k, x in vars(v).items() if not callable(x)}
    if isinstance(v, float) and v.is_integer():
        return int(v)
    return v


def make_exc(marker):
    cls_name = marker.get("__raise__", "RuntimeError")
    if cls_name == "HTTPException":
        return http.client.HTTPException(marker.get("msg", ""))
    msg = marker.get("msg", "")
    cls = getattr(builtins, cls_name, None)
    if not (isinstance(cls, type) and issubclass(cls, BaseException)):
        cls = type(cls_name, (Exception,), {})
    return cls(msg)


ARGMAP = {
    "read_env": lambda a, k: {"name": a[0]},
    "load_auth_store": lambda a, k: {},
    "credential_pool_is_usable": lambda a, k: {"provider": a[0],
                                               "raw_pool_present": k.get("raw_pool_present"),
                                               "for_picker": k.get("for_picker")},
    "has_vertex_credentials": lambda a, k: {},
    "get_model_config_dict": lambda a, k: {},
    "has_aws_credentials": lambda a, k: {},
    "cached_provider_model_ids": lambda a, k: {"slug": a[0], "non_blocking": k.get("non_blocking")},
    "merge_with_models_dev": lambda a, k: {"slug": a[0], "model_ids": a[1]},
    "without_generation_models": lambda a, k: {"model_ids": a[0]},
    "get_pricing_for_provider": lambda a, k: {"provider": a[0], "cached_only": k.get("cached_only")},
    "get_provider_auth_state": lambda a, k: {"provider": a[0]},
    "check_nous_free_tier": lambda a, k: {"force_fresh": k.get("force_fresh")},
    "union_with_portal_free_recommendations": lambda a, k: {"model_ids": a[0], "pricing": a[1], "portal": a[2]},
    "union_with_portal_paid_recommendations": lambda a, k: {"model_ids": a[0], "pricing": a[1], "portal": a[2]},
    "union_with_nous_on_sale_models": lambda a, k: {"model_ids": a[0], "pricing": a[1]},
    "nous_policy_allowed_ids": lambda a, k: {},
    "restrict_to_nous_policy": lambda a, k: {"model_ids": a[0], "allowed_ids": a[1],
                                             "rescue_empty": k.get("rescue_empty")},
    "nous_picker_usage": lambda a, k: {"any_age": k.get("any_age")},
    "featured_first": lambda a, k: {"model_ids": a[0], "featured_model_ids": a[1]},
    "has_guest": lambda a, k: {},
    "guest_allowed": lambda a, k: {},
    "free_inference_active": lambda a, k: {},
    "fetch_picker_live_models": lambda a, k: {"api_key": a[0], "api_url": a[1],
                                              "native_catalog_provider": a[2],
                                              "preserve_native_models": a[3],
                                              "headers": k.get("headers"), "timeout": k.get("timeout"),
                                              "api_mode": k.get("api_mode")},
    "cached_fetch_api_models": lambda a, k: {"api_key": a[0], "api_url": a[1],
                                             "cache_only": k.get("cache_only"), "timeout": k.get("timeout"),
                                             "headers": k.get("headers"), "api_mode": k.get("api_mode")},
    "is_provider_enabled": lambda a, k: {"cfg": a[0]},
    "model_requires_account_discovery": lambda a, k: {"slug": a[0], "model": a[1]},
    "load_config": lambda a, k: {},
    "same_base_url_origin": lambda a, k: {"url": a[0], "base_url": a[1]},
}


def convert(name, value):
    if name in ("fetch_picker_live_models", "cached_fetch_api_models"):
        if isinstance(value, dict) and "__native__" in value:
            return _BASE_NS["_NativePickerModelList"](value["__native__"])
        return value
    if name == "nous_picker_usage":
        return types.SimpleNamespace(**value) if isinstance(value, dict) else value
    if name in ("union_with_portal_free_recommendations", "union_with_portal_paid_recommendations"):
        return tuple(value) if isinstance(value, list) else value
    return value


def recorder(name, scripts):
    def rec(*a, **kw):
        args = {k: norm(v) for k, v in ARGMAP[name](a, kw).items() if v is not None}
        script = scripts.next(name)
        if isinstance(script, dict) and isinstance(script.get("__raise__"), str):
            marker = dict(script)
            scripts.trace.append({"ev": name, "args": args, "ret": marker})
            raise make_exc(marker)
        scripts.trace.append({"ev": name, "args": args, "ret": norm(script)})
        return convert(name, script)
    return rec


class _EnvD:
    """os.environ double: get() records read_env and honors the "" default."""
    def get(self, name, default=""):
        v = recorder("read_env", _SCRIPTS)(name)
        return v if v is not None else default


class _OsD:
    def __init__(self):
        self.environ = _EnvD()


_OS_SINGLETON = _OsD()


def install_module_stubs(scripts, env):
    def mod(name, **attrs):
        m = types.ModuleType(name)
        for k, v in attrs.items():
            setattr(m, k, v)
        sys.modules[name] = m
        return m

    registry = {k: types.SimpleNamespace(**v)
                for k, v in (env.get("provider_registry") or {}).items()}
    hc = mod("hermes_cli")
    hc.__path__ = []
    auth = mod("hermes_cli.auth",
               _load_auth_store=recorder("load_auth_store", scripts),
               PROVIDER_REGISTRY=registry,
               get_provider_auth_state=recorder("get_provider_auth_state", scripts))
    models = mod("hermes_cli.models",
                 _get_model_config_dict=recorder("get_model_config_dict", scripts),
                 _MODELS_DEV_PREFERRED=set(env.get("models_dev_preferred") or []),
                 _merge_with_models_dev=recorder("merge_with_models_dev", scripts),
                 cached_provider_model_ids=recorder("cached_provider_model_ids", scripts),
                 check_nous_free_tier=recorder("check_nous_free_tier", scripts),
                 union_with_nous_on_sale_models=recorder("union_with_nous_on_sale_models", scripts),
                 union_with_portal_free_recommendations=recorder("union_with_portal_free_recommendations", scripts),
                 union_with_portal_paid_recommendations=recorder("union_with_portal_paid_recommendations", scripts),
                 cached_fetch_api_models=recorder("cached_fetch_api_models", scripts),
                 _model_requires_account_discovery=recorder("model_requires_account_discovery", scripts))
    cc = mod("hermes_cli.chat_catalog",
             without_generation_models=recorder("without_generation_models", scripts))
    mp = mod("hermes_cli.models_pricing",
             get_pricing_for_provider=recorder("get_pricing_for_provider", scripts),
             nous_policy_allowed_ids=recorder("nous_policy_allowed_ids", scripts),
             restrict_to_nous_policy=recorder("restrict_to_nous_policy", scripts))
    mpu = mod("hermes_cli.model_picker_usage",
              featured_first=recorder("featured_first", scripts),
              nous_picker_usage=recorder("nous_picker_usage", scripts))
    aa = mod("hermes_cli.anon_auth",
             has_guest=recorder("has_guest", scripts),
             guest_allowed=recorder("guest_allowed", scripts),
             free_inference_active=recorder("free_inference_active", scripts),
             FREE_TIER_LABEL=FREE_TIER_LABEL, GUEST_MODEL=GUEST_MODEL)
    cfg = mod("hermes_cli.config",
              is_provider_enabled=recorder("is_provider_enabled", scripts),
              load_config=recorder("load_config", scripts))
    cp = mod("hermes_cli.config_providers", ENDPOINT_ADDRESS_FIELDS=ENDPOINT_ADDRESS_FIELDS)
    agent = mod("agent")
    agent.__path__ = []
    va = mod("agent.vertex_adapter", has_vertex_credentials=recorder("has_vertex_credentials", scripts))
    ba = mod("agent.bedrock_adapter", has_aws_credentials=recorder("has_aws_credentials", scripts))
    utils = mod("utils", same_base_url_origin=recorder("same_base_url_origin", scripts))
    hc.auth, hc.models, hc.chat_catalog = auth, models, cc
    hc.models_pricing, hc.model_picker_usage, hc.anon_auth = mp, mpu, aa
    hc.config, hc.config_providers = cfg, cp
    agent.vertex_adapter, agent.bedrock_adapter = va, ba


# ── base namespace (defs exec'd once; os singleton captures default args) ───

_BASE_NS = {
    "__name__": "msp_live_model_switch_providers",
    "__builtins__": builtins,
    "logging": logging,
    "logger": logging.getLogger("hermes_cli.model_switch"),
    "os": _OS_SINGLETON,
    "http": http,
    "_threading": threading,
    "Any": Any, "List": List, "Optional": Optional,
}
for _nm in CLASSES:
    exec(CLASS_CODE[_nm], _BASE_NS)
for _nm, _code in DEF_CODE.items():
    exec(_code, _BASE_NS)
for _node in ASSIGN_NODES:
    exec(_compile_node(_node, SRC), _BASE_NS)
_BASE_NS["logger"] = logging.getLogger("hermes_cli.model_switch")

# py3.14 dataclass/annotation resolution wants a registered module.
_MOD = types.ModuleType("msp_live_model_switch_providers")
_MOD.__dict__.update(_BASE_NS)
sys.modules.setdefault("msp_live_model_switch_providers", _MOD)

# Seamed claimed siblings: (live name, seam name).
SEAMED = [
    ("_credential_pool_is_usable", "credential_pool_is_usable"),
    ("_fetch_picker_live_models", "fetch_picker_live_models"),
]


def build_ns(rec):
    global _SCRIPTS
    inp = rec["in"]
    env = inp.get("env") or {}
    trace = []
    scripts = Scripts(inp.get("io") or {}, trace)
    _SCRIPTS = scripts
    install_module_stubs(scripts, env)
    # The extracted defs' __globals__ IS _BASE_NS, so per-case seam recorders
    # must be installed there (a dict copy would be invisible to internal calls).
    _BASE_NS["same_base_url_origin"] = recorder("same_base_url_origin", scripts)
    for live_nm, seam_nm in SEAMED:
        _BASE_NS[live_nm] = recorder(seam_nm, scripts)
    return _BASE_NS, trace, scripts


# ── live driver ─────────────────────────────────────────────────────────────


def drive(rec, ns, scripts, holder):
    func = rec["func"]
    inp = rec["in"]
    if func == "_discovered_catalog_stale":
        return ns[func](inp.get("entry"), inp.get("model_ids"))
    if func == "_any_env":
        return ns[func](inp.get("env_vars"))
    if func == "_skip":
        return ns[func](set(inp.get("seen") or []), set(inp.get("excluded") or []),
                        *(inp.get("keys") or []))
    if func == "_auth_store_has_provider":
        return ns[func](*(inp.get("keys") or []))
    if func == "_raw_pool_usable":
        return ns[func](inp.get("hermes_id", ""), for_picker=inp.get("for_picker", False))
    if func == "_pool_usable":
        return ns[func](inp.get("slug", ""), for_picker=inp.get("for_picker", False))
    if func == "_overlay_has_env_creds":
        overlay = types.SimpleNamespace(**(inp.get("overlay") or {}))
        return ns[func](inp.get("pid", ""), inp.get("hermes_slug", ""), overlay,
                        ns["os"].environ.get)
    if func == "_azure_entra_configured":
        return ns[func]()
    if func == "_has_fast_aws_sdk_signal":
        return ns[func]()
    if func == "_has_aws_sdk_creds_for_listing":
        return ns[func](inp.get("slug"), inp.get("current_provider"))
    if func == "_live_or_curated_ids":
        return ns[func](inp.get("slug", ""), inp.get("curated"), *(inp.get("fallback_keys") or []),
                        merge_models_dev=inp.get("merge_models_dev", True),
                        non_blocking=inp.get("non_blocking", False))
    if func == "_aws_live_or_curated_ids":
        return ns[func](inp.get("slug", ""), inp.get("curated"), *(inp.get("fallback_keys") or []),
                        non_blocking=inp.get("non_blocking", False))
    if func == "_nous_picker_model_ids":
        return ns[func](inp.get("curated"), inp.get("force_fresh_nous_tier", False))
    if func == "_free_tier_nous_row":
        return ns[func](inp.get("row"))
    if func == "_cap_models":
        return ns[func](inp.get("model_ids"), inp.get("max_models"), inp.get("slug", ""))
    if func == "_discover_endpoint_models":
        return ns[func](inp.get("api_key"), inp.get("api_url", ""),
                        inp.get("native_catalog_provider", ""), inp.get("has_explicit_models", False),
                        headers=inp.get("headers"), api_mode=inp.get("api_mode"),
                        probe_live=inp.get("probe_live", False),
                        discovery_allowed=inp.get("discovery_allowed", False),
                        fast_custom_probe=inp.get("fast_custom_probe", False))
    if func == "_finalize_picker_rows":
        results = inp.get("results")
        holder["results"] = results  # row mutations stay visible if the call raises
        ret = ns[func](results, inp.get("user_providers"), inp.get("current_model", ""))
        holder["state"] = norm(results)
        return ret
    if func == "_saved_entry_on_other_origin":
        return ns[func](inp.get("name", ""), inp.get("base_url", ""))
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
                if "results" in holder:
                    state = norm(holder["results"])
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