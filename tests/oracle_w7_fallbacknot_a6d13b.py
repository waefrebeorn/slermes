#!/usr/bin/env python3
"""Differential oracle for agent/fallback_notice.py (gate w7_fallbacknot_a6d13b).

Loads the LIVE module by path under its package name. FailoverReason, the
identity redactor and the machinery-kind predicate are stubbed with the exact
live values (all read from the quarry); the label / whitespace / truncation
logic is exercised directly on the live functions.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


class _Attr:
    def __init__(self, **kw):
        self.__dict__.update(kw)


class _Reason:
    def __init__(self, name):
        self.name = name
        self.value = name

    def __repr__(self):
        return "FailoverReason." + self.name


class _FailoverReason:
    pass


_NAMES = ("auth auth_permanent billing rate_limit upstream_rate_limit upstream_blocked "
          "overloaded server_error timeout ssl_cert_verification context_overflow "
          "payload_too_large image_too_large image_corrupt model_not_found "
          "provider_policy_blocked data_policy_denied content_policy_blocked "
          "guardrail_blocked model_entitlement incomplete_response format_error "
          "role_alternation invalid_encrypted_content multimodal_tool_content_unsupported "
          "reasoning_mandatory thinking_signature long_context_tier "
          "oauth_long_context_beta_forbidden llama_cpp_grammar_pattern unknown").split()
for _n in _NAMES:
    setattr(_FailoverReason, _n, _Reason(_n))

agent_pkg = _mod("agent")
agent_pkg.__path__ = [Q + "/agent"]
_mod("agent.error_classifier", FailoverReason=_FailoverReason)
_mod("agent.redact", redact_sensitive_text=lambda s: s)
gw_pkg = _mod("gateway")
gw_pkg.__path__ = [Q + "/gateway"]
_mod("gateway.response_filters",
     is_machinery_display_kind=lambda d: d in
     {"internal_notification", "process_complete", "async_delegation_complete"})

SRC = Q + "/agent/fallback_notice.py"
spec = importlib.util.spec_from_file_location("agent.fallback_notice", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["agent.fallback_notice"] = live
spec.loader.exec_module(live)


def _canon(x):
    if x is None: return ("null",)
    if isinstance(x, bool): return ("bool", x)
    if isinstance(x, (int, float)): return ("num", float(x))
    if isinstance(x, str): return ("str", x)
    if isinstance(x, (list, tuple)): return ("list", [_canon(v) for v in x])
    if isinstance(x, dict): return ("obj", sorted((k, _canon(v)) for k, v in x.items()))
    return ("other", repr(x))


def _reason(name):
    if name is None:
        return None
    return getattr(_FailoverReason, name, _Reason(name))


def compute(r):
    f = r.get("func")
    if f == "constants":
        return {"FALLBACK_STATUS_KIND": live.FALLBACK_STATUS_KIND,
                "FALLBACK_DETAIL_MAX_CHARS": live._FALLBACK_DETAIL_MAX_CHARS}
    if f == "reason_text":
        return live.fallback_reason_text(_reason(r.get("reason")))
    if f == "trigger_text":
        agent = _Attr()
        st = r.get("stashed")
        if isinstance(st, dict):
            agent._fallback_trigger = (_reason(st.get("reason")), st.get("detail"))
        return live.fallback_trigger_text(agent, _reason(r.get("reason")))
    if f == "notice_hidden":
        return live.fallback_notice_hidden(_Attr(_turn_display_kind=r.get("display_kind")))
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        want = compute(r)
    except Exception as exc:
        mism += 1
        print("LIVE ERROR %s case=%s: %r" % (r.get("func"), r.get("case"), exc), file=sys.stderr)
        continue
    got = r.get("out")
    if _canon(want) != _canon(got):
        mism += 1
        print("MISMATCH %s case=%s C=%r PY=%r" % (r.get("func"), r.get("case"), got, want),
              file=sys.stderr)
print("ORACLE w7_fallbacknot_a6d13b: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
