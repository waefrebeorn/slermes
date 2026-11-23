#!/usr/bin/env python3
"""Differential oracle for agent/replayed_reasoning.py (gate w10_replayedre_d3ae9b).

Imports the LIVE module with a stubbed message_metadata.stamped_reasoning_tokens
seam; the stamped count is injected per case from the fixture.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)

def _pkg(name):
    m = sys.modules.get(name)
    if m is None:
        m = types.ModuleType(name)
        m.__path__ = []
        sys.modules[name] = m
    return m

agent_pkg = _pkg("agent")

# Parent-package bootstrap for `from agent.message_metadata import stamped_reasoning_tokens`.
mm = types.ModuleType("agent.message_metadata")
mm.stamped_reasoning_tokens = lambda msg: None
agent_pkg.message_metadata = mm
sys.modules["agent.message_metadata"] = mm

SRC = QUARRY + "/agent/replayed_reasoning.py"
spec = importlib.util.spec_from_file_location("w10_replayedre_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w10_replayedre_live"] = live
spec.loader.exec_module(live)

STAMP = {"v": None}

def compute(r):
    func = r.get("func")
    STAMP["v"] = r.get("stamp")
    # `from agent.message_metadata import stamped_reasoning_tokens` binds at import
    # time, so patch the live module's own reference, not the stub module.
    live.stamped_reasoning_tokens = lambda msg: STAMP["v"]
    if func == "strip_opaque_replay_items":
        return live.strip_opaque_replay_items(r.get("items"))
    if func == "legacy_tokens":
        return live._legacy_tokens(r.get("items") or [])
    if func == "codex_reasoning_replay_tokens":
        return live.codex_reasoning_replay_tokens(r.get("msg"))
    if func == "signed_thinking_replay_tokens":
        return live.signed_thinking_replay_tokens(r.get("msg"), r.get("readable_tokens", 0))
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
        mism += 1
        print(f"MISMATCH case={r.get('case')} func={r.get('func')}: python raised {exc!r}", file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH case={r.get('case')} func={r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
if cases == 0:
    print("ORACLE w10_replayedre_d3ae9b: 0 cases (link failure in disguise)")
    sys.exit(1)
print(f"ORACLE w10_replayedre_d3ae9b: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
