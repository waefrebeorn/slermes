#!/usr/bin/env python3
"""Oracle reference for agent/replay_cleanup.py history strippers."""
import json, os, sys
sys.path.insert(0, "/home/wubu/hermes-agent-dev")
from agent.replay_cleanup import (
    strip_interrupted_tool_tails,
    strip_dangling_tool_call_tail,
    sanitize_replay_history,
)

HERE = os.path.dirname(os.path.abspath(__file__))

# Build a few histories exercising the branches.
def hist(*msgs): return list(msgs)

cases = {}

# 1) clean history -> unchanged
cases["clean"] = hist(
    {"role": "user", "content": "hi"},
    {"role": "assistant", "content": "ok", "tool_calls": [{"id": "1"}]},
    {"role": "tool", "content": "done", "tool_call_id": "1"},
)

# 2) interrupted assistant->tool block in MIDDLE (followed by real user msg)
cases["interrupted_mid"] = hist(
    {"role": "user", "content": "run it"},
    {"role": "assistant", "content": "", "tool_calls": [{"id": "a"}]},
    {"role": "tool", "content": "[command interrupted]", "tool_call_id": "a"},
    {"role": "user", "content": "still there?"},
    {"role": "assistant", "content": "yes", "tool_calls": [{"id": "b"}]},
    {"role": "tool", "content": "ok", "tool_call_id": "b"},
)

# 3) orphan interrupted tool result
cases["orphan_tool"] = hist(
    {"role": "user", "content": "x"},
    {"role": "tool", "content": "exit_code 130 interrupt"},
)

# 4) dangling tail: assistant(tool_calls) with NO answers at the end
cases["dangling_tail"] = hist(
    {"role": "user", "content": "restart gateway"},
    {"role": "assistant", "content": "", "tool_calls": [{"id": "z"}]},
)

# 5) completed assistant->tool pair left untouched (not dangling)
cases["completed_pair"] = hist(
    {"role": "user", "content": "do"},
    {"role": "assistant", "content": "", "tool_calls": [{"id": "c"}]},
    {"role": "tool", "content": "result", "tool_call_id": "c"},
)

ref = {}
for name, h in cases.items():
    ref[name] = {
        "input": h,
        "strip_interrupted": strip_interrupted_tool_tails([dict(m) for m in h]),
        "strip_dangling": strip_dangling_tool_call_tail([dict(m) for m in h]),
        "sanitize": sanitize_replay_history([dict(m) for m in h]),
    }

with open(os.path.join(HERE, "replay_cleanup.ref.json"), "w") as f:
    json.dump(ref, f, indent=2, sort_keys=True)
print("replay_cleanup oracle reference written for", list(cases.keys()))
