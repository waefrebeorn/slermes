#!/usr/bin/env python3
"""Differential oracle for gateway/control_dispatch.py (gate ctrldisp).

Drives the LIVE ControlRequestDispatcher.dispatch with tracking executors.
A fixture line's `raw` is handed to the real dispatch(); the executor that
actually receives handler.submit decides 'producers' vs 'controls'. That live
verdict is compared with the C harness's `out`.

Input (harness output, one JSON per line):
    {"func":"select"|"is_producer", "raw":<str>, "peer":..., "out":<verdict>}
Summary: ORACLE ctrldisp: N cases, M mismatches.
"""
import asyncio
import concurrent.futures
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)

SRC = QUARRY + "/gateway/control_dispatch.py"
_spec = importlib.util.spec_from_file_location("ctrldisp_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["ctrldisp_live"] = live
_spec.loader.exec_module(live)


class Tracker:
    """Minimal executor: runs submit inline and records which pool was picked."""

    def __init__(self, name, bag):
        self.name = name
        self.bag = bag

    def submit(self, fn, *args):
        self.bag.append(self.name)
        fut = concurrent.futures.Future()
        try:
            fut.set_result(fn(*args))
        except BaseException as exc:  # preserve a raising handler
            fut.set_exception(exc)
        return fut


def live_verdict(raw, peer):
    dispatcher = live.ControlRequestDispatcher()
    real_controls, real_producers = dispatcher._controls, dispatcher._producers
    controls, producers = [], []
    dispatcher._controls = Tracker("controls", controls)
    dispatcher._producers = Tracker("producers", producers)
    try:
        asyncio.run(dispatcher.dispatch(lambda b, p: b, raw.encode("utf-8"), peer))
    finally:
        real_controls.shutdown(wait=False)
        real_producers.shutdown(wait=False)
    if producers:
        return "producers"
    if controls:
        return "controls"
    return "none"


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec = json.loads(line)
        except ValueError:
            continue
        func = rec.get("func")
        if func not in ("select", "is_producer"):
            continue
        cases += 1
        py = live_verdict(rec.get("raw") or "", rec.get("peer"))
        want = py if func == "select" else (py == "producers")
        got = rec.get("out")
        if want != got:
            mism += 1
            print(f"MISMATCH ctrldisp case={rec.get('case')} func={func} "
                  f"raw={rec.get('raw')!r} C={got!r} PY={want!r}", file=sys.stderr)
    if cases == 0:
        print("ORACLE ctrldisp: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE ctrldisp: {cases} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
