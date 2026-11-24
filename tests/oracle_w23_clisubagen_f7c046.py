#!/usr/bin/env python3
"""Differential oracle for hermes_cli/cli_subagent_monitor.py (gate
w23_clisubagen_f7c046).

The monitor's live sources are the seams: the subagent backend, the process
registry, the session dock rows and the clock.  None of them is started.  A
real ``SubagentMonitor`` is built and handed the fixture's snapshot, so every
compared answer is produced by the live method -- the cell-width clip, the
count / title / activity fragments, the whole dock layout (collapsed and
expanded, wide and narrow, tall and short), the roster walk and the transcript
tail read.  ``procs.process_activity`` is the sibling module's live function.
"""
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "hermes_cli" / "cli_subagent_monitor.py"
GATE = "w23_clisubagen_f7c046"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("cli_subagent_monitor_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["cli_subagent_monitor_live"] = live
_spec.loader.exec_module(live)

WANTED = {"clip", "counts", "title", "agent_activity", "process_activity",
          "collapsed_activity", "dock_text", "select", "view",
          "modal_prompt_active", "read_tail", "consts"}


def monitor_for(r):
    mon = live.SubagentMonitor(None)
    mon.entries = list(r.get("entries") or [])
    mon.processes = list(r.get("processes") or [])
    mon.goal = r.get("goal") or ""
    mon.loop = r.get("loop") or ""
    mon.loop_badge = r.get("loop_badge") or ""
    mon.loop_prompt = r.get("loop_prompt") or ""
    mon.queued = list(r.get("queued") or [])
    mon.selected_id = r.get("selected_id")
    mon.collapsed = bool(r.get("collapsed"))
    return mon


def expected(r):
    f = r.get("func")
    if f == "clip":
        return live._clip(r.get("value"), r.get("width", 0))
    if f == "counts":
        return monitor_for(r)._counts(session=bool(r.get("session", True)))
    if f == "title":
        return monitor_for(r)._title()
    if f == "agent_activity":
        return monitor_for({})._agent_activity(r.get("row") or {})
    if f == "process_activity":
        return live.procs.process_activity(r.get("row") or {})
    if f == "collapsed_activity":
        return monitor_for(r)._collapsed_activity()
    if f == "dock_text":
        return monitor_for(r).dock_text(columns=int(r.get("columns", 0)), rows=int(r.get("rows", 0)))
    if f == "select":
        mon = monitor_for(r)
        mon.select(int(r.get("delta", 0)))
        return mon.selected_id
    if f == "view":
        mon = monitor_for(r)
        sel = mon.selected
        return {
            "has_rows": mon.has_rows,
            "roster": [mon._key(row) for row in mon.roster],
            "selected": mon._key(sel) if sel is not None else None,
            "selected_is_process": mon.selected_process is not None,
        }
    if f == "modal_prompt_active":
        attrs = r.get("attrs") or {}
        cli = types.SimpleNamespace(**{k: v for k, v in attrs.items()})
        return live.modal_prompt_active(cli)
    if f == "read_tail":
        return live.read_tail(r.get("path") or "")
    if f == "consts":
        return {"queue_rows": live.session_rows.QUEUE_ROWS}
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
