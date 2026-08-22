#!/usr/bin/env python3
"""sta_oracle_message_metadata.py — Python twin for agent/message_metadata.py.

Reads a JSON fixture (argv[1]) with an "ops" list:
  {"kind":"is_persistence_only","key":"timestamp"}
  {"kind":"stamp","message":{...},"timestamp":1700000000.0}
Emits one JSON line per op.
"""
import json
import sys
from time import time as wall_time

PERSISTENCE_ONLY_MESSAGE_FIELDS = frozenset({"timestamp"})


def stamp_message_timestamp(message, *, timestamp=None):
    if message.get("timestamp") is None:
        message["timestamp"] = wall_time() if timestamp is None else timestamp
    return message


def main():
    fixture = sys.argv[1]
    with open(fixture) as f:
        root = json.load(f)
    for c in root.get("ops", []):
        kind = c.get("kind")
        if kind == "is_persistence_only":
            key = c.get("key")
            out = key in PERSISTENCE_ONLY_MESSAGE_FIELDS
        elif kind == "stamp":
            msg = c.get("message", {})
            ts = c.get("timestamp")          # absent -> wall clock; C side uses 0 sentinel
            out = stamp_message_timestamp(dict(msg), timestamp=ts)
            if ts is None and isinstance(out.get("timestamp"), float):
                out["timestamp"] = "<wallclock>"
        else:
            out = None
        print(json.dumps({"out": out}, ensure_ascii=False, sort_keys=True))


if __name__ == "__main__":
    main()
