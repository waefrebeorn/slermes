#!/usr/bin/env python3
"""Differential oracle for tui_gateway/_stdin_recovery.py (gate w15_stdinreco_6979ca).

The live module reads fd 0 through POSIX fcntl/socket; on this Windows host it
falls back to the no-fcntl path. The oracle injects the same resolved facts into
the live functions via module-global stubs and compares the exact log strings,
the 60s window filter, the rate cap and the True/False decision with the C
harness `out`.
"""
import importlib.util
import json
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

_spec = importlib.util.spec_from_file_location(
    "stdinreco_live", QUARRY + "/tui_gateway/_stdin_recovery.py")
live = importlib.util.module_from_spec(_spec)
sys.modules["stdinreco_live"] = live
_spec.loader.exec_module(live)


class RawRepr:
    """repr() returns the injected string verbatim."""
    def __init__(self, s):
        self.s = s

    def __repr__(self):
        return self.s


def canon(x):
    if isinstance(x, (list, tuple)):
        return [canon(v) for v in x]
    if isinstance(x, dict):
        return {k: canon(v) for k, v in x.items()}
    if isinstance(x, float) and x.is_integer():
        return int(x)
    return x


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    try:
        if func == "diagnose":
            if r.get("fcntl_available"):
                flags = r.get("flags", 0)
                err = r.get("fgetfl_error")

                class FakeFcntl:
                    F_GETFL = 3

                    def fcntl(self, fd, cmd):
                        if err:
                            raise Exception(err)
                        return flags

                live.fcntl = FakeFcntl()
                live.os = types.SimpleNamespace(O_NONBLOCK=r.get("nonblock_flag", 2048))
            else:
                live.fcntl = None
            tv = r.get("rcvtimeout_repr")
            live._stdin_sockopt = lambda getter: (RawRepr(tv) if tv is not None else None)
            py = live.diagnose_stdin_state()
        elif func == "handle":
            live.fcntl = object()  # not None -> scripted _stdin_nonblock below
            live._stdin_nonblock = lambda: bool(r.get("nonblock"))
            now = r.get("now", 0)
            live.time = types.SimpleNamespace(time=lambda: now, monotonic=lambda: now,
                                              sleep=lambda d: None)
            live.MAX_RECOVERIES_PER_MINUTE = r.get("max_recoveries", 10)
            diag = r.get("diag_state", "")
            live.diagnose_stdin_state = lambda: diag
            live.os = types.SimpleNamespace(set_blocking=lambda *a: None, O_NONBLOCK=2048)
            live._stdin_sockopt = lambda getter: None
            times = list(r.get("recovery_times") or [])
            logs = []
            ret = live.handle_spurious_eof(times, logs.append)
            py = {"recovered": bool(ret), "logs": logs, "recovery_times": times}
        else:
            continue
    except Exception as e:
        print("PY-ERROR %r: %s" % (r, e), file=sys.stderr)
        mism += 1
        continue
    cases += 1
    want, got = canon(py), canon(r.get("out"))
    if want != got:
        mism += 1
        print("MISMATCH %r py=%r c=%r" % (r, want, got), file=sys.stderr)

print("ORACLE w15_stdinreco_6979ca: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
