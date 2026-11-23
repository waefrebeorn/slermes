#!/usr/bin/env python3
"""Differential oracle for w11_eventrepla_0d019f (tui_gateway/event_replay.py).

Loads the LIVE module via importlib.util.spec_from_file_location with a
sys.path.insert(0, quarry) + parent-package bootstrap, drives the same op
script the C harness drives, and compares the JSON result list.
"""
import copy
import importlib.util
import json
import os
import sys

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

Q = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, Q)
MOD = "tui_gateway/event_replay"


def _load_live():
    parts = MOD.split("/")
    for i in range(1, len(parts)):
        pname = ".".join(parts[:i])
        if pname in sys.modules:
            continue
        pdir = os.path.join(Q, *parts[:i])
        init = os.path.join(pdir, "__init__.py")
        sp = importlib.util.spec_from_file_location(
            pname, init, submodule_search_locations=[pdir])
        pm = importlib.util.module_from_spec(sp)
        sys.modules[pname] = pm
        try:
            sp.loader.exec_module(pm)
        except Exception:
            pass
    name = ".".join(parts)
    sp = importlib.util.spec_from_file_location(name, os.path.join(Q, MOD + ".py"))
    live = importlib.util.module_from_spec(sp)
    sys.modules[name] = live
    sp.loader.exec_module(live)
    return live


live = _load_live()

DEFAULTS = {
    "_REPLAY_BUFFER_MAX": 512,
    "_REPLAY_SESSIONS_MAX": 64,
    "_REPLAY_BUFFER_BYTES_MAX": 4 * 1024 * 1024,
    "_REPLAY_PROCESS_BYTES_MAX": 64 * 1024 * 1024,
}


def _canon(v):
    return json.loads(json.dumps(v, ensure_ascii=False))


def drive(ops):
    for k, v in DEFAULTS.items():
        setattr(live, k, v)
    live.reset_replay_state()
    out = []
    for op in ops:
        n = op[0]
        if n == "reset":
            for k, v in DEFAULTS.items():
                setattr(live, k, v)
            live.reset_replay_state()
            out.append(None)
        elif n == "config":
            live._REPLAY_BUFFER_MAX = int(op[1])
            live._REPLAY_SESSIONS_MAX = int(op[2])
            live._REPLAY_BUFFER_BYTES_MAX = int(op[3])
            live._REPLAY_PROCESS_BYTES_MAX = int(op[4])
            out.append(None)
        elif n == "stamp":
            evt = copy.deepcopy(op[1])
            live._stamp_event(evt)
            params = evt.get("params")
            seq = params.get("seq") if isinstance(params, dict) else None
            out.append(0 if seq is None else seq)  # C stamp returns 0 when not applied
        elif n == "events_since":
            out.append(live.events_since(op[1], int(op[2])))
        elif n == "is_truncated":
            out.append(live.is_truncated(op[1], int(op[2])))
        elif n == "latest_seq":
            out.append(live.latest_seq(op[1]))
        elif n == "forget":
            live.forget_session(op[1])
            out.append(None)
        elif n == "release":
            live.release_key(op[1])
            out.append(None)
        elif n == "stats":
            out.append(live.replay_stats())
        else:
            out.append(None)
    return _canon(out)


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec = json.loads(line)
        except Exception:
            continue
        if rec.get("func") != "batch":
            continue
        cases += 1
        got = rec.get("out")
        try:
            want = drive(rec.get("ops") or [])
        except BaseException as exc:
            mism += 1
            print("MISMATCH oracle-drive %s: %s: %s" %
                  (type(exc).__name__, exc), file=sys.stderr)
            continue
        if _canon(got) != want:
            mism += 1
            print("MISMATCH w11_eventrepla_0d019f case=%d\n  py=%s\n  c =%s" %
                  (cases, json.dumps(want, ensure_ascii=False),
                   json.dumps(_canon(got), ensure_ascii=False)), file=sys.stderr)
    print("ORACLE w11_eventrepla_0d019f: %d cases, %d mismatches" % (cases, mism))
    if cases == 0:
        sys.exit(2)
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
