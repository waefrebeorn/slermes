#!/usr/bin/env python3
"""Oracle for the tools/web_tools.py FLOW port (src/tools/web_tools_flow.c).

Drives the LIVE quarry defs of tools/web_tools.py + its UNCLAIMED sync leaves
in tools/web_tools_rescue.py, tools/web_tools_truncate.py and
tools/web_tools_extract.py with SCRIPTED seams: every external collaborator is
a recording stub fed the same scripts the C harness feeds wt_call. Compares the
DECISION SEQUENCE (ordered "trace" of {ev,args,ret} seam events), the canonical
return ("ret") and the post-call mutation window ("state": the debug_call_data
dict for _finish_debug, {} otherwise — echoed on RAISE too).

UNCLAIMED targets driven live (27). Ported siblings called by a target stay REAL
via AST extraction into ONE shared namespace (so cross-module calls such as
_managed_search_fallback -> _wt._managed_web_search resolve to the live def).
CLAIMED siblings are SEAMED (_load_web_config, _has_env, _ddgs_package_importable,
_list_registered_web_providers, _registered_web_provider,
_ensure_web_plugins_loaded, _get_extract_char_limit, _rescue_eligible,
_policy_blocked_result, read_selection, selection_exists, selection_error).

Live defs are AST-extracted (annotations kept as strings via the __future__
flag); selected module-level Assign constants are seeded after the def execs.
The heavy quarry import chain is never imported: in-function imports ride
per-case sys.modules stubs (every intermediate package registered). The exec
namespace is a registered sys.modules module ("tools.web_tools") so `from tools
import web_tools as _wt` and `from tools.web_tools import _load_web_config`
resolve. Fail-closed: 0 cases => exit 1; a MISSING seam script raises
RuntimeError("no script for <name>"); any harness/oracle exception per case rides
as {"__raise__": cls} (BaseException catch, so SystemExit/KeyboardInterrupt/
GeneratorExit pass through as specs, not exits).
Summary: 'ORACLE web_tools_flow: N cases, M mismatches'.

Representation contract (both sides, see src/tools/web_tools_flow.c):
- scripts: MISSING key -> RuntimeError("no script for <name>") (fail-closed);
  null -> None; {"$seq":[...]} per-call queue (exhaustion raises
  RuntimeError("script exhausted: <name>")); {"__raise__":cls,...} raises
  (trace ret is the WHOLE marker); any other value reused (a bare ARRAY is a
  LIST RESULT, not a queue). None-valued seam arg keys dropped on both sides.
- provider doubles are ProviderDouble attribute objects (method recorders via
  __getattr__); search_memo.flight_lock records its script as ret but returns an
  inert context manager. tuple returns project as lists; integral floats collapse
  to ints; uncaught exception -> {"__raise__": cls} (class only).
- _extract_error_json / _refuse_all return Python json.dumps COMPACT text.
- Targets catching `except Exception` swallow Exception subclasses but pass the
  BaseException trio through; _clamp_or_default / _extract_timeout_seconds catch
  only (TypeError, ValueError). The C port carries the FULL builtin hierarchy.
"""
import __future__
import ast
import builtins
import contextlib
import json
import logging
import sys
import types
from pathlib import Path

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
if hasattr(sys.stdin, "reconfigure"):
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "tools" / "web_tools.py").is_file():
    QUARRY = Path.home() / "hermes-agent"

NAME = "web_tools_flow"
MODFILES = {
    "web_tools": QUARRY / "tools" / "web_tools.py",
    "rescue": QUARRY / "tools" / "web_tools_rescue.py",
    "truncate": QUARRY / "tools" / "web_tools_truncate.py",
    "extract": QUARRY / "tools" / "web_tools_extract.py",
}

PORTED = [
    # web_tools.py
    "_configured_backend", "_registry_call", "_probe", "_autodetect_backend",
    "_keyless_backend", "_managed_web_search", "_xai_available", "_finish_debug",
    "_memoized_search",
    # web_tools_rescue.py
    "_ring_vendor_keyless", "_managed_search_fallback", "_rescue_search", "_rescue_extract",
    # web_tools_truncate.py
    "_clamp_char_limit", "_clamp_or_default", "_effective_char_limit", "_trim_results",
    # web_tools_extract.py
    "_disabled_plugin_error", "_no_provider_error", "_strict_selection_error",
    "_result_entry", "_extract_error_json", "_refuse_all", "_merge_in_order",
    "_resolve_extract_provider", "_extract_timeout_seconds", "_backend_failed_whole_batch",
]
WANTED = set(PORTED)
# Which module each target lives in (for extraction).
TARGET_MOD = {}
for _m in ("web_tools",):
    for _f in PORTED[:9]:
        TARGET_MOD[_f] = _m
