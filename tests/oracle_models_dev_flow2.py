#!/usr/bin/env python3
"""Oracle for the agent/models_dev.py UNCLAIMED-remainder FLOW port
(src/agent/models_dev_flow2.c).

Drives the LIVE models_dev.py defs with SCRIPTED seams: claimed siblings and
foreign-module imports are recording stubs fed the same scripts the C harness
feeds md2_call. The 21 ported defs are AST-extracted into ONE per-case namespace
and call each other REAL, matching the C port's internal composition. Compares
the ordered "trace" of {ev,args,ret} seam events and the canonical return
("ret"). State is not echoed: mutable globals are seeded FRESH per case from
in.env (one target invocation per case).

Fail-closed: 0 cases => exit 1; any harness/oracle exception per case rides as
{"__raise__": cls}. Summary: 'ORACLE models_dev_flow2: N cases, M mismatches'.

Representation contract (both sides, see src/agent/models_dev_flow2.c):
- tuple/set -> array (sets sorted); None -> null; integral floats collapse to
  ints; PurePosixPath -> str; uncaught exception -> {"__raise__": cls}; an
  exception passed as a logger arg projects to {"__raise__": cls, "msg": str(e)}.
- scripts: absent key -> None; {"$seq": [...]} per-call queue (exhaustion raises
  RuntimeError); {"__raise__": cls, "msg": m} raises (trace ret is the WHOLE
  marker); any other value reused. None-valued seam arg keys dropped both sides.
- except Exception uses the FULL builtin hierarchy; the BaseException trio
  propagates. Scripted _NotModified raises the module's real class.
- fixture domain: ASCII str/strip operands; simple absolute POSIX paths;
  numeric coercion in int64 range; provider objects carry name/models_dev_id;
  profile objects carry model_capabilities; builtin_metadata is keyed
  "provider|model" with a lower-cased model.
"""
import __future__
import ast
import builtins
import json
import logging
import os
import sys
import types
from pathlib import Path, PurePath, PurePosixPath
from types import SimpleNamespace
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
if not (QUARRY / "agent" / "models_dev.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "agent" / "models_dev.py")

NAME = "models_dev_flow2"

PORTED = [
    "_profile_models_dev_ids", "models_dev_provider_map", "models_dev_id_for",
    "_dict_or_empty", "_configured_catalog_provider", "_models_dev_id",
    "_cfg_get", "_hermes_path", "_quietly", "_serve_stale", "_refresh_locked",
    "_registry_provider", "_registry_models", "_openrouter_catalog_lookup_base",
    "_iter_model_entries", "_extract_limit", "_builtin_model_metadata",
    "_relay_vision_marker_metadata", "_provider_model_capabilities",
    "_apply_overrides", "_entry_supports_vision",
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

# ── canonical projection ────────────────────────────────────────────────────


def norm(v):
    if isinstance(v, BaseException):
        return {"__raise__": type(v).__name__, "msg": str(v)}
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
        return {k: denorm(x) for k, x in v.items()}
    if isinstance(v, list):
        return [denorm(x) for x in v]
    return v


class _NotModified(Exception):
    """agent/models_dev.py::_NotModified — identity for `except _NotModified`."""


_NOT_MODIFIED_CLS = _NotModified


def make_exc(marker):
    cls_name = marker.get("__raise__", "RuntimeError")
    msg = marker.get("msg", "")
    if cls_name == "_NotModified":
        return _NOT_MODIFIED_CLS(msg)
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
    # foreign imports / bare globals
    "list_providers": lambda a, k: {},
    "cfg_get": lambda a, k: {"cfg": a[0], "keys": list(a[1:]), "default": k.get("default")},
    "load_config_readonly": lambda a, k: {},
    "get_hermes_home": lambda a, k: {},
    "openrouter_variant_base": lambda a, k: {"model": a[0]},
    "opencode_provider_family": lambda a, k: {"provider": a[0]},
    "get_provider_profile": lambda a, k: {"provider": a[0]},
    # claimed siblings (bare-global ns overrides)
    "fetch_models_dev": lambda a, k: ({"allow_network": k["allow_network"]}
                                       if "allow_network" in k else {}),
    "_mark_stale_cache_grace": lambda a, k: {},
    "_start_background_refresh_models_dev": lambda a, k: {},
    "_fetch_models_dev_from_network": lambda a, k: {"conditional": k["conditional"]},
    "_commit_registry": lambda a, k: {"data": a[0], "etag": k["etag"], "where": k["where"]},
    "_confirm_cache_not_modified": lambda a, k: {"where": k["where"]},
    "_note_refresh_failure": lambda a, k: {"exc": a[0], "where": k["where"]},
    "_merge_catalog_entry_with_override": lambda a, k: {"raw": a[0], "override": a[1]},
    "_override_for": lambda a, k: {"provider": a[0], "model": a[1],
                                    "catalog_hit": k["catalog_hit"], "config": k.get("config")},
    # synthetic callback (quietly) + logger
    "quietly_fn": lambda a, k: {"what": k["what"]},
    "logger.debug": lambda a, k: dict(k),
    "logger.warning": lambda a, k: dict(k),
}


def materialize(name, script):
    if script is None:
        return None
    if name == "list_providers":
        # live reads p.name / p.models_dev_id directly; defaults model absent keys
        return [SimpleNamespace(name=d.get("name", ""),
                                models_dev_id=d.get("models_dev_id")) for d in script]
    if name == "get_provider_profile":
        # live reads profile.model_capabilities directly (possibly absent)
        return SimpleNamespace(model_capabilities=script.get("model_capabilities", {}))
    if name == "get_hermes_home":
        return PurePosixPath(script)
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


_LOGGER_SINGLETON = LoggerDouble()


# ── stub modules for in-function imports ────────────────────────────────────
def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


def install_stubs():
    prov = _mod("providers",
                list_providers=SeamFn("list_providers"),
                get_provider_profile=SeamFn("get_provider_profile"))
    hc = _mod("hermes_cli")
    hc.__path__ = []
    cfg = _mod("hermes_cli.config",
               cfg_get=SeamFn("cfg_get"),
               load_config_readonly=SeamFn("load_config_readonly"))
    mdl = _mod("hermes_cli.models",
               opencode_provider_family=SeamFn("opencode_provider_family"))
    hc.config = cfg
    hc.models = mdl
    _mod("hermes_constants",
         get_hermes_home=SeamFn("get_hermes_home"),
         openrouter_variant_base=SeamFn("openrouter_variant_base"))


# ── base namespace ──────────────────────────────────────────────────────────
_BASE_NS = {
    "__name__": "md2_live_models_dev",
    "__builtins__": builtins,
    "json": json,
    "logging": logging,
    "Any": Any, "Optional": Optional,
    # module-top bare-global imports (never exec'd by AST extraction)
    "openrouter_variant_base": SeamFn("openrouter_variant_base"),
    "_NotModified": _NotModified,
}
_MOD = types.ModuleType("md2_live_models_dev")
_MOD.__dict__.update(_BASE_NS)
sys.modules["md2_live_models_dev"] = _MOD
_BASE_NS = _MOD.__dict__

# claimed siblings that live functions call by bare global name
_CLAIMED_SIBLINGS = {
    "fetch_models_dev": "fetch_models_dev",
    "_mark_stale_cache_grace": "_mark_stale_cache_grace",
    "_start_background_refresh_models_dev": "_start_background_refresh_models_dev",
    "_fetch_models_dev_from_network": "_fetch_models_dev_from_network",
    "_commit_registry": "_commit_registry",
    "_confirm_cache_not_modified": "_confirm_cache_not_modified",
    "_note_refresh_failure": "_note_refresh_failure",
    "_merge_catalog_entry_with_override": "_merge_catalog_entry_with_override",
    "_override_for": "_override_for",
}
for _live_nm, _seam_nm in _CLAIMED_SIBLINGS.items():
    _BASE_NS[_live_nm] = SeamFn(_seam_nm)
install_stubs()


def build_ns(rec):
    global _SCRIPTS, _NOT_MODIFIED_CLS
    inp = rec["in"]
    env = inp.get("env") or {}
    trace = []
    scripts = Scripts(inp.get("io") or {}, trace)
    _SCRIPTS = scripts
    ns = dict(_BASE_NS)
    ns["logger"] = _LOGGER_SINGLETON
    # module constants (seeded BEFORE def-exec: defaults read them)
    ns["PROVIDER_TO_MODELS_DEV"] = dict(env.get("provider_map") or {})
    ns["_BUILTIN_MODEL_METADATA"] = {
        tuple(key.split("|", 1)): val
        for key, val in (env.get("builtin_metadata") or {}).items()
    }
    ns["_UNKNOWN_MODEL_BASE"] = dict(env.get("unknown_model_base")
                                     or {"limit": {"context": 200000}, "tool_call": True})
    ns["_OPENROUTER_CATALOG_PROVIDERS"] = frozenset(env.get("openrouter_providers") or ["openrouter"])
    ns["_models_dev_cache"] = env.get("cache") or {}
    ns["_UNKNOWN_CATALOG_PROVIDER_WARNED"] = set(tuple(x) for x in (env.get("warned") or []))
    ns["_NotModified"] = _NotModified
    for _nm, _code in DEF_CODE.items():
        exec(_code, ns)
    # claimed siblings stay seamed (override any exec'd binding); logger re-set
    for _live_nm, _seam_nm in _CLAIMED_SIBLINGS.items():
        ns[_live_nm] = SeamFn(_seam_nm)
    ns["logger"] = _LOGGER_SINGLETON
    _NOT_MODIFIED_CLS = ns["_NotModified"]
    return ns, trace, scripts


# ── live driver ─────────────────────────────────────────────────────────────
def drive(rec, ns, scripts, holder):
    func = rec["func"]
    inp = rec["in"]
    g = inp.get
    if func in ("_profile_models_dev_ids", "models_dev_provider_map"):
        return ns[func]()
    if func == "models_dev_id_for":
        provider = g("provider")
        return ns[func](provider)
    if func == "_dict_or_empty":
        return ns[func](g("value"))
    if func == "_configured_catalog_provider":
        provider = g("provider")
        config = g("config")
        return ns[func](provider, config=config)
    if func == "_models_dev_id":
        provider = g("provider")
        config = g("config")
        return ns[func](provider, config=config)
    if func == "_cfg_get":
        keys = tuple(g("keys") or ())
        default = g("default")
        config = g("config")
        return ns[func](*keys, default=default, config=config)
    if func == "_hermes_path":
        name = g("name")
        return ns[func](name)
    if func == "_quietly":
        what = g("what")
        default = g("default")

        def fn():
            return record("quietly_fn", {"what": what})
        return ns[func](what, fn, default=default)
    if func == "_serve_stale":
        msg = g("msg", "")
        args = tuple(g("args") or ())
        return ns[func](msg, *args)
    if func == "_refresh_locked":
        where = g("where", "")
        return ns[func](where)
    if func == "_registry_provider":
        mdev = g("mdev_id", "")
        allow = g("allow_network", False)
        return ns[func](mdev, allow)
    if func == "_registry_models":
        mdev = g("mdev_id", "")
        allow = g("allow_network", False)
        return ns[func](mdev, allow_network=allow)
    if func == "_openrouter_catalog_lookup_base":
        provider = g("provider", "")
        model = g("model", "")
        return ns[func](provider, model)
    if func == "_iter_model_entries":
        models = g("models")
        model = g("model", "")
        sf = g("suffix_fallback", True)
        provider = g("provider", "")
        return list(ns[func](models, model, suffix_fallback=sf, provider=provider))
    if func == "_extract_limit":
        entry = g("entry")
        key = g("key", "")
        return ns[func](entry, key)
    if func == "_builtin_model_metadata":
        provider = g("provider", "")
        model = g("model", "")
        config = g("config")
        return ns[func](provider, model, config=config)
    if func == "_relay_vision_marker_metadata":
        provider = g("provider", "")
        model = g("model", "")
        return ns[func](provider, model)
    if func == "_provider_model_capabilities":
        provider = g("provider", "")
        model = g("model", "")
        return ns[func](provider, model)
    if func == "_apply_overrides":
        provider = g("provider", "")
        model = g("model", "")
        entry = g("entry")
        config = g("config")
        return ns[func](provider, model, entry, config=config)
    if func == "_entry_supports_vision":
        return ns[func](g("entry"))
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
                print(f"  C={json.dumps(got, ensure_ascii=False)[:900]}")
                print(f"  P={json.dumps(out, ensure_ascii=False)[:900]}")
    print(f"ORACLE {NAME}: {n} cases, {mism} mismatches")
    sys.exit(1 if (mism or n == 0) else 0)


# ── self-test ───────────────────────────────────────────────────────────────
def selftest(fixtures):
    import faulthandler
    faulthandler.dump_traceback_later(60, exit=True)
    bad = 0
    names = set(PORTED) | set(SEAM_ARGS)
    for i, rec in enumerate(fixtures):
        holder = {}
        try:
            ns, trace, scripts = build_ns(rec)
            try:
                spec = norm(drive(rec, ns, scripts, holder))
            except BaseException as e:  # noqa: BLE001
                spec = {"__raise__": type(e).__name__, "msg": str(e)}
            if isinstance(spec, dict) and spec.get("__raise__") == "AttributeError":
                msg = spec.get("msg", "")
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
        fx = _HERE.parent / "fixtures" / "models_dev_flow2.jsonl"
        recs = [json.loads(l) for l in fx.read_text(encoding="utf-8").splitlines() if l.strip()]
        sys.exit(1 if selftest(recs) else 0)
    main()
