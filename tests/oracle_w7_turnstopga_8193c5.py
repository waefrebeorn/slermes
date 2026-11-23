#!/usr/bin/env python3
"""Differential oracle for w7_turnstopga_8193c5 -> agent/turn_stop_gates.py.

Live module loaded via spec_from_file_location; all lazily imported nudge
sources and append_message are stubbed so apply_stop_gates runs deterministically
with the same seams the C harness receives.
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


def _pkg(name):
    m = types.ModuleType(name)
    m.__path__ = []
    sys.modules[name] = m
    return m


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


_pkg("agent")
_pkg("hermes_cli")
_pkg("agent.lsp")
SEAMS = {}          # reset per case
_mod("agent.message_metadata", append_message=lambda msgs, msg: msgs.append(msg))
_mod("agent.verification_stop",
     verify_on_stop_enabled=lambda: True,
     build_verify_on_stop_nudge=lambda **kw: SEAMS.get("verify_nudge"))
_mod("agent.verify_hooks", max_verify_nudges=lambda: 9999)
_mod("hermes_cli.lifecycle", has_hook=lambda name: True)
_mod("hermes_cli.plugins",
     get_pre_verify_continue_message=lambda **kw: SEAMS.get("pre_verify_nudge"))
_mod("agent.coding_context", is_coding_context=lambda **kw: False)
_mod("agent.kanban_stop",
     build_kanban_stop_nudge=lambda **kw: SEAMS.get("kanban_nudge"))
_mod("agent.lsp.deferred", settle_for_agent=lambda a: SEAMS.get("lsp_notes"))

SRC = Q + "/agent/turn_stop_gates.py"
spec = importlib.util.spec_from_file_location("turnstopga_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["turnstopga_live"] = live
spec.loader.exec_module(live)


class FakeAgent:
    def __init__(self, r):
        self._lsp_turn_end_notes = int(r.get("lsp_issued") or 0)
        self._verification_stop_nudges = 0
        self._pre_verify_nudges = 0
        self._kanban_stop_nudges = 0
        self._turn_file_mutation_paths = set(["changed.py"])
        self._resolved_is_coding = False
        self._interim_stream = bool(r.get("interim_streamed", False))
        self.session_id = "s"
        self.platform = "cli"
        self.model = "m"
        self.emit_count = 0
        self.emit_finish_reason = ""
        self.flush_attempts = 0
        self.diagnostic_count = 0
        self._session_messages = None

    def _emit_interim_assistant_message(self, m):
        self.emit_count += 1
        self.emit_finish_reason = m.get("finish_reason", "")

    def _flush_messages_to_session_db(self, m, h):
        self.flush_attempts += 1
        if SEAMS.get("flush_fail"):
            raise RuntimeError("flush failed")

    def _interim_content_was_streamed(self, x):
        return self._interim_stream

    def _emit_diagnostic_status(self, s):
        self.diagnostic_count += 1


def canonical(x):
    if x is None or isinstance(x, (bool, str, int)):
        return x
    if isinstance(x, float):
        return int(x) if x.is_integer() else x
    if isinstance(x, dict):
        return {k: canonical(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [canonical(v) for v in x]
    return x


def run(r):
    SEAMS.clear()
    SEAMS["verify_nudge"] = r.get("verify_nudge")
    SEAMS["pre_verify_nudge"] = r.get("pre_verify_nudge")
    SEAMS["kanban_nudge"] = r.get("kanban_nudge")
    SEAMS["lsp_notes"] = r.get("lsp_notes")
    SEAMS["flush_fail"] = bool(r.get("flush_fail", False))
    agent = FakeAgent(r)
    final_msg = json.loads(json.dumps(r.get("final_msg") or {}))
    messages = json.loads(json.dumps(r.get("messages") or []))
    v = live.apply_stop_gates(
        agent, final_msg, final_response=r.get("final_response", ""),
        messages=messages, conversation_history=[],
        pending_verification_response=r.get("pending_verification_response", ""),
        pending_verification_response_previewed=r.get("pending_verification_response_previewed", False))
    return {
        "verdict": {
            "continue_turn": v.continue_turn,
            "final_response": v.final_response,
            "pending_verification_response": v.pending_verification_response,
            "pending_verification_response_previewed": v.pending_verification_response_previewed,
        },
        "final_msg": final_msg,
        "messages": messages,
        "lsp_issued": agent._lsp_turn_end_notes,
        "verification_stop_nudges": agent._verification_stop_nudges,
        "pre_verify_nudges": agent._pre_verify_nudges,
        "kanban_stop_nudges": agent._kanban_stop_nudges,
        "emit_count": agent.emit_count,
        "emit_finish_reason": agent.emit_finish_reason,
        "flush_attempts": agent.flush_attempts,
        "diagnostic_count": agent.diagnostic_count,
    }


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") != "apply":
        continue
    try:
        py = canonical(run(r))
    except Exception as exc:
        print(f"LIVE ERROR args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = canonical(r.get("out"))
    if py != got:
        mism += 1
        print(f"MISMATCH args={r}\n  py ={py!r}\n  c  ={got!r}", file=sys.stderr)

print(f"ORACLE w7_turnstopga_8193c5: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
