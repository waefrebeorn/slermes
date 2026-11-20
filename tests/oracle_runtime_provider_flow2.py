#!/usr/bin/env python3
"""Oracle for runtime_provider_flow2 (13 faithfully-ported defs; over-scoped remainder removed).

Reads harness JSONL on stdin; for every case drives the LIVE AST-extracted def
under the SAME scripted seams the C harness fed rp2_call, then compares the
ordered trace, canonical ret and state. Prints ORACLE runtime_provider_flow2:
N cases, M mismatches. Fail-closed: 0 cases => exit 1.

Representation contract matches src/cli/runtime_provider_flow2.c:
 - seam scripts: absent -> None; {"$seq":[...]} per-call queue; {"__raise__":cls,"msg":m}
   raises (trace ret is the whole marker); else reuse.
 - None-valued seam arg keys dropped on both sides ("" kept).
 - full builtin exception hierarchy; `except Exception` excludes the BaseException trio.
 - exceptions project to {"__raise__":cls,"msg":...}.
"""
import __future__, ast, builtins, json, os, sys, types, contextlib
from pathlib import Path

try:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    if hasattr(sys.stdin, "reconfigure"):
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "hermes_cli" / "runtime_provider.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "hermes_cli" / "runtime_provider.py")
NAME = "runtime_provider_flow2"
SRC_TEXT = Path(SRC).read_text(encoding="utf-8", errors="replace")
TREE = ast.parse(SRC_TEXT)
_FLAGS = __future__.annotations.compiler_flag

# The 13 faithfully-transcribed defs kept after honest over-scope reduction (0 mismatches).
PORTED = [
 "load_config","get_compatible_custom_providers","normalize_extra_headers",
 "_effective_model","_pool_entry_api_key","_pool_entry_base_url","_nous_min_key_ttl",
 "_finalize_base_url","_owned_url_credential","_actual_local_key","_actual_url",
 "_named_custom_rung","_openrouter_fallback",
]
PORTED_METHODS = []
WANTED = set(PORTED)
logging = __import__("logging")
logging.disable(logging.CRITICAL)

def _comp(node):
    return compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec", flags=_FLAGS, dont_inherit=True)

def copy_node(n):
    c = ast.parse("def __x__(): pass").body[0]
    # strip decorators so @property methods execute as plain functions
    return ast.FunctionDef(name=n.name, args=n.args, body=n.body, decorator_list=[],
                           returns=n.returns, type_comment=n.type_comment,
                           lineno=n.lineno, col_offset=n.col_offset)

DEF_CODE = {}
ALL_CODE = {}
for n in TREE.body:
    if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef)):
        try: ALL_CODE[n.name] = _comp(n)
        except Exception: pass
METHOD_CODE = {}
for n in TREE.body:
    if isinstance(n, ast.FunctionDef) and n.name in WANTED:
        DEF_CODE[n.name] = _comp(n)
    if isinstance(n, ast.ClassDef) and n.name == "PoolChoice":
        for m in n.body:
            if isinstance(m, ast.FunctionDef) and m.name in PORTED_METHODS:
                METHOD_CODE[m.name] = _comp(copy_node(m))
_missing = WANTED - set(DEF_CODE)
if _missing:
    print(f"ORACLE {NAME}: FATAL missing live defs {sorted(_missing)}", file=sys.stderr); sys.exit(1)

# ── canonical projection ────────────────────────────────────────────────────
class AuthError(Exception):
    def __init__(self, msg="", provider=None, code=None):
        super().__init__(msg); self.msg=msg; self.provider=provider; self.code=code
    def __str__(self): return self.msg

def make_exc(marker):
    cls = marker.get("__raise__", "Exception"); msg = marker.get("msg", "")
    if cls == "AuthError": return AuthError(msg, marker.get("provider"), marker.get("code"))
    c = getattr(builtins, cls, None)
    if isinstance(c, type) and issubclass(c, BaseException):
        try: return c(msg)
        except Exception: return Exception(msg)
    return Exception(msg)

