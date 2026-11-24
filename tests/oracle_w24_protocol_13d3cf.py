#!/usr/bin/env python3
"""Differential oracle for plugins/google_meet/node/protocol.py (gate w24_protocol_13d3cf).

The live module is the codec itself -- dicts in, dicts out, no socket -- so it
is called directly for every function:

  * ``_nonempty_str`` -- the isinstance+truthy predicate;
  * ``make_request`` / ``make_response`` / ``make_error`` -- envelopes, with
    ``uuid.uuid4`` replaced by a fixed fake so the generated id is comparable
    (the C side takes that hex through its ``uuid`` seam);
  * ``encode`` / ``decode`` -- the compact codec, and the ValueError text's
    stable head for a malformed envelope;
  * ``validate_request`` -- the ordered (ok, reason) chain;
  * ``VALID_REQUEST_TYPES``.
"""
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_protocol_13d3cf"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from plugins.google_meet.node import protocol as live  # noqa: E402

WANTED = {"nonempty_str", "make_request", "make_response", "make_error", "encode",
          "decode", "validate_request", "constants"}

FAKE_HEX = "00000000000000000000000000000000"


class _FakeUuid:
    hex = FAKE_HEX


# The uuid4() seam: same hex the fixtures hand the C side.
live.uuid = types.SimpleNamespace(uuid4=lambda: _FakeUuid())


def _value_error(fn, *args):
    try:
        return fn(*args)
    except ValueError as exc:
        return {"error": str(exc)}


def _decode_reason(exc):
    """The stable head of the ValueError text ('malformed JSON: <detail>')."""
    msg = str(exc)
    return "malformed JSON" if msg.startswith("malformed JSON") else msg


def expected(r):
    f = r.get("func")
    if f == "nonempty_str":
        return live._nonempty_str(r.get("value"))
    if f == "make_request":
        return _value_error(live.make_request, r.get("type"), r.get("token"),
                            r.get("payload"), r.get("req_id"))
    if f == "make_response":
        return _value_error(live.make_response, r.get("req_id"), r.get("payload"))
    if f == "make_error":
        return live.make_error(r.get("req_id"), r.get("error"))
    if f == "encode":
        return live.encode(r.get("msg"))
    if f == "decode":
        try:
            return {"ok": True, "envelope": live.decode(r.get("raw"))}
        except ValueError as exc:
            return {"ok": False, "reason": _decode_reason(exc)}
    if f == "validate_request":
        return list(live.validate_request(r.get("msg"), r.get("expected_token")))
    if f == "constants":
        return {"valid_request_types": sorted(live.VALID_REQUEST_TYPES),
                "response_type": "response", "error_type": "error"}
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
