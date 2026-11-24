#!/usr/bin/env python3
"""Differential oracle for hermes_cli/webapp_relay_target.py (gate w24_webapprela_ffd6bc).

The live module is imported from the quarry; its two seams are replaced with
fixture-driven fakes: ``store.read_registry`` answers the saved-connection
registry and ``reach.resolve_target`` answers the blocking resolver.  What is
compared is the relay's own decision -- the Target bearer, what a connection id
names, resolve() -> None / Target / Unreachable with the exact text, the
own-ticket socket table and the constants.

Nothing dials, blocks or writes: the pool, the lock and the ticket POST are
never reached because only ``_resolve``/``entry_url`` (value layers) are called.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_webapprela_ffd6bc"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from hermes_cli import webapp_relay_target as live  # noqa: E402

WANTED = {"bearer", "entry_url", "resolve", "own_ticket", "constants"}


def bearer(r):
    headers = r.get("headers") or {}
    return live.Target(connection_id="c", label="l", base_url="b", headers=headers).bearer


def registry_for(entry):
    return {"connections": [entry] if entry is not None else []}


def entry_url(r):
    entry = r.get("entry")
    orig = live.store.read_registry
    live.store.read_registry = lambda: registry_for(entry)
    try:
        cid = entry.get("id") if isinstance(entry, dict) else "missing"
        return live.entry_url(cid)
    finally:
        live.store.read_registry = orig


def resolve(r):
    entry = r.get("entry")
    spec = r.get("reach")
    if not isinstance(spec, dict):
        spec = {"kind": "ok"}
    orig_reg = live.store.read_registry
    orig_reach = live.reach.resolve_target

    def fake_resolve(connection_id, rejected_token=None):
        kind = spec.get("kind", "ok")
        if kind == "nosuch":
            raise live.store.NoSuchConnection(spec.get("message") or "gone")
        if kind == "error":
            raise live.oauth.NotSignedIn(spec.get("message") or "boom")
        return spec.get("base_url") or "", dict(spec.get("headers") or {})

    live.store.read_registry = lambda: registry_for(entry)
    live.reach.resolve_target = fake_resolve
    try:
        target = live._resolve(r.get("connection_id"), "")
    except live.Unreachable as exc:
        return {"kind": "error", "message": str(exc)}
    finally:
        live.store.read_registry = orig_reg
        live.reach.resolve_target = orig_reach
    if target is None:
        return {"kind": "none"}
    return {"kind": "target", "target": {
        "connection_id": target.connection_id,
        "label": target.label,
        "base_url": target.base_url,
        "headers": target.headers,
    }}


def own_ticket(r):
    return live._OWN_TICKET_PATHS.get(r.get("path"))


def constants(r):
    return {
        "own_ticket_paths": dict(live._OWN_TICKET_PATHS),
        "session_token_header": live.reach.SESSION_TOKEN_HEADER,
    }


def expected(r):
    f = r.get("func")
    if f == "bearer":
        return bearer(r)
    if f == "entry_url":
        return entry_url(r)
    if f == "resolve":
        return resolve(r)
    if f == "own_ticket":
        return own_ticket(r)
    if f == "constants":
        return constants(r)
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
