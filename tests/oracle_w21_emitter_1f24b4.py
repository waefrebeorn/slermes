#!/usr/bin/env python3
"""Differential oracle for agent/monitoring/emitter.py (gate w21_emitter_1f24b4).

The live MonitoringEmitter is driven scenario by scenario. Two seams are
stubbed, both named in the port header:

  * the daemon thread -- `_ensure_started` becomes a no-op so the queue is only
    drained when the scenario says so (`_run`'s exact get_nowait batch loop);
  * the clock -- `time.time_ns` is pinned to the op's `now`, so `emit`'s
    `payload.setdefault("ts_ns", ...)` is deterministic.

Everything compared (drop-oldest under a full queue, dict(event) vs to_dict(),
subscriber enable/disable, fail-isolated dispatch, counters) is live code.
`capacity` is Queue(maxsize=...) -- 0 means unbounded, exactly as Python does.
"""
import importlib
import json
import queue
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w21_emitter_1f24b4"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

live = importlib.import_module("agent.monitoring.emitter")


class _Record:
    """A dataclass-shaped event: hasattr(event, "to_dict") is True."""
    def __init__(self, payload):
        self._payload = payload

    def to_dict(self):
        return dict(self._payload)


def _callback(cid, failing):
    def cb(batch, _cid=cid):
        if failing:
            raise RuntimeError("subscriber %s failed" % _cid)
    return cb


def _drain(em):
    """One `_run()` iteration body, without the blocking timeout."""
    try:
        first = em._q.get_nowait()
    except queue.Empty:
        return []
    batch = [first]
    while len(batch) < live._DRAIN_BATCH:
        try:
            batch.append(em._q.get_nowait())
        except queue.Empty:
            break
    try:
        em._dispatch(batch)
    finally:
        for _ in batch:
            em._q.task_done()
    return batch


def run_scenario(r):
    em = live.MonitoringEmitter(enabled=bool(r.get("enabled", True)))
    em._ensure_started = lambda: None
    capacity = r.get("capacity")
    if isinstance(capacity, int) and not isinstance(capacity, bool):
        em._q = queue.Queue(maxsize=capacity)
    callbacks = {}
    records = []
    ops = r.get("ops")
    for op in (ops if isinstance(ops, list) else []):
        kind = op.get("op")
        if kind == "emit":
            now = op.get("now", 0)
            live.time.time_ns = lambda _n=now: _n
            event = op.get("event")
            if isinstance(event, dict) and "__to_dict__" in event:
                event = _Record(event["__to_dict__"])
            em.emit(event)
            records.append({"op": "emit", "queued": em._q.qsize(), "dropped": em._dropped})
        elif kind == "drain":
            batch = _drain(em)
            records.append({"op": "drain", "batch": batch, "dispatched": em._dispatched})
        elif kind == "subscribe":
            cid = op.get("id", "")
            cb = callbacks.get(cid)
            if cb is None:
                cb = callbacks[cid] = _callback(cid, bool(op.get("failing")))
            em.subscribe(cb)
            records.append({"op": "subscribe", "subscribers": len(em._subscribers),
                            "enabled": em._enabled})
        elif kind == "unsubscribe":
            cid = op.get("id", "")
            cb = callbacks.pop(cid, None)
            if cb is None:
                cb = _callback(cid, False)     # a different object: remove() is a no-op
            em.unsubscribe(cb)
            records.append({"op": "unsubscribe", "subscribers": len(em._subscribers),
                            "enabled": em._enabled})
        elif kind == "stats":
            records.append({"op": "stats", "stats": em.stats()})
        else:
            records.append({"op": "unknown"})
    return records


def expected(r):
    f = r.get("func")
    if f == "run":
        return run_scenario(r)
    if f == "constants":
        return {"queue_cap": live._MAX_QUEUE, "drain_batch": live._DRAIN_BATCH}
    return "UNKNOWN_FUNC"


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
        if not isinstance(r, dict) or r.get("func") not in ("run", "constants"):
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001 - reported as the harness shapes it
            py = {"status": "error", "type": type(exc).__name__, "message": str(exc)}
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH case=%r\n  py=%r\n   c=%r" % (r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
