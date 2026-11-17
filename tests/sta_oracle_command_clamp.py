import os
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
import sys

_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
if _ROOT not in sys.path:
    sys.path.insert(0, _ROOT)

from hermes_cli.commands_platforms import _clamp_command_names  # noqa: E402


def emit(obj):
    sys.stdout.write(json.dumps(obj, separators=(",", ":"), ensure_ascii=False) + "\n")


def main():
    case_no = 0
    entries = []
    reserved = set()
    # Read the FIXTURE (argv), not the C output (stdin). This is a
    # reference oracle, not a validator of the C stream.
    with open(sys.argv[1], encoding="utf-8") as f:
      for raw in f:
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
        # cmd_entry_t.name is char[CMD_NAME_LIMIT+1]; the harness caps at 32,
        # so the C function never sees the original longer name. Mirror that.
        name = parts[0][:32]
        desc = parts[1] if len(parts) > 1 else None
        key = parts[2] if len(parts) > 2 else None
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
