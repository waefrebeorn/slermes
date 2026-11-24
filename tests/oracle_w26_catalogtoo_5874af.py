#!/usr/bin/env python3
"""Differential oracle for tools/connectors/catalog_tool.py (gate w26_catalogtoo_5874af).

``_check_args`` is called live.  ``manage_catalog`` is called live too, with only
its two seams replaced: ``search`` and ``install`` are stubbed at the module
level (the catalog resolver / hub / approval card / installer), so the real
``manage_catalog`` body decides error vs not-here vs search vs install and
returns the real ``tool_error`` bodies.
"""
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_catalogtoo_5874af"

os.environ["USERPROFILE"] = str(REPO.parent)
os.environ["HOME"] = str(REPO.parent)

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from tools.connectors import catalog_tool as live  # noqa: E402

WANTED = {"check_args", "dispatch", "constants"}


def stub_search(query, kind, *, installer=None):
    return {"route": "search"}


def stub_install(items, *, session_id, tool_call_id, connection_callback, installer=None):
    return json.dumps({"route": "install"})


live.search = stub_search
live.install = stub_install


def dispatch(req):
    args = req.get("args")
    card_surface = bool(req.get("card_surface"))
    callback = (lambda row: None) if req.get("have_callback") else None
    out = live.manage_catalog(args, session_id="oracle-session", tool_call_id="oracle-call",
                              connection_callback=callback, card_surface=card_surface)
    payload = json.loads(out)
    if "route" in payload:
        return payload
    route = "error" if live._check_args(args) else "not_here"
    return {"route": route, "error": payload.get("error")}


def expected(r):
    f = r.get("func")
    if f == "check_args":
        return live._check_args(r.get("args"))
    if f == "dispatch":
        return dispatch(r)
    if f == "constants":
        return {
            "actions": list(live.ACTIONS),
            "item_kinds": list(live.ITEM_KINDS),
            "search_limit": live.SEARCH_LIMIT,
            "top_keys": sorted(live._TOP_KEYS),
            "item_keys": sorted(live._ITEM_KEYS),
        }
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
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
