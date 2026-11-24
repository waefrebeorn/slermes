#!/usr/bin/env python3
"""Differential oracle for gateway/session_native_recovery_routes.py
(gate w24_sessionnat_5f7f78).

The live module is imported and driven through its real code paths, with only
the I/O seams replaced by values from the case:

  * ``_retry_recovery`` is actually awaited through ``asyncio.run``, with
    ``asyncio.sleep`` shrinking to a recorder and ``_try_recover`` answering the
    case's attempt results -- so the loop's stop/give-up behaviour and the delays
    it slept are the live ones;
  * ``recovered_native_source`` is called with ``authority_persistence`` answering
    a row (or None) and ``restore_native_source`` either returning a source or
    raising ``AuthorityRefusal``;
  * the relay predicate is evaluated exactly as ``verified_restored_relay_source``
    evaluates it over one admitted envelope.

A live exception is the seam's error answer: the C port returns null there, so
the expected value is None.
"""
import asyncio
import json
import os
import sys
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_sessionnat_5f7f78"

_HOME = REPO.parent
for _key, _val in (("USERPROFILE", str(_HOME)),
                   ("LOCALAPPDATA", str(_HOME / "AppData" / "Local")),
                   ("APPDATA", str(_HOME / "AppData" / "Roaming"))):
    if not os.environ.get(_key):
        os.environ[_key] = _val

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

import gateway.session_native_recovery_routes as live  # noqa: E402

WANTED = {"relay_match", "retry_plan", "recovered_source", "constants"}


def relay_match(r):
    envelope = r.get("envelope")
    key = r.get("session_key")
    # verified_restored_relay_source, one envelope at a time
    return envelope.get("route") == key and "relay" in envelope.get("provenance", {})


def retry_plan(r):
    running = r.get("running")
    raw = r.get("attempts")
    attempts = [bool(a) for a in (raw if isinstance(raw, list) else [])]
    slept = []
    calls = {"n": 0}
    orig_sleep = asyncio.sleep
    orig_try = live._try_recover

    async def fake_sleep(delay, *a, **k):
        slept.append(delay)

    async def fake_try(runner, authority):
        i = calls["n"]
        calls["n"] += 1
        return attempts[i] if i < len(attempts) else False

    runner = SimpleNamespace()
    if running is not None:
        runner._running = bool(running)
    asyncio.sleep = fake_sleep
    live._try_recover = fake_try
    try:
        asyncio.run(live._retry_recovery(runner, object()))
    finally:
        asyncio.sleep = orig_sleep
        live._try_recover = orig_try
    return {"attempts": calls["n"],
            "delays": list(slept),
            "gave_up": calls["n"] == len(live._RECOVERY_RETRY_DELAYS)}


def recovered_source(r):
    payload = r.get("payload")
    refused = bool(r.get("refused"))
    fallback = r.get("fallback")
    orig_ap = live.authority_persistence
    orig_restore = live.restore_native_source

    def fake_ap(authority):
        admissions = SimpleNamespace(
            latest_admission=lambda **kw: (SimpleNamespace(payload=payload) if payload is not None else None))
        return SimpleNamespace(admissions=admissions)

    def fake_restore(p, runner):
        if refused:
            raise live.AuthorityRefusal("stale_generation")
        return {"from": "verified", "payload": p}

    runner = SimpleNamespace(_restored_source=lambda entry: {"from": "fallback", "tag": fallback})
    live.authority_persistence = fake_ap
    live.restore_native_source = fake_restore
    try:
        return live.recovered_native_source(runner, object(), None, "owner")
    finally:
        live.authority_persistence = orig_ap
        live.restore_native_source = orig_restore


def constants():
    return {"recovery_retry_delays": [float(d) for d in live._RECOVERY_RETRY_DELAYS]}


def expected(r):
    f = r.get("func")
    if f == "relay_match":
        return relay_match(r)
    if f == "retry_plan":
        return retry_plan(r)
    if f == "recovered_source":
        return recovered_source(r)
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
        except Exception:  # noqa: BLE001 - the live AttributeError/TypeError seam
            py = None
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
