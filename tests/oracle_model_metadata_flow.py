#!/usr/bin/env python3
"""Oracle for the agent/model_metadata.py UNCLAIMED-remainder FLOW port
(src/cli/model_metadata_flow.c). The task brief names "hermes_cli/model_metadata.py"
but the DEFINING module in the quarry is agent/model_metadata.py.

Drives the LIVE model_metadata.py defs with SCRIPTED seams: every collaborator
(claimed siblings, foreign-module imports, clock reads, logger, file open /
json.load, callback probes) is a recording stub fed the same scripts the C
harness feeds mm_call. Compares the DECISION SEQUENCE (ordered "trace" of
{ev,args,ret} seam events), the canonical return ("ret") and the post-call
out-state ("state"). Ported functions call each other REAL (AST-extracted into
one per-case namespace): _ttl_memo_get/_ttl_memo_put -> _load_json_dict,
_parse_models_payload -> _endpoint_model_entry, fetch_codex_catalog_entries ->
_codex_catalog_urls.

Live defs are AST-extracted from the quarry (annotations kept as strings via the
__future__ flag); only the needed module constants are seeded. The heavy quarry
import chain is never imported. Every exec namespace is a registered sys.modules
module (py3.14 annotation resolution wants a real module alias). Fail-closed:
0 cases => exit 1; any harness/oracle exception per case rides as
{"__raise__": cls} (BaseException, so SystemExit/KeyboardInterrupt propagate).
Summary: 'ORACLE model_metadata_flow: N cases, M mismatches'.

Representation contract (both sides, see src/cli/model_metadata_flow.c):
- tuple/set -> array (sets sorted); None -> null; integral floats collapse to
  ints; Path -> POSIX str; uncaught exception -> {"__raise__": cls}; an exception
  passed as a seam/logger arg projects to {"__raise__": cls, "msg": str(e)}.
- scripts: absent key -> None; {"$seq": [...]} per-call queue (exhaustion raises
  RuntimeError("script exhausted: <name>")); {"__raise__": cls, "msg": m} raises
  (trace ret is the WHOLE marker); any other value reused. None-valued arg keys
  dropped on both sides.
- except Exception never catches the BaseException trio.
- fixture domain: str()/float()/int() operands are ASCII strings/numbers/bools/
  null; int() tokens allow underscores BETWEEN digits only; paths/URLs POSIX
  ASCII; memo/cache keys are opaque strings; isinstance(int) positions carry
  integral ints or bools; file content scripted at the json.load level.
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
from typing import Any, Optional, Callable

try:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    if hasattr(sys.stdin, "reconfigure"):
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "agent" / "model_metadata.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "agent" / "model_metadata.py")

NAME = "model_metadata_flow"

PORTED = [
    "_count_parts", "_lmstudio_loaded_context", "_codex_catalog_urls",
    "_ollama_show_context", "_endpoint_model_entry", "_note_if_connect_timeout",
    "_save_unless_skipped", "_probe_local_context_length",
    "save_provider_context_length", "_is_ollama_server", "_cache_file",
    "_load_json_dict", "_ttl_memo_get", "_ttl_memo_put", "_memo_local_probe",
    "_bedrock_probe_failed_recently", "_remember_no_codex_catalog",
    "_cached_codex_catalog_max", "_validate_cached_context_length",
    "fetch_codex_catalog_entries", "_codex_catalog_probe_allowed",
    "_parse_models_payload", "estimate_native_anthropic_messages_tokens_rough",
    "estimate_native_anthropic_request_tokens_rough",
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

# module constants the ported defs read (pure-literal Assign nodes)
WANTED_CONSTANTS = {
    "CODEX_MODELS_CATALOG_ENDPOINT", "CODEX_NEWEST_CLIENT_VERSION",
    "CODEX_UNGATED_CLIENT_VERSION", "_MAX_COMPLETION_KEYS", "_CONTEXT_LENGTH_KEYS",
    "_LOCAL_CTX_PROBE_TTL_SECONDS", "_BEDROCK_PROBE_FAILURE_TTL_SECONDS",
    "_CODEX_OAUTH_CONTEXT_CACHE_TTL", "_CODEX_OAUTH_CONTEXT_NEGATIVE_TTL",
}


# ── canonical projection ────────────────────────────────────────────────────
def norm(v):
    if isinstance(v, BaseException):
        return {"__raise__": type(v).__name__, "msg": str(v)}
    if isinstance(v, PurePath):
        return v.as_posix()
    if isinstance(v, (list, tuple)):
        return [norm(x) for x in v]
    if isinstance(v, (set, frozenset)):
        return sorted((norm(x) for x in v), key=lambda s: str(s))
    if isinstance(v, dict):
        return {k: norm(x) for k, x in v.items()}
    if isinstance(v, float) and v.is_integer():
        return int(v)
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
        if name not in self.io:
            return None
        script = self.io[name]
        if isinstance(script, dict) and "$seq" in script:
            seq = script["$seq"]
            i = self.cursors.get(name, 0)
            if i >= len(seq):
                return {"__raise__": "RuntimeError", "msg": f"script exhausted: {name}"}
            self.cursors[name] = i + 1
            return seq[i]
        return script


# ── seam arg projections (must match the C mm_arg key sets) ─────────────────
SEAM_ARGS = {
    "is_connect_timeout": lambda a, k: {"exc": a[0]},
    "note_endpoint_blackholed": lambda a, k: {"base_url": a[0]},
    "skip_persistent_context_cache": lambda a, k: {"base_url": a[0], "provider": a[1]},
    "save_context_length": lambda a, k: {"model": a[0], "base_url": a[1], "length": a[2], "source": k.get("source")},
    "query_local_context_length": lambda a, k: {"model": a[0], "base_url": a[1], "api_key": k.get("api_key")},
    "maybe_cache_local_context_length": lambda a, k: {"model": a[0], "base_url": a[1], "length": a[2]},
    "is_bedrock_context": lambda a, k: {"base_url": a[0], "provider": a[1]},
    "detect_local_server_type": lambda a, k: {"base_url": a[0], "api_key": k.get("api_key")},
    "get_hermes_home": lambda a, k: {},
    "atomic_json_write": lambda a, k: {"path": str(a[0]), "data": a[1], "indent": k.get("indent"), "separators": k.get("separators")},
    "probe": lambda a, k: {},
    "bound_codex_caches": lambda a, k: {"cache_key": a[0]},
    "codex_oauth_token_fingerprint": lambda a, k: {"access_token": a[0], "base_url": a[1]},
    "codex_catalog_key": lambda a, k: {"keys": a[0], "slug": a[1]},
    "model_name_suggests_stale_32k_underreport": lambda a, k: {"model": a[0]},
    "stale_pre_catalog_cache_entry": lambda a, k: {"model": a[0], "cached": a[1]},
    "invalidate_cached_context_length": lambda a, k: {"model": a[0], "base_url": a[1]},
    "infer_provider_from_url": lambda a, k: {"base_url": a[0]},
    "is_local_endpoint": lambda a, k: {"base_url": a[0]},
    "reconcile_local_cached_context_length": lambda a, k: {"model": a[0], "base_url": a[1], "cached": a[2], "api_key": k.get("api_key")},
    "get": lambda a, k: {"url": a[0]},
    "base_url_host_matches": lambda a, k: {"base_url": a[0], "domain": a[1]},
    "decode_jwt_claims": lambda a, k: {"access_token": a[0]},
    "extract_first_int": lambda a, k: {"payload": a[0], "keys": a[1]},
    "extract_pricing": lambda a, k: {"payload": a[0]},
    "native_anthropic_accounting_projection": lambda a, k: {"messages": a[0]},
    "estimate_messages_tokens_rough": lambda a, k: {"messages": a[0]},
    "estimate_request_tokens_rough": lambda a, k: {"messages": a[0], "system_prompt": k.get("system_prompt"), "tools": k.get("tools")},
    "time.time": lambda a, k: {},
    "time.monotonic": lambda a, k: {},
}


class _InertCM:
    def __enter__(self):
        return self

    def __exit__(self, *exc):
        return False


class RespDouble:
    def __init__(self, d):
        self.d = d if isinstance(d, dict) else {}

    @property
    def status_code(self):
        return self.d.get("status_code")

    def json(self):
        return self.d.get("json")


class PathDouble:
    def __init__(self, s):
        self.s = s

    def open(self, mode="r", encoding=None):
        record("open", {"path": self.s, "mode": mode, "encoding": encoding})
        return _InertCM()

    def __str__(self):
        return self.s


def materialize(name, script):
    if name == "get_hermes_home":
        return PurePosixPath(script) if isinstance(script, str) else script
    if name == "get":
        return RespDouble(script)
    if script is None:
        return None
    return script


def record(name, args):
    scripts = _SCRIPTS
    args = {k: norm(v) for k, v in args.items() if v is not None}
    script = scripts.next(name)
    if isinstance(script, dict) and isinstance(script.get("__raise__"), str):
        marker = dict(script)
        scripts.trace.append({"ev": name, "args": args, "ret": marker})
        raise make_exc(marker)
    value = materialize(name, script)
    scripts.trace.append({"ev": name, "args": args, "ret": norm(script)})
    return value


class SeamFn:
    def __init__(self, name):
        self.name = name

    def __call__(self, *a, **kw):
        return record(self.name, SEAM_ARGS[self.name](a, kw))


class LoggerDouble:
    def _log(self, level, msg, args, kw):
        d = {"msg": msg, "args": [norm(x) if not isinstance(x, BaseException) else str(x) for x in args]}
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


class JsonDouble:
    def load(self, f):
        return record("json.load", {})

    def __getattr__(self, k):
        return getattr(json, k)


_JSON_DOUBLE = JsonDouble()


def _add_model_aliases_rec(cache, model_id, entry):
    record("add_model_aliases", {"cache": cache, "model_id": model_id, "entry": entry})
    cache[model_id] = entry
    if "/" in model_id:
        cache.setdefault(model_id.split("/", 1)[1], entry)


# ── stub modules for lazy imports ───────────────────────────────────────────
def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


def install_stubs():
    _mod("hermes_constants", get_hermes_home=SeamFn("get_hermes_home"))
    hc = _mod("hermes_cli")
    hc.__path__ = []
    ac = _mod("hermes_cli.auth_constants", _decode_jwt_claims=SeamFn("decode_jwt_claims"))
    hc.auth_constants = ac
    agent = _mod("agent")
    agent.__path__ = []
    ms = _mod("agent.message_sanitization",
              native_anthropic_accounting_projection=SeamFn("native_anthropic_accounting_projection"))
    agent.message_sanitization = ms


# ── base namespace ──────────────────────────────────────────────────────────
_BASE_NS = {
    "__name__": "mm_live_model_metadata",
    "__builtins__": builtins,
    "json": _JSON_DOUBLE,
    "Any": Any, "Optional": Optional, "Callable": Callable,
    "Path": Path, "PurePosixPath": PurePosixPath,
    "MEMO_LOCK": _InertCM(),
}
_MOD = types.ModuleType("mm_live_model_metadata")
_MOD.__dict__.update(_BASE_NS)
sys.modules["mm_live_model_metadata"] = _MOD
_BASE_NS = _MOD.__dict__

# seed the pure-literal module constants from the quarry AST
_seeded = set()
for _node in _TREE.body:
    if isinstance(_node, ast.Assign):
        for _t in _node.targets:
            if isinstance(_t, ast.Name) and _t.id in WANTED_CONSTANTS:
                exec(_compile_node(_node, SRC), _BASE_NS)
                _seeded.add(_t.id)
    elif isinstance(_node, ast.AnnAssign) and isinstance(_node.target, ast.Name) \
            and _node.target.id in WANTED_CONSTANTS and _node.value is not None:
        exec(_compile_node(_node, SRC), _BASE_NS)
        _seeded.add(_node.target.id)
if _seeded != WANTED_CONSTANTS:
    print(f"ORACLE {NAME}: FATAL missing constants: {sorted(WANTED_CONSTANTS - _seeded)}", file=sys.stderr)
    sys.exit(1)

# bare-global module-top imports -> seam recorders (live name -> seam name)
_BARE_GLOBALS = {
    "atomic_json_write": "atomic_json_write",
    "base_url_host_matches": "base_url_host_matches",
    "bound_codex_caches": "bound_codex_caches",
}
_CLAIMED_SIBLINGS = {
    "_is_connect_timeout": "is_connect_timeout",
    "_note_endpoint_blackholed": "note_endpoint_blackholed",
    "_skip_persistent_context_cache": "skip_persistent_context_cache",
    "save_context_length": "save_context_length",
    "_query_local_context_length": "query_local_context_length",
    "_maybe_cache_local_context_length": "maybe_cache_local_context_length",
    "_is_bedrock_context": "is_bedrock_context",
    "detect_local_server_type": "detect_local_server_type",
    "_extract_first_int": "extract_first_int",
    "_extract_pricing": "extract_pricing",
    "_codex_oauth_token_fingerprint": "codex_oauth_token_fingerprint",
    "_codex_catalog_key": "codex_catalog_key",
    "_model_name_suggests_stale_32k_underreport": "model_name_suggests_stale_32k_underreport",
    "_stale_pre_catalog_cache_entry": "stale_pre_catalog_cache_entry",
    "_invalidate_cached_context_length": "invalidate_cached_context_length",
    "_infer_provider_from_url": "infer_provider_from_url",
    "is_local_endpoint": "is_local_endpoint",
    "_reconcile_local_cached_context_length": "reconcile_local_cached_context_length",
    "estimate_messages_tokens_rough": "estimate_messages_tokens_rough",
    "estimate_request_tokens_rough": "estimate_request_tokens_rough",
}
for _live_nm, _seam_nm in _BARE_GLOBALS.items():
    _BASE_NS[_live_nm] = SeamFn(_seam_nm)
for _live_nm, _seam_nm in _CLAIMED_SIBLINGS.items():
    _BASE_NS[_live_nm] = SeamFn(_seam_nm)
_BASE_NS["_add_model_aliases"] = _add_model_aliases_rec
install_stubs()


def build_ns(rec):
    global _SCRIPTS
    inp = rec["in"]
    state = inp.get("state") or {}
    trace = []
    scripts = Scripts(inp.get("io") or {}, trace)
    _SCRIPTS = scripts
    ns = dict(_BASE_NS)
    ns["logger"] = _LOGGER_SINGLETON
    ns["time"] = _TIME_SINGLETON
    ns["json"] = _JSON_DOUBLE
    ns["MEMO_LOCK"] = _InertCM()
    # per-case mutable state windows
    ns["_LOCAL_CTX_PROBE_CACHE"] = {k: tuple(v) for k, v in (state.get("_LOCAL_CTX_PROBE_CACHE") or {}).items()}
    ns["_BEDROCK_PROBE_FAILURE_CACHE"] = dict(state.get("_BEDROCK_PROBE_FAILURE_CACHE") or {})
    ns["_codex_oauth_context_cache"] = {k: tuple(v) for k, v in (state.get("_codex_oauth_context_cache") or {}).items()}
    ns["_codex_oauth_max_context_cache"] = {k: dict(v) for k, v in (state.get("_codex_oauth_max_context_cache") or {}).items()}
    for _nm, _code in DEF_CODE.items():
        exec(_code, ns)
    for _live_nm, _seam_nm in _CLAIMED_SIBLINGS.items():
        ns[_live_nm] = SeamFn(_seam_nm)
    for _live_nm, _seam_nm in _BARE_GLOBALS.items():
        ns[_live_nm] = SeamFn(_seam_nm)
    ns["_add_model_aliases"] = _add_model_aliases_rec
    ns["logger"] = _LOGGER_SINGLETON
    ns["time"] = _TIME_SINGLETON
    ns["json"] = _JSON_DOUBLE
    return ns, trace, scripts


# ── live driver ─────────────────────────────────────────────────────────────
def drive(rec, ns, scripts, holder):
    func = rec["func"]
    inp = rec["in"]
    if func == "_count_parts":
        return ns[func](inp.get("parts"), set(inp.get("types") or []))
    if func == "_lmstudio_loaded_context":
        return ns[func](inp.get("model"))
    if func == "_codex_catalog_urls":
        return ns[func](inp.get("base_url", ""))
    if func == "_ollama_show_context":
        return ns[func](inp.get("data"), gguf_first=inp.get("gguf_first", False), minimum=inp.get("minimum"))
    if func == "_endpoint_model_entry":
        return ns[func](inp.get("model"), inp.get("model_id", ""), inp.get("context_length"))
    if func == "_note_if_connect_timeout":
        exc = make_exc(inp["exc"]) if inp.get("exc") else None
        return ns[func](exc, inp.get("base_url"))
    if func == "_save_unless_skipped":
        return ns[func](inp.get("model"), inp.get("base_url"), inp.get("ctx"), inp.get("provider"))
    if func == "_probe_local_context_length":
        return ns[func](inp.get("model"), inp.get("base_url"), inp.get("api_key"), inp.get("provider"))
    if func == "save_provider_context_length":
        return ns[func](inp.get("model"), inp.get("base_url"), inp.get("length"), inp.get("provider", ""))
    if func == "_is_ollama_server":
        return ns[func](inp.get("base_url"), inp.get("api_key"))
    if func == "_cache_file":
        return ns[func](inp.get("name"))
    if func == "_load_json_dict":
        return ns[func](PathDouble(inp.get("path", "")))
    if func == "_ttl_memo_get":
        return ns[func](PathDouble(inp.get("path", "")), inp.get("key"), inp.get("ttl"),
                        ts_key=inp.get("ts_key", "ts"), value_key=inp.get("value_key", "value"))
    if func == "_ttl_memo_put":
        return ns[func](PathDouble(inp.get("path", "")), inp.get("key"), inp.get("value"), inp.get("ttl"),
                        ts_key=inp.get("ts_key", "ts"), value_key=inp.get("value_key", "value"),
                        what=inp.get("what", ""), ts_first=inp.get("ts_first", False))
    if func == "_memo_local_probe":
        ret = ns[func](inp.get("cache_key"), SeamFn("probe"))
        holder["state"] = {"_LOCAL_CTX_PROBE_CACHE": norm(ns["_LOCAL_CTX_PROBE_CACHE"])}
        return ret
    if func == "_bedrock_probe_failed_recently":
        ret = ns[func](inp.get("key"))
        holder["state"] = {"_BEDROCK_PROBE_FAILURE_CACHE": norm(ns["_BEDROCK_PROBE_FAILURE_CACHE"])}
        return ret
    if func == "_remember_no_codex_catalog":
        ret = ns[func](inp.get("cache_key"))
        holder["state"] = {"_codex_oauth_context_cache": norm(ns["_codex_oauth_context_cache"])}
        return ret
    if func == "_cached_codex_catalog_max":
        return ns[func](inp.get("access_token"), inp.get("base_url"), inp.get("slug"))
    if func == "_validate_cached_context_length":
        return ns[func](inp.get("model"), inp.get("base_url"), inp.get("cached"), api_key=inp.get("api_key", ""))
    if func == "fetch_codex_catalog_entries":
        return ns[func](SeamFn("get"), inp.get("base_url", ""))
    if func == "_codex_catalog_probe_allowed":
        return ns[func](inp.get("access_token"), inp.get("base_url", ""))
    if func == "_parse_models_payload":
        return ns[func](inp.get("payload"))
    if func == "estimate_native_anthropic_messages_tokens_rough":
        return ns[func](inp.get("messages"))
    if func == "estimate_native_anthropic_request_tokens_rough":
        return ns[func](inp.get("messages"), system_prompt=inp.get("system_prompt", ""), tools=inp.get("tools"))
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
                out = {"ret": {"__raise__": type(e).__name__}, "trace": trace, "state": {}}
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
                msg = spec.get("msg", "")
                # only a binding bug when the missing attribute is a LIVE function
                # that should be bound (not a builtin dict/str method like get/split)
                names = set(PORTED) | set(_CLAIMED_SIBLINGS) | set(_BARE_GLOBALS)
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
        fx = Path(__file__).resolve().parent / "fixtures" / "model_metadata_flow.jsonl"
        recs = [json.loads(l) for l in fx.read_text(encoding="utf-8").split("\n") if l.strip()]
        sys.exit(1 if selftest(recs) else 0)
    main()