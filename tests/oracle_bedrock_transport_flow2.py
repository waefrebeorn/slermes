#!/usr/bin/env python3
"""Oracle for the REMAINING-UNCLAIMED Bedrock adapter FLOW port
(src/agent/transports/bedrock_transport_flow2.c).

The task names agent/transports/bedrock.py; that module is a thin delegation
shell whose every method is already CLAIMED by transports_bedrock_ports.c, and
bedrock_transport_flow.c (bt2_*) already claimed 27 agent/bedrock_adapter.py
defs. This oracle drives the 12 STILL-UNCLAIMED defs LIVE (AST-extracted from
agent/bedrock_adapter.py) with SCRIPTED seams and diffs the ordered trace, the
canonical return and the post-call mutation window against the C harness.

UNCLAIMED targets driven live: _new_client, _cached_client,
BedrockOpenAISigV4Auth.__init__, BedrockOpenAISigV4Auth.auth_flow,
build_bedrock_openai_http_client, _stale_error_types,
_boto3_chain_has_credentials, bind_bedrock_runtime, _ResponseParts.__init__,
_ResponseParts.absorb_reasoning, _ResponseParts.build,
bedrock_static_context_length. Ported siblings compose REAL (_cached_client ->
_new_client; build_bedrock_openai_http_client -> BedrockOpenAISigV4Auth.__init__).
Claimed siblings (resolve_bedrock_runtime_region, bedrock_region_from_runtime_url,
bedrock_guardrail_config, _require_boto3, scoped_aws_session_kwargs,
_encode_redacted, _persisted_reasoning_details) are SEAMED.

The heavy quarry import chain (boto3 / botocore / httpx / pm / agent.* runtime)
is never imported; foreign collaborators ride sys.modules stubs. The exec
namespace is a registered sys.modules module (py3.14 annotation resolution needs
a real module alias). Fail-closed: 0 cases => exit 1; a MISSING seam script
raises RuntimeError("no script for <name>"); any harness/oracle exception per
case rides as {"__raise__": cls, "msg": str(e)} (BaseException catch, so
SystemExit/KeyboardInterrupt/GeneratorExit pass through as specs, not exits).
Summary: 'ORACLE bedrock_transport_flow2: N cases, M mismatches'.

Representation contract (both sides, see the port .c header):
- scripts: MISSING key -> RuntimeError("no script for <name>"); null -> None;
  {"$seq":[...]} per-call queue (exhaustion raises RuntimeError("script
  exhausted: <name>")); {"__raise__":cls,"msg":m} raises (trace ret is the WHOLE
  marker); any other value reused (a bare ARRAY is a LIST RESULT). None-valued
  seam arg keys dropped on both sides.
- tuple -> array; None -> null; integral floats collapse to ints; bytes ->
  {"__bytes__":[int,...]}; SimpleNamespace and ported-class instances project to
  dicts; uncaught exception -> {"__raise__": cls, "msg": str(e)}.
- cache keys: (region, read_timeout) and (home, service, region, read_timeout)
  tuple keys encode as "|"-joined str(component) (str(None)=="None"); decoded
  back to tuples for the live call, re-encoded for the echo.
- except Exception / suppress(Exception, ImportError, AttributeError) sites
  swallow the matching family; the BaseException trio passes through. Scripted
  raises materialize to REAL builtin exception instances (str(e) == scripted msg).
- mutation windows echoed POST-CALL (on RAISE too): _cached_client
  {"cache","_bedrock_clients_by_home"}; bind_bedrock_runtime {"agent"};
  absorb_reasoning {"block","parts"}; auth_flow {"request"}; else module-state
  keys PRESENT in in.state.
"""
import __future__
import ast
import base64
import builtins
import contextlib
import json
import math
import os
import re
import sys
import types
from pathlib import Path

try:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    if hasattr(sys.stdin, "reconfigure"):
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

