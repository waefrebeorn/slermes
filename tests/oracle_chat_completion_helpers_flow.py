#!/usr/bin/env python3
"""Oracle for the cch2 second-wave FLOW + leaf port of agent/chat_completion_helpers.py
(src/agent/chat_completion_helpers_flow.c, cch2_* section).

Reads the harness JSONL on stdin ({"func","in","ret","trace","st"}), recomputes the
LIVE reference from rec["in"] with scripted universal-seam stubs, and compares the
ordered trace, the return, and the state projection. Live defs are AST-extracted;
claimed siblings (_scale_stale_timeout_for_context, estimate_request_context_tokens,
_bedrock_reasoning_stale_floor) and all I/O are recording seams fed the SAME scripts
the C harness feeds. Ported siblings stay REAL so composition is exercised.

Fail-closed: 0 cases => exit 1. BaseException caught per case -> __raise__ spec.
Summary: 'ORACLE chat_completion_helpers_flow: N cases, M mismatches'.

Domain (documented in the port header): str()-positions carry strings/integral
numbers; strip/lower ASCII; exc.body in _rejects_stream_options is str/None; pool /
compressor / transport doubles are objects (truthiness = is-not-None); no scripted
KeyError raises (str-projection domain).
"""
import ast
import copy
import enum
import json
import os
import sys
import types
import contextlib
import re as _re
import __future__
from pathlib import Path
from types import SimpleNamespace

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_HERE = Path(__file__).resolve()
_REPO = _HERE.parent.parent
QUARRY = _REPO.parent / "hermes-agent"
if not (QUARRY / "agent" / "chat_completion_helpers.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))
SRC = str(QUARRY / "agent" / "chat_completion_helpers.py")

ANN_FLAG = __future__.annotations.compiler_flag

# func key (live name) -> C entry suffix (identical here)
FUNCS = [
    "_is_provider_stream_empty_frame_error", "_rejects_stream_options",
    "_is_sse_connection_error", "_relay_stream_identity", "_relay_stream_metadata",
    "_stream_final_text", "_high_effort_silence_floor", "_consume_ephemeral_max_output",
    "_model_dump_safe", "_dump_if_model", "_codex_silent_hang_hint",
    "_stream_env_stale_base", "_local_stream_stale_timeout_default", "cap_to_run_budget",
    "_cloud_stale_timeout", "_stream_stale_timeout_is_explicit", "_cloud_stale_timeout_for",
    "_fallback_chain_exhausted", "_log_fallback_activated", "_rebind_fallback_credential_pool",
    "_candidate_pool_exhausted", "_update_fallback_context_compressor",
    "_reresolve_fallback_reasoning_config", "_rescope_fallback_extra_body",
    "_fallback_api_mode_hint", "_is_anthropic_wire_url", "_managed_summary_call",
    "_summary_text", "_with_stream_emitters", "_finalize_bedrock_relay_events",
]

# claimed siblings seamed (live name -> seam name)
SEAM_SIBLINGS = {
    "estimate_request_context_tokens": "estimate_request_context_tokens",
    "_scale_stale_timeout_for_context": "scale_stale_timeout_for_context",
    "_bedrock_reasoning_stale_floor": "bedrock_reasoning_stale_floor",
}
# module-global I/O recorders (live name -> seam name)
SEAM_GLOBALS = {
    "env_float": "env_float",
    "get_provider_stale_timeout": "get_provider_stale_timeout",
    "is_router_timeout_shim": "is_router_timeout_shim",
}
# in-function imports: module -> {attr: seam name}
SEAM_IMPORTS = {
    "hermes_cli.config": {"load_config_readonly": "load_config_readonly", "load_config": "load_config"},
    "agent.reasoning_timeouts": {"get_reasoning_stale_timeout_floor": "get_reasoning_stale_timeout_floor"},
    "hermes_constants": {"get_hermes_home": "get_hermes_home", "profile_name_for_home": "profile_name_for_home",
                         "resolve_reasoning_config": "resolve_reasoning_config"},
    "agent.custom_entry_pool": {"load_route_pool": "load_route_pool"},
    "agent.credential_pool": {"load_pool": "load_pool"},
    "agent.model_metadata": {"get_model_context_length": "get_model_context_length"},
    "agent.conversation_compression_feasibility": {"revalidate_compression_feasibility": "revalidate_compression_feasibility"},
    "agent.agent_init": {"_custom_provider_extra_body_for_agent": "custom_provider_extra_body_for_agent",
                         "_merge_custom_provider_extra_body": "merge_custom_provider_extra_body"},
    "hermes_cli.runtime_provider": {"_get_named_custom_provider": "get_named_custom_provider",
                                    "_parse_api_mode": "parse_api_mode"},
    "hermes_cli.providers": {"host_mandated_api_mode": "host_mandated_api_mode"},
    "agent.relay_llm": {"execute_current": "relay_llm.execute_current"},
    "agent.bedrock_adapter": {"stream_converse_with_callbacks": "stream_converse_with_callbacks"},
    "agent.reasoning_effort": {"EFFORT_LADDER": None},  # real constant, bound below
}

CURRENT = {"rec": None}


def _stub_fn(seam):
    def f(*a, **k):
        return CURRENT["rec"](seam)(*a, **k)
    return f


class Raise:
    def __init__(self, spec, exc):
        self.spec = spec
        self.exc = exc


class EmptyStreamError(Exception):
    pass


def build_exc(spec):
    kind = spec.get("__raise__", "RuntimeError")
    msg = spec.get("message", "")
    base = {"SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
            "GeneratorExit": GeneratorExit}
    builtin = {"TypeError": TypeError, "ValueError": ValueError, "RuntimeError": RuntimeError,
               "AttributeError": AttributeError, "InterruptedError": InterruptedError,
               "TimeoutError": TimeoutError, "Exception": Exception, "KeyError": KeyError}
    if kind in base:
        cls = base[kind]
    elif kind in builtin:
        cls = builtin[kind]
    elif kind == "EmptyStreamError":
        cls = EmptyStreamError
    elif kind == "CancelledError":
        import asyncio
        cls = asyncio.CancelledError
    else:
        cls = type(kind, (Exception,), {})
    return cls(msg)


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


_MISSING = object()


def norm_value(v):
    if isinstance(v, SimpleNamespace):
        return {k: norm_value(x) for k, x in vars(v).items()}
    if isinstance(v, BaseException):
        return str(v)
    if isinstance(v, dict):
        return {k: norm_value(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [norm_value(x) for x in v]
    if isinstance(v, float) and v == int(v):
        return int(v)
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


def proj(name, a, k):
    if name == "env_float": return {"name": a[0], "default": a[1]}
    if name == "env_has": return {"name": a[0]}
    if name == "get_provider_stale_timeout": return {"provider": a[0], "model": a[1]}
    if name == "estimate_request_context_tokens": return {"api_kwargs": a[0]}
    if name == "scale_stale_timeout_for_context": return {"base": a[0], "est_tokens": a[1]}
    if name in ("get_reasoning_stale_timeout_floor", "bedrock_reasoning_stale_floor"): return {"model_id": a[0]}
    if name in ("load_config_readonly", "load_config", "get_hermes_home", "emit_stream_start",
                "run", "pool_has_credentials", "revalidate_compression_feasibility"): return {}
    if name == "profile_name_for_home": return {"home": a[0]}
    if name == "load_route_pool": return {"provider": a[0], "base_url": a[1]}
    if name == "load_pool": return {"provider": a[0]}
    if name in ("pool_has_available", "pool_next_available_at", "hint_fn"): return {"model": k.get("model")}
    if name == "model_dump": return dict(k)
    if name.startswith("logger."): return {"fmt": a[0], "args": list(a[1:])}
    if name == "get_model_context_length": return {"model": a[0], **k}
    if name == "compressor.update_model": return dict(k)
    if name == "resolve_reasoning_config": return {"config": a[0], "model": a[1]}
    if name == "custom_provider_extra_body_for_agent": return dict(k)
    if name == "merge_custom_provider_extra_body": return {"custom_providers": a[1]}
    if name == "parse_api_mode": return {"value": a[0]}
    if name == "get_named_custom_provider": return {"provider": a[0]}
    if name == "host_mandated_api_mode": return {"url": a[0]}
    if name == "relay_llm.execute_current":
        return {"request": a[0], "callback": "agent._interruptible_api_call",
                "name": k["name"], "model_name": k["model_name"], "metadata": k["metadata"],
                "defer_logical_completion": k["defer_logical_completion"]}
    if name == "is_router_timeout_shim": return {"response": a[0]}
    if name == "transport.normalize_response": return {"response": a[0], **k}
    if name == "emit_stream_end": return dict(k)
    if name == "stream_converse_with_callbacks": return {"kwargs": a[0]}
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


def ns_recursive(v):
    if isinstance(v, dict):
        return SimpleNamespace(**{k: ns_recursive(x) for k, x in v.items()})
    if isinstance(v, list):
        return [ns_recursive(x) for x in v]
    return v


def make_pool(raw):
    if not isinstance(raw, dict):
        return raw
    p = SimpleNamespace()
    p._src = raw
    p.provider = raw.get("provider", "")
    p.has_credentials = CURRENT["rec"]("pool_has_credentials")
    p.has_available = CURRENT["rec"]("pool_has_available")
    p.next_available_at = CURRENT["rec"]("pool_next_available_at")
    return p


def pool_proj(p):
    if p is None:
        return None
    if isinstance(p, SimpleNamespace) and hasattr(p, "_src"):
        return p._src
    return norm_value(p)


def materialize(name, raw):
    if name in ("load_pool", "load_route_pool"):
        return make_pool(raw)
    if name == "transport.normalize_response":
        if isinstance(raw, dict):
            d = {"tool_calls": None, "content": None}
            d.update(raw)
            return SimpleNamespace(**d)
        return raw
    if name == "run":
        return ns_recursive(raw)
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


class EnvironDouble:
    def __contains__(self, name):
        return CURRENT["rec"]("env_has")(name)
    def get(self, name, default=None):
        return CURRENT["rec"]("env_get")(name, default)


class APIErrorStub(Exception):
    pass


# ---- live namespace seeding ----
NS = {}


def _exec_seg(src, node, ns):
    seg = ast.get_source_segment(src, node)
    import textwrap
    seg = textwrap.dedent(seg)
    code = compile(seg, SRC, "exec", flags=ANN_FLAG, dont_inherit=True)
    exec(code, ns)


def load_ns():
    global NS
    mod = types.ModuleType("cch2_live")
    sys.modules["cch2_live"] = mod
    ns = mod.__dict__
    NS = ns
    ns.update({
        "__name__": "cch2_live", "contextlib": contextlib, "re": _re, "enum": enum,
        "os": SimpleNamespace(environ=EnvironDouble()),
        "time": SimpleNamespace(time=_stub_fn("time.time"), monotonic=_stub_fn("time.monotonic"),
                                sleep=_stub_fn("time.sleep")),
        "logging": SimpleNamespace(debug=lambda *a, **k: None),
        "logger": RecLogger(), "SimpleNamespace": SimpleNamespace, "json": json,
        "EmptyStreamError": EmptyStreamError,
        "Any": object, "Optional": object,
    })
    src = Path(SRC).read_text(encoding="utf-8")
    tree = ast.parse(src)
    # module constants used by targets
    want_const = {"_SSE_CONN_PHRASES", "HIGH_EFFORT_SILENCE_FLOOR_SECONDS", "_FALLBACK_EXHAUSTED_COOLDOWN_S"}
    for node in tree.body:
        if isinstance(node, ast.Assign):
            names = {t.id for t in node.targets if isinstance(t, ast.Name)}
            if names & want_const:
                _exec_seg(src, node, ns)
    # FailoverReason enum + empty-frame code from error_classifier
    ec_src = (QUARRY / "agent" / "error_classifier.py").read_text(encoding="utf-8")
    ec_tree = ast.parse(ec_src)
    for node in ec_tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "FailoverReason":
            seg = ast.get_source_segment(ec_src, node)
            exec(compile(seg, "error_classifier.py", "exec", flags=ANN_FLAG, dont_inherit=True), ns)
        if isinstance(node, ast.Assign):
            names = {t.id for t in node.targets if isinstance(t, ast.Name)}
            if "PROVIDER_STREAM_EMPTY_FRAME_ERROR_CODE" in names:
                seg = ast.get_source_segment(ec_src, node)
                exec(compile(seg, "error_classifier.py", "exec"), ns)
    # EFFORT_LADDER from reasoning_effort
    re_src = (QUARRY / "agent" / "reasoning_effort.py").read_text(encoding="utf-8")
    re_tree = ast.parse(re_src)
    for node in re_tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgt = node.targets[0] if isinstance(node, ast.Assign) else node.target
            if isinstance(tgt, ast.Name) and tgt.id == "EFFORT_LADDER":
                seg = ast.get_source_segment(re_src, node)
                exec(compile(seg, "reasoning_effort.py", "exec"), ns)
    # _RATE_LIMIT_FAILOVER_REASONS from fallback_cooldown (needs FailoverReason)
    fc_src = (QUARRY / "agent" / "fallback_cooldown.py").read_text(encoding="utf-8")
    fc_tree = ast.parse(fc_src)
    for node in fc_tree.body:
        if isinstance(node, ast.Assign):
            names = {t.id for t in node.targets if isinstance(t, ast.Name)}
            if "_RATE_LIMIT_FAILOVER_REASONS" in names:
                seg = ast.get_source_segment(fc_src, node)
                exec(compile(seg, "fallback_cooldown.py", "exec"), ns)
    # target defs
    found = set()
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in FUNCS:
            _exec_seg(src, node, ns)
            found.add(node.name)
    missing = set(FUNCS) - found
    if missing:
        raise RuntimeError("missing target defs: %s" % sorted(missing))
    # claimed siblings + module-global I/O recorders
    for live, seam in {**SEAM_SIBLINGS, **SEAM_GLOBALS}.items():
        ns[live] = _stub_fn(seam)
    # in-function import stubs. OVERWRITE the parent packages (not setdefault):
    # the quarry is importable in the venv, so a stale real package would let
    # live submodule imports leak past the stubs (enum-class identity drift).
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
    # EFFORT_LADDER real constant into its stub module
    sys.modules["agent.reasoning_effort"].EFFORT_LADDER = ns["EFFORT_LADDER"]
    # _RATE_LIMIT_FAILOVER_REASONS real constant (built from NS FailoverReason so
    # enum membership agrees with reason_obj's class)
    fc = types.ModuleType("agent.fallback_cooldown")
    fc._RATE_LIMIT_FAILOVER_REASONS = ns["_RATE_LIMIT_FAILOVER_REASONS"]
    sys.modules["agent.fallback_cooldown"] = fc
    agent_pkg.fallback_cooldown = fc
    # openai stub for _is_sse_connection_error
    openai = types.ModuleType("openai")
    openai.APIError = APIErrorStub
    sys.modules["openai"] = openai
    # audit: every name the drivers read is bound
    for nm in ("FailoverReason", "EFFORT_LADDER", "_RATE_LIMIT_FAILOVER_REASONS",
               "PROVIDER_STREAM_EMPTY_FRAME_ERROR_CODE", "_SSE_CONN_PHRASES",
               "HIGH_EFFORT_SILENCE_FLOOR_SECONDS", "_FALLBACK_EXHAUSTED_COOLDOWN_S", "EmptyStreamError"):
        if nm not in ns:
            raise RuntimeError("seed missing: " + nm)
    return ns


def make_agent(inp):
    ad = inp.get("agent") or {}
    attrs = {}
    for k, v in ad.items():
        if k == "_credential_pool":
            attrs[k] = make_pool(v) if isinstance(v, dict) else v
        elif k == "context_compressor":
            if isinstance(v, dict):
                c = SimpleNamespace(**v)
                c.update_model = CURRENT["rec"]("compressor.update_model")
                attrs[k] = c
            else:
                attrs[k] = v
        else:
            attrs[k] = v
    agent = SimpleNamespace(**attrs)
    if inp.get("hint_callable"):
        agent._codex_silent_hang_hint = CURRENT["rec"]("hint_fn")
    if ad.get("_emit_stream_start") is not None:
        agent._emit_stream_start = CURRENT["rec"]("emit_stream_start")
    if ad.get("_emit_stream_end") is not None:
        agent._emit_stream_end = CURRENT["rec"]("emit_stream_end")
    transport = SimpleNamespace()
    transport.normalize_response = CURRENT["rec"]("transport.normalize_response")
    agent._get_transport = lambda: transport
    return agent


def make_exc_obj(inp):
    ed = inp.get("exc") or {}
    msg = ed.get("message", "")
    if inp.get("is_api_error"):
        exc = APIErrorStub(msg)
    else:
        exc = Exception(msg)
    if "status_code" in ed:
        exc.status_code = ed["status_code"]
    if "body" in ed:
        exc.body = ed["body"]
    return exc


def reason_obj(inp):
    r = inp.get("reason")
    if r is None:
        return None
    return NS["FailoverReason"][r]


def drive(func, inp):
    """Return (ret, st). Raises on live exception (caught by run_case)."""
    agent = make_agent(inp)
    st = {}
    if func == "_is_provider_stream_empty_frame_error":
        return NS[func](make_exc_obj(inp)), st
    if func == "_rejects_stream_options":
        return NS[func](make_exc_obj(inp)), st
    if func == "_is_sse_connection_error":
        return NS[func](make_exc_obj(inp)), st
    if func == "_relay_stream_identity":
        return NS[func](agent, inp.get("name_default")), st
    if func == "_relay_stream_metadata":
        return NS[func](agent, inp.get("api_mode")), st
    if func == "_stream_final_text":
        return NS[func](ns_recursive(inp.get("response"))), st
    if func == "_high_effort_silence_floor":
        return NS[func](agent), st
    if func == "_consume_ephemeral_max_output":
        ret = NS[func](agent)
        st["_ephemeral_max_output_tokens"] = getattr(agent, "_ephemeral_max_output_tokens", None)
        return ret, st
    if func == "_model_dump_safe":
        obj = SimpleNamespace(model_dump=CURRENT["rec"]("model_dump"))
        return NS[func](obj), st
    if func == "_dump_if_model":
        if inp.get("has_model_dump"):
            val = SimpleNamespace(model_dump=CURRENT["rec"]("model_dump"))
        else:
            val = inp.get("value")
        return NS[func](val), st
    if func == "_codex_silent_hang_hint":
        return NS[func](agent, inp.get("api_kwargs") or {}), st
    if func == "_stream_env_stale_base":
        return NS[func](), st
    if func == "_local_stream_stale_timeout_default":
        return NS[func](), st
    if func == "cap_to_run_budget":
        return NS[func](agent, inp.get("timeout")), st
    if func == "_cloud_stale_timeout":
        return NS[func](inp.get("base"), inp.get("api_kwargs") or {}), st
    if func == "_stream_stale_timeout_is_explicit":
        return NS[func](agent), st
    if func == "_cloud_stale_timeout_for":
        return NS[func](agent, inp.get("api_kwargs") or {}), st
    if func == "_fallback_chain_exhausted":
        ret = NS[func](agent, reason_obj(inp))
        st["_rate_limited_until"] = getattr(agent, "_rate_limited_until", 0) or 0
        return ret, st
    if func == "_log_fallback_activated":
        return NS[func](agent, reason_obj(inp), inp.get("old_model"), inp.get("old_provider"),
                        inp.get("fb_model"), inp.get("fb_provider")), st
    if func == "_rebind_fallback_credential_pool":
        pp = inp.get("prefetched_pool")
        pp = make_pool(pp) if isinstance(pp, dict) else pp
        ret = NS[func](agent, inp.get("fb_provider"), inp.get("fb_model"), prefetched_pool=pp)
        st["_credential_pool"] = pool_proj(getattr(agent, "_credential_pool", None))
        st["_credential_pool_entry_id"] = getattr(agent, "_credential_pool_entry_id", None)
        return ret, st
    if func == "_candidate_pool_exhausted":
        return NS[func](agent, inp.get("fb_provider"), inp.get("fb_model")), st
    if func == "_update_fallback_context_compressor":
        return NS[func](agent), st
    if func == "_reresolve_fallback_reasoning_config":
        ret = NS[func](agent)
        st["reasoning_config"] = getattr(agent, "reasoning_config", None)
        return ret, st
    if func == "_rescope_fallback_extra_body":
        ret = NS[func](agent, inp.get("old_model"), inp.get("old_provider"), inp.get("old_base_url"))
        st["request_overrides"] = getattr(agent, "request_overrides", {})
        return ret, st
    if func == "_fallback_api_mode_hint":
        return NS[func](inp.get("fb") or {}, inp.get("fb_provider"), inp.get("fb_base_url_hint")), st
    if func == "_is_anthropic_wire_url":
        return NS[func](inp.get("url")), st
    if func == "_managed_summary_call":
        return NS[func](agent, inp.get("api_request_id"), inp.get("request"),
                        lambda *a, **k: None, retry_count=inp.get("retry_count")), st
    if func == "_summary_text":
        return NS[func](agent, inp.get("response"), **(inp.get("normalize_kwargs") or {})), st
    if func == "_with_stream_emitters":
        return NS[func](agent, CURRENT["rec"]("run")), st
    if func == "_finalize_bedrock_relay_events":
        return NS[func](inp.get("events") or []), st
    raise RuntimeError("unknown func " + func)


def run_case(func, inp):
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
        # normalize got integral floats too (parsed C JSON already int-like)
        got = norm_value(got)
        if got != want:
            mism += 1
            if mism <= 12:
                print("MISMATCH [%s] case %d" % (func, n))
                for key in ("ret", "trace", "st"):
                    if got.get(key) != want.get(key):
                        print("  %s GOT : %s" % (key, json.dumps(got.get(key), ensure_ascii=False)[:600]))
                        print("  %s WANT: %s" % (key, json.dumps(want.get(key), ensure_ascii=False)[:600]))
    print("ORACLE chat_completion_helpers_flow: %d cases, %d mismatches" % (n, mism))
    if n == 0:
        sys.exit(1)
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()