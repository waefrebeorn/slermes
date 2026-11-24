#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/conversation/messages.py (gate p1).

The live repository module is imported as a package module and only its pure
wire-shaping helpers are called -- no StoreHolder, no connection, no schema.
That is the seam: what is compared is exactly what those values decide.

  * ``_REWIND_REFUSALS``   the RuntimeError text -> contract reason table;
  * ``_visibility``        the RowVisibility rule;
  * ``_message_dict``      a NewMessage as the legacy writers' message dict;
  * ``_held_dict``         a HeldMessage as the held dict, retired rows recursed;
  * ``_pruned_original``   before's dict with the held original's fields over it;
  * ``_transcript_row``    a stored row dict as its TranscriptRow wire projection;
  * ``_sidecar``           a JSON-text sidecar decoded;
  * ``_MESSAGE_OPTIONALS``/``_WIRE_FIELDS``/``_SIDECARS`` the field lists.
"""
import dataclasses
import json
import sys
from enum import Enum
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w30_messages_29cf8f_p1"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

import persistence.sqlite.conversation.messages as live  # noqa: E402
from persistence.conversation.messages import HeldMessage, NewMessage, PrunedRowChange  # noqa: E402

WANTED = {"rewind_refusal_reason", "visibility", "message_dict", "held_dict", "pruned_original",
          "transcript_row", "sidecar", "wire_fields"}

_NEW_TUPLES = ("tool_calls", "absorbed_row_ids", "data_use", "absorbed_message_uids")
_HELD_TUPLES = ("tool_calls", "absorbed_row_ids", "tool_call_ids")


def _plain(value, names):
    out = {}
    for key, item in (value or {}).items():
        if key in names and isinstance(item, list):
            item = tuple(item)
        out[key] = item
    return out


def new_message(raw):
    return NewMessage(**_plain(raw or {}, _NEW_TUPLES))


def held_message(raw):
    fields = _plain(raw or {}, _HELD_TUPLES)
    if isinstance(fields.get("retired_durable_rows"), list):
        fields["retired_durable_rows"] = tuple(held_message(row) for row in fields["retired_durable_rows"])
    return HeldMessage(**fields)


def norm(value):
    if dataclasses.is_dataclass(value):
        value = dataclasses.asdict(value)
    if isinstance(value, Enum):
        return value.value
    if isinstance(value, dict):
        return {key: norm(item) for key, item in value.items()}
    if isinstance(value, (list, tuple)):
        return [norm(item) for item in value]
    return value


def expected(r):
    f = r.get("func")
    if f == "rewind_refusal_reason":
        return live._REWIND_REFUSALS.get(r.get("text"))
    if f == "visibility":
        return live._visibility(r.get("active"), r.get("compacted")).value
    if f == "message_dict":
        return norm(live._message_dict(new_message(r.get("message"))))
    if f == "held_dict":
        return norm(live._held_dict(held_message(r.get("held"))))
    if f == "pruned_original":
        change = PrunedRowChange(original=held_message(r.get("original")),
                                 replacement=new_message(r.get("replacement") or {"role": "user", "content": ""}),
                                 before=new_message(r.get("before")))
        return norm(live._pruned_original(change))
    if f == "transcript_row":
        return norm(live._transcript_row(r.get("row") or {}))
    if f == "sidecar":
        return norm(live._sidecar(r.get("value")))
    if f == "wire_fields":
        return {"message_optionals": list(live._MESSAGE_OPTIONALS), "wire_fields": list(live._WIRE_FIELDS),
                "sidecars": list(live._SIDECARS)}
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
            py = norm(expected(r))
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        got = norm(got)
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
