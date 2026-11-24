#!/usr/bin/env python3
"""Differential oracle for gateway/stream_consumer_redact.py (w18_streamcons_0914fb).

Only pure boundary helpers are ported; redactor imports are stubbed (the ported
functions never call them) so the live module loads deterministically.
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


def _mk(name, **attrs):
    m = types.ModuleType(name)
    m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


for pkg in ("gateway", "agent"):
    if pkg not in sys.modules:
        _mk(pkg)
_mk("agent.redact", _PREFIX_SUBSTRINGS=("AKIA", "sk-", "ghp_"))
_mk("agent.redact_control_split", _strip_shadow=lambda s: (s,))
_mk("agent.redact_phone_policy", redact_for_chat_egress=lambda s: s)

SRC = Q + "/gateway/stream_consumer_redact.py"
spec = importlib.util.spec_from_file_location("w18_streamcons_0914fb_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)

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
        if func == "probe_start":
            py = live._probe_start(r.get("text", ""), r.get("token_start", 0))
        elif func == "lookback_start":
            py = live._lookback_start(r.get("text", ""), r.get("upto", 0))
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} {r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w18_streamcons_0914fb: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
