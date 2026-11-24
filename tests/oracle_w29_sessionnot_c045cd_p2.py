#!/usr/bin/env python3
"""Differential oracle for tui_gateway/session_notifications.py (gate
w29_sessionnot_c045cd_p2: kanban line rendering, completion/timeout suffixes,
delegation display metadata, per-turn note prefix).

The live module is imported as a package module.  Nothing it talks to runs:

  * kanban ``task`` / event objects arrive as records and are rebuilt as
    attribute objects (the live formatters use getattr);
  * ``async_delegation_display_text`` lives in
    tools.process_registry_notifications and is patched per case with the
    fixture's text -- the cross-module renderer is the seam, the metadata
    arithmetic around it is what is compared;
  * ``_kb_board_key``'s DB-path resolution stays in the caller (not ported).
"""
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w29_sessionnot_c045cd_p2"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from tui_gateway import session_notifications as live  # noqa: E402
import tools.process_registry_notifications as prn  # noqa: E402

WANTED = {"first_line", "kb_completed", "kb_timed_out", "kanban_text",
          "delegation_meta", "prepend_note"}


def _task(rec):
    rec = rec if isinstance(rec, dict) else {}
    return types.SimpleNamespace(**rec)


def _event(rec):
    """An event object: only the attributes the record carries exist."""
    rec = rec if isinstance(rec, dict) else {}
    ev = types.SimpleNamespace()
    if "kind" in rec:
        ev.kind = rec["kind"]
    if "payload" in rec:
        ev.payload = rec["payload"]
    return ev


def delegation_meta(req):
    text = req.get("display_text")
    evt = req.get("evt") if isinstance(req.get("evt"), dict) else {}
    prn.async_delegation_display_text = lambda _evt: text
    return live._async_delegation_display_metadata(evt)


def expected(r):
    f = r.get("func")
    if f == "first_line":
        return live._kb_first_line(r.get("value"), r.get("limit"))
    if f == "kb_completed":
        return live._kb_completed(_task(r.get("task")), r.get("payload") or {}, r.get("title") or "")
    if f == "kb_timed_out":
        return live._kb_timed_out(_task(r.get("task")), r.get("payload") or {}, r.get("title") or "")
    if f == "kanban_text":
        return live._format_kanban_event_text(
            r.get("sub") or {}, _task(r.get("task")), _event(r.get("ev")),
            r.get("board_slug") or "")
    if f == "delegation_meta":
        return delegation_meta(r)
    if f == "prepend_note":
        return live._prepend_note(r.get("run_message"), r.get("note"))
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
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
