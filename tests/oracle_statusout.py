#!/usr/bin/env python3
"""Differential oracle for agent/status_output.py (statusout gate).

The real StatusOutputMixin methods are called on a minimal fake agent whose
predicate results are injected per case; the recorded print sink tells us
whether the gate let the message through. Module-level imports are stubbed.
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


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


agent = _mod("agent")
agent.__path__ = []
_mod("agent.i18n", t=lambda *a, **k: "")
_mod("agent.session_activity",
     ActivityProvenance=SimpleNamespace(AGENT_COMPRESSION_COOLDOWN="cooldown"))

SRC = Q + "/agent/status_output.py"
spec = importlib.util.spec_from_file_location("statusout_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["statusout_live"] = live
spec.loader.exec_module(live)

M = live.StatusOutputMixin


class Rec:
    def __init__(self):
        self.calls = []

    def __call__(self, *a, **k):
        self.calls.append((a, k))


def fake(r, rec):
    f = object.__new__(M)
    f.suppress_status_output = bool(r.get("suppress", False))
    f.quiet_mode = bool(r.get("quiet_mode", False))
    f.tool_progress_callback = (lambda: None) if r.get("tool_progress_cb") else None
    f.platform = r.get("platform", "cli")
    f._print_fn = rec
    f._mute_notification_reply = bool(r.get("mute_notification", False))
    f._warning_presentation_enabled = lambda: bool(r.get("warning_enabled", True))
    f._mute_post_response = bool(r.get("mute_post", False))
    f._has_stream_consumers = lambda: bool(r.get("has_consumers", False))
    f._executing_tools = bool(r.get("executing", False))
    return f


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    try:
        if func == "quiet_tool_messages":
            py = bool(M._should_emit_quiet_tool_messages(fake(r, Rec())))
        elif func == "quiet_spinner":
            f = object.__new__(M)
            f._print_fn = (lambda *a, **k: None) if r.get("has_print_fn") else None
            live.sys = SimpleNamespace(stdout=SimpleNamespace(isatty=lambda: bool(r.get("isatty", False))))
            py = bool(M._should_start_quiet_spinner(f))
        elif func == "safe_print":
            rec = Rec()
            M._safe_print(fake(r, rec), "msg", diagnostic=bool(r.get("diagnostic", False)))
            py = bool(rec.calls)
        elif func == "vprint":
            rec = Rec()
            M._vprint(fake(r, rec), "msg", force=bool(r.get("force", False)),
                      diagnostic=bool(r.get("diagnostic", False)))
            py = bool(rec.calls)
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE statusout: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