for _f in PORTED[9:13]:
    TARGET_MOD[_f] = "rescue"
for _f in PORTED[13:17]:
    TARGET_MOD[_f] = "truncate"
for _f in PORTED[17:]:
    TARGET_MOD[_f] = "extract"

# Module-level constants to seed (exec the real Assign nodes; skip _debug ctor).
CONST_NAMES = {
    "web_tools": {"_BUILTIN_AVAILABILITY", "_LEGACY_WEB_BACKENDS", "_WEB_CHECK_SKIP"},
    "rescue": {"_RING_KEY_VARS"},
    "truncate": {"DEFAULT_EXTRACT_CHAR_LIMIT", "MAX_STORED_TEXT_CHARS",
                 "_CHAR_LIMIT_FLOOR", "_CHAR_LIMIT_CEILING", "_UNAMBIGUOUS_BINARY_KINDS"},
    "extract": {"_NO_RESULT_ERROR", "_DEFAULT_EXTRACT_TIMEOUT_S",
                "_EXTRACT_BACKENDS_HINT", "_INVALID_ITEM_ERROR"},
}

_FLAGS = __future__.annotations.compiler_flag


def _compile_node(node, src):
    return compile(ast.Module(body=[node], type_ignores=[]), src, "exec",
                   flags=_FLAGS, dont_inherit=True)


DEF_CODE = {}
CONST_NODES = []   # (module, node) in per-module source order
for _mod, _path in MODFILES.items():
    _src = str(_path)
    _text = _path.read_text(encoding="utf-8")
    _tree = ast.parse(_text)
    for _node in _tree.body:
        if isinstance(_node, ast.FunctionDef) and _node.name in WANTED:
            DEF_CODE[_node.name] = _compile_node(_node, _src)
        elif isinstance(_node, (ast.Assign, ast.AnnAssign)):
            if isinstance(_node, ast.Assign):
                _names = {t.id for t in _node.targets if isinstance(t, ast.Name)}
                # tuple-unpack targets (e.g. _CHAR_LIMIT_FLOOR, _CHAR_LIMIT_CEILING = ...)
                for _t in _node.targets:
                    if isinstance(_t, ast.Tuple):
                        _names |= {e.id for e in _t.elts if isinstance(e, ast.Name)}
            else:
                _names = {_node.target.id} if isinstance(_node.target, ast.Name) else set()
            if _names & CONST_NAMES.get(_mod, set()):
                CONST_NODES.append((_mod, _compile_node(_node, _src)))
_missing = WANTED - set(DEF_CODE)
if _missing:
    print(f"ORACLE {NAME}: FATAL missing live defs: {sorted(_missing)}", file=sys.stderr)
    sys.exit(1)

# ── scripted seams ──────────────────────────────────────────────────────────


class Scripts:
    def __init__(self, io, trace):
        self.io = io or {}
        self.trace = trace
        self.cursors = {}

    def next(self, name):
        if name not in self.io:
            return {"__raise__": "RuntimeError", "msg": "no script for " + name}
        script = self.io[name]
        if isinstance(script, dict) and "$seq" in script:
            seq = script["$seq"]
            i = self.cursors.get(name, 0)
            if i >= len(seq):
                return {"__raise__": "RuntimeError", "msg": "script exhausted: " + name}
            self.cursors[name] = i + 1
            return seq[i]
        return script


class ProviderDouble:
    """Attribute double: data fields from the dict; any other attribute is a
    provider.<name> method recorder. norm() projects only the data fields."""
    def __init__(self, data, seams):
        self._data = dict(data) if isinstance(data, dict) else {}
        self._seams = seams

    def __getattr__(self, name):
        if name in ("_data", "_seams"):
            raise AttributeError(name)
        data = self.__dict__["_data"]
        if name in data:
            return data[name]
        return self.__dict__["_seams"].recorder("provider." + name)


