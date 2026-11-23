#!/usr/bin/env python3
"""Differential oracle for gateway/session_local_close.py (gate sessclose).

Loads the LIVE module via importlib with stubbed session_history_mount and
session_persistence (the real import chain needs `ruamel`, absent here), then
drives the two pure verdicts with scripted collaborator results. The real
AuthorityRefusal class is used for the _local_target refusal path.

Input (harness output, one JSON per line):
    {"func":"closed_idle_unviewed"|"closed_and_unviewed", ... , "out":bool}
Summary: ORACLE sessclose: N cases, M mismatches.
"""
import importlib.util
import json
import sys
import types
from types import SimpleNamespace as NS

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)


def _seed(name, **attrs):
    mod = types.ModuleType(name)
    mod.__path__ = []
    for k, v in attrs.items():
        setattr(mod, k, v)
    sys.modules[name] = mod
    parts = name.split(".")
    for i in range(1, len(parts)):
        parent = ".".join(parts[:i])
        if parent in sys.modules and not hasattr(sys.modules[parent], parts[i]):
            setattr(sys.modules[parent], parts[i], mod)
    return mod


try:
    import gateway  # noqa: F401
except Exception:
    _seed("gateway")

AuthorityRefusal = __import__("persistence.core.errors", fromlist=["AuthorityRefusal"]).AuthorityRefusal
STATE = {}


def is_history_mount(live):
    return bool(STATE.get("history", False))


def authority_persistence(authority):
    class Bindings:
        def receipt(self, session_id):
            if STATE.get("receipt_raises"):
                raise AuthorityRefusal("x")
            return NS(current_target=STATE.get("target"))

    class Sessions:
        def list_row(self, target):
            if STATE.get("no_row"):
                return None
            return NS(ended_at=STATE.get("row_ended_at"))

    return NS(bindings=Bindings(), sessions=Sessions())


_seed("gateway.session_history_mount", is_history_mount=is_history_mount)
_seed("gateway.session_persistence", authority_persistence=authority_persistence)

SRC = QUARRY + "/gateway/session_local_close.py"
_spec = importlib.util.spec_from_file_location("sessclose_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["sessclose_live"] = live
_spec.loader.exec_module(live)


def live_live(rec):
    d = rec.get("live")
    if d is None:
        return None
    return NS(subscribers=d.get("subscribers", 0), detached=bool(d.get("detached", False)))


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
        if func not in ("closed_idle_unviewed", "closed_and_unviewed"):
            continue
        cases += 1
        STATE.clear()
        STATE.update(history=bool(rec.get("history_mount")),
                     target=rec.get("target"),
                     receipt_raises=bool(rec.get("receipt_raises")),
                     no_row=bool(rec.get("no_row")),
                     row_ended_at=rec.get("row_ended_at"))
        live_obj = live_live(rec)
        state = rec.get("execution_state")
        if func == "closed_idle_unviewed":
            py = live.closed_idle_unviewed(live_obj, state, rec.get("target_ended_at"))
        else:
            authority = NS(sessions={"s1": live_obj} if live_obj is not None else {})
            py = live.closed_and_unviewed(authority, "s1", NS(execution_state=state))
        got = rec.get("out")
        if bool(py) != got:
            mism += 1
            print(f"MISMATCH sessclose case={rec.get('case')} func={func} "
                  f"C={got!r} PY={bool(py)!r}", file=sys.stderr)
    if cases == 0:
        print("ORACLE sessclose: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE sessclose: {cases} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
