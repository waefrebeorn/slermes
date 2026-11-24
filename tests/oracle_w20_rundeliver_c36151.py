#!/usr/bin/env python3
"""Differential oracle for gateway/run_delivery_queue_watch.py (w20_rundeliver_c36151).

Imports the live module out of the quarry and injects the two seams the real
code sits on: ``os.stat`` (fixture-scripted, so a missing row is an OSError) and
``time.monotonic``/``stop_event.wait`` (fixture-scripted clocks).  ``queue_path``
is stubbed to a posix-ish path so the stamp strings are platform independent;
the watch's own tuple building and comparison are the live code.
"""
import importlib.util
import json
import os
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

DEFAULT_HOME = "/h/default"


class P:
    """A posix path with just the two tricks _stamp uses: str(), name, with_name()."""

    def __init__(self, s):
        self.s = s

    def __str__(self):
        return self.s

    def __repr__(self):
        return "P(%r)" % self.s

    @property
    def name(self):
        return self.s.rsplit("/", 1)[-1]

    def with_name(self, name):
        return P(self.s.rsplit("/", 1)[0] + "/" + name)


def make_queue_module():
    mod = types.ModuleType("cron.delivery_queue")
    mod.queue_path = lambda home=None: P(
        (str(home) if home is not None else DEFAULT_HOME) + "/cron/deliveries.db")
    if "cron" not in sys.modules:
        pkg = types.ModuleType("cron")
        pkg.__path__ = []
        sys.modules["cron"] = pkg
    sys.modules["cron.delivery_queue"] = mod


make_queue_module()

NAME = "w20_rundeliver_c36151_live"
sp = importlib.util.spec_from_file_location(
    NAME, os.path.join(Q, "gateway", "run_delivery_queue_watch.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)


def stat_table(stats):
    table = {}
    for rec in stats or []:
        if not isinstance(rec, dict):
            continue
        path = rec.get("path")
        if not isinstance(path, str):
            continue
        if rec.get("errno"):
            table[path] = None
        else:
            table[path] = (int(rec.get("mtime_ns") or 0), int(rec.get("size") or 0))
    return table


def install_os(stats):
    table = stat_table(stats)

    def fake_stat(path):
        key = str(path)
        if key not in table or table[key] is None:
            raise OSError(2, "no such file")
        mtime, size = table[key]
        return types.SimpleNamespace(st_mtime_ns=mtime, st_size=size)

    live.os = types.SimpleNamespace(stat=fake_stat)


def make_watch(homes, stats, seen=None):
    install_os(stats)
    watch = live.DeliveryQueueWatch(lambda: list(homes), lambda: None)
    if seen is not None:
        watch._seen = tuple(tuple(x) for x in seen)
    return watch


class StopEvent:
    def __init__(self, results):
        self.results = list(results)
        self.events = []
        self.last = None

    def wait(self, timeout=None):
        result = self.results.pop(0) if self.results else True
        self.events.append(("wait", timeout))
        self.last = result
        return result


class Watch:
    def __init__(self, flags):
        self.flags = list(flags)
        self.events = []
        self.drain = lambda: self.events.append(("drain-inner", None))

    def refresh_homes(self):
        self.events.append(("refresh", None))

    def changed(self):
        return bool(self.flags.pop(0)) if self.flags else False


def expected(r):
    f = r.get("func")
    if f == "watch_seconds":
        return live.DELIVERY_QUEUE_WATCH_SECONDS
    if f == "drain_label":
        return live.DRAIN_LABEL
    if f == "stamp":
        watch = make_watch(r.get("homes") or [], r.get("stats") or [])
        return [[str(p), str(m), str(s)] for p, m, s in watch._seen]
    if f == "changed":
        watch = make_watch(r.get("homes") or [], r.get("stats") or [])
        if "seen" in r:
            seen = r.get("seen")
            watch._seen = tuple(tuple(x) for x in seen) if isinstance(seen, list) else None
        moved = watch.changed()
        return {"changed": bool(moved),
                "seen": [[str(p), str(m), str(s)] for p, m, s in watch._seen]}
    if f == "wait_plan":
        interval = r.get("interval", 0.0)
        clock = list(r.get("clock") or [])
        ticks = iter(clock)
        live.time = types.SimpleNamespace(monotonic=lambda: next(ticks))
        watch_spec = r.get("watch")
        watch = Watch(r.get("changed_flags") or []) if isinstance(watch_spec, dict) else None
        stop = StopEvent(r.get("stop_results") or [])

        def chore(label, fn):
            stop.events.append(("drain", label))
            fn()

        try:
            live.wait_for_next_tick(stop, interval, watch, chore)
        except StopIteration:
            pass
        actions = []
        if watch is not None:
            actions.append({"action": "refresh"})
        for kind, payload in stop.events:
            if kind == "wait":
                actions.append({"action": "wait", "timeout": payload})
            elif kind == "drain":
                actions.append({"action": "drain", "label": payload})
        actions.append({"action": "return",
                        "why": "no-watch" if watch is None
                        else ("stopped" if stop.last else "deadline")})
        drains = sum(1 for a in actions if a["action"] == "drain")
        return {"actions": actions, "drains": drains}
    raise AssertionError("unknown func %r" % (f,))


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if not isinstance(r, dict) or r.get("func") is None:
        continue
    cases += 1
    try:
        exp = expected(r)
    except Exception as exc:  # noqa: BLE001
        mism += 1
        print("MISMATCH %s case=%s live-error=%r" % (r.get("func"), r.get("case"), exc),
              file=sys.stderr)
        continue
    got = r.get("out")
    if exp != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), exp, got),
              file=sys.stderr)
print("ORACLE w20_rundeliver_c36151: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