def norm(v):
    if isinstance(v, ProviderDouble):
        return {k: norm(x) for k, x in v._data.items()}
    if isinstance(v, types.SimpleNamespace):
        return {k: norm(x) for k, x in vars(v).items()}
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


def project(name, a, k):
    if name.startswith("provider."):
        if name == "provider.search":
            return {"query": a[0], "limit": a[1]}
        return {}
    P = {
        "get_provider": lambda a, k: {"backend": a[0]},
        "list_providers": lambda a, k: {},
        "get_active_extract_provider": lambda a, k: {},
        "get_active_search_provider": lambda a, k: {},
        "_keyless_preference": lambda a, k: {},
        "_keyless_tier_enabled": lambda a, k: {},
        "_disabled_web_plugin_for": lambda a, k: {"capability": k.get("capability")},
        "_has_env": lambda a, k: {"name": a[0]},
        "_load_web_config": lambda a, k: {},
        "_is_tool_gateway_ready": lambda a, k: {},
        "_ddgs_package_importable": lambda a, k: {},
        "_list_registered_web_providers": lambda a, k: {},
        "_ensure_web_plugins_loaded": lambda a, k: {},
        "_registered_web_provider": lambda a, k: {"name": a[0]},
        "read_selection": lambda a, k: {"section": a[0]},
        "selection_exists": lambda a, k: {"section": a[0]},
        "selection_error": lambda a, k: {"section": a[0], "selection_name": a[1], "failure": a[2]},
        "resolve_free_search_gateway": lambda a, k: {"token_reader": "peek_nous_access_token"},
        "has_xai_credentials": lambda a, k: {},
        "debug_log_call": lambda a, k: {"call_name": a[0], "debug_call_data": a[1]},
        "debug_save": lambda a, k: {},
        "tool_error": lambda a, k: {"message": a[0]},
        "search_memo.lookup": lambda a, k: {"name": a[0], "query": a[1], "limit": a[2]},
        "search_memo.flight_lock": lambda a, k: {"name": a[0], "query": a[1], "limit": a[2]},
        "search_memo.store": lambda a, k: {"name": a[0], "query": a[1], "limit": a[2], "response": a[3]},
        "bucket_limit": lambda a, k: {"limit": a[0]},
        "slice_search_response": lambda a, k: {"response": a[0], "limit": a[1]},
        "_rescue_eligible": lambda a, k: {"provider": a[0]},
        "_use_keyless_ring": lambda a, k: {},
        "get_provider_env": lambda a, k: {"var": a[0]},
        "use_keyless": lambda a, k: {"name": a[0], "key": a[1]},
        "search_with_failover": lambda a, k: {"provider_name": a[0], "query": a[1], "limit": a[2]},
        "extract_with_failover": lambda a, k: {"provider_name": a[0], "urls": a[1]},
        "_policy_blocked_result": lambda a, k: {"result": a[0]},
        "is_origin_http_error": lambda a, k: {"error": a[0]},
        "_get_extract_char_limit": lambda a, k: {},
    }
    if name in P:
        return P[name](a, k)
    return {"args": list(a)}


class Seams:
    def __init__(self, io, trace):
        self.scripts = Scripts(io, trace)
        self.trace = trace
        self.build = {
            "get_provider": lambda r: self.provider(r) if isinstance(r, dict) else r,
            "_registered_web_provider": lambda r: self.provider(r) if isinstance(r, dict) else r,
            "get_active_extract_provider": lambda r: self.provider(r) if isinstance(r, dict) else r,
            "get_active_search_provider": lambda r: self.provider(r) if isinstance(r, dict) else r,
            "_list_registered_web_providers": lambda r: [self.provider(x) if isinstance(x, dict) else x
                                                         for x in r] if isinstance(r, list) else r,
            "search_memo.flight_lock": lambda r: contextlib.nullcontext(),
        }

    def recorder(self, name):
        def rec(*a, **kw):
            args = {k: norm(v) for k, v in project(name, a, kw).items() if v is not None}
            script = self.scripts.next(name)
            if isinstance(script, dict) and isinstance(script.get("__raise__"), str):
                marker = dict(script)
                self.trace.append({"ev": name, "args": args, "ret": marker})
                raise make_exc(marker)
            self.trace.append({"ev": name, "args": args, "ret": norm(script)})
            b = self.build.get(name)
            return b(script) if b else script
        return rec

    def provider(self, data):
        return ProviderDouble(data, self)


