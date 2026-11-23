#!/usr/bin/env python3
"""Differential oracle for agent/turn_recovery.py (gate w6_turnrecover_012061).

_image_error_max_dimension is pure (str/re/math only); all of turn_recovery's
module-level imports are stubbed so the LIVE function can be loaded in
isolation. A fake exception supplies str(exc), .message and .body.
"""
import importlib.util
import json
import sys
import types
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _stub(name, **attrs):
    parts = name.split(".")
    for i in range(1, len(parts)):
        pn = ".".join(parts[:i])
        if pn not in sys.modules:
            pm = types.ModuleType(pn)
            pm.__path__ = [Q + "/" + "/".join(parts[:i])]
            sys.modules[pn] = pm
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


_d = lambda *a, **k: None
_stub("agent")
_stub("agent.conversation_compression", COMPRESSION_RETRY_CONTEXT_REDUCED_STATUS_TEMPLATE="")
_stub("agent.fast_mode", fast_mode_unprovisioned=_d, mark_fast_mode_unavailable=_d)
_stub("agent.model_metadata", is_output_cap_error=_d, parse_available_output_tokens_from_error=_d)
_stub("agent.retry_utils", is_zai_coding_overload_error=_d, zai_coding_overload_retry_ceiling=_d)
class _FailoverReason:
    """Hashable stand-in; module-level frozensets/dicts need unique keys."""
    def __getattr__(self, name):
        return name


_stub("agent.error_classifier", FailoverReason=_FailoverReason(), classify_api_error=_d)
_stub("agent.message_sanitization",
      _looks_like_corrupt_image_rejection=_d, _looks_like_image_content_rejection=_d,
      _messages_carry_images=_d, _sanitize_messages_non_ascii=_d,
      _sanitize_messages_surrogates=_d, _sanitize_structure_non_ascii=_d,
      _sanitize_structure_surrogates=_d, _strip_images_from_messages=_d,
      _strip_non_ascii=_d, close_interrupted_tool_sequence=_d,
      record_reasoning_field_rejection=_d)
_stub("agent.thinking_timeout_guidance", build_thinking_timeout_guidance=_d, is_thinking_timeout=_d)
_stub("agent.vision_message_prep", _provider_model_key=_d)
_stub("agent.turn_failure_copy",
      CONTENT_POLICY_NEXT_STEPS="", content_policy_copy=_d, exhausted_copy=_d,
      limit_reset_copy=_d, nonretryable_copy=_d, provider_label_for=_d,
      site_copy=_d, stamp_failure=_d)
_stub("agent.route_verdicts", route_visit=_d)
_stub("agent.turn_retry_state", TurnRetryState=object)
_stub("agent.turn_recovery_codex",
      _codex_soft_failure_error=_d, _is_codex_token_expired=_d,
      _recover_stale_codex_reasoning=_d)
_stub("hermes_constants", display_hermes_home="")
_stub("utils", base_url_host_matches=_d)

LIVE_NAME = "w6_turnrecover_012061_live"
spec = importlib.util.spec_from_file_location(LIVE_NAME, Q + "/agent/turn_recovery.py")
live = importlib.util.module_from_spec(spec)
sys.modules[LIVE_NAME] = live
spec.loader.exec_module(live)


class FakeError(Exception):
    def __init__(self, text, message, body):
        super().__init__(text)
        self._text = text
        self.message = message
        self.body = body

    def __str__(self):
        return self._text


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") != "image_dim":
        continue

    err = FakeError(r.get("error") or "", r.get("message") or None,
                    r.get("body") or None)
    py = live._image_error_max_dimension(err)

    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH image_dim args=%r py=%r c=%r" % (r, py, got), file=sys.stderr)

print("ORACLE w6_turnrecover_012061: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