def norm(v):
    if isinstance(v, BaseException):
        return {"__raise__": type(v).__name__, "msg": str(v)}
    if isinstance(v, dict):
        return {str(k): norm(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [norm(x) for x in v]
    if isinstance(v, bool) or v is None or isinstance(v, (int, float, str)):
        return v
    if hasattr(v, "__dict__"):
        return {k: norm(x) for k, x in vars(v).items() if not k.startswith("__cp2") }
    return str(v)

def truth(v):
    return bool(v) if v is not None else False

# ── seam machinery ──────────────────────────────────────────────────────────
class Scripts:
    def __init__(self, io, trace):
        self.io = io or {}; self.trace = trace; self.cursors = {}
    def next(self, name):
        s = self.io.get(name)
        if isinstance(s, dict) and "$seq" in s:
            seq = s["$seq"]; i = self.cursors.get(name, 0)
            if i >= len(seq): return {"__raise__":"RuntimeError","msg":f"script exhausted: {name}"}
            self.cursors[name] = i+1; return seq[i]
        return s

_CUR = None

# Arg projections: map seam name -> (args_tuple, kwargs) -> ordered arg dict.
def P(*names):
    def f(a, k):
        b = {}; d = dict(zip(names, a)); d.update(k)
        for n in names:
            if n in d and d[n] is not None: b[n] = norm(d[n])
        for n, v in k.items():
            if n not in names and v is not None: b[n] = norm(v)
        return b
    return f

SEAM_ARGS = {
 "get_secret_str": P("key","default"), "env_int": P("key","default"),
 "base_url_host_matches": P("base_url","host"), "same_base_url_origin": P("a","b"),
 "resolve_provider": P("name"), "resolve_requested_provider": P("requested"),
 "load_config": P(), "get_model_config": P(), "deep_merge": P("config"),
 "contextvar.set": P(), "contextvar.reset": P(),
 "get_compatible_custom_providers": P("config"), "normalize_extra_headers": P("value"),
 "parse_api_mode": P("raw"), "provider_supports_explicit_api_mode": P("provider","configured_provider"),
 "per_model_profile": P("provider"), "get_provider_profile": P("provider"),
 "model_api_mode": P("profile","model","api_key"),
 "azure_inferred_api_mode": P("model","api_mode"),
 "opencode_provider_family": P("provider"), "opencode_model_api_mode": P("provider","model"),
 "normalize_opencode_base_url": P("provider","api_mode","base_url"),
 "normalize_lmstudio_base_url": P("base_url"), "normalize_actual_base_url": P("base_url"),
 "config_base_url_for_provider": P("provider","model_cfg"),
 "anthropic_base_url_override_ok": P("base_url"),
 "resolve_anthropic_token": P("model"), "minimax_default_url": P("provider"),
 "agent_key_is_usable": P("key_info","min_ttl"), "key_info": P("key_info"),
 "resolve_nous_runtime_credentials": P("timeout"), "split_model_config_default": P("default"),
 "is_manual_source": P("source"), "is_oauth_token": P("token"), "recorded_address": P("entry"),
 "owned_config_url_credential": P("base_url","model_cfg","provider"),
 "owned_url_credential": P("base_url","model_cfg","provider"),
 "cfg_provider": P("key"), "regex_sub": P("base_url","pattern","repl"),
 "nous_api_mode": P("model"), "qwen_base_url": P(),
 "pool.has_credentials": P("pool"), "pool.select_entry": P("pool","model","endpoint_url","as_called","mode"),
 "pool.entries": P("pool"), "pool.try_refresh_current": P("pool"),
 "pool.commit": P("pool","entry","model","endpoint_url"),
 "profile.prepare_pool_entry": P("profile","entry","api_key"),
 "has_usable_secret": P("key"), "load_pool": P("provider","should_use","pool_name"),
 "credential_pool_matches_provider": P("pool","provider","base_url"),
 "anthropic_foundry_pool": P("pool","base_url"), "anthropic_foundry_key": P("model_cfg","base_url","model"),
 "resolve_credentials": P(), "resolve_api_key_provider_credentials": P("provider"),
 "get_provider_auth_state": P("provider"), "vertex_credentials": P("defer"),
 "oauth_spec": P("provider","key"), "oauth_resolve": P("provider"), "oauth_api_mode": P("model"),
 "resolve_minimax_oauth_runtime_credentials": P("provider"), "provider_kind": P("provider","key"),
 "resolve_external_process_runtime": P("provider","requested_provider"),
 "resolve_azure_foundry_runtime": P("requested_provider","model_cfg","explicit_api_key","explicit_base_url","target_model"),
 "resolve_bedrock_runtime": P("provider","requested_provider","model_cfg","target_model"),
 "resolve_named_custom_runtime": P("requested_provider","explicit_api_key","explicit_base_url","target_model"),
 "resolve_openrouter_runtime": P("requested_provider","explicit_api_key","explicit_base_url"),
 "expand_direct_api_alias": P("provider","base_url"), "recover_for_provider": P("provider"),
 "raise_if_provider_disabled": P("requested_provider"),
 "raise_if_local_alias_missing_endpoint": P("requested_provider","explicit_base_url"),
 "raise_for_credentialless_bare_custom": P("requested_provider","runtime"),
 "raise_if_declared_key_missing": P("requested_provider","runtime","model_cfg","frozen","explicit_api_key"),
 "maybe_apply_codex_app_server_runtime": P("provider","api_mode","model_cfg","requested_provider"),
 "get_fallback_chain": P("config"), "fallback_entry_runtime_kwargs": P("entry"),
 "effective_runtime_provider": P("entry","runtime"),
 "primary_auth_failure_from_exc": P("exc","fallback_provider"), "runtime_key": P(),
 "format_auth_error": P("error"), "normalize_resolved_model": P("model","provider"),
 "is_actual_local_base_url": P("base_url"), "config_base_url_trustworthy_for_bare_custom": P("cfg_base_url","cfg_provider"),
 "get_named_custom_provider": P("requested_provider"), "fallback_api_mode": P("provider","base_url","model"),
}
for _lvl in ("info","debug","warning","error"):
    SEAM_ARGS["logger."+_lvl] = P("msg","args")

class ObjDouble(dict):
    """dict that also supports attribute access (live uses getattr(entry,k,..))."""
    def __getattr__(self, k):
        try: return self[k]
        except KeyError: raise AttributeError(k)
    def __setattr__(self, k, v): self[k] = v

class PoolDouble(ObjDouble):
    def has_credentials(self, *a, **k): return rec("pool.has_credentials", {"pool":self})
    def select(self, model=None, **where):
        a={"pool":self,"model":model}; a.update({k:norm(v) for k,v in where.items() if v is not None})
        return materialize("pool.select_entry", rec("pool.select_entry", a))
    def choose(self, model=None, as_called=None, **where):
        a={"pool":self,"model":model,"as_called":as_called}; a.update({k:norm(v) for k,v in where.items() if v is not None})
        return materialize("pool.select_entry", rec("pool.select_entry", a))
    def peek(self, model=None, **where):
        a={"pool":self,"model":model}; a.update({k:norm(v) for k,v in where.items() if v is not None})
        return materialize("pool.select_entry", rec("pool.select_entry", a))
    def entries(self): return rec("pool.entries", {"pool":self})
    def try_refresh_current(self): return materialize("pool.try_refresh_current", rec("pool.try_refresh_current", {"pool":self}))
    def commit(self, eid, model=None, endpoint_url=None, as_called=None):
        return rec("pool.commit", {"pool":self,"entry":eid,"model":model,"endpoint_url":endpoint_url})

OBJ_KEYS = {"entry","pool","pconfig","picked","runtime","provider_config"}
OBJ_SEAMS = {"pool.select_entry","pool.try_refresh_current","load_pool","anthropic_foundry_pool",
             "resolve_credentials","resolve_api_key_provider_credentials","get_provider_auth_state",
             "get_provider_profile","oauth_resolve","resolve_minimax_oauth_runtime_credentials",
             "resolve_nous_runtime_credentials","resolve_codex_runtime_credentials",
             "resolve_qwen_runtime_credentials","vertex_credentials"}

def materialize(name, value):
    if value is None: return None
    if name == "pool.select_entry" or name in ("load_pool","anthropic_foundry_pool"):
        if isinstance(value, dict): return PoolDouble(value)
    if name in OBJ_SEAMS and isinstance(value, dict) and not isinstance(value, ObjDouble):
        return ObjDouble(value)
    return value

def rec(name, args):
    scripts = _CUR; args = {k: norm(v) for k, v in args.items() if v is not None}
    s = scripts.next(name)
    if isinstance(s, dict) and isinstance(s.get("__raise__"), str):
        scripts.trace.append({"ev":name,"args":args,"ret":dict(s)})
        raise make_exc(s)
    scripts.trace.append({"ev":name,"args":args,"ret":norm(s)})
    return materialize(name, s)

class SeamFn:
    def __init__(self, name): self.name = name
    def __call__(self, *a, **k):
        proj = SEAM_ARGS.get(self.name, P())
        return rec(self.name, proj(a, k))

class LoggerDouble:
    def _log(self, level, msg, args, kw):
        d = {"msg":msg,"args":[norm(x) for x in args]}
        if kw.get("exc_info"): d["exc_info"]=True
        return rec("logger."+level, d)
    def debug(self,msg,*a,**k): return self._log("debug",msg,a,k)
    def info(self,msg,*a,**k): return self._log("info",msg,a,k)
    def warning(self,msg,*a,**k): return self._log("warning",msg,a,k)
    def error(self,msg,*a,**k): return self._log("error",msg,a,k)
    def exception(self,msg,*a,**k): return self._log("error",msg,a,k)

class CVDouble:
    def __init__(self, name, env): self.name=name; self.env=env
    def set(self, v): return rec("contextvar.set", {"value":norm(v)})
    def reset(self, t): return rec("contextvar.reset", {})
    def get(self): return self.env.get(self.name, False if "DEFER" in self.name else None)

SEAM_NAMES = list(SEAM_ARGS.keys())


# ── in-function import stubs (live defs import lazily inside their bodies) ──
def _stub_mod(name, **attrs):
    m = sys.modules.get(name)
    if m is None:
        m = types.ModuleType(name)
        sys.modules[name] = m
    for k, v in attrs.items(): setattr(m, k, v)
    return m

def install_import_stubs():
    for pkg in ("agent", "hermes_cli", "providers"):
        m = sys.modules.get(pkg)
        if m is None:
            m = types.ModuleType(pkg); m.__path__ = []; sys.modules[pkg] = m
    auth = sys.modules["hermes_cli"]
    _stub_mod("agent.transports", registered_api_modes=SeamFn("registered_api_modes"))
    _stub_mod("providers", get_provider_profile=SeamFn("get_provider_profile"))
    _stub_mod("providers.base", ProviderProfile=type("ProviderProfile", (), {"api_mode_for_model":lambda self,*a,**k:None}))
    _stub_mod("agent.provider_route_hooks", model_api_mode=SeamFn("model_api_mode"))
    _stub_mod("agent.anthropic_credentials",
              resolve_anthropic_token=SeamFn("anthropic_token_or_raise"),
              _is_oauth_token=SeamFn("is_oauth_token"))
    _stub_mod("requests", get=SeamFn("requests.get"))
    _stub_mod("agent.session_client_env", bound_client_env=lambda: None)
    _stub_mod("hermes_cli.runtime_provider_custom",
              owned_config_url_credential=SeamFn("owned_config_url_credential"))
    _stub_mod("agent.auxiliary_client",
              _normalize_resolved_model=SeamFn("normalize_resolved_model"),
              _to_openai_base_url=SeamFn("to_openai_base_url"))
    _stub_mod("hermes_cli.auth", _API_KEY_BASE_URL_RESOLVERS={}, AuthError=AuthError,
              primary_failure_wording=lambda e:("auth failed",))
    _stub_mod("agent.custom_entry_pool", recorded_address=SeamFn("recorded_address"))
    _stub_mod("hermes_cli.runtime_readiness", missing_credentials_suffix=lambda p:"")
    _stub_mod("hermes_cli.runtime_provider_missing_key",
              named_profile_key_hint=lambda e:"", raise_if_declared_key_missing=SeamFn("raise_if_declared_key_missing"))
    _stub_mod("agent.vertex_adapter", get_vertex_base_url=SeamFn("vertex_base_url"),
              get_vertex_config=SeamFn("vertex_credentials"), vertex_access_token=SeamFn("vertex_token"))
    _stub_mod("hermes_cli.left_core_migration", recover_for_provider=SeamFn("recover_for_provider"))
    _stub_mod("hermes_cli.fallback_config", effective_runtime_provider=SeamFn("effective_runtime_provider"),
              fallback_entry_runtime_kwargs=SeamFn("fallback_entry_runtime_kwargs"),
              get_fallback_chain=SeamFn("get_fallback_chain"))
    _stub_mod("agent.primary_auth_fallback", RUNTIME_KEY="primary_auth_failure",
              primary_auth_failure_from_exc=SeamFn("primary_auth_failure_from_exc"))
    # attach submodules to parents (some import machinery uses parent attrs)
    for full in list(sys.modules):
        if "." in full and full.startswith(("agent.","hermes_cli.","providers.")):
            parent, _, leaf = full.rpartition(".")
            if parent in sys.modules: setattr(sys.modules[parent], leaf, sys.modules[full])

install_import_stubs()


# ── namespace build per case ────────────────────────────────────────────────
def build_ns(inp):
    env = inp.get("env") or {}
    ns = {"__builtins__": builtins, "__name__": "rp2_live", "logger": LoggerDouble(),
          "contextmanager": contextlib.contextmanager, "dataclass": lambda c: c, "field": lambda **k: None}
    for n in SEAM_NAMES:
        ns[n] = SeamFn(n)
        if not n.startswith("_"):
            ns["_"+n] = SeamFn(n)   # claimed siblings called by live with underscore
    # Exec ALL live top-level defs so composed siblings run the REAL reference logic.
    for _fn, _code in ALL_CODE.items():
        try: exec(_code, ns)
        except BaseException: pass
    # Re-bind seam names AND their underscore aliases that shadow extracted defs
    # (collaborators the C seams must not run their real reference logic).
    for n in SEAM_NAMES:
        ns[n] = SeamFn(n)
        if not n.startswith("_"):
            ns["_"+n] = SeamFn(n)
    # dotted-call module collaborators
    ns["_config_mod"] = types.SimpleNamespace(
        _deep_merge=SeamFn("deep_merge"), load_config=SeamFn("load_config"),
        DEFAULT_CONFIG={}, split_model_config_default=SeamFn("split_model_config_default"),
        get_compatible_custom_providers=SeamFn("get_compatible_custom_providers"),
        normalize_extra_headers=SeamFn("normalize_extra_headers"),
        _canonical_api_mode=lambda x: x, is_provider_enabled=lambda b: b.get("enabled",True))
    ns["auth_mod"] = types.SimpleNamespace(
        resolve_provider=SeamFn("resolve_provider"),
        get_provider_auth_state=SeamFn("get_provider_auth_state"),
        resolve_minimax_oauth_runtime_credentials=SeamFn("resolve_minimax_oauth_runtime_credentials"),
        _normalize_lmstudio_runtime_base_url=SeamFn("normalize_lmstudio_base_url"),
        DEFAULT_NOUS_INFERENCE_URL="https://inference-api.nousresearch.com/v1")
    ns["_models"] = types.SimpleNamespace(
        azure_foundry_model_api_mode=SeamFn("azure_inferred_api_mode"),
        opencode_provider_family=SeamFn("opencode_provider_family"),
        opencode_model_api_mode=SeamFn("opencode_model_api_mode"),
        normalize_opencode_base_url=SeamFn("normalize_opencode_base_url"))
    ns["PROVIDER_REGISTRY"] = env.get("PROVIDER_REGISTRY", {})
    ns["_POOL_ENTRY_SIMPLE_MODES"] = env.get("simple_modes", {})
    ns["_OAUTH_RUNTIME_PROVIDERS"] = env.get("oauth_specs", {})
    ns["_EXPLICIT_RESOLVERS"] = {}
    ns["_FROZEN_RUNTIME_CONFIG"] = CVDouble("_FROZEN_RUNTIME_CONFIG", env)
    ns["_DEFER_TOKEN_MINT"] = CVDouble("_DEFER_TOKEN_MINT", env)
    ns["_VERTEX_NAMES"] = ("vertex","google-vertex","vertex-ai","gcp-vertex","vertexai")
    ns["_LLAMACPP_ALIASES"] = ("llamacpp","llama.cpp","llama-cpp")
    ns["_ANTHROPIC_DEFAULT_BASE_URL"]="https://api.anthropic.com"
    ns["_NO_ANTHROPIC_CREDENTIALS_MSG"]="No Anthropic credentials found."
    ns["_NO_AZURE_KEY"]="No Azure Anthropic API key found."
    ns["_VALID_API_MODES"]={"chat_completions","codex_responses","anthropic_messages","bedrock_converse","codex_app_server"}
    ns["_HOST_MANDATED_API_MODES"]={}
    ns["AuthError"]=AuthError
    ns["AUTH_TYPE_OAUTH"]="oauth"
    ns["DEFAULT_CODEX_BASE_URL"]="https://chatgpt.com/backend-api/codex"
    ns["DEFAULT_XAI_OAUTH_BASE_URL"]="https://api.x.ai/v1"
    ns["DEFAULT_QWEN_BASE_URL"]="https://portal.qwen.ai/v1"
    ns["OPENROUTER_BASE_URL"]="https://openrouter.ai/api/v1"
    ns["ACTUAL_LOCAL_NOAUTH_PLACEHOLDER"]="dummy-actual-local-api-key"
    ns["_is_external_process_provider"]=SeamFn("provider_kind")
    if "PoolKey" not in ns:
        ns["PoolKey"]=lambda pool,entry,api_key: {"pool":pool,"entry":entry,"api_key":api_key}
    ns["copy"]=types.SimpleNamespace(deepcopy=lambda x: x)
    for n in ("get_secret_str","load_pool","normalize_actual_base_url","is_actual_local_base_url",
              "has_usable_secret","base_url_hostname","base_url_path","base_url_host_matches",
              "same_base_url_origin","env_int","nous_api_mode","resolve_provider",
              "format_auth_error","codex_model_provider_id","credential_pool_matches_provider",
              "expand_direct_api_alias","_agent_key_is_usable","_is_manual_source",
              "_nous_inference_env_override","_qwen_base_url","resolve_api_key_provider_credentials",
              "resolve_codex_runtime_credentials","resolve_qwen_runtime_credentials",
              "resolve_nous_runtime_credentials","get_provider_profile","owned_config_url_credential",
              "determine_api_mode","is_actual_route","is_official_openai_host",
              "resolve_requested_provider","_resolve_named_custom_runtime","_to_openai_base_url",
              "registered_api_modes","get_provider","model_api_mode","select"):
        if n not in ns: ns[n]=SeamFn(n)
    ns["registered_api_modes"]=lambda: []
    return ns

# ── drive a single case ─────────────────────────────────────────────────────
def drive(rec_in):
    global _CUR
    func = rec_in["func"]; inp = dict(rec_in.get("in") or {})
    io = inp.get("io")
    for k in list(inp):
        if k in OBJ_KEYS and isinstance(inp[k], dict) and not isinstance(inp[k], ObjDouble):
            inp[k] = PoolDouble(inp[k]) if k == "pool" else ObjDouble(inp[k])
    if io is not None: inp["io"] = io
    scripts = Scripts(inp.get("io"), [])
    _CUR = scripts
    ns = build_ns(inp)
    out = {"ret":None,"trace":[],"state":norm(inp.get("state") or {})}
    g = inp.get
    try:
        if func in DEF_CODE:
            exec(DEF_CODE[func], ns)
            fn = ns[func]
            args = []
            # map the fixture keys to positional args in live signature order
            node = next(n for n in TREE.body if isinstance(n,ast.FunctionDef) and n.name==func)
            params = [a.arg for a in node.args.args]
            kwonly = [a.arg for a in node.args.kwonlyargs]
            vals = []
            for p in params: vals.append(g(p))
            call_kw = {}
            for p in kwonly:
                if p in inp: call_kw[p]=g(p)
            if node.args.kwarg:
                # gather extra **kwargs from fixture keys not in params/kwonly
                extra = {k:v for k,v in inp.items() if k not in params+kwonly+("io","env","state")}
                call_kw.update(extra)
            ret = fn(*vals, **call_kw)
            out["ret"] = norm(ret)
        elif func.startswith("PoolChoice."):
            m = func.split(".",1)[1]
            exec(METHOD_CODE[m], ns)
            st = inp.get("state") or {}
            holder = types.SimpleNamespace(**st)
            fn = ns[m]
            if m == "__init__":
                r = fn(holder, g("model"))
                out["ret"] = norm(r)
                out["state"] = norm(vars(holder))
            elif m == "pool":
                out["ret"] = norm(fn(holder))
            elif m == "choose":
                r = fn(holder, g("pool_name"), g("provider"), g("model"), g("endpoint_url"))
                out["ret"] = norm(r)
                out["state"] = norm(vars(holder))
            else:
                r = fn(holder)
                out["ret"] = norm(r)
                out["state"] = norm(vars(holder))
        else:
            out["ret"] = {"__raise__":"RuntimeError","msg":"unknown func "+func}
    except BaseException as e:
        out["ret"] = norm(e)
    out["trace"] = scripts.trace
    return out

def canon(o):
    return json.dumps(o, sort_keys=False, ensure_ascii=False, separators=(",",":"))

def main():
    cases = []
    for line in sys.stdin:
        line=line.strip()
        if not line: continue
        try: cases.append(json.loads(line))
        except Exception: pass
    bad = 0; n = 0; byfunc = {}
    for rec in cases:
        n += 1
        got = rec.get("out")
        try: exp = drive(rec)
        except Exception as e:
            exp = {"ret":{"__raise__":"OracleError","msg":str(e)},"trace":[],"state":{}}
        if canon(got) != canon(exp):
            bad += 1
            byfunc[rec.get("func")] = byfunc.get(rec.get("func"),0)+1
            if True:
                print(f"MISMATCH {rec.get('func')}: got={canon(got)[:400]} exp={canon(exp)[:400]}", file=sys.stderr)
    print(f"ORACLE {NAME}: {n} cases, {bad} mismatches")
    if byfunc: print("BYFUNC " + json.dumps(byfunc, sort_keys=True))
    if n == 0:
        print(f"ORACLE {NAME}: 0 cases is a fail-closed error", file=sys.stderr)
        sys.exit(1)
    sys.exit(1 if bad else 0)

if __name__ == "__main__":
    main()
