#!/usr/bin/env python3
"""Differential oracle for the local login backend wired to real vault files.

src/agent/vault_backends/local.c already had the backend helpers over an in-memory records array, and
vault_file.c now reads the encrypted store -- but nothing joined them, so `local` could not answer a
question about an actual vault. src/agent/vault_local_store.c is that join; this compares it with
agent.vault_backends.local.LocalLoginBackend over the same on-disk vault:

  list      every login item's metadata
  meta      a point lookup, and a miss
  password  the resolved password
  secret    the whole decrypted payload (this is the one that must never leak into a listing)
  otp       the minted TOTP code at a fixed epoch, so "now" cannot make it flaky
  info      the backend descriptor

Needs cryptography (see the fernet oracle note); exits 3 and says SKIPPED when it is unavailable.

Usage: python tests/oracle/vault_local_backend_oracle.py [--keep]
"""
from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

QUARRY = Path(os.environ.get("HERMES_QUARRY", r"C:/Users/eman5/hermes-agent"))
REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(QUARRY))

if not (os.environ.get("USERPROFILE") or os.environ.get("HOME")):
    print("SKIPPED: neither USERPROFILE nor HOME is set, so the quarry cannot resolve its home dir")
    raise SystemExit(3)

try:
    import cryptography  # noqa: F401
except Exception as exc:                                                    # noqa: BLE001
    print(f"SKIPPED: cryptography is not importable ({type(exc).__name__}: {exc})")
    raise SystemExit(3)

import agent.vault_store as vault_store                   # noqa: E402
from agent.vault_backends.local import LocalLoginBackend  # noqa: E402

VaultStore = vault_store.VaultStore


def build_fixture(base: Path) -> list[str]:
    store = VaultStore(base)
    store.add_item("login", "With OTP",
                   {"identifier_type": "email", "identifier": "ada@example.com",
                    "password": "hunter2", "otp_secret": "JBSWY3DPEHPK3PXP"},
                   origin="https://mail.example.com")
    store.add_item("login", "Plain",
                   {"identifier_type": "username", "identifier": "ada", "password": "pw2"},
                   origin="https://example.com/login")
    store.add_item("payment", "Visa", {"card_number": "4111111111111111", "cvc": "123",
                                       "exp_month": "12", "exp_year": "2030"})
    return [m.id for m in store.list_items()]


def value_of(payload):
    """The backend helpers answer {"ok": true, "value": ...} or {"ok": false, "error": ...}, which is
    upstream's wrapper; unwrap it so a comparison names the value, not the envelope."""
    if isinstance(payload, dict) and "ok" in payload:
        return payload.get("value") if payload.get("ok") else {"error": payload.get("error")}
    return payload


def run_c(exe: Path, *args: str) -> dict:
    proc = subprocess.run([str(exe), *args], capture_output=True, text=True)
    if proc.returncode != 0:
        raise SystemExit(f"C harness failed ({proc.returncode}): {proc.stderr[-500:]}")
    return json.loads(proc.stdout)


def build_exe() -> Path:
    exe = REPO / ".testbin" / "vault_local_diff.exe"
    if exe.exists():
        return exe
    build = subprocess.run(
        ["gcc", "-O1", "-w", "-I", "include", "-I", "lib", "-I", "lib/libjson",
         "-I", "lib/libcrypto", "-I", "lib/libbase64",
         "src/agent/vault_local_store.c", "src/agent/vault_file.c", "src/agent/fernet.c",
         "src/agent/vault_backends/local.c", "tests/unit/vault_local_diff.c",
         "lib/libjson/json.o", "lib/libcrypto/crypto.o", "-lcrypto", "-o", str(exe)],
        cwd=str(REPO), capture_output=True, text=True)
    if build.returncode != 0:
        print("cannot build the harness:")
        print(build.stderr[-2500:])
        raise SystemExit(2)
    return exe


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--keep", action="store_true")
    args = ap.parse_args()

    work = REPO / ".testbin" / "vault_local_fixture"
    shutil.rmtree(work, ignore_errors=True)
    work.mkdir(parents=True)
    ids = build_fixture(work)
    exe = build_exe()

    agree = failed = 0
    def check(name: str, want, have) -> None:
        nonlocal agree, failed
        if want == have:
            agree += 1
        else:
            failed += 1
            print(f"FAIL {name}")
            print(f"   want: {want!r}")
            print(f"   got:  {have!r}")

    # Point the backend at the fixture. agent/vault_backends/local.py resolves its store through
    # agent.vault_store.get_vault_store precisely so a test can patch that one name, which is what this
    # does -- otherwise the oracle would compare against whatever vault the real profile happens to hold.
    backup = vault_store.get_vault_store
    vault_store.get_vault_store = lambda: VaultStore(work)
    backend = LocalLoginBackend()

    # 1. info
    info = run_c(exe, "info")
    check("info", {"name": "local", "display_name": "Hermes vault",
                   "prefix": "vault_", "needs_unlock": False}, info)

    # 2. listing
    want_list = [m.to_dict() for m in backend.list_items()]
    got_list = value_of(run_c(exe, "list", str(work)))
    check("list", want_list, got_list)
    blob = json.dumps(got_list)
    check("no-secret-in-list", [], [s for s in ("hunter2", "pw2", "JBSWY3DPEHPK3PXP",
                                                "4111111111111111") if s in blob])

    # 3. point lookups + miss
    for item_id in ids:
        want = backend.get_meta(item_id)
        check(f"meta[{item_id}]", want.to_dict() if want else None,
              value_of(run_c(exe, "meta", str(work), item_id)))
    check("meta-missing", None, value_of(run_c(exe, "meta", str(work), "v_nope")))

    # 4. password + secret. A payment item has no password half: upstream's local backend answers ""
    #    for it because resolve_secret(...).get("password") or "" -- so "" is the expected value, not
    #    an error.
    for item_id, meta in zip(ids, backend.list_items()):
        check(f"password[{item_id}]", backend.resolve_password(item_id),
              value_of(run_c(exe, "password", str(work), item_id)))
        check(f"secret[{item_id}]",
              {k: str(v) for k, v in backend.resolve_secret(item_id).items()},
              value_of(run_c(exe, "secret", str(work), item_id)))

    # 5. TOTP at a fixed epoch: reproducible, and equal to upstream's own totp_now
    from agent.vault_store import totp_now
    epoch = "1700000000"
    for item_id in ids:
        secret = VaultStore(work).resolve_secret(item_id)
        seed = str(secret.get("otp_secret") or "")
        want = totp_now(seed, at=float(epoch)) if seed else None
        check(f"otp[{item_id}]", want, value_of(run_c(exe, "otp", str(work), item_id, epoch)))

    vault_store.get_vault_store = backup
    print(f"vault-local-backend oracle: {agree} agree, {failed} problem(s)")
    if not args.keep:
        shutil.rmtree(work, ignore_errors=True)
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
