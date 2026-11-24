#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/slack/adapter_reactions_context.py
(gate w21_adapterrea_e0609e).

The live module is pure stdlib, so the oracle imports it by path and calls the
real `slack_reactions_marker` on every fixture message; `name` dispatches the
module's own `_UNSAFE_NAME_CHARS` substitution + `.strip(":")`, `limit` reads
`REACTION_MARKER_MAX`. A non-dict message raises AttributeError upstream, which
is exactly the {"status":"error",...} shape the C port returns.
"""
import importlib.util
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "plugins" / "platforms" / "slack" / "adapter_reactions_context.py"
GATE = "w21_adapterrea_e0609e"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("slack_reactions_context_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["slack_reactions_context_live"] = live
_spec.loader.exec_module(live)


def name_of(value):
    """_UNSAFE_NAME_CHARS.sub("", str(value or "")).strip(":")"""
    return live._UNSAFE_NAME_CHARS.sub("", str(value or "")).strip(":")


def more_label(total):
    """The "+N more" tail slack_reactions_marker appends past REACTION_MARKER_MAX."""
    extra = total - live.REACTION_MARKER_MAX
    return "+%d more" % extra if extra > 0 else ""


def err_of(exc):
    return {"status": "error", "type": type(exc).__name__, "message": str(exc)}


def expected(r):
    f = r.get("func")
    if f == "name":
        return name_of(r.get("value"))
    if f == "limit":
        return live.REACTION_MARKER_MAX
    if f == "more":
        return more_label(r.get("parts") or 0)
    try:
        return live.slack_reactions_marker(r.get("msg"))
    except Exception as exc:  # noqa: BLE001 - reported as the harness shapes it
        return err_of(exc)


def main():
    cases = mism = 0
    wanted = {"marker", "name", "limit", "more"}
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in wanted:
            continue
        cases += 1
        py = expected(r)
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
