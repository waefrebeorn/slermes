#!/usr/bin/env python3
"""Oracle for hermes_cli/heartbeat.py — pure functions over fixtures.

Reads a fixture file (argv[1]); each non-empty, non-comment line:
    op|arg0|arg1|...
Writes one JSON result object per line to stdout.

The C harness (tests/t_port_*.c) emits the same shape, so the generic
run_oracle.sh runner diffs the two.

Note: status_line is NOT oracle-verified here because it reads wall-clock
time (time.time()), making it non-deterministic across the C↔Python diff.
is_due IS deterministic — the fixture supplies `now`.
"""
import json
import sys

from hermes_cli.heartbeat import (
    parse_interval,
    format_interval,
    MIN_INTERVAL_SECONDS,
    POLL_SECONDS,
    HEARTBEAT_PROMPT_TEMPLATE,
)
from hermes_cli.heartbeat import HeartbeatState


def emit(op, args, result):
    print(json.dumps({"op": op, "args": args, "result": result}))


def state_from(raw):
    return HeartbeatState(
        prompt=raw.get("prompt", ""),
        interval_seconds=int(raw.get("interval_seconds", 0)),
        status=raw.get("status", "active"),
        created_at=float(raw.get("created_at", 0.0)),
        last_fired_at=float(raw.get("last_fired_at", 0.0)),
        fire_count=int(raw.get("fire_count", 0)),
    )


def main():
    path = sys.argv[1]
    with open(path) as f:
        for raw in f:
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split("|")
            op = parts[0]
            args = parts[1:]

            if op == "parse_interval":
                text = args[0] if len(args) > 0 else ""
                emit(op, [text], parse_interval(text))

            elif op == "format_interval":
                seconds = int(args[0]) if len(args) > 0 and args[0] else 0
                emit(op, [seconds], format_interval(seconds))

            elif op == "is_due":
                raw_state = json.loads(args[0]) if args else {}
                state = state_from(raw_state)
                now = float(args[1]) if len(args) > 1 and args[1] else None
                emit(op, [raw_state, now], state.is_due(now))

            elif op == "render_prompt":
                raw_state = json.loads(args[0]) if args else {}
                state = state_from(raw_state)
                emit(op, [raw_state], state.render_prompt())

            else:
                emit(op, args, {"error": "unknown op: " + op})


if __name__ == "__main__":
    main()
