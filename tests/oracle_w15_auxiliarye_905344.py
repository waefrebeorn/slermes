#!/usr/bin/env python3
"""Differential oracle for agent/auxiliary_error_policy.py (gate w15_auxiliarye_905344)."""
import enum
import importlib.util
import json
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass


def _pkg(name, path):
    m = types.ModuleType(name)
    m.__path__ = [path]
    sys.modules[name] = m


_pkg("agent", QUARRY + "/agent")

_NAMES = ("auth auth_permanent billing rate_limit upstream_rate_limit model_not_found "
          "model_entitlement incomplete_response overloaded server_error long_context_tier "
          "timeout content_policy_blocked ssl_cert_verification format_error role_alternation "
          "multimodal_tool_content_unsupported reasoning_mandatory llama_cpp_grammar_pattern "
          "image_too_large image_corrupt invalid_encrypted_content thinking_signature "
          "context_overflow payload_too_large oauth_long_context_beta_forbidden unknown "
          "data_policy_denied guardrail_blocked upstream_blocked provider_policy_blocked").split()
FailoverReason = enum.Enum("FailoverReason", {n: n for n in _NAMES})
TRANSIENT_REASONS = frozenset({FailoverReason.timeout, FailoverReason.overloaded,
                               FailoverReason.server_error})
POLICY_REFUSAL_REASONS = frozenset({FailoverReason.data_policy_denied,
                                    FailoverReason.content_policy_blocked,
                                    FailoverReason.guardrail_blocked})


class ClassifiedError:
    def __init__(self, reason, retryable=True, should_fallback=False,
                 should_rotate_credential=False, status_code=None, error_context=None):
        self.reason = FailoverReason[reason] if isinstance(reason, str) else reason
        self.retryable = retryable
        self.should_fallback = should_fallback
        self.should_rotate_credential = should_rotate_credential
        self.status_code = status_code
        self.error_context = error_context or {}


ec = types.ModuleType("agent.error_classifier")
ec.FailoverReason = FailoverReason
ec.ClassifiedError = ClassifiedError
ec.TRANSIENT_REASONS = TRANSIENT_REASONS
ec.POLICY_REFUSAL_REASONS = POLICY_REFUSAL_REASONS
ec.classify_api_error = lambda *a, **kw: ClassifiedError("unknown")
sys.modules["agent.error_classifier"] = ec

aco = types.ModuleType("agent.auxiliary_call_outcome")
aco.is_cancellation = lambda e: False
sys.modules["agent.auxiliary_call_outcome"] = aco

spec = importlib.util.spec_from_file_location(
    "agent.auxiliary_error_policy_live", QUARRY + "/agent/auxiliary_error_policy.py")
live = importlib.util.module_from_spec(spec)
sys.modules["agent.auxiliary_error_policy_live"] = live
spec.loader.exec_module(live)


def _verdict(d):
    d = d or {}
    return ClassifiedError(
        reason=d.get("reason", "unknown"),
        retryable=d.get("retryable", True),
        should_fallback=d.get("should_fallback", False),
        should_rotate_credential=d.get("should_rotate_credential", False),
        status_code=d.get("status_code"),
        error_context=d.get("error_context") or {})


def _failure(r):
    d = r.get("failure") or {}
    return live.Failure(error=None, verdict=_verdict(d.get("verdict")),
                        rejects_parameter=bool(d.get("rejects_parameter", False)), client=None)


PRED = {
    "same_provider_retry": live.same_provider_retry,
    "auth_refresh": live.auth_refresh,
    "same_key_retry": live.same_key_retry,
    "resend_cannot_help": live.resend_cannot_help,
    "benches_provider": live.benches_provider,
    "stream_negotiation_reraises": live.stream_negotiation_reraises,
}


def compute(r):
    f = r.get("func")
    if f in PRED:
        return PRED[f](_verdict(r.get("verdict") or {}))
    if f == "fallback_label":
        return live.fallback_label(_failure(r), rung_spent=bool(r.get("rung_spent", False)))
    if f == "ladder_accepts":
        return live.ladder_accepts(_failure(r), rung_spent=bool(r.get("rung_spent", False)))
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
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        print("PYERROR %r %r" % (r, exc), file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %r py=%r c=%r" % (r, py, got), file=sys.stderr)
print("ORACLE w15_auxiliarye_905344: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
