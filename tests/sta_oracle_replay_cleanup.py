#!/usr/bin/env python3
"""Faithfulness oracle for port_agent_replay_cleanup.c.

Reads the history JSON fixture from argv[1] and recomputes the SAME three
helpers from the LIVE agent/replay_cleanup.py:
  strip_interrupted_tool_tails
  strip_dangling_tool_call_tail
  sanitize_replay_history

Emits compact JSON (no spaces) so it diffs byte-for-byte against the C
harness's json_serialize() output.
"""
import sys, os, json

sys.path.insert(0, os.path.expanduser("~/hermes-agent-dev"))
from agent.replay_cleanup import (
    strip_interrupted_tool_tails,
    strip_dangling_tool_call_tail,
    sanitize_replay_history,
    strip_stale_dangerous_confirmations,
)


def compact(obj):
    return json.dumps(obj, separators=(",", ":"), ensure_ascii=False)


def main():
    if len(sys.argv) < 2:
        sys.stderr.write("usage: sta_oracle_replay_cleanup.py <history.json>\n")
        return 2
    with open(sys.argv[1], "r", encoding="utf-8") as f:
        history = json.load(f)

    si = strip_interrupted_tool_tails(history)
    sd = strip_dangling_tool_call_tail(history)
    sa = sanitize_replay_history(history)

    # Compact separators (no spaces) to match the C json_serialize() output
    # byte-for-byte (the runner diffs raw JSON lines).
    print(json.dumps({"fn": "strip_interrupted", "out": compact(si)},
                     separators=(",", ":"), ensure_ascii=False))
    print(json.dumps({"fn": "strip_dangling", "out": compact(sd)},
                     separators=(",", ":"), ensure_ascii=False))
    print(json.dumps({"fn": "sanitize", "out": compact(sa)},
                     separators=(",", ":"), ensure_ascii=False))

    dc = strip_stale_dangerous_confirmations(history, now=2000.0, expiry_seconds=60.0)
    print(json.dumps({"fn": "strip_dangerous", "out": compact(dc)},
                     separators=(",", ":"), ensure_ascii=False))
    return 0


if __name__ == "__main__":
    sys.exit(main())
