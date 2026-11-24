#!/usr/bin/env python3
"""Differential oracle for hermes_cli/gateway_connections_watch.py
(gate w22_gatewaycon_d39075).

Seams stubbed, decisions live:
  filesystem  -- `_stamp(path)` is replaced by the fixture's stamp tuple.
  store read  -- `store.read_registry(path)` hands back the fixture registry.
  teardown    -- the four imported modules (oauth/reach/ssh/relay_proxy) are
                 replaced by recorders, so the live `_teardown` builds its step
                 list and calls it in order against them.

`fingerprints` is pure and runs as-is; `check` runs the live StoreWatch.check
on a stack-allocated instance (object.__new__) whose stamp/revision/prints are
seeded from the previous registry.
"""
import importlib
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "hermes_cli" / "gateway_connections_watch.py"
GATE = "w22_gatewaycon_d39075"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("gateway_connections_watch_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["gateway_connections_watch_live"] = live
_spec.loader.exec_module(live)

WANTED = {"dial_fields", "poll_interval", "fingerprints", "check", "teardown"}


def stamp(value):
    if value is None:
        return None
    return (value.get("mtime_ns"), value.get("size"), value.get("ino"))


def fingerprints(r):
    try:
        return {"ok": True, "value": live.fingerprints(r)}
    except Exception as exc:  # noqa: BLE001
        return {"ok": False, "error": type(exc).__name__}


def check(r):
    prev_registry = r.get("prev_registry")
    new_registry = r.get("new_registry")
    watch = object.__new__(live.StoreWatch)
    watch.path = None
    orig_stamp, orig_read = live._stamp, live.store.read_registry
    try:
        watch.stamp = stamp(r.get("prev_stamp"))
        watch.revision = prev_registry["revision"]
        watch.prints = live.fingerprints(prev_registry)
        live._stamp = lambda _p: stamp(r.get("new_stamp"))
        live.store.read_registry = lambda _p: new_registry
        changed, removed = watch.check()
    except Exception as exc:  # noqa: BLE001
        return {"status": "raised", "type": type(exc).__name__}
    finally:
        live._stamp, live.store.read_registry = orig_stamp, orig_read
    return {"changed": list(changed), "removed": list(removed)}


def _fake(dotted, attr, recorder):
    mod = types.ModuleType(dotted)

    def step(connection_id, _name=attr):
        recorder.append([_name, connection_id])

    step.__name__ = attr
    setattr(mod, attr, step)
    parent, _, leaf = dotted.rpartition(".")
    try:
        pkg = importlib.import_module(parent)
    except Exception:  # noqa: BLE001
        pkg = None
    sys.modules[dotted] = mod
    if pkg is not None:
        setattr(pkg, leaf, mod)
    return (dotted, pkg, leaf, getattr(pkg, leaf, None) if pkg is not None else None)


def teardown(r):
    calls = []
    changed = r.get("changed")
    removed = r.get("removed")
    changed = changed if isinstance(changed, list) else []
    removed = removed if isinstance(removed, list) else []
    saved = []
    for dotted, attr in (("hermes_cli.gateway_connections_oauth", "cancel_sign_in"),
                         ("hermes_cli.gateway_connections_reach", "forget"),
                         ("hermes_cli.gateway_connections_ssh", "disconnect"),
                         ("hermes_cli.web_routers.webapp_relay_proxy", "close_gateway_sockets")):
        saved.append((dotted, _fake(dotted, attr, calls)))
    try:
        live._teardown(changed, removed)
    finally:
        for dotted, (name, pkg, leaf, old) in saved:
            if pkg is not None and old is not None:
                setattr(pkg, leaf, old)
            sys.modules.pop(dotted, None)
    return calls


def expected(r):
    f = r.get("func")
    if f == "dial_fields":
        return list(live._DIAL_FIELDS)
    if f == "poll_interval":
        return live.POLL_INTERVAL_S
    if f == "fingerprints":
        return fingerprints(r.get("registry"))
    if f == "check":
        return check(r)
    if f == "teardown":
        return teardown(r)
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
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