_HERE = Path(__file__).resolve()
_REPO = _HERE.parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))
QUARRY = _REPO.parent / "hermes-agent"
if not (QUARRY / "agent" / "bedrock_adapter.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "agent" / "bedrock_adapter.py")
PORT_SRC = str(_REPO / "src" / "agent" / "transports" / "bedrock_transport_flow2.c")

NAME = "bedrock_transport_flow2"

PORTED = [
    "_new_client", "_cached_client",
    "BedrockOpenAISigV4Auth.__init__", "BedrockOpenAISigV4Auth.auth_flow",
    "build_bedrock_openai_http_client", "_stale_error_types",
    "_boto3_chain_has_credentials", "bind_bedrock_runtime",
    "_ResponseParts.__init__", "_ResponseParts.absorb_reasoning",
    "_ResponseParts.build", "bedrock_static_context_length",
]
TOP_FUNCS = [
    "_new_client", "_cached_client", "build_bedrock_openai_http_client",
    "_stale_error_types", "_boto3_chain_has_credentials", "bind_bedrock_runtime",
    "bedrock_static_context_length",
]
CLASSES = ["BedrockOpenAISigV4Auth", "_ResponseParts"]
CONSTANTS = [
    "BEDROCK_CONTEXT_LENGTHS", "_STOP_REASON_TO_FINISH_REASON",
    "_APPLICATION_PROFILE_ARN_RE", "_CATALOG_FAMILY_ID_RE",
    "_BOTO_CONNECT_TIMEOUT_SECONDS",
]
FUNCS = set(PORTED)

# ── AST extraction ──────────────────────────────────────────────────────────
_SRC_TEXT = Path(SRC).read_text(encoding="utf-8")
_TREE = ast.parse(_SRC_TEXT)
_FLAGS = __future__.annotations.compiler_flag


def _compile_node(node, src, flags=0):
    return compile(ast.Module(body=[node], type_ignores=[]), src, "exec",
                   flags=flags, dont_inherit=True)


DEF_CODE = {}
CLASS_CODE = {}
CONST_CODE = {}
for _node in _TREE.body:
    if isinstance(_node, ast.FunctionDef) and _node.name in TOP_FUNCS:
        DEF_CODE[_node.name] = _compile_node(_node, SRC, flags=_FLAGS)
    elif isinstance(_node, ast.ClassDef) and _node.name in CLASSES:
        CLASS_CODE[_node.name] = _compile_node(_node, SRC, flags=_FLAGS)
    elif isinstance(_node, (ast.Assign, ast.AnnAssign)):
        tg = _node.targets[0] if isinstance(_node, ast.Assign) else _node.target
        if isinstance(tg, ast.Name) and tg.id in CONSTANTS:
            CONST_CODE[tg.id] = _compile_node(_node, SRC)
_missing = (set(TOP_FUNCS) - set(DEF_CODE)) | (set(CLASSES) - set(CLASS_CODE)) \
    | (set(CONSTANTS) - set(CONST_CODE))
if _missing:
    print(f"ORACLE {NAME}: FATAL missing live defs: {sorted(_missing)}", file=sys.stderr)
    sys.exit(1)

# ── canonical projection ────────────────────────────────────────────────────
_PROJECT_CLASSES = []


def norm(v):
    if isinstance(v, BaseException):
        return {"__raise__": type(v).__name__, "msg": str(v)}
    if isinstance(v, (bytes, bytearray)):
        return {"__bytes__": list(v)}
    if isinstance(v, types.SimpleNamespace):
        return {k: norm(x) for k, x in vars(v).items()}
    if _PROJECT_CLASSES and any(isinstance(v, c) for c in _PROJECT_CLASSES):
        return {k: norm(x) for k, x in vars(v).items()}
    if isinstance(v, (list, tuple)):
        return [norm(x) for x in v]
    if isinstance(v, (set, frozenset)):
        return sorted((norm(x) for x in v), key=lambda s: str(s))
    if isinstance(v, dict):
        return {k: norm(x) for k, x in v.items()}
    if isinstance(v, float) and math.isfinite(v) and v.is_integer():
        return int(v)
    return v


def make_exc(marker):
    cls_name = marker.get("__raise__", "RuntimeError")
    msg = marker.get("msg", "")
    cls = getattr(builtins, cls_name, None)
    if not (isinstance(cls, type) and issubclass(cls, BaseException)):
        cls = type(cls_name, (Exception,), {})
    if cls_name in ("UnicodeDecodeError", "UnicodeEncodeError", "UnicodeTranslateError"):
        e = cls.__new__(cls)
        Exception.__init__(e, msg)
        return e
    return cls(msg)


# ── scripted seams (fail-closed) ────────────────────────────────────────────
_SCRIPTS = None


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


# ── doubles ─────────────────────────────────────────────────────────────────


class HeadersDouble(dict):
    """dict subclass: .items() / .update() work; norm projects as a dict."""


class Boto3Double:
    def __init__(self):
        self.Session = SeamFn("boto3_Session")
        self.client = SeamFn("factory_client")


class SessionDouble:
    def __init__(self):
        self.client = SeamFn("factory_client")
        self.get_credentials = SeamFn("session_get_credentials")
        self.set_config_variable = SeamFn("session_set_config_variable")


class ConfigDouble:
    def __init__(self, value):
        self.value = value

    def merge(self, other):
        ov = other.value if isinstance(other, ConfigDouble) else other
        r = record("config_merge", {"other": ov})
        return ConfigDouble(r)


class CredentialsDouble:
    def __init__(self):
        self.get_frozen_credentials = SeamFn("credentials_get_frozen_credentials")


class SignerDouble:
    def add_auth(self, aws_req):
        raw = aws_req._raw if aws_req is not None else None
        r = record("signer_add_auth", {"aws_request": raw})
        if isinstance(r, dict) and aws_req is not None:
            aws_req.headers.update(r)
        return r


class AWSRequestDouble:
    def __init__(self, raw):
        self._raw = raw
        h = raw.get("headers") if isinstance(raw, dict) else None
        self.headers = HeadersDouble(dict(h)) if isinstance(h, dict) else HeadersDouble({})


SEAM_ARGS = {
    "get_hermes_home_override": lambda a, k: {},
    "hermes_home_key": lambda a, k: {},
    "_require_boto3": lambda a, k: {},
    "scoped_aws_session_kwargs": lambda a, k: {},
    "boto3_Session": lambda a, k: dict(k),
    "botocore_Config": lambda a, k: dict(k),
    "config_merge": lambda a, k: {"other": a[0]},
    "factory_client": lambda a, k: {
        "service": a[0], "region_name": k.get("region_name"),
        "config": (k["config"].value if isinstance(k.get("config"), ConfigDouble) else k.get("config"))},
    "resolve_bedrock_runtime_region": lambda a, k: {},
    "httpx_Client": lambda a, k: dict(k),
    "session_get_credentials": lambda a, k: {},
    "credentials_get_frozen_credentials": lambda a, k: {},
    "SigV4Auth_ctor": lambda a, k: {"credentials": a[0], "service": a[1], "region": a[2]},
    "signer_add_auth": lambda a, k: {"aws_request": a[0]},
    "AWSRequest_ctor": lambda a, k: dict(k),
    "import_module": lambda a, k: {"module": a[0]},
    "botocore_get_session": lambda a, k: {},
    "session_set_config_variable": lambda a, k: {"name": a[0], "value": a[1]},
    "bedrock_region_from_runtime_url": lambda a, k: {"base_url": a[0]},
    "bedrock_guardrail_config": lambda a, k: {},
    "build_anthropic_bedrock_client": lambda a, k: {"region": a[0], "timeout": k.get("timeout")},
    "get_provider_request_timeout": lambda a, k: {"provider": a[0], "model": a[1]},
    "on_text": lambda a, k: {"text": a[0]},
    "_encode_redacted": lambda a, k: {"redacted": a[0]},
    "_persisted_reasoning_details": lambda a, k: {"ordered_blocks": a[0]},
    "_longest_key_match": lambda a, k: {"key": a[1]},
}

BUILD = {
    "_require_boto3": lambda r, a: None if r is None else Boto3Double(),
    "boto3_Session": lambda r, a: None if r is None else SessionDouble(),
    "botocore_Config": lambda r, a: None if r is None else ConfigDouble(r),
    "session_get_credentials": lambda r, a: None if r is None else CredentialsDouble(),
    "SigV4Auth_ctor": lambda r, a: None if r is None else SignerDouble(),
    "AWSRequest_ctor": lambda r, a: None if r is None else AWSRequestDouble(r),
    "botocore_get_session": lambda r, a: None if r is None else SessionDouble(),
    "credentials_get_frozen_credentials": lambda r, a: types.SimpleNamespace(**r) if isinstance(r, dict) else r,
    "import_module": lambda r, a: types.SimpleNamespace(**r) if isinstance(r, dict) else r,
    "_longest_key_match": lambda r, a: tuple(r) if isinstance(r, list) else r,
}


def record(name, args):
    scripts = _SCRIPTS
    args_norm = {k: norm(v) for k, v in args.items() if v is not None}
    script = scripts.next(name)
    if isinstance(script, dict) and isinstance(script.get("__raise__"), str):
        marker = dict(script)
        scripts.trace.append({"ev": name, "args": args_norm, "ret": marker})
        raise make_exc(marker)
    result = BUILD[name](script, args) if name in BUILD else script
    scripts.trace.append({"ev": name, "args": args_norm, "ret": norm(script)})
    return result


class SeamFn:
    def __init__(self, name):
        self.name = name

    def __call__(self, *a, **kw):
        return record(self.name, SEAM_ARGS[self.name](a, kw))


# ── base namespace ──────────────────────────────────────────────────────────
_BASE_NS = {
    "__name__": "bt3_live_bedrock_transport",
    "__builtins__": builtins,
    "json": json, "base64": base64, "re": re,
    "SimpleNamespace": types.SimpleNamespace,
    "suppress": contextlib.suppress,
    "importlib": types.SimpleNamespace(import_module=SeamFn("import_module")),
    "logger": types.SimpleNamespace(warning=SeamFn("logger_warning"),
                                    debug=SeamFn("logger_debug"),
                                    info=SeamFn("logger_info")),
}
_MOD = types.ModuleType("bt3_live_bedrock_transport")
_MOD.__dict__.update(_BASE_NS)
sys.modules["bt3_live_bedrock_transport"] = _MOD
_BASE_NS = _MOD.__dict__

# Claimed siblings -> ns-global recorders.
for _nm in ["resolve_bedrock_runtime_region", "bedrock_region_from_runtime_url",
            "bedrock_guardrail_config", "_require_boto3", "scoped_aws_session_kwargs",
            "_encode_redacted", "_persisted_reasoning_details"]:
    _BASE_NS[_nm] = SeamFn(_nm)


def _stub_mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


# Foreign-module collaborators ride sys.modules stubs (in-function imports).
_botocore = _stub_mod("botocore")
_botocore.__path__ = []
_config_mod = _stub_mod("botocore.config", Config=SeamFn("botocore_Config"))
_auth_mod = _stub_mod("botocore.auth", SigV4Auth=SeamFn("SigV4Auth_ctor"))
_awsreq_mod = _stub_mod("botocore.awsrequest", AWSRequest=SeamFn("AWSRequest_ctor"))
_session_mod = _stub_mod("botocore.session", get_session=SeamFn("botocore_get_session"))
_botocore.config = _config_mod
_botocore.auth = _auth_mod
_botocore.awsrequest = _awsreq_mod
_botocore.session = _session_mod
_stub_mod("hermes_constants",
          get_hermes_home_override=SeamFn("get_hermes_home_override"),
          hermes_home_key=SeamFn("hermes_home_key"))
_agent = _stub_mod("agent")
_agent.__path__ = []
_anthropic_mod = _stub_mod("agent.anthropic_adapter",
                           build_anthropic_bedrock_client=SeamFn("build_anthropic_bedrock_client"))
_agent.anthropic_adapter = _anthropic_mod
_mm_mod = _stub_mod("agent.model_metadata", DEFAULT_CONTEXT_LENGTHS={},
                    _longest_key_match=SeamFn("_longest_key_match"))
_agent.model_metadata = _mm_mod
_hermes_cli = _stub_mod("hermes_cli")
_hermes_cli.__path__ = []
_timeouts_mod = _stub_mod("hermes_cli.timeouts",
                          get_provider_request_timeout=SeamFn("get_provider_request_timeout"))
_hermes_cli.timeouts = _timeouts_mod
_httpx = _stub_mod("httpx", Auth=type("Auth", (), {}), Client=SeamFn("httpx_Client"))
_BASE_NS["httpx"] = _httpx

for _code in DEF_CODE.values():
    exec(_code, _BASE_NS)
for _code in CLASS_CODE.values():
    exec(_code, _BASE_NS)
for _code in CONST_CODE.values():
    exec(_code, _BASE_NS)

_PROJECT_CLASSES.extend([_BASE_NS["BedrockOpenAISigV4Auth"], _BASE_NS["_ResponseParts"]])
_BASE_NS["_inference_profile_model_cache"] = {}
_BASE_NS["_bedrock_clients_by_home"] = {}


# ── cache-key bridging ──────────────────────────────────────────────────────


def _rt(s):
    return None if s == "None" else int(s)


def dec2(s):
    region, rt = s.split("|")
    return (region, _rt(rt))


def dec4(s):
    h, svc, reg, rt = s.split("|")
    return (h, svc, reg, _rt(rt))


def enc2(k):
    return f"{k[0]}|{k[1]}"


def enc4(k):
    return "|".join(str(x) for x in k)


def build_ns(rec):
    global _SCRIPTS
    inp = rec["in"]
    trace = []
    _SCRIPTS = Scripts(inp.get("io") or {}, trace)
    st = inp.get("state") or {}
    _BASE_NS["_inference_profile_model_cache"] = dict(st.get("_inference_profile_model_cache") or {})
    _BASE_NS["_bedrock_clients_by_home"] = {
        dec4(k): v for k, v in (st.get("_bedrock_clients_by_home") or {}).items()}
    return _BASE_NS, trace, _SCRIPTS


# ── input doubles ───────────────────────────────────────────────────────────


def mk_factory(c):
    return None if c is None else Boto3Double()


def mk_agent(a):
    return None if a is None else types.SimpleNamespace(**dict(a))


def mk_parts(p):
    inst = _BASE_NS["_ResponseParts"]()
    p = p or {}
    inst.text_parts = list(p.get("text_parts") or [])
    inst.reasoning_details = list(p.get("reasoning_details") or [])
    inst.tool_calls = list(p.get("tool_calls") or [])
    return inst


def mk_request(r):
    r = r or {}
    h = r.get("headers")
    headers = HeadersDouble(dict(h)) if isinstance(h, dict) else None
    content = r.get("content")
    if isinstance(content, dict) and "__bytes__" in content:
        content = bytes(content["__bytes__"])
    return types.SimpleNamespace(method=r.get("method"), url=r.get("url"),
                                 content=content, headers=headers)


def mk_on_text(o):
    return SeamFn("on_text") if o else None


# ── live driver ─────────────────────────────────────────────────────────────


def drive(rec, ns, scripts, holder):
    func = rec["func"]
    inp = rec["in"]
    g = inp.get
    if func == "_new_client":
        return ns["_new_client"](mk_factory(g("factory")), g("service"), g("region"), g("read_timeout"))
    if func == "_cached_client":
        cache = {dec2(k): v for k, v in (g("cache") or {}).items()}
        home = ns["_bedrock_clients_by_home"]
        holder["win"] = {"cache": cache, "_bedrock_clients_by_home": home}
        return ns["_cached_client"](cache, g("service"), g("region"), read_timeout=g("read_timeout"))
    if func == "BedrockOpenAISigV4Auth.__init__":
        service = inp["service"] if "service" in inp else "bedrock"
        return ns["BedrockOpenAISigV4Auth"](g("region"), service)
    if func == "BedrockOpenAISigV4Auth.auth_flow":
        service = inp["service"] if "service" in inp else "bedrock"
        self_ns = types.SimpleNamespace(region=g("region"), service=service)
        req = mk_request(g("request"))
        holder["win"] = {"request": req}
        gen = ns["BedrockOpenAISigV4Auth"].auth_flow(self_ns, req)
        return list(gen)
    if func == "build_bedrock_openai_http_client":
        return ns[func](g("region"), timeout=g("timeout"))
    if func == "_stale_error_types":
        return ns[func]()
    if func == "_boto3_chain_has_credentials":
        im = inp["instance_metadata"] if "instance_metadata" in inp else True
        return ns[func](instance_metadata=im)
    if func == "bind_bedrock_runtime":
        agent = mk_agent(g("agent"))
        holder["win"] = {"agent": agent}
        return ns[func](agent, g("base_url"), g("api_mode"))
    if func == "_ResponseParts.__init__":
        return ns["_ResponseParts"]()
    if func == "_ResponseParts.absorb_reasoning":
        parts = mk_parts(g("parts"))
        block = g("block") or {}
        holder["win"] = {"block": block, "parts": parts}
        return parts.absorb_reasoning(g("reasoning"), block, mk_on_text(g("on_text")))
    if func == "_ResponseParts.build":
        parts = mk_parts(g("parts"))
        usage = g("usage_data") or {}
        return parts.build(g("ordered_blocks"), usage, g("stop_reason"), g("model"))
    if func == "bedrock_static_context_length":
        return ns[func](g("model_id"))
    raise KeyError(f"unknown func {func}")


def echo_state(holder, inp):
    w = holder.get("win")
    if w is not None:
        out = {}
        for k, v in w.items():
            if k == "cache":
                out[k] = {enc2(kk): norm(vv) for kk, vv in v.items()}
            elif k == "_bedrock_clients_by_home":
                out[k] = {enc4(kk): norm(vv) for kk, vv in v.items()}
            else:
                out[k] = norm(v)
        return out
    st = inp.get("state") or {}
    out = {}
    if "_inference_profile_model_cache" in st:
        out["_inference_profile_model_cache"] = dict(_BASE_NS["_inference_profile_model_cache"])
    if "_bedrock_clients_by_home" in st:
        out["_bedrock_clients_by_home"] = {
            enc4(k): norm(v) for k, v in _BASE_NS["_bedrock_clients_by_home"].items()}
    return out


def raise_spec(e):
    return {"__raise__": type(e).__name__, "msg": str(e)}


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
                out = {"ret": norm(ret), "trace": trace, "state": echo_state(holder, rec["in"])}
            except BaseException as e:  # noqa: BLE001 - fail-closed per case
                out = {"ret": raise_spec(e), "trace": trace,
                       "state": echo_state(holder, rec["in"])}
        except BaseException as e:  # noqa: BLE001 - seeding failure is a mismatch
            out = {"ret": raise_spec(e), "trace": [], "state": {}}
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


# ── self-test ───────────────────────────────────────────────────────────────


def cross_check(fixtures):
    bad = 0
    ctext = Path(PORT_SRC).read_text(encoding="utf-8")
    c_names = set(re.findall(r'bt3_call\(ctx,\s*"([A-Za-z_][A-Za-z_0-9]*)"', ctext))
    if c_names != set(SEAM_ARGS):
        bad += 1
        print(f"CROSSCHECK seam-name drift: C-only={sorted(c_names - set(SEAM_ARGS))} "
              f"oracle-only={sorted(set(SEAM_ARGS) - c_names)}")
    fx_io = set()
    for rec in fixtures:
        fx_io |= set((rec["in"].get("io") or {}).keys())
    if fx_io - c_names:
        bad += 1
        print(f"CROSSCHECK fixture io keys with no C seam: {sorted(fx_io - c_names)}")
    fx_funcs = {rec["func"] for rec in fixtures}
    if fx_funcs - FUNCS:
        bad += 1
        print(f"CROSSCHECK fixture funcs not extracted: {sorted(fx_funcs - FUNCS)}")
    if FUNCS - fx_funcs:
        bad += 1
        print(f"CROSSCHECK extracted funcs with no fixture: {sorted(FUNCS - fx_funcs)}")
    checks = [
        ("BEDROCK_CONTEXT_LENGTHS len", len(_BASE_NS["BEDROCK_CONTEXT_LENGTHS"]), 53),
        ("_STOP_REASON_TO_FINISH_REASON", _BASE_NS["_STOP_REASON_TO_FINISH_REASON"],
         {"end_turn": "stop", "stop_sequence": "stop", "tool_use": "tool_calls",
          "max_tokens": "length", "content_filtered": "content_filter",
          "guardrail_intervened": "content_filter"}),
        ("_APPLICATION_PROFILE_ARN_RE", _BASE_NS["_APPLICATION_PROFILE_ARN_RE"].pattern,
         ":application-inference-profile/"),
        ("_CATALOG_FAMILY_ID_RE", _BASE_NS["_CATALOG_FAMILY_ID_RE"].pattern,
         r"(?:^|[./])(?:anthropic|openai|xai)\.([a-z0-9][a-z0-9.:-]*)$"),
        ("_BOTO_CONNECT_TIMEOUT_SECONDS", _BASE_NS["_BOTO_CONNECT_TIMEOUT_SECONDS"], 60),
    ]
    for nm, got, want in checks:
        if got != want:
            bad += 1
            print(f"CROSSCHECK constant {nm} drifted: {got!r} != {want!r}")
    return bad


def selftest(fixtures):
    import faulthandler
    faulthandler.dump_traceback_later(120, exit=True)
    bad = cross_check(fixtures)
    names = FUNCS | set(SEAM_ARGS)
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
                documented = any(sub in msg for sub in (
                    "'NoneType' object has no attribute 'client'",
                    "'NoneType' object has no attribute 'Session'",
                    "'NoneType' object has no attribute 'merge'",
                    "'NoneType' object has no attribute 'get_credentials'",
                    "'NoneType' object has no attribute 'add_auth'",
                    "'NoneType' object has no attribute 'headers'",
                    "'NoneType' object has no attribute 'items'",
                    "'NoneType' object has no attribute 'get'",
                    "and no __dict__ for setting new attributes",
                    "object has no attribute 'strip'",
                    "object has no attribute 'lower'",
                ))
                if not documented and any(f"'{nm}'" in msg for nm in names):
                    bad += 1
                    print(f"SELFTEST case {i} {rec['func']}: AttributeError naming ported/seam: {msg}")
            if isinstance(spec, dict) and spec.get("__raise__") == "NameError":
                bad += 1
                print(f"SELFTEST case {i} {rec['func']}: NameError (seeding gap): {spec.get('msg', '')}")
        except BaseException as e:  # noqa: BLE001
            bad += 1
            print(f"SELFTEST case {i} {rec['func']}: DRIVE FAILURE {type(e).__name__}: {e}")
    faulthandler.cancel_dump_traceback_later()
    print(f"SELFTEST {NAME}: {len(fixtures)} cases, {bad} suspicious")
    return bad


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        fx = Path(__file__).resolve().parent / "fixtures" / "bedrock_transport_flow2.jsonl"
        recs = [json.loads(l) for l in fx.read_text(encoding="utf-8").splitlines() if l.strip()]
        sys.exit(1 if selftest(recs) else 0)
    main()