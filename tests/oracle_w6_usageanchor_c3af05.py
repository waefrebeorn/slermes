#!/usr/bin/env python3
"""Oracle for upstream Hermes agent/usage_anchor.py (gate w6_usageanchor_c3af05).

The delta token estimator is stubbed with the fixture's injected integer so
the anchored arithmetic is compared without importing the full model stack.
"""
import importlib.util
import json
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)

# parent-package bootstrap + stub for the lazy model-metadata import
ag = types.ModuleType("agent")
ag.__path__ = [QUARRY + "/agent"]
sys.modules["agent"] = ag
mm = types.ModuleType("agent.model_metadata")
DELTA = [0]
mm.estimate_messages_tokens_rough = lambda delta, charge_stale_thinking=True: DELTA[0]
sys.modules["agent.model_metadata"] = mm

spec = importlib.util.spec_from_file_location(
    "live_usage_anchor", QUARRY + "/agent/usage_anchor.py")
live = importlib.util.module_from_spec(spec)
sys.modules["live_usage_anchor"] = live
spec.loader.exec_module(live)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    func = r.get("func")
    if func == "fp":
        py = live.message_fingerprint(r.get("msg"))
    elif func == "capture":
        py = live.capture_usage_anchor(r.get("prompt_tokens"),
                                       r.get("completion_tokens"), r.get("messages"))
    elif func == "match":
        py = live._anchor_matches(r.get("messages"), r.get("anchor"))
    elif func == "ser":
        py = live._serialize(r.get("anchor"))
    elif func == "ctx":
        DELTA[0] = int(r.get("delta_estimate", 0))
        py = live.anchored_context_tokens(r.get("messages"), r.get("anchor"))
    else:
        continue
    got = r.get("out")
    cases += 1
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (func, r.get("case"), py, got), file=sys.stderr)

print("ORACLE w6_usageanchor_c3af05: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
