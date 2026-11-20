#!/usr/bin/env python3
"""Oracle for the FLOW + leaf port of the UNCLAIMED remainder of
agent/error_classifier.py (src/agent/error_classifier_flow.c, ec_* entries).

Reads the harness JSONL on stdin ({"func","in","ret","trace","st"}), recomputes
the LIVE reference from rec["in"] with scripted universal-seam stubs, and
compares the ordered trace, the return, and the state projection ({} -- these
classifiers mutate no caller state). Live defs are AST-extracted (py3.14 quarry
venv); claimed siblings (_extract_error_code, _error_obj, _json_dict,
_has_usage_limit_transient_signal, _rate_limit_reset_seconds,
_model_id_missing_known_prefix, _is_openrouter_upstream_error,
_extract_upstream_provider_name, _classify_400, _classify_402) and the unported
_classify_image_tool_422 plus all I/O are recording seams fed the SAME scripts
the C harness feeds. Ported siblings stay REAL so composition is exercised.

Fail-closed: 0 cases => exit 1; a missing/exhausted script RETURNS a Raise spec
(recorded before raising) so the trace matches the C dispatcher. BaseException
caught per case -> __raise__ spec, st={}. The `except Exception` sites
(_plugin_verdict, _profile_verdict, _body_of) pass the BaseException trio
(+CancelledError) through via the real builtin hierarchy.
Summary: 'ORACLE error_classifier_flow: N cases, M mismatches'.

Domain (documented in the port header): ASCII; str()-positions integral;
reason is a FailoverReason value string; exc doubles use kinds whose
str(cls(msg))==msg (KeyError doubles excluded); status_code attrs int-or-null;
response is a plain dict (botocore) or a tagged __response__ object double;
ctx fixtures carry every raw field explicitly.
"""
import ast
import builtins
import copy
import enum
import json
import math
import os
import re
import sys
import types
import textwrap
import __future__
import itertools
from pathlib import Path
from types import SimpleNamespace

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_HERE = Path(__file__).resolve()
_REPO = _HERE.parent.parent
QUARRY = _REPO.parent / "hermes-agent"
if not (QUARRY / "agent" / "error_classifier.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))

EC_SRC = str(QUARRY / "agent" / "error_classifier.py")
ANN_FLAG = __future__.annotations.compiler_flag

FUNCS = [
    "_v", "_first_match", "_billing_hints", "_is_anthropic_thinking_rejection",
    "_gemini_prompt_block_marker", "_is_gemini_prompt_block", "_from_cause_chain",
    "_status_of", "_body_of", "_headers_of", "_status_code_from_body",
    "_code_from_payload", "_body_message_candidates", "_openrouter_wrapped_message",
    "_build_error_msg", "_is_server_injected_param_rejection",
    "_is_codex_masked_replay_rejection", "_moa_special_cases", "_nous_welcome_tier",
    "_provider_special_cases", "_status_403", "_status_404", "_status_429",
    "_status_5xx", "_is_nous_route", "_by_status", "_by_error_code", "_by_transport",
    "_shape_context", "_off_route_host", "_plugin_verdict", "_profile_verdict",
    "__post_init__", "large_session",
]
CTX_FUNCS = {
    "_is_codex_masked_replay_rejection", "_moa_special_cases", "_nous_welcome_tier",
    "_provider_special_cases", "_status_403", "_status_404", "_status_429",
    "_status_5xx", "_is_nous_route", "_by_status", "_by_error_code", "_by_transport",
    "_shape_context", "_off_route_host", "_plugin_verdict", "_profile_verdict",
}

# claimed module-global siblings (live name -> seam name)
SEAM_GLOBALS = {
    "_extract_error_code": "extract_error_code",
    "_error_obj": "error_obj",
    "_json_dict": "json_dict",
    "_has_usage_limit_transient_signal": "has_usage_limit_transient_signal",
    "_rate_limit_reset_seconds": "rate_limit_reset_seconds",
    "_model_id_missing_known_prefix": "model_id_missing_known_prefix",
    "_is_openrouter_upstream_error": "is_openrouter_upstream_error",
    "_extract_upstream_provider_name": "extract_upstream_provider_name",
    "_classify_400": "classify_400",
    "_classify_402": "classify_402",
    "_classify_image_tool_422": "classify_image_tool_422",
}
# in-function imports: module -> {attr: seam name | None for real constant/class}
SEAM_IMPORTS = {
    "hermes_cli.plugins": {"get_plugin_error_classification": "get_plugin_error_classification"},
    "providers": {"get_provider_profile": "get_provider_profile"},
    "hermes_cli.anon_auth": {"parse_welcome_refusal": "parse_welcome_refusal",
                             "route_is_welcome_host": "route_is_welcome_host",
                             "welcome_route_refusal": "welcome_route_refusal",
                             "WELCOME_TIER_GATE_REASONS": None},
    "agent.nous_rate_guard": {"welcome_refusal_from_headers": "welcome_refusal_from_headers"},
    "agent.reasoning_timeouts": {"get_reasoning_stale_timeout_floor": "get_reasoning_stale_timeout_floor"},
    "hermes_cli.route_identity": {"provider_owns_route": "provider_owns_route"},
    "utils": {"base_url_hostname": "base_url_hostname",
              "base_url_host_matches": "base_url_host_matches"},
    "agent.errors": {"InvalidProviderResponseError": None, "MoAPresetNotFoundError": None},
}

CURRENT = {"rec": None}
_MISSING = object()

STUB_EXC = {
    "MoAPresetNotFoundError": type("MoAPresetNotFoundError", (ValueError,), {}),
    "InvalidProviderResponseError": type("InvalidProviderResponseError", (RuntimeError,), {}),
    "GeminiAPIError": type("GeminiAPIError", (Exception,), {}),
    "APIError": type("APIError", (Exception,), {}),
    "RateLimitError": type("RateLimitError", (Exception,), {}),
    "AuthenticationError": type("AuthenticationError", (Exception,), {}),
}


def exc_class(kind):
    if kind in STUB_EXC:
        return STUB_EXC[kind]
    b = getattr(builtins, kind, None)
    if isinstance(b, type) and issubclass(b, BaseException):
        return b
    return type(kind, (Exception,), {})


def _stub_fn(seam):
    def f(*a, **k):
        return CURRENT["rec"](seam)(*a, **k)
    return f


class Raise:
    def __init__(self, spec, exc):
        self.spec = spec
        self.exc = exc


def build_exc(spec):
    kind = spec.get("__raise__", "RuntimeError")
    msg = spec.get("message", "")
    return exc_class(kind)(msg)


class Script:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.handed = []

    def next(self, name):
        entry = self.io.get(name, _MISSING)
        if entry is _MISSING:
            return self._fail(name)
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                return self._fail(name)
            self.cursors[name] = idx + 1
            val = entry[idx]
        else:
            val = entry
        return self._unwrap(name, val)

    def _fail(self, name):
        spec = {"__raise__": "RuntimeError", "message": "no script for " + name}
        r = Raise(spec, build_exc(spec))
        self.handed.append(r)
        return r

    def _unwrap(self, name, val):
        if isinstance(val, dict) and "__raise__" in val:
            r = Raise(val, build_exc(val))
            self.handed.append(r)
            return r
        if isinstance(val, dict) and val.get("__script__") is True:
            return val.get("ret")
        return val


def norm_value(v):
    if isinstance(v, SimpleNamespace):
        return {k: norm_value(x) for k, x in vars(v).items()}
    if isinstance(v, BaseException):
        return str(v)
    if isinstance(v, dict):
        return {k: norm_value(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [norm_value(x) for x in v]
    if isinstance(v, (set, frozenset)):
        return sorted(norm_value(x) for x in v)
    if isinstance(v, float):
        if math.isfinite(v) and v == int(v):
            return int(v)
        return v
    if isinstance(v, enum.Enum):
        return v.value
    return v


def norm_args(v):
    v = norm_value(v)
    if isinstance(v, dict):
        return {k: norm_args(x) for k, x in v.items() if x is not None}
    if isinstance(v, list):
        return [norm_args(x) for x in v]
    return v


def proj_exc(e):
    if isinstance(e, BaseException):
        return {"kind": type(e).__name__, "message": str(e)}
    if isinstance(e, dict):
        return {"kind": e.get("kind", ""), "message": e.get("message", "")}
    return {"kind": "", "message": ""}


def proj_ctx(c):
    keys = ["status_code", "body", "msg", "provider", "model", "approx_tokens",
            "context_length", "num_messages", "base_url", "anonymous",
            "error_type", "error_code", "code", "headers", "provider_slug", "model_slug"]
    o = {k: getattr(c, k, None) for k in keys}
    o["error"] = proj_exc(c.error)
    return o


def proj(name, a, k):
    if name == "extract_error_code": return {"body": a[0]}
    if name == "error_obj": return {"body": a[0]}
    if name == "json_dict": return {"text": a[0]}
    if name in ("has_usage_limit_transient_signal", "rate_limit_reset_seconds"):
        return {"error_msg": a[0], "body": a[1], "response_headers": a[2]}
    if name == "model_id_missing_known_prefix": return {"model": a[0], "provider": a[1]}
    if name == "is_openrouter_upstream_error": return {"body": a[0], "provider": a[1]}
    if name == "extract_upstream_provider_name": return {"body": a[0]}
    if name in ("classify_400", "classify_image_tool_422"): return {"ctx": proj_ctx(a[0])}
    if name == "classify_402": return {"error_msg": a[0], "code": a[2], "body": a[3]}
    if name == "first_match.verdict": return {"error_msg": a[0]}
    if name == "pick": return {"exc": proj_exc(a[0])}
    if name == "response.json": return {}
    if name == "get_plugin_error_classification":
        return {"provider": k.get("provider"), "model": k.get("model"),
                "status_code": k.get("status_code"), "error_type": k.get("error_type"),
                "error_code": k.get("error_code"), "error_message": k.get("error_message"),
                "error_body": k.get("error_body"), "error": proj_exc(k.get("error")),
                "approx_tokens": k.get("approx_tokens"), "context_length": k.get("context_length"),
                "num_messages": k.get("num_messages")}
    if name == "get_provider_profile": return {"provider_slug": a[0]}
    if name == "profile.classify_api_error":
        return {"error": proj_exc(a[0]), "status_code": k.get("status_code"),
                "error_code": k.get("error_code"), "message": k.get("message"),
                "body": k.get("body"), "model": k.get("model")}
    if name == "welcome_route_refusal":
        return {"status": a[0], "msg": a[1], "base_url": a[2] if len(a) > 2 else None}
    if name == "route_is_welcome_host": return {"base_url": a[0]}
    if name == "parse_welcome_refusal": return {"body": a[0]}
    if name == "welcome_refusal_from_headers": return {"headers": a[0], "body_wait": k.get("body_wait")}
    if name == "get_reasoning_stale_timeout_floor": return {"model": a[0]}
    if name == "provider_owns_route": return {"provider_slug": a[0], "base_url": a[1]}
    if name == "base_url_hostname": return {"url": a[0]}
    if name == "base_url_host_matches": return {"base_url": a[0], "host": a[1]}
    if name == "time.time": return {}
    if name in ("logger.debug", "logger.info"): return {"fmt": a[0], "args": list(a[1:])}
    return {**{f"arg{i}": v for i, v in enumerate(a)}, **k}


class CaseCtx:
    def __init__(self, inp):
        self.inp = inp
        self.trace = []
        self.script = Script(inp.get("io"))

    def recorder(self, name):
        ctx = self

        def call(*a, **k):
            args = norm_args(proj(name, a, k))
            raw = ctx.script.next(name)
            if isinstance(raw, Raise):
                ctx.trace.append({"ev": name, "args": args, "ret": raw.spec})
                raise raw.exc
            ctx.trace.append({"ev": name, "args": args, "ret": norm_value(raw)})
            return materialize(name, raw)
        return call


def materialize_verdict(raw):
    if isinstance(raw, dict) and isinstance(raw.get("reason"), str):
        out = dict(raw)
        try:
            out["reason"] = NS["FailoverReason"](raw["reason"])
        except ValueError:
            pass
        return out
    return raw


def materialize(name, raw):
    if name in ("classify_400", "classify_402", "classify_image_tool_422",
                "first_match.verdict", "get_plugin_error_classification"):
        return materialize_verdict(raw)
    if name == "get_provider_profile":
        if isinstance(raw, dict):
            p = SimpleNamespace(**{k: v for k, v in raw.items() if k != "classify_api_error"})
            if raw.get("classify_api_error"):
                p.classify_api_error = CURRENT["rec"]("profile.classify_api_error")
            return p
        return raw
    return raw


class RecLogger:
    def _log(self, lvl):
        def f(fmt, *args):
            return CURRENT["rec"]("logger." + lvl)(fmt, *args)
        return f

    def __getattr__(self, name):
        if name in ("info", "warning", "debug", "error"):
            return self._log(name)
        raise AttributeError(name)


def materialize_response(v):
    if isinstance(v, dict) and v.get("__response__"):
        r = SimpleNamespace()
        if "status_code" in v:
            r.status_code = v["status_code"]
        if "headers" in v:
            r.headers = v["headers"]
        r.json = CURRENT["rec"]("response.json")
        return r
    return v


def materialize_exc(spec):
    if not isinstance(spec, dict):
        return spec
    kind = spec.get("kind", "Exception")
    msg = spec.get("message", "")
    e = exc_class(kind)(msg)
    for k, v in (spec.get("attrs") or {}).items():
        if k == "response":
            setattr(e, k, materialize_response(v))
        else:
            setattr(e, k, v)
    if spec.get("cause"):
        e.__cause__ = materialize_exc(spec["cause"])
    if spec.get("context"):
        e.__context__ = materialize_exc(spec["context"])
    return e


def make_ctx(cinp):
    c = SimpleNamespace()
    c.error = materialize_exc(cinp.get("error"))
    for k in ("status_code", "body", "msg", "provider", "model", "approx_tokens",
              "context_length", "num_messages", "base_url", "anonymous"):
        setattr(c, k, cinp.get(k))
    c.large_session = types.MethodType(NS["large_session"], c)
    return c


def materialize_rules(rules):
    out = []
    for rule in rules or []:
        pats, verdict = rule[0], rule[1]
        if isinstance(verdict, dict) and verdict.get("__fn__") == "billing_hints":
            v = NS["_billing_hints"]
        elif isinstance(verdict, dict) and verdict.get("__callable__") is True:
            v = CURRENT["rec"]("first_match.verdict")
        elif isinstance(verdict, dict):
            v = materialize_verdict(verdict)
        else:
            v = verdict
        out.append((pats, v))
    return out


# ---- live namespace seeding ----
NS = {}
REAL = {}


def _exec_seg(src, node, ns, filename):
    seg = ast.get_source_segment(src, node)
    seg = textwrap.dedent(seg)
    code = compile(seg, filename, "exec", flags=ANN_FLAG, dont_inherit=True)
    exec(code, ns)


def _assign_names(node):
    names = set()
    tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
    for t in tgts:
        if isinstance(t, ast.Name):
            names.add(t.id)
        elif isinstance(t, (ast.Tuple, ast.List)):
            for e in t.elts:
                if isinstance(e, ast.Name):
                    names.add(e.id)
    return names


def _plain_target(node):
    tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
    for t in tgts:
        if isinstance(t, ast.Name):
            continue
        if isinstance(t, (ast.Tuple, ast.List)) and all(isinstance(e, ast.Name) for e in t.elts):
            continue
        return False
    return True


def load_ns():
    global NS, REAL
    mod = types.ModuleType("ec_live")
    sys.modules["ec_live"] = mod
    ns = mod.__dict__
    NS = ns
    ns.update({
        "__name__": "ec_live", "enum": enum, "math": math, "json": json, "re": re,
        "itertools": itertools, "time": SimpleNamespace(time=_stub_fn("time.time")),
        "logging": SimpleNamespace(getLogger=lambda name: RecLogger()),
        "logger": RecLogger(),
        "Any": object, "Optional": object, "Callable": object, "Dict": dict,
        "Iterator": object, "Sequence": object,
        "dataclass": lambda c: c, "field": lambda **k: None,
    })
    src = Path(EC_SRC).read_text(encoding="utf-8")
    tree = ast.parse(src)
    # FailoverReason enum FIRST
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "FailoverReason":
            _exec_seg(src, node, ns, EC_SRC)
    if "FailoverReason" not in ns:
        raise RuntimeError("seed missing: FailoverReason")
    # target top-level defs
    found = set()
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in FUNCS:
            _exec_seg(src, node, ns, EC_SRC)
            found.add(node.name)
    # _Ctx methods
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "_Ctx":
            for sub in node.body:
                if isinstance(sub, ast.FunctionDef) and sub.name in ("__post_init__", "large_session"):
                    _exec_seg(src, sub, ns, EC_SRC)
                    found.add(sub.name)
    missing = set(FUNCS) - found
    if missing:
        raise RuntimeError("missing target defs: %s" % sorted(missing))
    # module constants (plain-Name / Tuple-of-Name targets), skip handler table
    skip = {"_STATUS_HANDLERS", "_STAGES", "logger"}
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            if isinstance(node, ast.AnnAssign) and node.value is None:
                continue
            if not _plain_target(node):
                continue
            if _assign_names(node) & skip:
                continue
            _exec_seg(src, node, ns, EC_SRC)
    # claimed sibling recorders
    for live, seam in SEAM_GLOBALS.items():
        ns[live] = _stub_fn(seam)
    # in-function import stubs (overwrite parent packages)
    agent_pkg = types.ModuleType("agent"); agent_pkg.__path__ = []
    sys.modules["agent"] = agent_pkg
    hc_pkg = types.ModuleType("hermes_cli"); hc_pkg.__path__ = []
    sys.modules["hermes_cli"] = hc_pkg
    for modname, attrs in SEAM_IMPORTS.items():
        m = types.ModuleType(modname)
        for attr, seam in attrs.items():
            if seam is None:
                continue
            setattr(m, attr, _stub_fn(seam))
        sys.modules[modname] = m
        parts = modname.split(".")
        if len(parts) == 2:
            setattr(sys.modules[parts[0]], parts[1], m)
    # real constants / classes into their stub modules
    sys.modules["hermes_cli.anon_auth"].WELCOME_TIER_GATE_REASONS = frozenset({"model_not_free", "feature_not_free"})
    sys.modules["agent.errors"].MoAPresetNotFoundError = STUB_EXC["MoAPresetNotFoundError"]
    sys.modules["agent.errors"].InvalidProviderResponseError = STUB_EXC["InvalidProviderResponseError"]
    # per-case handler table for _by_status (lambdas resolve NS globals at call time)
    ns["_STATUS_HANDLERS"] = {
        400: ns["_classify_400"],
        401: lambda c: ns["_V_AUTH_ROTATE"],
        402: lambda c: ns["_classify_402"](c.msg, dict, c.code if ns["_is_nous_route"](c) else "", c.body),
        403: ns["_status_403"],
        404: ns["_status_404"],
        408: lambda c: ns["_V_TIMEOUT"],
        413: lambda c: ns["_V_PAYLOAD_TOO_LARGE"],
        422: lambda c: ns["_classify_image_tool_422"](c),
        429: ns["_status_429"],
        500: ns["_status_5xx"], 502: ns["_status_5xx"],
        503: lambda c: ns["_first_match"](c.msg, ns["_OVERFLOW_AS_5XX_RULES"]) or ns["_V_OVERLOADED"],
        529: lambda c: ns["_first_match"](c.msg, ns["_OVERFLOW_AS_5XX_RULES"]) or ns["_V_OVERLOADED"],
    }
    # audit
    for nm in FUNCS + ["FailoverReason", "_V_BILLING", "_V_AUTH_FALLBACK", "_OVERFLOW_AS_5XX_RULES",
                       "_404_RULES", "_ERROR_CODE_VERDICTS", "_STATUS_HANDLERS"]:
        if nm not in ns:
            raise RuntimeError("seed missing: " + nm)
    REAL = {nm: ns[nm] for nm in FUNCS}


def drive(func, inp):
    if func == "_v":
        reason = NS["FailoverReason"](inp.get("reason"))
        return NS["_v"](reason, **(inp.get("hints") or {})), {}
    if func == "_first_match":
        return NS["_first_match"](inp.get("error_msg"), materialize_rules(inp.get("rules"))), {}
    if func == "_billing_hints":
        return NS["_billing_hints"](inp.get("error_msg")), {}
    if func == "_is_anthropic_thinking_rejection":
        return NS[func](inp.get("msg")), {}
    if func == "_is_server_injected_param_rejection":
        return NS[func](inp.get("error_msg"), inp.get("provider")), {}
    if func in ("_gemini_prompt_block_marker", "_status_of", "_body_of", "_headers_of"):
        return NS[func](materialize_exc(inp.get("exc"))), {}
    if func == "_is_gemini_prompt_block":
        return NS[func](materialize_exc(inp.get("error"))), {}
    if func == "_from_cause_chain":
        pick = CURRENT["rec"]("pick")
        return NS[func](materialize_exc(inp.get("error")), pick, inp.get("default")), {}
    if func == "_status_code_from_body":
        return NS[func](inp.get("body")), {}
    if func == "_code_from_payload":
        return NS[func](inp.get("payload"), inp.get("top_keys") or [], inp.get("peek_message", False)), {}
    if func == "_body_message_candidates":
        return list(NS[func](inp.get("body"))), {}
    if func == "_openrouter_wrapped_message":
        return NS[func](inp.get("err_obj")), {}
    if func == "_build_error_msg":
        return NS[func](materialize_exc(inp.get("error")), inp.get("body")), {}
    if func == "large_session":
        c = make_ctx(inp.get("ctx") or {})
        return NS["large_session"](c, inp.get("frac"), inp.get("tokens"), inp.get("messages")), {}
    if func == "__post_init__":
        c = make_ctx(inp.get("ctx") or {})
        NS["__post_init__"](c)
        return {k: getattr(c, k) for k in ("error_type", "error_code", "code", "headers",
                                           "provider_slug", "model_slug")}, {}
    if func in CTX_FUNCS:
        c = make_ctx(inp.get("ctx") or {})
        NS["__post_init__"](c)
        if func == "_shape_context":
            verdict = materialize_verdict(inp.get("verdict") or {})
            return NS[func](c, verdict, hooked=inp.get("hooked", False)), {}
        return NS[func](c), {}
    raise RuntimeError("unknown func " + func)


def run_case(func, inp):
    for nm, fn in REAL.items():
        NS[nm] = fn
    ctx = CaseCtx(inp)
    CURRENT["rec"] = ctx.recorder
    try:
        ret, st = drive(func, inp)
        return {"ret": norm_value(ret), "trace": ctx.trace, "st": norm_value(st)}
    except BaseException as e:
        spec = None
        for r in ctx.script.handed:
            if e is r.exc:
                spec = r.spec
                break
        if spec is None:
            spec = {"__raise__": type(e).__name__, "message": str(e)}
        return {"ret": spec, "trace": ctx.trace, "st": {}}


def main():
    load_ns()
    n = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        func = rec.get("func")
        inp = rec.get("in") or {}
        n += 1
        want = run_case(func, inp)
        got = {"ret": rec.get("ret"), "trace": rec.get("trace"), "st": rec.get("st")}
        got = norm_value(got)
        if got != want:
            mism += 1
            if mism <= 12:
                print("MISMATCH [%s] case %d" % (func, n))
                for key in ("ret", "trace", "st"):
                    if got.get(key) != want.get(key):
                        print("  %s GOT : %s" % (key, json.dumps(got.get(key), ensure_ascii=False)[:600]))
                        print("  %s WANT: %s" % (key, json.dumps(want.get(key), ensure_ascii=False)[:600]))
    print("ORACLE error_classifier_flow: %d cases, %d mismatches" % (n, mism))
    if n == 0:
        sys.exit(1)
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()