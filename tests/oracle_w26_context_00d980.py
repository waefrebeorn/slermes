#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/workers/_context.py
(gate w26_context_00d980).

The live module's six closed operations are called with the transaction
connection and the store holder replaced by fakes.  A payload the owner admits
gets past the gate and dies in the SQL/holder seam (or returns), which is
"accepted"; an `AuthorityRefusal` is the refusal code the C port must return.
`stamp_cache_generation` is called with a fake connection carrying the durable
archived-row count, so the only thing compared is the value it returns.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_context_00d980"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from persistence.core.errors import AuthorityRefusal  # noqa: E402
from persistence.sqlite.workers import _context as live  # noqa: E402
from persistence.workers.vocabulary import RECEIPT_PAYLOAD_MAX_BYTES, SIDECAR_KEYS  # noqa: E402

WANTED = {"validate", "stamp", "constants"}


class _Row:
    def __init__(self, value):
        self.value = value

    def fetchone(self):
        return self.value


class _Conn:
    """A transaction connection reduced to the rows these operations read."""

    def __init__(self, generation=0):
        self.generation = generation

    def execute(self, sql, params=()):
        return _Row((self.generation,))


def _finish(holder, conn, sid, payload):
    """execution.finish: the one handler with no payload at all."""
    if payload:
        raise AuthorityRefusal("invalid_params")
    return {"status": "terminal"}


OPS = {
    "worker_context": live.worker_context,
    "worker_prompt": live.worker_prompt,
    "worker_sidecars": live.worker_sidecars,
    "worker_tool_names": live.worker_tool_names,
    "worker_tail_role": live.worker_tail_role,
    "worker_row_role": live.worker_row_role,
    "worker_finish": _finish,
}


def validate(req):
    op = req.get("op")
    fn = OPS.get(op)
    if fn is None:
        return "UNKNOWN_OP"
    try:
        fn(None, _Conn(), "s1", req.get("payload"))
    except AuthorityRefusal as exc:
        return str(exc)
    except Exception:  # noqa: BLE001 -- reached the SQL/holder seam: the payload was admitted
        return None
    return None


def stamp(req):
    clock = req.get("clock")
    generation = req.get("generation")
    try:
        parsed = json.loads(clock)
        is_event = isinstance(parsed, dict)
    except ValueError:
        is_event = False
    out = live.stamp_cache_generation(_Conn(int(generation or 0)), "s1", clock)
    if not is_event:
        return {"parsed": False, "value": out}
    return {"parsed": True, "value": json.loads(out)}


def constants():
    return {
        "sidecar_keys": sorted(SIDECAR_KEYS),
        "operations": sorted(OPS),
        "payload_max_bytes": RECEIPT_PAYLOAD_MAX_BYTES,
        # literals of worker_tool_names: 0 < len(name) <= 256, len(backend) <= 64
        "max_tool_name": 256,
        "max_backend": 64,
    }


def expected(r):
    f = r.get("func")
    if f == "validate":
        return validate(r)
    if f == "stamp":
        return stamp(r)
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
