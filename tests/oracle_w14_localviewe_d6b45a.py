#!/usr/bin/env python3
"""Differential oracle for gateway/local_viewer_events.py (gate w14_localviewe_d6b45a).

The LIVE publish_progress runs against a fake turn that captures
_publish_execution calls. show_reasoning is patched on the module and
gateway.session_cli_turn is registered in sys.modules (parent-package
bootstrap) so the injected classic_cli_turn is what the live function imports.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
SRC = "C:/Users/eman5/hermes-agent/gateway/local_viewer_events.py"
spec = importlib.util.spec_from_file_location("w14_lve_live", SRC)
live = importlib.util.module_from_spec(spec)
spec.loader.exec_module(live)


class Turn:
    def __init__(self):
        self.events = []

    def _publish_execution(self, event_type, payload):
        self.events.append((event_type, payload))


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") != "publish_progress":
        continue

    turn = Turn()
    live.show_reasoning = lambda t: bool(r.get("show_reasoning", False))
    fake = types.ModuleType("gateway.session_cli_turn")
    fake.classic_cli_turn = lambda t: bool(r.get("classic_cli", False))
    sys.modules["gateway.session_cli_turn"] = fake
    if "gateway" in sys.modules:
        setattr(sys.modules["gateway"], "session_cli_turn", fake)

    live.publish_progress(turn, r.get("event_type", ""), r.get("name", ""),
                          r.get("preview", ""), r.get("kwargs") or {})
    want = {
        "emit": bool(turn.events),
        "event_type": turn.events[0][0] if turn.events else None,
        "payload": turn.events[0][1] if turn.events else None,
    }
    got = r.get("out")
    cases += 1
    if want != got:
        mism += 1
        print(f"MISMATCH in={r} py={want!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w14_localviewe_d6b45a: {cases} cases, {mism} mismatches")
sys.exit(1 if mism or cases == 0 else 0)