# Claimed siblings + foreign bare globals seamed in the shared namespace.
BARE_SEAMS = [
    "_load_web_config", "_has_env", "_is_tool_gateway_ready", "_ddgs_package_importable",
    "_list_registered_web_providers", "_ensure_web_plugins_loaded", "_registered_web_provider",
    "read_selection", "selection_exists", "selection_error", "tool_error",
    "_rescue_eligible", "_policy_blocked_result", "_get_extract_char_limit",
    "check_firecrawl_api_key",
]

_logger = logging.getLogger("wt_flow_live")
_logger.addHandler(logging.NullHandler())
_logger.setLevel(logging.CRITICAL + 1)

# ── base namespace (defs exec'd once; per-case recorders override globals) ──

_MOD = types.ModuleType("tools.web_tools")
_MOD.__dict__.update({
    "__name__": "tools.web_tools",
    "__builtins__": builtins,
    "json": json,
    "logging": logging,
    "logger": _logger,
    "NOUS_MANAGED_PROVIDER": "nous",
})
sys.modules["tools.web_tools"] = _MOD
_NS = _MOD.__dict__
for _nm in PORTED:
    exec(DEF_CODE[_nm], _NS)
for _mod, _code in CONST_NODES:
    exec(_code, _NS)
# Re-seed constants after defs (some Assign values reference ported names lazily).
for _mod, _code in CONST_NODES:
    exec(_code, _NS)


def _install_module_stubs(seams):
    def mod(name, **attrs):
        m = types.ModuleType(name)
        for k, v in attrs.items():
            setattr(m, k, v)
        sys.modules[name] = m
        return m

    def pkg(name):
        if name not in sys.modules:
            m = types.ModuleType(name)
            m.__path__ = []
            sys.modules[name] = m
        return sys.modules[name]

    R = seams.recorder
    pkg("agent")
    reg = mod("agent.web_search_registry",
              get_provider=R("get_provider"), list_providers=R("list_providers"),
              get_active_extract_provider=R("get_active_extract_provider"),
              get_active_search_provider=R("get_active_search_provider"),
              _keyless_preference=R("_keyless_preference"),
              _keyless_tier_enabled=R("_keyless_tier_enabled"),
              _disabled_web_plugin_for=R("_disabled_web_plugin_for"))
    sys.modules["agent"].web_search_registry = reg
    wsp = mod("agent.web_search_provider", get_provider_env=R("get_provider_env"))
    sys.modules["agent"].web_search_provider = wsp

    pkg("plugins"); pkg("plugins.web")
    fc = mod("plugins.web.firecrawl.provider",
             _use_keyless_ring=R("_use_keyless_ring"),
             _is_tool_gateway_ready=R("_is_tool_gateway_ready"),
             check_firecrawl_api_key=R("check_firecrawl_api_key"))
    sys.modules["plugins.web"].firecrawl = pkg("plugins.web.firecrawl")
    sys.modules["plugins.web.firecrawl"].provider = fc
    km = mod("plugins.web.keyless_mcp",
             use_keyless=R("use_keyless"),
             search_with_failover=R("search_with_failover"),
             extract_with_failover=R("extract_with_failover"))
    sys.modules["plugins.web"].keyless_mcp = km
    cm = mod("plugins.web._common", is_origin_http_error=R("is_origin_http_error"))
    sys.modules["plugins.web"]._common = cm

    pkg("tools")
    sys.modules["tools"].web_tools = _MOD
    mtg = mod("tools.managed_tool_gateway",
              peek_nous_access_token=R("peek_nous_access_token"),
              resolve_free_search_gateway=R("resolve_free_search_gateway"))
    sys.modules["tools"].managed_tool_gateway = mtg
    xai = mod("tools.xai_http", has_xai_credentials=R("has_xai_credentials"))
    sys.modules["tools"].xai_http = xai
    wrc = mod("tools.web_result_cache",
              bucket_limit=R("bucket_limit"),
              search_memo=types.SimpleNamespace(
                  lookup=R("search_memo.lookup"),
                  flight_lock=R("search_memo.flight_lock"),
                  store=R("search_memo.store")),
              slice_search_response=R("slice_search_response"))
    sys.modules["tools"].web_result_cache = wrc


