#!/usr/bin/env python3
"""Differential oracle for agent/reasoning_params.py (w8_reasoningpa_062303 gate).

Collaborators are injected from fixtures (family match map, host-match list,
profile / probe results, thought-signature probe); LIVE defs are driven with the
same inputs and compared to the C harness output.
"""
import importlib.util
import json
import sys
import types

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

STATE = {"matches": [], "family_matches": {}, "keep_signature": False,
         "ask": None, "caps": None}


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


def _family(fam, prov, model, url):
    key = "%s|%s|%s|%s" % (fam, prov or "", model or "", url or "")
    return bool(STATE["family_matches"].get(key, False))


def _host(url, host):
    return host in STATE["matches"]


agent = _mod("agent")
agent.__path__ = []
_mod("agent.lazy_forward",
     forward=lambda *a, **k: (lambda self, *aa, **kk: None),
     forward_static=lambda *a, **k: (lambda *aa, **kk: None),
     lazy_attr=lambda *a, **k: (lambda: None))
_mod("agent.message_sanitization",
     matches_reasoning_echo_family=_family,
     reasoning_replay_route=lambda *a, **k: types.SimpleNamespace(pad=False),
     rejected_reasoning_carriers=lambda s: [])
utils = _mod("utils", base_url_host_matches=_host)
hc = _mod("hermes_cli")
hc.__path__ = []
tr = _mod("agent.transports")
tr.__path__ = []
_mod("agent.transports.chat_completions",
     _model_consumes_thought_signature=lambda *a, **k: STATE["keep_signature"])
ph = _mod("agent.provider_route_hooks", ask_profile=lambda *a, **k: STATE["ask"])
_mod("hermes_cli.models_reasoning_caps",
     openrouter_model_reasoning_capabilities=lambda m: STATE["caps"],
     warm_openrouter_reasoning_caps_async=lambda: None)

SRC = Q + "/agent/reasoning_params.py"
spec = importlib.util.spec_from_file_location("w8_reasoningpa_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w8_reasoningpa_live"] = live
spec.loader.exec_module(live)
Mixin = live.ReasoningParamsMixin


def make_obj(args):
    o = Mixin.__new__(Mixin)
    o.provider = args.get("provider", "")
    o.base_url = args.get("base_url", "")
    o.model = args.get("model", "")
    o._base_url_lower = args.get("base_url_lower", args.get("base_url", "") or "")
    o._reasoning_endpoint_profile = lambda: args.get("profile")
    o._is_openrouter_url = lambda: bool(args.get("is_openrouter", False))
    o._lmstudio_reasoning_options_cached = lambda: args.get("lmstudio_options") or []
    o._ollama_supports_thinking_cached = lambda: bool(args.get("ollama_thinking", False))
    return o


def drive(r):
    func = r.get("func")
    if func == "should_sanitize":
        return Mixin._should_sanitize_tool_calls(
            types.SimpleNamespace(api_mode=r.get("api_mode")))
    if func == "echo_opt_in":
        ns = types.SimpleNamespace()
        if "flag" in r:
            ns._reasoning_echo_flag = r.get("flag")
        return Mixin._reasoning_echo_opt_in(ns)
    if func in ("kimi", "deepseek", "mimo"):
        a = r.get("args") or {}
        STATE["family_matches"] = a.get("family_matches") or {}
        o = types.SimpleNamespace(provider=a.get("provider"), model=a.get("model"),
                                  base_url=a.get("base_url"))
        if func == "kimi":
            return Mixin._needs_kimi_tool_reasoning(o)
        if func == "deepseek":
            return Mixin._needs_deepseek_tool_reasoning(o)
        return Mixin._needs_mimo_tool_reasoning(o)
    if func == "sanitize":
        STATE["keep_signature"] = bool(r.get("keep_signature", False))
        return Mixin._sanitize_tool_calls_for_strict_api(r.get("api_msg"))
    if func == "supports":
        a = r.get("args") or {}
        STATE["matches"] = a.get("matches") or []
        STATE["ask"] = a.get("ask_profile_value")
        STATE["caps"] = a.get("caps")
        return make_obj(a)._supports_reasoning_extra_body()
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    try:
        py = drive(r)
    except Exception as exc:
        print("LIVE ERROR %s: %r" % (r.get("func"), exc), file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s py=%r c=%r" % (r.get("func"), py, got), file=sys.stderr)

print("ORACLE w8_reasoningpa_062303: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if (mism or cases == 0) else 0)
