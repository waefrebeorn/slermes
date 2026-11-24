#!/usr/bin/env python3
"""Differential oracle for tui_gateway/contracts/events.py (gate w32_events_3805ad).

The live catalog is read by re-executing the module with ``registry.event``
swapped for a recorder: the registry is process-wide and other contract modules
declare their own events into it, so "what events.py owns" is exactly the set its
own ``event(...)`` calls produce, in declaration order.  That gives the live
EventContract objects (payload class, ``extra`` policy, doc) without touching a
socket, an emitter, or pydantic's compiled validators -- which this host's
application-control policy refuses to load, hence the seam stubs.

Compared, value for value:
  * ``catalog`` / ``schema_all`` / ``schema`` -- the declared surface and each
    payload's field names, annotations and requiredness;
  * ``problems`` / ``frames`` -- ``registry.check_payload``'s structural
    verdict (missing required field, undeclared key on a closed payload, a
    payload on a payloadless frame);
  * ``enums`` -- the module's WireEnum value sets;
  * ``select`` -- catalog filtering by openness / payload model / doc text.
"""
import enum
import importlib
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w32_events_3805ad"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))
sys.path.append(str(Path(__file__).resolve().parent))

from hermes_seam_stubs import install as _install_seams  # noqa: E402

_install_seams()

import tui_gateway.contracts  # noqa: E402,F401  (populate the registry normally)
import tui_gateway.contracts.registry as registry  # noqa: E402


def _capture():
    """(live module, [EventContract, ...]) for events.py's own declarations."""
    seen = []

    def record(name, payload=None, *, doc=""):
        entry = registry.EventContract(name, payload, doc)
        seen.append(entry)
        return entry

    original = registry.event
    registry.event = record
    try:
        sys.modules.pop("tui_gateway.contracts.events", None)
        live = importlib.import_module("tui_gateway.contracts.events")
    finally:
        registry.event = original
    return live, seen


LIVE, CONTRACTS = _capture()
BY_NAME = {c.name: c for c in CONTRACTS}
ORDER = [c.name for c in CONTRACTS]
WANTED = {"catalog", "schema_all", "schema", "problems", "enums", "select", "frames"}


def _model_name(contract):
    return contract.payload.__name__ if contract.payload is not None else None


def _is_open(contract):
    payload = contract.payload
    if payload is None:
        return False
    return (getattr(payload, "model_config", None) or {}).get("extra") == "allow"


def _field_rows(contract):
    """[(name, annotation, required)] -- inherited fields first, in order.

    ``required`` is read off the class that last declared the annotation: a
    subclass that re-annotates a field without a default makes it required even
    when the parent gave it one.
    """
    payload = contract.payload
    if payload is None:
        return []
    rows = {}
    for base in reversed(payload.__mro__):
        for name, ann in (base.__dict__.get("__annotations__") or {}).items():
            rows[name] = (str(ann), not hasattr(base, name))
    return [(name, ann, required) for name, (ann, required) in rows.items()]


def _entry(contract):
    rows = _field_rows(contract)
    return {
        "name": contract.name,
        "payload": _model_name(contract),
        "open": _is_open(contract),
        "required": [f for f, _, required in rows if required],
        "optional": [f for f, _, required in rows if not required],
        "types": {f: ann for f, ann, _ in rows},
    }


def catalog(_req):
    return [{"name": c.name, "payload": _model_name(c), "open": _is_open(c), "doc": c.doc}
            for c in CONTRACTS]


def schema_all(_req):
    return [_entry(c) for c in CONTRACTS]


def schema(req):
    event = req.get("event")
    contract = BY_NAME.get(event) if isinstance(event, str) else None
    return _entry(contract) if contract else None


def problems(event, payload):
    contract = BY_NAME.get(event) if isinstance(event, str) else None
    if contract is None:
        return None
    if contract.payload is None:
        return ["payload:forbidden"] if payload else []
    if payload is None:
        payload = {}
    if not isinstance(payload, dict):
        return ["payload:dict_type"]
    rows = _field_rows(contract)
    out = [f + ":missing" for f, _, required in rows if required and f not in payload]
    if not _is_open(contract):
        known = {f for f, _, _ in rows}
        out += [k + ":extra_forbidden" for k in payload if k not in known]
    return out


def enums(_req):
    return {name: [member.value for member in cls]
            for name, cls in vars(LIVE).items()
            if isinstance(cls, type)
            and issubclass(cls, enum.Enum)
            and cls.__module__ == LIVE.__name__}


def select(req):
    raw = req.get("filter")
    flt = raw if isinstance(raw, dict) else {}
    want_open = flt.get("open")
    want_payload = flt.get("payload")
    want_doc = flt.get("doc_has")
    out = []
    for contract in CONTRACTS:
        if want_open is not None and bool(want_open) != _is_open(contract):
            continue
        if want_payload is not None and str(want_payload) != _model_name(contract):
            continue
        if want_doc is not None and str(want_doc) not in contract.doc:
            continue
        out.append(contract.name)
    return out


def frames(req):
    raw = req.get("frames")
    rows = raw if isinstance(raw, list) else []
    out = []
    for index, frame in enumerate(rows):
        if isinstance(frame, dict):
            event, payload = frame.get("event"), frame.get("payload")
        else:
            event, payload = None, None
        out.append({"index": index, "problems": problems(event, payload)})
    return out


def expected(r):
    func = r.get("func")
    if func == "catalog":
        return catalog(r)
    if func == "schema_all":
        return schema_all(r)
    if func == "schema":
        return schema(r)
    if func == "problems":
        return problems(r.get("event"), r.get("payload"))
    if func == "enums":
        return enums(r)
    if func == "select":
        return select(r)
    if func == "frames":
        return frames(r)
    raise AssertionError("unknown func %r" % (func,))


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
            print("MISMATCH %s case=%r py=%s c=%s"
                  % (r.get("func"), r.get("case"), json.dumps(py)[:400], json.dumps(got)[:400]),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
