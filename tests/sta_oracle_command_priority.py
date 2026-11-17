#!/usr/bin/env python3
"""Pure reference oracle for command_priority.

Reads the fixture on argv and emits the same JSON records as the C harness:
one "priority" record and one "sort" record per case. The old version read the
C output on stdin and printed ok/MISMATCH prose, which the batch comparator
can never match.

Upstream replaced _telegram_effective_priority /
_prioritize_telegram_menu_commands with the rank-table based
_prioritize_telegram_menu_candidates; the effective priority list is the
sanitized merge of the configured list and _TELEGRAM_MENU_PRIORITY.
"""
import json
import os
import sys
import tempfile

_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
if _ROOT not in sys.path:
    sys.path.insert(0, _ROOT)

_TMP = tempfile.mkdtemp(prefix="slermes_oracle_priority_")
os.environ["HERMES_HOME"] = _TMP
os.makedirs(_TMP, exist_ok=True)

from hermes_cli.commands_platforms import (  # noqa: E402
    _TELEGRAM_MENU_PRIORITY,
    _prioritize_telegram_menu_candidates,
    _sanitize_telegram_name,
    _telegram_command_menu_config,
)


def emit(obj):
    sys.stdout.write(json.dumps(obj, separators=(",", ":"), ensure_ascii=False) + "\n")


def write_config(command_menu):
    cfg = {"platforms": {"telegram": {"extra": {"command_menu": command_menu}}}}
    with open(os.path.join(_TMP, "config.yaml"), "w", encoding="utf-8") as f:
        json.dump(cfg, f)


def sanitized(names):
    out, seen = [], set()
    for raw in names:
        name = _sanitize_telegram_name(str(raw))
        if name and name not in seen:
            seen.add(name)
            out.append(name)
    return out


def effective_priority():
    cfg = _telegram_command_menu_config()
    configured = sanitized(cfg["priority"])
    defaults = sanitized(_TELEGRAM_MENU_PRIORITY)
    mode = cfg["priority_mode"]
    if mode == "replace":
        return configured
    if mode == "append":
        return defaults + [n for n in configured if n not in defaults]
    return configured + [n for n in defaults if n not in configured]


def main():
    cur_cfg = None
    entries = []

    def flush():
        if cur_cfg is None and not entries:
            return
        write_config(cur_cfg or {})
        emit({"op": "priority", "priority": effective_priority()})
        if entries:
            candidates = [(name, desc, "core", name) for name, desc in entries]
            ordered = _prioritize_telegram_menu_candidates(candidates)
            emit({"op": "sort", "order": [c[0] for c in ordered]})

    with open(sys.argv[1], encoding="utf-8") as f:
        for raw in f:
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            if line.startswith("==="):
                flush()
                cur_cfg, entries = None, []
            elif line.startswith("cfg "):
                try:
                    cur_cfg = json.loads(line[4:])
                except Exception:
                    cur_cfg = {}
            elif line.startswith("entry "):
                parts = line[6:].split("|")
                name = parts[0] if parts else ""
                desc = parts[1] if len(parts) > 1 else ""
                entries.append((name, desc))
    flush()


if __name__ == "__main__":
    main()
