#!/usr/bin/env python3
"""Differential oracle for hermes_cli/web_host_terminal_ssh.py (gate w25_webhostter_aa608d).

The live module is imported as a package module.  Its three value decisions are
called through the same seams the route uses:

  * ``_profile`` on a websocket whose ``query_params`` are a plain dict;
  * ``_entry`` with ``store.read_registry`` pinned to the fixture's registry --
    the registry *file* is the seam, its search is the logic;
  * ``SshOwner.matches`` on an owner built with ``SshOwner(...)`` and a websocket
    whose ``state.ssh_terminal`` is the fixture's selected pair;
  * ``store.connection_dial_fields_changed`` over two saved records, plus the
    dial-field table itself.

No websocket, PTY or SSH master is ever opened.
"""
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_webhostter_aa608d"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from hermes_cli import web_host_terminal_ssh as live  # noqa: E402
from hermes_cli import gateway_connections_normalize as normalize  # noqa: E402

WANTED = {"profile", "entry", "dial_fields_changed", "matches", "constants"}
DIAL_FIELDS = ("url", "authMode", "org", "host", "user", "port", "keyPath",
               "remoteHermesPath", "remoteProfile")


def _obj(value):
    return value if isinstance(value, dict) else {}


def profile(req):
    ws = types.SimpleNamespace(query_params=_obj(req.get("query_params")))
    return live._profile(ws)


def entry(req):
    registry = _obj(req.get("registry"))
    live.store.read_registry = lambda _path: registry
    found = live._entry(Path("/fixture/connections.json"), req.get("connection_id"))
    return found


def dial_fields_changed(req):
    return normalize.connection_dial_fields_changed(_obj(req.get("before")), _obj(req.get("after")))


def matches(req):
    owner = _obj(req.get("owner"))
    selected = req.get("selected")
    connection = {"id": owner.get("connection_id")}
    store_path = Path(str(owner.get("store_path")))
    ssh_terminal = None
    if isinstance(selected, dict):
        ssh_terminal = ({"id": selected.get("connection_id")}, Path(str(selected.get("store_path"))))
    owner_obj = live.SshOwner(("provider", "user"), Path("/home"), "ssh", "", store_path,
                              connection, owner.get("profile"))
    ws = types.SimpleNamespace(state=types.SimpleNamespace(ssh_terminal=ssh_terminal),
                               query_params=_obj(req.get("query_params")))
    return owner_obj.matches(ws)


def constants():
    return {
        "dial_fields": list(DIAL_FIELDS),
        "default_profile": "current",
        "ssh_kind": "ssh",
    }


def expected(r):
    f = r.get("func")
    if f == "profile":
        return profile(r)
    if f == "entry":
        return entry(r)
    if f == "dial_fields_changed":
        return dial_fields_changed(r)
    if f == "matches":
        return matches(r)
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
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
