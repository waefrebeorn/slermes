#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/telegram/telegram_held_inbound.py (gate w19_telegramhe_c40f34).

The live hold queue runs against a fake adapter (its fatal/drop flags, its scheduler and its
successor are fixture-controlled), so the compared value is the queue the module leaves.
"""
import ast
import itertools
import json
import logging
import sys
import types
import weakref
import __future__
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
Q = REPO.parent / "hermes-agent"
SRC = Q / "plugins" / "platforms" / "telegram" / "telegram_held_inbound.py"
GATE = "w19_telegramhe_c40f34"
_ANN = __future__.annotations.compiler_flag
_FN = (ast.FunctionDef, ast.AsyncFunctionDef)

WANT_FUNCS = {"_insert_by_arrival", "_note_arrival", "_hold_inbound_event", "_adopt_held_event"}
WANT_ASSIGNS = {"_ARRIVAL_ATTR", "_ARRIVALS"}


class Logger:
    def warning(self, *a, **k):
        return None

    def error(self, *a, **k):
        return None

    def exception(self, *a, **k):
        return None


def extract():
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    g = {"__name__": "thi_live", "itertools": itertools, "weakref": weakref,
         "logger": Logger(), "Optional": object, "MessageEvent": object,
         "asyncio": __import__("asyncio"), "cancel_task": None}
    for node in tree.body:
        if isinstance(node, _FN) and node.name in WANT_FUNCS:
            _run(node, g, SRC)
        elif isinstance(node, ast.Assign) and any(
                isinstance(t, ast.Name) and t.id in WANT_ASSIGNS for t in node.targets):
            _run(node, g, SRC)
        elif isinstance(node, ast.ClassDef) and node.name == "TelegramHeldInboundMixin":
            for sub in node.body:
                if isinstance(sub, _FN) and sub.name in WANT_FUNCS:
                    _run(sub, g, SRC)
    return g


def _run(node, g, path):
    mod = ast.Module(body=[node], type_ignores=[])
    exec(compile(mod, str(path), "exec", flags=_ANN, dont_inherit=True), g)


G = extract()
ARRIVAL_ATTR = G["_ARRIVAL_ATTR"]


class Counter:
    """itertools.count with a readable position."""

    def __init__(self, start):
        self.n = start

    def __next__(self):
        value = self.n
        self.n += 1
        return value


def mk_event(eid, seq, text_len=0):
    ev = SimpleNamespace(id=eid, text="x" * int(text_len or 0), source=None)
    if seq is not None:
        setattr(ev, ARRIVAL_ATTR, seq)
    return ev


def case_insert_by_arrival(r):
    held = [mk_event("h%d" % i, s) for i, s in enumerate(r.get("held_stamps") or [])]
    event = mk_event("new", r.get("seq"))
    G["_insert_by_arrival"](held, event)
    return [getattr(ev, ARRIVAL_ATTR, None) for ev in held]


def case_note_arrival(r):
    counter = Counter(r.get("counter", 0))
    G["_ARRIVALS"] = counter
    event = mk_event("e", r.get("existing_stamp"))
    G["_note_arrival"](event)
    return {"stamp": getattr(event, ARRIVAL_ATTR, None), "next": counter.n}


class FakeAdapter:
    def __init__(self, fatal=False, drop=False, max_n=64, queue=None, successor=None):
        self._held_inbound_events = list(queue or [])
        self.HELD_INBOUND_MAX = max_n
        self.fatal = fatal
        self.drop = drop
        self.accepted = 0
        self.scheduled = 0
        self._held_inbound_successor = (lambda: successor) if successor is not None else None

    def _is_permanent_fatal(self):
        return self.fatal

    def _should_drop_delayed_delivery(self):
        return self.drop

    def _accept_update(self):
        self.accepted += 1

    def _schedule_held_inbound_redispatch(self):
        self.scheduled += 1

    def _adopt_held_event(self, event, *, where, schedule):
        G["_adopt_held_event"](self, event, where=where, schedule=schedule)


# the mixin methods are plain functions here; bind them onto the fake adapter
FakeAdapter._hold_inbound_event = lambda self, *a, **k: G["_hold_inbound_event"](self, *a, **k)
FakeAdapter._adopt_held_event = lambda self, *a, **k: G["_adopt_held_event"](self, *a, **k)


def render(queue):
    return [{"id": ev.id, "_seq": getattr(ev, ARRIVAL_ATTR, None)} for ev in queue]


def case_hold_event(r):
    queue = [mk_event(e.get("id"), e.get("_seq"), e.get("len")) for e in (r.get("queue") or [])]
    spec = r.get("event") or {}
    same_as = spec.get("same_as")
    if same_as is not None and 0 <= int(same_as) < len(queue):
        event = queue[int(same_as)]           # the very object already held
    else:
        event = mk_event(spec.get("id"), spec.get("_seq"), spec.get("len"))
    successor = None
    if r.get("successor_queue") is not None:
        successor = FakeAdapter(queue=[mk_event(e.get("id"), e.get("_seq"), e.get("len"))
                                       for e in r.get("successor_queue") or []])
    adapter = FakeAdapter(fatal=bool(r.get("fatal")), drop=bool(r.get("drop_delayed")),
                          max_n=int(r.get("max_n", 64)), queue=queue, successor=successor)
    G["_hold_inbound_event"](adapter, event, where="live", schedule=bool(r.get("schedule", True)))
    return {"queue": render(adapter._held_inbound_events), "accepted": adapter.accepted,
            "scheduled": adapter.scheduled, "forwarded": adapter.accepted == 1 and successor is not None,
            "target_queue": render(successor._held_inbound_events) if successor is not None else None}


CASES = {"insert_by_arrival": case_insert_by_arrival, "note_arrival": case_note_arrival,
         "hold_event": case_hold_event}


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
        f = r.get("func")
        if f not in CASES:
            continue
        cases += 1
        try:
            py = json.loads(json.dumps(CASES[f](r), default=str))
        except BaseException as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s oracle raised %r" % (f, exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s py=%r c=%r" % (f, py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
