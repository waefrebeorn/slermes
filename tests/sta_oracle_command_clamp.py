#!/usr/bin/env python3
"""
sta_oracle_command_clamp.py — Python oracle for the PURE command-name clamp
helper _clamp_command_names (hermes_cli/commands.py), ported in
src/cli/gateway_command_sanitize.c:commands_clamp_names.

Imports the REAL hermes_cli.commands module and calls the genuine function.
Output contract matches tests/t_port_command_clamp.c: one JSON object per
case (separated by '==='), sorted keys, ensure_ascii=False, compact separators.

Fixtures use 'name|desc|key' entries; a 'reserved:' line adds a reserved name.
The Python entry tuple is (name, desc, key) so the extra-element 'key' survives
and is surfaced as 'key', mirroring the C cmd_entry_t (name/desc/key).
"""

import json
import os
import sys

_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
if _ROOT not in sys.path:
    sys.path.insert(0, _ROOT)

from hermes_cli.commands import _clamp_command_names  # noqa: E402


def emit(obj):
    sys.stdout.write(json.dumps(obj, separators=(",", ":"), ensure_ascii=False) + "\n")


def main():
    case_no = 0
    entries = []
    reserved = set()
    for raw in sys.stdin:
        line = raw.rstrip("\n")
        if line.startswith("==="):
            result = _clamp_command_names(list(entries), reserved)
            kept = []
            for e in result:
                name, desc, *extra = e
                key = extra[0] if extra else ""
                kept.append({"name": name, "desc": desc, "key": key})
            emit({"case": case_no, "dropped": len(entries) - len(kept), "kept": kept})
            entries = []
            reserved = set()
            case_no += 1
            continue
        if not line.strip():
            continue
        if line.startswith("reserved:"):
            reserved.add(line[9:].strip())
            continue
        parts = line.split("|")
        name = parts[0]
        desc = parts[1] if len(parts) > 1 else ""
        key = parts[2] if len(parts) > 2 else ""
        entries.append((name, desc, key))

    # flush trailing case
    if entries or reserved:
        result = _clamp_command_names(list(entries), reserved)
        kept = []
        for e in result:
            name, desc, *extra = e
            key = extra[0] if extra else ""
            kept.append({"name": name, "desc": desc, "key": key})
        emit({"case": case_no, "dropped": len(entries) - len(kept), "kept": kept})


if __name__ == "__main__":
    sys.exit(main())
