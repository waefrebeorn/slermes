#!/usr/bin/env python3
"""Differential oracle for agent/vault_backends/local.py (gate w22_local_259750).

The Fernet store is the seam: `agent.vault_store.get_vault_store` is replaced by
a FakeStore that walks the fixture's decrypted rows. It mirrors VaultStore
exactly -- the same `VaultStore._meta` for the metadata rendering and the same
`VaultError` text for a miss -- so the live backend code runs unmodified on top
of it. The wall clock is the second seam: `agent.vault_store.totp_now` is
wrapped with the fixture's `at`, so resolve_otp stays live but deterministic.

Every answer is the wrapper the C side returns:
  {"ok": True, "value": ...} / {"ok": False, "error": "..."}
"""
import importlib
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w22_local_259750"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

live = importlib.import_module("agent.vault_backends.local")
store_mod = importlib.import_module("agent.vault_store")
VaultStore = store_mod.VaultStore
VaultError = store_mod.VaultError

WANTED = {"info", "list", "meta", "password", "secret", "otp"}


class FakeStore:
    """The DB seam: the decrypted rows, no file walk and no Fernet key."""

    def __init__(self, records):
        self.records = records

    def list_items(self):
        return [VaultStore._meta(r) for r in self.records]

    def get_meta(self, handle):
        for rec in self.records:
            if rec.get("id") == handle:
                return VaultStore._meta(rec)
        return None

    def resolve_secret(self, handle):
        for rec in self.records:
            if rec.get("id") == handle:
                return dict(rec.get("secret") or {})
        raise VaultError(f"no vault item with id {handle!r}")


def wrap(call):
    try:
        return {"ok": True, "value": call()}
    except VaultError as exc:
        return {"ok": False, "error": str(exc)}


def expected(r):
    store_mod.get_vault_store = lambda: FakeStore(r.get("records") or [])
    backend = live.LocalLoginBackend()
    handle = r.get("handle")
    f = r.get("func")
    if f == "info":
        return {"ok": True, "value": {
            "name": backend.name,
            "display_name": backend.display_name,
            "prefix": backend.prefix,
            "needs_unlock": backend.needs_unlock,
        }}
    if f == "list":
        return wrap(lambda: [m.to_dict() for m in backend.list_items()])
    if f == "meta":
        return wrap(lambda: (lambda m: m.to_dict() if m is not None else None)(backend.get_meta(handle)))
    if f == "password":
        return wrap(lambda: backend.resolve_password(handle))
    if f == "secret":
        return wrap(lambda: backend.resolve_secret(handle))
    if f == "otp":
        at = r.get("at")
        original = store_mod.totp_now
        if at is None:
            store_mod.totp_now = original
        else:
            store_mod.totp_now = lambda seed: original(seed, at=float(at))
        try:
            return wrap(lambda: backend.resolve_otp(handle))
        finally:
            store_mod.totp_now = original
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
