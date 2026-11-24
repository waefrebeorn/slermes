#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/workers/_receipt.py
(gate w26_receipt_cbecab).

Each closed operation is called live with the transaction connection replaced by
a stub that answers the two reads the gate makes (the ``_row_id`` adoption check
and the archived-row count).  A payload the gate admits reaches the transaction's
own seam and dies there -- or, for a read-only verb, returns -- so the compared
value is only ever "admitted" or the refusal code.  ``row_annotations`` and the
contract vocabulary come straight from ``persistence.workers.vocabulary``.
"""
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_receipt_cbecab"

# The import chain resolves the Hermes home at import time; give it one.
_BASE = str(REPO.parent)
os.environ["USERPROFILE"] = _BASE
os.environ["HOME"] = _BASE
os.environ.setdefault("LOCALAPPDATA", str(Path(_BASE) / "AppData" / "Local"))

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from persistence.core.errors import AuthorityRefusal  # noqa: E402
from persistence.sqlite.workers import _receipt as live  # noqa: E402
from persistence.sqlite.workers._context import (  # noqa: E402
    worker_prompt,
    worker_row_role,
    worker_sidecars,
    worker_tail_role,
    worker_tool_names,
)
from persistence.workers import vocabulary as vocab  # noqa: E402

WANTED = {"validate", "annotations", "constants"}

_TOKEN_COUNTERS = ("input_tokens", "output_tokens", "cache_read_tokens",
                   "cache_write_tokens", "reasoning_tokens")


class _Row:
    def __init__(self, value):
        self.value = value

    def fetchone(self):
        return self.value


class _Conn:
    """The receipt connection reduced to the rows the gate reads."""

    total_changes = 0

    def __init__(self, row_exists=True, generation=0):
        self.row_exists = row_exists
        self.generation = generation

    def execute(self, sql, params=()):
        if "FROM messages WHERE id=? AND session_id=?" in sql:
            return _Row((1,) if self.row_exists else None)
        return _Row((self.generation,))


def _classify(call):
    """AuthorityRefusal -> its code; anything past it -> admitted (None)."""
    try:
        call()
    except AuthorityRefusal as exc:
        return str(exc)
    except Exception:  # noqa: BLE001 -- reached the transaction seam: admitted
        return None
    return None


def validate(req):
    op = req.get("op")
    payload = req.get("payload")
    row_exists = bool(req.get("row_exists"))
    gen = int(req.get("generation") or 0)

    if op == "transcript.append":
        return _classify(lambda: live._worker_append(None, _Conn(row_exists), "s1", payload))
    if op in ("turn.acquire", "turn.renew"):
        return _classify(lambda: live._worker_turn(None, _Conn(row_exists), "s1", payload, op))
    if op == "usage.main":
        return _classify(lambda: live._worker_usage(None, _Conn(row_exists, gen), "s1", payload))
    if op == "usage.auxiliary":
        return _classify(lambda: live._worker_usage(None, _Conn(row_exists, gen), "s1", payload,
                                                    auxiliary=True))
    if op == "execution.finish":
        return _classify(lambda: live._worker_finish(None, _Conn(row_exists), "s1", payload))
    if op == "receipt.params":
        handlers = {name: None for name in (req.get("handlers") or [])}
        return _classify(lambda: live.apply_worker_receipt(
            None, epoch=1, execution_id="exec-1", session_id="s1", generation=1,
            sequence=req.get("sequence"), operation=req.get("operation"),
            payload=payload, handlers=handlers))
    if op == "session.prompt":
        return _classify(lambda: worker_prompt(None, _Conn(row_exists), "s1", payload))
    if op == "session.sidecars":
        return _classify(lambda: worker_sidecars(None, _Conn(row_exists), "s1", payload))
    if op == "session.tools":
        return _classify(lambda: worker_tool_names(None, _Conn(row_exists), "s1", payload))
    if op == "session.tail_role":
        return _classify(lambda: worker_tail_role(None, _Conn(row_exists), "s1", payload))
    if op == "session.row_role":
        return _classify(lambda: worker_row_role(None, _Conn(row_exists), "s1", payload))
    return "UNKNOWN_OP"


def annotations(req):
    return vocab.row_annotations(req.get("messages") or [])


def constants():
    return {
        "message_fields": sorted(vocab.WORKER_MESSAGE_FIELDS),
        "annotation_keys": list(vocab.ROW_ANNOTATION_KEYS),
        "token_counters": list(_TOKEN_COUNTERS),
        "transcript_batch_max": vocab.TRANSCRIPT_BATCH_MAX,
        "payload_max_bytes": vocab.RECEIPT_PAYLOAD_MAX_BYTES,
        "token_max": 2 ** 53,
        "cost_max": 1e12,
        "text_max": 4096,
    }


def expected(r):
    f = r.get("func")
    if f == "validate":
        return validate(r)
    if f == "annotations":
        return annotations(r)
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
