#!/usr/bin/env python3
"""
sta_oracle_command_priority.py — Python oracle for the PURE Telegram menu
prioritization helpers in hermes_cli/commands.py:
  _telegram_effective_priority()        (mirrors commands_telegram_effective_priority)
  _prioritize_telegram_menu_commands()  (mirrors commands_prioritize_telegram_menu)

Faithful E2E: for each case we write the fixture's command_menu config block to
a temp HERMES_HOME/config.yaml (the exact path _telegram_command_menu_config()
reads), then call the REAL Python functions. The C harness receives the same
raw command_menu JSON and calls commands_telegram_effective_priority /
commands_prioritize_telegram_menu. Both derive their ordering from the same
config, so a difference is a genuine divergence.

Output contract matches tests/t_port_command_priority.c: one JSON object per
op per case, sorted keys, ensure_ascii=False, compact separators.
"""

import json
import os
import sys
import tempfile
import yaml

_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
if _ROOT not in sys.path:
    sys.path.insert(0, _ROOT)

# Configure a temp HERMES_HOME BEFORE importing the config-dependent module.
_TMP = tempfile.mkdtemp(prefix="slermes_oracle_priority_")
os.environ["HERMES_HOME"] = _TMP
# Some installs gate on HERMES_DEV; not required for read_raw_config.
os.makedirs(_TMP, exist_ok=True)

from hermes_cli.commands import (  # noqa: E402
    _telegram_effective_priority,
    _prioritize_telegram_menu_commands,
)


def emit(obj):
    sys.stdout.write(json.dumps(obj, separators=(",", ":"), ensure_ascii=False) + "\n")


def write_config(command_menu):
    cfg = {"platforms": {"telegram": {"extra": {"command_menu": command_menu}}}}
    with open(os.path.join(_TMP, "config.yaml"), "w", encoding="utf-8") as fh:
        yaml.safe_dump(cfg, fh)


def split_case(line):
    line = line.rstrip("\n")
    if not line.strip():
        return None
    return line


def main():
    # buffer current case
    cur_cfg = None
    cur_entries = []

    def flush(cfg, entries):
        if cfg is None and not entries:
            return
        # Write config so _telegram_command_menu_config() reads it.
        write_config(cfg if cfg is not None else {})
        # effective priority
        eff = list(_telegram_effective_priority())
        emit({"op": "priority", "priority": eff})
        # sort
        if entries:
            result = _prioritize_telegram_menu_commands(list(entries))
            order = [e[0] for e in result]
            emit({"op": "sort", "order": order})

    for raw in sys.stdin:
        line = raw.rstrip("\n")
        if line.startswith("==="):
            flush(cur_cfg, cur_entries)
            cur_cfg = None
            cur_entries = []
            continue
        if not line.strip():
            continue
        if line.startswith("cfg "):
            cur_cfg = json.loads(line[4:])
            continue
        if line.startswith("entry "):
            e = line[6:]
            parts = e.split("|")
            name = parts[0]
            desc = parts[1] if len(parts) > 1 else ""
            key = parts[2] if len(parts) > 2 else ""
            cur_entries.append((name, desc, key))
            continue

    flush(cur_cfg, cur_entries)


if __name__ == "__main__":
    sys.exit(main())
