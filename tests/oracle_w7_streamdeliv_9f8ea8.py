#!/usr/bin/env python3
"""Differential oracle for w7_streamdeliv_9f8ea8 -> agent/stream_delivery.py.

Live module loaded via spec_from_file_location; its four module-level imports
are registered as stubs first. Think-strip is seamed to identity.
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
_pkg("session_rules")
_mod("agent.memory_context", fence_recalled_context=lambda s: s)
_mod("session_rules.turns",
     sanitize_context=lambda s: s,
     flatten_message_text=lambda c: c if isinstance(c, str) else "")
_mod("agent.history_commentary",
     COMMENTARY_WITHHELD="_commentary_withheld",
     commentary_items=lambda m: [], visible_commentary=lambda s, strip_thinking: s)

SRC = Q + "/agent/stream_delivery.py"
spec = importlib.util.spec_from_file_location("streamdeliv_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["streamdeliv_live"] = live
spec.loader.exec_module(live)


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


class Fake(live.StreamDeliveryMixin):
    def __init__(self, streamed):
        self._current_streamed_assistant_text = streamed or ""
        self._strip_think_blocks = lambda s: s


def fake_for(r):
    return Fake(r.get("streamed", ""))


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
        if func == "normalize":
            py = live.StreamDeliveryMixin._normalize_interim_visible_text(r.get("value"))
        elif func == "was_streamed":
            py = fake_for(r)._interim_content_was_streamed(r.get("content", ""))
        elif func == "fully_streamed":
            py = fake_for(r)._interim_content_fully_streamed(r.get("content", ""))
        elif func == "codex_texts":
            py = live._codex_commentary_texts(types.SimpleNamespace(show_commentary=True),
                                              r.get("assistant_msg") or {})
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = canonical(r.get("out"))
    if canonical(py) != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={canonical(py)!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w7_streamdeliv_9f8ea8: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
