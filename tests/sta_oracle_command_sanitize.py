import os
#!/usr/bin/env python3
"""
sta_oracle_command_sanitize.py — Python oracle for the PURE command-name
sanitizers _sanitize_telegram_name / _sanitize_slack_name
(hermes_cli/commands.py), ported in src/cli/gateway_command_sanitize.c.

Imports the REAL hermes_cli.commands module and calls the genuine functions.
Output contract matches tests/t_port_command_sanitize.c: one JSON object per
line, sorted keys, ensure_ascii=False, compact separators.
"""

import json
import sys

_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
if _ROOT not in sys.path:
    sys.path.insert(0, _ROOT)

from hermes_cli.commands_platforms import (  # noqa: E402
    _sanitize_telegram_name,
    _sanitize_slack_name,
)


def emit(obj):
    sys.stdout.write(json.dumps(obj, separators=(",", ":"), ensure_ascii=False) + "\n")


def split_kv(line):
    line = line.rstrip("\n")
    if not line.strip() or line.startswith("#"):
        return None
    op, _, rest = line.partition(" ")
    return op, rest


def main():
    # The batch spec supplies the fixture on argv and the C output on stdin.
    # A reference oracle must read the FIXTURE and compute Python's values;
    # reading stdin made this a validator emitting prose-shaped records that
    # never matched the C output.
    with open(sys.argv[1], encoding="utf-8") as f:
        for raw in f:
            parsed = split_kv(raw)
            if parsed is None:
                continue
            op, rest = parsed
            v = rest if rest else ""

            if op == "telegram":
                emit({"op": "telegram", "in": v, "out": _sanitize_telegram_name(v)})
            elif op == "slack":
                emit({"op": "slack", "in": v, "out": _sanitize_slack_name(v)})
            else:
                emit({"op": "unknown", "raw": raw.rstrip("\n")})


if __name__ == "__main__":
    sys.exit(main())
