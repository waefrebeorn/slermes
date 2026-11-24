#!/usr/bin/env python3
"""Differential oracle for hermes_cli/dashboard_auth/peer_pass.py
(gate w25_peerpass_8acf90).

The live module is imported from the Python quarry.  Its three value decisions
are compared, with the seams fixed by the request:

  * ``is_peer_path`` on a path string;
  * ``peer_access_contract`` with ``list_providers`` replaced by the request's
    provider list (only ``supports_peer_pass`` is consulted);
  * ``_verify`` driven with real provider objects whose ``verify_peer_pass``
    answers from the request's ``verify`` verdict ("accept" -> a principal,
    "reject" -> None, "unreachable" -> ProviderError), run under ``asyncio.run``
    exactly as the middleware awaits it.

``PEER_PREFIX`` / ``PEER_ACCESS_CONTRACT`` are read off the module.
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
GATE = "w25_peerpass_8acf90"

if not os.environ.get("USERPROFILE") and not os.environ.get("HOMEPATH"):
    os.environ["USERPROFILE"] = str(REPO.parent)

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from hermes_cli.dashboard_auth import peer_pass as live  # noqa: E402

WANTED = {"is_peer_path", "peer_access_contract", "verify", "constants"}

REAL_LIST_PROVIDERS = live.list_providers


class FakeProvider:
    """A registered provider whose peer-pass verify outcome is fixed."""

    def __init__(self, spec):
        self.name = spec.get("name")
        self.supports_peer_pass = spec.get("supports_peer_pass", False)
        self._verdict = spec.get("verify", "reject")

    def verify_peer_pass(self, token=None):
        if self._verdict == "unreachable":
            raise live.ProviderError("provider down")
        if self._verdict == "accept":
            return SimpleNamespace(name=self.name, token=token)
        return None


def _providers(req):
    raw = req.get("providers")
    return [FakeProvider(p) for p in raw] if isinstance(raw, list) else raw


def is_peer_path(req):
    return live.is_peer_path(req.get("path"))


def peer_access_contract(req):
    providers = _providers(req)
    live.list_providers = lambda: providers
    try:
        return live.peer_access_contract()
    finally:
        live.list_providers = REAL_LIST_PROVIDERS


def verify(req):
    providers = _providers(req)
    principal, unreachable = asyncio.run(live._verify(providers, req.get("token")))
    return {"accepted": None if principal is None else principal.name,
            "unreachable": unreachable}


def constants(_req):
    return {"peer_prefix": live.PEER_PREFIX,
            "peer_access_contract": live.PEER_ACCESS_CONTRACT}


def expected(r):
    f = r.get("func")
    if f == "is_peer_path":
        return is_peer_path(r)
    if f == "peer_access_contract":
        return peer_access_contract(r)
    if f == "verify":
        return verify(r)
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
            py = {"__error__": type(exc).__name__}
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
