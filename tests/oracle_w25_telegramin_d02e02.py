#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/telegram/telegram_ingress.py
(gate w25_telegramin_d02e02).

The live module is imported from the Python quarry.  The PTB update, the
adapter's buffers and its flush tasks are seams, so:

  * ``contact_text`` is called with a stand-in contact / SimpleNamespace built
    from the request's attribute dict (a null contact exercises getattr's
    default);
  * ``_drop_matching`` is called with a pending dict + task dict derived from
    the request's buffer list (a null ``task_done`` means no task), and a
    stand-in adapter whose ``_event_session_key`` answers from each entry;
  * ``discard_session_media_buffers`` is called through the same stand-in
    adapter, so the photo-burst and album buffers are compared together.

A live call that raises (a non-str name part, a non-str vcard) is reported as
``{"__error__": "<type>"}`` so the error seam is compared too.
"""
import json
import os
import sys
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_telegramin_d02e02"

if not os.environ.get("USERPROFILE") and not os.environ.get("HOMEPATH"):
    os.environ["USERPROFILE"] = str(REPO.parent)

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from plugins.platforms.telegram import telegram_ingress as live  # noqa: E402

WANTED = {"contact_text", "drop_matching", "discard_media_buffers", "constants"}


class FakeTask:
    """A flush task: done() is fixed, cancel() is recorded but not compared."""

    def __init__(self, done):
        self._done = done
        self.cancelled = False

    def done(self):
        return self._done

    def cancel(self):
        self.cancelled = True


class FakeAdapter:
    """The adapter's buffered-flush surface, answered from the request."""

    def __init__(self, req):
        self._pending_photo_batches, self._pending_photo_batch_tasks = _buffer(
            req.get("photo_batches"))
        self._media_group_events, self._media_group_tasks = _buffer(
            req.get("media_group_events"))

    def _event_session_key(self, buffered):
        return buffered.get("session")


def _buffer(entries):
    pending, tasks = {}, {}
    for entry in entries if isinstance(entries, list) else []:
        key = entry.get("key")
        pending[key] = {"session": entry.get("session")}
        if entry.get("task_done") is not None:
            tasks[key] = FakeTask(entry.get("task_done"))
    return pending, tasks


def contact_text(req):
    contact = req.get("contact")
    if isinstance(contact, dict):
        contact = SimpleNamespace(**contact)
    return live.contact_text(contact)


def drop_matching(req):
    pending, tasks = _buffer(req.get("pending"))
    adapter = FakeAdapter({})
    return live._drop_matching(adapter, pending, tasks, req.get("session_key"))


def discard_media_buffers(req):
    return live.discard_session_media_buffers(FakeAdapter(req), req.get("session_key"))


def constants(_req):
    return {"header": "[The user shared a contact card.]",
            "labels": ["Name", "Phone", "Telegram user id"],
            "vcard_label": "vCard:",
            "separator": "\n"}


def expected(r):
    f = r.get("func")
    if f == "contact_text":
        return contact_text(r)
    if f == "drop_matching":
        return drop_matching(r)
    if f == "discard_media_buffers":
        return discard_media_buffers(r)
    if f == "constants":
        return constants(r)
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
            py = {"__error__": type(exc).__name__}
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
