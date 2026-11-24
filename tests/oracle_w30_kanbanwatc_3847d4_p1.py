#!/usr/bin/env python3
"""Differential oracle for gateway/kanban_watchers_notifier.py -- gate w30_kanbanwatc_3847d4_p1.

Everything under comparison is what one claimed terminal event *means* and
what it *says*: the kind tables, ``diagnostic_event``, the payload text helpers
(``_payload`` / ``_clip`` / ``_first_line`` / ``_safe_review_reason``), the nine
formatters behind ``format_event``, the ``format_event`` accumulator, and the
``_send_pings`` selection filter.

Seams that stay in the caller and never run here:
  * the adapter ``send()`` / ``record_notify_ping`` / cursor writes -- the gate
    stops at "which event ids would be pinged";
  * the DB claim that produced the events;
  * ``agent.redact.redact_sensitive_text`` -- ``safe_review_reason`` fixtures
    carry the redacted text as the seam value, and this oracle proves the
    fixture's seam value IS what the live redactor produces.  ``format_event``
    reads payload text as-is, so its ``changes_requested`` cases assert the
    payload text is redaction-stable (redactor is the identity there).
"""
import importlib.util
import json
import logging
import os
import sys
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w30_kanbanwatc_3847d4_p1"
TRUTH = QUARRY / "gateway" / "kanban_watchers_notifier.py"

os.environ["USERPROFILE"] = os.environ.get("USERPROFILE") or str(REPO.parent)
os.environ["HOME"] = str(REPO.parent)
# The C side embeds the bundled ``en`` templates; pin the live catalog to them.
os.environ["HERMES_LANGUAGE"] = "en"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))
sys.path.append(str(Path(__file__).resolve().parent))  # stub helper only
from hermes_seam_stubs import install as _install_seams  # noqa: E402
_install_seams()

_spec = importlib.util.spec_from_file_location("kwn_live_%s" % GATE, TRUTH)
live = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(live)

from agent import i18n  # noqa: E402
from agent.redact import redact_sensitive_text  # noqa: E402

i18n.reset_language_cache()
if i18n.get_language() != "en":
    raise SystemExit("oracle needs the en catalog, got %r" % (i18n.get_language(),))

logging.getLogger("gateway.run").setLevel(logging.CRITICAL)


class Ev:
    """Stand-in for a claimed event row: only kind / payload / id are read."""

    def __init__(self, d):
        d = d if isinstance(d, dict) else {}
        self.kind = d.get("kind") or ""
        self.payload = d.get("payload")
        self.id = d.get("id")


def mk_task(d):
    if not isinstance(d, dict):
        return None
    return SimpleNamespace(
        title=d.get("title"), assignee=d.get("assignee"), result=d.get("result"),
        task_id=d.get("task_id"), status=d.get("status"), session_id=d.get("session_id"),
    )


def notif(req, events=None):
    sub = req.get("sub") if isinstance(req.get("sub"), dict) else {}
    return live._KanbanNotification(
        None,
        {"sub": sub, "task": mk_task(req.get("task")), "board": req.get("board"),
         "events": list(events or [])},
        platform_cls=None, sub_fail_counts={},
    )


def redaction_stable(payload, keys):
    """The port reads payload text as-is: prove the redactor is the identity."""
    payload = payload if isinstance(payload, dict) else {}
    for key in keys:
        raw = payload.get(key)
        text = "" if raw is None else str(raw)
        got = redact_sensitive_text(text, force=True, redact_url_credentials=True)
        if got != text:
            raise AssertionError("fixture payload %r is not redaction-stable: %r -> %r"
                                 % (key, text, got))


def f_constants(_req):
    return {
        "max_send_failures": live.MAX_SEND_FAILURES,
        "terminal_kinds": list(live.TERMINAL_KINDS),
        "wake_kinds": list(live._WAKE_KINDS),
        "thread_chat_types": sorted(live._THREAD_CHAT_TYPES),
    }


def f_diagnostic(req):
    return live.diagnostic_event(Ev(req.get("event")))


def f_first_line(req):
    text = req.get("text")
    if not isinstance(text, str):
        return None
    return live._first_line(text, int(req.get("limit", 160)))


def f_clip(req):
    return live._clip(Ev(req.get("event")), req.get("key") or "", req.get("tkey") or "",
                      int(req.get("limit", 160)))


def f_safe_review_reason(req):
    limit = int(req.get("limit", 160))
    raw = req.get("value")
    redacted = redact_sensitive_text("" if raw is None else str(raw),
                                     force=True, redact_url_credentials=True)
    if redacted != req.get("redacted"):
        raise AssertionError("seam mismatch: fixture redacted=%r live=%r"
                             % (req.get("redacted"), redacted))
    return live._safe_review_reason(raw, limit)


def f_format_event(req):
    ev = req.get("event") if isinstance(req.get("event"), dict) else {}
    if ev.get("kind") == "changes_requested":
        redaction_stable(ev.get("payload"), ("reason", "reviewer", "implementer"))
    n = notif(req)
    msg = n.format_event(Ev(ev))
    return {"msg": msg,
            "handoff": n.wake_handoff or None,
            "review_detail": n.wake_review_detail or None}


def f_format_batch(req):
    events = req.get("events") if isinstance(req.get("events"), list) else []
    for ev in events:
        if isinstance(ev, dict) and ev.get("kind") == "changes_requested":
            redaction_stable(ev.get("payload"), ("reason", "reviewer", "implementer"))
    n = notif(req, events)
    messages = [n.format_event(Ev(ev)) for ev in events]
    return {"messages": messages,
            "wake_handoff": n.wake_handoff,
            "wake_review_detail": n.wake_review_detail}


def f_ping_plan(req):
    events = req.get("events") if isinstance(req.get("events"), list) else []
    sub = req.get("sub") if isinstance(req.get("sub"), dict) else {}
    n = notif(req, events)
    n.is_push_adapter = bool(req.get("is_push_adapter", True))
    ids, messages = [], []
    for raw in events:
        ev = Ev(raw)
        msg = n.format_event(ev)
        messages.append(msg)
        if msg is None:
            continue
        if not n.is_push_adapter and n.wake_agent:
            continue
        if not n.send_passive:
            continue
        if ev.id <= sub.get("last_ping_event_id", 0):
            continue
        ids.append(ev.id)
    return {"ids": ids, "messages": messages,
            "wake_handoff": n.wake_handoff,
            "wake_review_detail": n.wake_review_detail}


WANTED = {
    "constants", "diagnostic_event", "first_line", "clip", "safe_review_reason",
    "format_event", "format_batch", "ping_plan",
}

FUNCS = {
    "constants": f_constants,
    "diagnostic_event": f_diagnostic,
    "first_line": f_first_line,
    "clip": f_clip,
    "safe_review_reason": f_safe_review_reason,
    "format_event": f_format_event,
    "format_batch": f_format_batch,
    "ping_plan": f_ping_plan,
}


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
            py = FUNCS[r["func"]](r)
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
