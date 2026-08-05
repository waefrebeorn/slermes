"""Differential oracle for agent/monitoring/emitter.py.

Reads the C driver's JSON lines from stdin and compares each against the expected
value computed from LIVE Python (the parent repo's agent/monitoring/emitter.py).
Prints `agent/monitoring/emitter oracle: N cases, M mismatches` and one
`MISMATCH case=...` line per divergence. Exit 0 on full match.
"""

import sys
import json
import os
import threading
import time

_PARENT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
if _PARENT not in sys.path:
    sys.path.insert(0, _PARENT)

from agent.monitoring import emitter as emod  # noqa: E402


def main():
    raw = sys.stdin.read()
    cases = [json.loads(line) for line in raw.splitlines() if line.strip()]
    mismatches = 0

    captured = []
    def sub_capture(batch):
        captured.clear()
        captured.extend(list(batch))
    cnt = {"n": 0}
    def sub_count(batch):
        cnt["n"] += len(batch)

    def run(case):
        if case == "disabled_emit":
            em = emod.MonitoringEmitter(enabled=False)
            r = em.emit({"x": 1})
            # Python returns None on the disabled hot-path; the C port signals the
            # same no-op with -1. Compare the *effect* (queued stays 0).
            return {"ret": -1 if r is None else (1 if r else 0), "queued": em.stats()["queued"]}
        if case == "drain":
            em = emod.MonitoringEmitter(enabled=True)
            em.subscribe(sub_capture)
            em.emit({"name": "a"})
            em.emit({"name": "b"})
            n = em._dispatch_drain_once() if hasattr(em, "_dispatch_drain_once") else _drain_once(em)
            return {"dispatched": n, "captured": len(captured),
                    "first_name": (captured[0]["name"] == "a") if captured else False}
        if case == "drop_oldest":
            import queue as _queue
            em = emod.MonitoringEmitter(enabled=True)
            em._q = _queue.Queue(maxsize=3)
            em.emit({"i": 1}); em.emit({"i": 2}); em.emit({"i": 3}); em.emit({"i": 4});
            st = em.stats()
            return {"dropped": st["dropped"], "queued": st["queued"]}
        if case == "sub_toggle":
            em = emod.MonitoringEmitter(enabled=False)
            before = em.emit({"x": 1})
            em.subscribe(sub_count)
            after = em.emit({"x": 1})
            em.unsubscribe(sub_count)
            after_unsub = em.emit({"x": 1})
            return {"before": -1 if before is None else (1 if before else 0),
                    "after": 1 if after else 0,
                    "after_unsub": -1 if after_unsub is None else (1 if after_unsub else 0)}
        if case == "fail_iso":
            em = emod.MonitoringEmitter(enabled=True)
            em.subscribe(sub_capture)
            em.subscribe(sub_count)
            em.emit({"x": 1})
            # make capture raise on next dispatch
            def bad(batch):
                raise RuntimeError("boom")
            em._subscribers[0] = bad
            n = em._dispatch_drain_once() if hasattr(em, "_dispatch_drain_once") else _drain_once(em)
            return {"dispatched": n, "peer_count": cnt["n"]}
        if case == "stats":
            em = emod.MonitoringEmitter(enabled=True)
            em.subscribe(sub_capture)
            em.emit({"k": 1}); em.emit({"k": 2})
            em._dispatch_drain_once() if hasattr(em, "_dispatch_drain_once") else _drain_once(em)
            st = em.stats()
            return {"dispatched": st["dispatched"], "subscribers": st["subscribers"]}
        if case == "singleton":
            a = emod.get_emitter(); b = emod.get_emitter()
            return {"same": a is b}
        if case == "reset":
            c = emod.MonitoringEmitter(enabled=False)
            emod.reset_emitter_for_tests(c)
            d = emod.get_emitter()
            emod.reset_emitter_for_tests(None)
            return {"changed": d is c}
        return {}

    for c in cases:
        case = c["case"]
        expected = run(case)
        for k, ev in expected.items():
            if k not in c:
                mismatches += 1
                print("MISMATCH case=%s key=%s PY=%r C=MISSING" % (case, k, ev))
                continue
            cv = c[k]
            if isinstance(ev, bool):
                eq = (cv is True or cv == "true" or cv == 1) == ev
            elif isinstance(ev, int):
                try:
                    eq = (int(cv) == ev)
                except (TypeError, ValueError):
                    eq = False
            else:
                eq = (cv == ev)
            if not eq:
                mismatches += 1
                print("MISMATCH case=%s %s PY=%r C=%r" % (case, k, ev, cv))

    print("agent/monitoring/emitter oracle: %d cases, %d mismatches" % (len(cases), mismatches))
    sys.exit(1 if mismatches else 0)


def _sig(cls):
    import inspect
    return inspect.signature(cls.__init__)


def _drain_once(em):
    """Mirror the dispatcher's batch extract + fan-out synchronously."""
    items = []
    try:
        items.append(em._q.get_nowait())
    except Exception:
        return 0
    while len(items) < emod._DRAIN_BATCH:
        try:
            items.append(em._q.get_nowait())
        except Exception:
            break
    batch = items
    em._dispatch(batch)
    for _ in batch:
        em._q.task_done()
    return len(batch)


if __name__ == "__main__":
    main()