def build_ns(rec):
    inp = rec["in"]
    trace = []
    seams = Seams(inp.get("io") or {}, trace)
    _install_module_stubs(seams)
    for nm in BARE_SEAMS:
        _NS[nm] = seams.recorder(nm)
    _NS["_debug"] = types.SimpleNamespace(
        log_call=seams.recorder("debug_log_call"), save=seams.recorder("debug_save"))
    return _NS, trace, seams


# ── live driver ─────────────────────────────────────────────────────────────


def _prov(seams, inp, key="provider"):
    v = inp.get(key)
    return seams.provider(v) if isinstance(v, dict) else v


def drive(rec, ns, seams, holder):
    func = rec["func"]
    inp = rec["in"]
    if func == "_configured_backend":
        return ns[func](inp.get("key", "backend"))
    if func == "_registry_call":
        return ns[func](inp.get("func_name", ""), inp.get("default"), *(inp.get("args") or []))
    if func == "_probe":
        return ns[func](_prov(seams, inp), inp.get("method", "is_available"))
    if func in ("_autodetect_backend", "_keyless_backend", "_managed_web_search",
                "_xai_available", "_extract_timeout_seconds"):
        return ns[func]()
    if func == "_finish_debug":
        dcd = inp.get("debug_call_data")
        holder["win"] = ("debug_call_data", dcd)
        return ns[func](inp.get("call_name", ""), dcd, inp.get("error_msg"))
    if func == "_memoized_search":
        return ns[func](_prov(seams, inp), inp.get("query", ""), inp.get("limit", 5))
    if func == "_ring_vendor_keyless":
        return ns[func](inp.get("name", ""))
    if func == "_managed_search_fallback":
        return ns[func](_prov(seams, inp), inp.get("original_error", ""),
                        inp.get("query", ""), inp.get("limit", 5))
    if func == "_rescue_search":
        return ns[func](inp.get("provider_name", ""), inp.get("original_error", ""),
                        inp.get("query", ""), inp.get("limit", 5))
    if func == "_rescue_extract":
        return ns[func](inp.get("provider_name", ""), inp.get("urls"), inp.get("results"))
    if func in ("_clamp_char_limit", "_clamp_or_default"):
        return ns[func](inp.get("value"))
    if func == "_effective_char_limit":
        return ns[func](inp.get("char_limit"))
    if func == "_trim_results":
        return ns[func](inp.get("results"))
    if func == "_disabled_plugin_error":
        return ns[func](inp.get("capability", ""), inp.get("disabled_key", ""))
    if func == "_no_provider_error":
        return ns[func](inp.get("capability", ""), inp.get("fallback", ""))
    if func == "_strict_selection_error":
        return ns[func](inp.get("capability", ""), inp.get("backend", ""))
    if func == "_result_entry":
        return ns[func](inp.get("url", ""), inp.get("error"))
    if func in ("_extract_error_json", "_refuse_all"):
        return ns[func](inp.get("error", ""))
    if func == "_merge_in_order":
        fixed = {int(k): v for k, v in (inp.get("fixed") or {}).items()}
        return ns[func](inp.get("total", 0), fixed, inp.get("fetch_positions"),
                        inp.get("fetch_urls"), inp.get("results"))
    if func == "_resolve_extract_provider":
        return ns[func](inp.get("backend", ""))
    if func == "_backend_failed_whole_batch":
        return ns[func](inp.get("results"))
    raise KeyError(f"unknown func {func}")


def echo_state(holder):
    w = holder.get("win")
    if not w:
        return {}
    kind, obj = w
    return {kind: norm(obj)}


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
            ns, trace, seams = build_ns(rec)
            try:
                ret = drive(rec, ns, seams, holder)
                out = {"ret": norm(ret), "trace": trace, "state": echo_state(holder)}
            except BaseException as e:  # noqa: BLE001 - fail-closed per case
                out = {"ret": {"__raise__": type(e).__name__}, "trace": trace,
                       "state": echo_state(holder)}
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