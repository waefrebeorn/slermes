#!/usr/bin/env python3
"""Interop oracle for the ported vault document layer (src/agent/vault_file.c).

The claim being tested is not "C can read its own vault" -- it is "C reads the vault Python writes, and
Python reads the vault C writes". So the fixture is built by the real VaultStore:

  * a login item with an identifier and a password
  * a login item with an OTP seed (has_otp must be true, and the seed must never appear in the listing)
  * a login item with no OTP seed (has_otp must be absent, not false)
  * a payment and an address item (fully secret payloads)
  * an empty-string OTP seed (falsy in Python, so has_otp must be absent)

Then: compare the metadata listing, a point lookup, secret resolution, the three corruption messages,
and a C-written vault read back by VaultStore.

Needs cryptography (see the fernet oracle note); exits 3 and says SKIPPED when it is unavailable.

Usage: python tests/oracle/vault_file_oracle.py [--keep]
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

# The quarry's hermes_constants resolves the home directory from the environment and raises a bare
# RuntimeError when neither variable is set (which is how a stripped MSYS shell looks). Say so instead.
if not (os.environ.get("USERPROFILE") or os.environ.get("HOME")):
    print("SKIPPED: neither USERPROFILE nor HOME is set, so the quarry cannot resolve its home dir")
    raise SystemExit(3)

try:
    import cryptography  # noqa: F401
except Exception as exc:                                                    # noqa: BLE001
    print(f"SKIPPED: cryptography is not importable ({type(exc).__name__}: {exc})")
    raise SystemExit(3)

from agent.vault_store import VaultError, VaultStore  # noqa: E402


def build_fixture(base: Path) -> VaultStore:
    store = VaultStore(base)
    store.add_item("login", "Work email",
                   {"identifier_type": "email", "identifier": "ada@example.com",
                    "password": "hunter2", "otp_secret": "JBSWY3DPEHPK3PXP"},
                   origin="https://mail.example.com")
    # a login with no OTP: has_otp must be absent, not false
    store.add_item("login", "Personal",
                   {"identifier_type": "username", "identifier": "ada", "password": "pw2"},
                   origin="https://example.com/login")
    store.add_item("login", "Empty otp",
                   {"identifier_type": "username", "identifier": "ada",
                    "password": "pw", "otp_secret": ""},
                   origin="https://example.org")
    store.add_item("payment", "Visa", {"card_number": "4111111111111111", "cvc": "123",
                                       "exp_month": "12", "exp_year": "2030"})
    store.add_item("address", "Home", {"address_line1": "1 Main St", "city": "Springfield",
                                       "postal_code": "12345", "country": "US"})
    return store


def run_c(exe: Path, *args: str) -> dict:
    proc = subprocess.run([str(exe), *args], capture_output=True, text=True)
    if proc.returncode != 0:
        raise SystemExit(f"C harness failed ({proc.returncode}): {proc.stderr[-500:]}")
    return json.loads(proc.stdout)


def build_exe() -> Path:
    exe = REPO / ".testbin" / "vault_file_diff.exe"
    if exe.exists():
        return exe
    build = subprocess.run(
        ["gcc", "-O1", "-w", "-I", "include", "-I", "lib", "-I", "lib/libjson",
         "-I", "lib/libcrypto", "-I", "lib/libbase64",
         "src/agent/vault_file.c", "src/agent/fernet.c", "tests/unit/vault_file_diff.c",
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

    work = REPO / ".testbin" / "vault_file_fixture"
    shutil.rmtree(work, ignore_errors=True)
    work.mkdir(parents=True)
    store = build_fixture(work)
    exe = build_exe()

    agree = failed = 0
    def check(name: str, want, have, extra: str = "") -> None:
        nonlocal agree, failed
        if want == have:
            agree += 1
        else:
            failed += 1
            print(f"FAIL {name}{extra}")
            print(f"   want: {want!r}")
            print(f"   got:  {have!r}")

    # 1. metadata listing
    want_meta = [m.to_dict() for m in store.list_items()]
    got = run_c(exe, "list", str(work))
    check("list", want_meta, got.get("items"))
    check("list-count", len(want_meta), len(got.get("items") or []))

    # the listing must not carry a secret value under any key
    blob = json.dumps(got.get("items"))
    leaked = [s for s in ("hunter2", "JBSWY3DPEHPK3PXP", "4111111111111111", "1 Main St") if s in blob]
    check("no-secret-in-listing", [], leaked)

    # 2. point lookups, including a miss
    for meta in store.list_items():
        got_one = run_c(exe, "find", str(work), meta.id)
        check(f"find[{meta.label}]", meta.to_dict(), got_one.get("found"))
    check("find-missing", None, run_c(exe, "find", str(work), "v_missing").get("found"))

    # 3. secret resolution
    for meta in store.list_items():
        want_secret = store.resolve_secret(meta.id)
        got_secret = run_c(exe, "secret", str(work), meta.id).get("secret")
        check(f"secret[{meta.label}]", want_secret, got_secret)
    missing = run_c(exe, "secret", str(work), "v_missing").get("error", "")
    check("secret-missing-message", True, "no vault item with id" in missing, f" ({missing!r})")

    # 4. corruption messages, compared with upstream's wording
    keyfile = REPO / ".testbin" / "vault_key_fixture"
    keyfile.mkdir(exist_ok=True)
    shutil.copy(work / "vault.key", keyfile / "key.txt")
    errs = run_c(exe, "errors", str(keyfile))
    check("errors.bad_token", "vault file could not be decrypted (key mismatch or corruption)",
          errs.get("bad_token"))
    check("errors.bad_json", "vault file is corrupted (invalid JSON)", errs.get("bad_json"))
    check("errors.bad_shape", "vault file is corrupted (unexpected shape)", errs.get("bad_shape"))

    # 5. the direction that matters for writing: C rewrites, Python still reads it
    rewritten = REPO / ".testbin" / "vault_rewritten.enc"
    ok = run_c(exe, "reserialize", str(work), str(rewritten)).get("ok")
    check("reserialize", True, bool(ok))
    if ok:
        shutil.copy(rewritten, work / "vault.json.enc")
        try:
            reread = [m.to_dict() for m in store.list_items()]
            check("python-reads-c-written-vault", want_meta, reread)
        except VaultError as exc:
            failed += 1
            print(f"FAIL python-reads-c-written-vault: {exc}")

    print(f"vault-file oracle: {agree} agree, {failed} problem(s)")
    if not args.keep:
        shutil.rmtree(work, ignore_errors=True)
        shutil.rmtree(keyfile, ignore_errors=True)
        rewritten.unlink(missing_ok=True)
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
