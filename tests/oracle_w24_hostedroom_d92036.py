#!/usr/bin/env python3
"""Differential oracle for gateway/hosted_room_work_lineage.py
(gate w24_hostedroom_d92036).

Two of the module's three public entry points take a store connection.  Here
that connection is a list of row dicts, and the two reads the module performs
before deciding -- ``read_history_locked`` and ``validate_history`` -- are
replaced by the case's own spans.  Nothing is opened, nothing is compiled: the
seam is exactly the read, and what is compared is the verification decision the
live function makes over those rows (sequence continuity, epoch agreement, the
gateway actor's identity, the transfer-kind boundary, receipt origins).
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_hostedroom_d92036"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from gateway import hosted_room_work_lineage as live  # noqa: E402
from gateway.hosted_room_authority_history import (  # noqa: E402
    MAX_AUTHORITY_SPANS,
    AuthorityHistoryError,
    AuthoritySpan,
)
from gateway.hosted_room_work_records import (  # noqa: E402
    WorkRecordError,
    WorkRecordPrefixError,
)

WANTED = {"at_sequence", "prefix", "provenance", "task_origins", "constants"}

PREFIX_CODES = {
    "successor work evidence has no verified lineage": "no_lineage",
    "work record history prefix is not verified": "unverified",
    "work record history prefix is unavailable": "unavailable",
}


class FakeConn:
    def __init__(self, events):
        self.events = [dict(e) for e in events] if isinstance(events, list) else []

    def execute(self, sql, params=()):  # noqa: ARG002 - the SQL is never compiled
        return list(self.events)


def spans_of(raw):
    if raw is None:
        return None
    return tuple(AuthoritySpan(item["gateway_id"], item["epoch"], item["from_seq"]) for item in raw)


def span_view(span):
    return {"gateway_id": span.gateway_id, "epoch": span.epoch, "from_seq": span.from_seq}


def at_sequence(req):
    try:
        return span_view(live.at_sequence(spans_of(req.get("spans")) or (), int(req.get("seq") or 0)))
    except AuthorityHistoryError:
        return {"ok": False, "error": "precedes"}


def prefix(req):
    history = req.get("spans")
    spans = spans_of(history)
    live.read_history_locked = lambda *a, **k: history  # noqa: ARG005
    live.validate_history = lambda *a, **k: spans  # noqa: ARG005
    authority = {"gateway_id": "g1", "epoch": 1}
    try:
        verified, _digest = live.source_prefix_locked(FakeConn(req.get("events")), "room-1", authority, int(req.get("seq") or 0))
    except WorkRecordPrefixError as exc:
        return {"ok": False, "error": PREFIX_CODES.get(str(exc), "unverified")}
    except AuthorityHistoryError:
        return {"ok": False, "error": "precedes"}
    except Exception:  # noqa: BLE001 - an undecodable actor row / bad coordinate
        return {"ok": False, "error": "decode"}
    return {"ok": True, "spans": [span_view(span) for span in verified]}


def provenance(req):
    try:
        live.validate_provenance(req.get("record"), spans_of(req.get("spans")) or ())
    except WorkRecordPrefixError:
        return {"ok": False, "error": "no_authority_span"}
    except WorkRecordError:
        return {"ok": False, "error": "no_authority_span"}
    except AuthorityHistoryError:
        return {"ok": False, "error": "precedes"}
    except (KeyError, TypeError, ValueError):
        return {"ok": False, "error": "key"}
    return {"ok": True}


def task_origins(req):
    try:
        origins = live.task_origins(req.get("record"), spans_of(req.get("spans")) or ())
    except AuthorityHistoryError:
        return {"ok": False, "error": "precedes"}
    except (KeyError, TypeError, ValueError):
        return {"ok": False, "error": "key"}
    return {key: {"gateway_id": value["gateway_id"], "epoch": value["epoch"]} for key, value in origins.items()}


def constants():
    return {"transfer_kinds": list(live.TRANSFER_KINDS), "max_authority_spans": MAX_AUTHORITY_SPANS}


def expected(r):
    f = r.get("func")
    if f == "at_sequence":
        return at_sequence(r)
    if f == "prefix":
        return prefix(r)
    if f == "provenance":
        return provenance(r)
    if f == "task_origins":
        return task_origins(r)
    if f == "constants":
        return constants()
    raise AssertionError("unknown func %r" % (f,))


def eq(a, b):
    """Python equality: absent key == null == None, True == 1, 1 == 1.0."""
    if isinstance(a, bool) or isinstance(b, bool):
        return isinstance(a, (bool, int, float)) and isinstance(b, (bool, int, float)) and a == b
    if isinstance(a, dict) and isinstance(b, dict):
        return set(a) == set(b) and all(eq(a[k], b[k]) for k in a)
    if isinstance(a, list) and isinstance(b, list):
        return len(a) == len(b) and all(eq(x, y) for x, y in zip(a, b))
    if a is None or b is None:
        return a is b
    return a == b


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
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if not eq(py, got):
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
