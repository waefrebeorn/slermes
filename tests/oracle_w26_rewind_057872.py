#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/conversation/_store/rewind.py (gate w26_rewind_057872).

The rewind itself is a read-plan plus one write transaction against a live
StoreHolder; those are seams.  What is compared is the pure decision layer the
plan and the write are built from:

  * the ordinal resolution of ``plan_user_turn_rewind`` -- negative counts back
    from the newest and clamps at the oldest, out-of-range raises
    ``RewindTargetUnavailableError``; ``turns_undone = len(durable_user) - ordinal``;
  * ``retryable_user_text``  -- the lossless /retry text, or the ValueError
    that fails closed before any write;
  * ``flatten_message_text`` -- the display flattening a prefill installs;
  * ``_is_ephemeral_scaffolding`` -- which warm rows the plan drops;
  * the module's message-shape constants (marker names, dataclass fields).
"""
import json
import sys
from dataclasses import fields
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_rewind_057872"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from persistence.sqlite.conversation._store import rewind as live  # noqa: E402
from session_rules import turns as rules  # noqa: E402

WANTED = {"plan_ordinal", "retry_text", "flatten_text", "non_scaffolding", "constants"}

TARGET_GONE = "target user message is no longer in session history"


def plan_ordinal(r):
    count = r.get("count")
    ordinal = r.get("ordinal")
    if ordinal < 0:
        ordinal = max(count + ordinal, 0)
    if ordinal >= count:
        return {"ok": False, "error": TARGET_GONE}
    return {"ok": True, "ordinal": ordinal, "turns_undone": count - ordinal}


def retry_text(r):
    try:
        return {"ok": True, "text": rules.retryable_user_text(r.get("content"))}
    except ValueError as exc:
        return {"ok": False, "error": str(exc)}


def flatten_text(r):
    return rules.flatten_message_text(r.get("content"))


def non_scaffolding(r):
    raw = r.get("messages")
    messages = raw if isinstance(raw, list) else []
    return [m for m in messages if not rules._is_ephemeral_scaffolding(m)]


def constants():
    return {
        "history_changed": live._HISTORY_CHANGED,
        "db_persisted_marker": rules._DB_PERSISTED_MARKER,
        "message_uid": rules.MESSAGE_UID,
        "plan_fields": [f.name for f in fields(live.RewindPlan)],
        "outcome_fields": [f.name for f in fields(live.RewindOutcome)],
        "ephemeral_flags": list(rules._EPHEMERAL_SCAFFOLDING_FLAGS),
    }


def expected(r):
    f = r.get("func")
    if f == "plan_ordinal":
        return plan_ordinal(r)
    if f == "retry_text":
        return retry_text(r)
    if f == "flatten_text":
        return flatten_text(r)
    if f == "non_scaffolding":
        return non_scaffolding(r)
    if f == "constants":
        return constants()
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
