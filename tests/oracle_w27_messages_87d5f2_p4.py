#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/conversation/_store/messages.py
(gate w27_messages_87d5f2_p4: replay dedupe + the fork predicate).

Compared against the live module with values only — a message dict, a list of
already-decoded messages, a session row:

  * ``_canonical_replayed_user_content`` — the live content a user row replays;
  * ``_exact_replayed_user_clone_key`` — the column-exact rotation-clone key;
  * ``_find_duplicate_replayed_user_message`` — the adjacent replay duplicate;
  * ``_is_explicit_fork_child_row`` — a branch/delegate/tool child test.

Python tuples are compared as lists.  A composite user row (a compaction
handoff carrier) is the documented seam of this gate: it needs
``session_rules.turns.split_user_originated_turn``, so the fixtures stay on
plain user/assistant rows and ``composite`` is False on every case here.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w27_messages_87d5f2_p4"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from persistence.sqlite.conversation._store import messages as live  # noqa: E402

WANTED = {
    "canonical_replayed_user_content",
    "exact_replayed_user_clone_key",
    "find_duplicate_replayed_user_message",
    "is_explicit_fork_child_row",
}


def expected(r):
    f = r.get("func")
    if f == "canonical_replayed_user_content":
        content, composite = live._canonical_replayed_user_content(r.get("msg"))
        return {"content": content, "composite": composite}
    if f == "exact_replayed_user_clone_key":
        key = live._exact_replayed_user_clone_key(r.get("timestamp"), r.get("content"))
        return list(key) if key is not None else None
    if f == "find_duplicate_replayed_user_message":
        messages = r.get("messages")
        messages = list(messages) if isinstance(messages, list) else []
        dup = live._find_duplicate_replayed_user_message(messages, r.get("msg"))
        return list(dup) if dup is not None else None
    if f == "is_explicit_fork_child_row":
        return live._is_explicit_fork_child_row(None, r.get("session"),
                                                 include_reset=bool(r.get("include_reset")))
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
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
