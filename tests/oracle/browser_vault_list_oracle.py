#!/usr/bin/env python3
"""Differential oracle for `browser_vault action=list` (src/tools/browser_vault_list.c).

Two configurations, because the listing has two shapes:

  populated  a vault with a login (with OTP), a login without one, and a payment with an origin --
             the payment must be "available" only because it has an origin, the OTP login must carry
             two_factor, and no handle may carry a secret
  empty      a directory with no vault at all: upstream emits the save-login hint, and that hint is
             the thing that stops a model from typing a password it saw on the page

Compared against tools.browser_vault_tool.browser_vault_list with the store pointed at the fixture.
The JSON is compared as parsed objects (key sets and values); the raw strings are compared too and
reported separately, because a key-order difference is worth knowing about but is not a behaviour
difference.

Usage: python tests/oracle/browser_vault_list_oracle.py [--keep]
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

import agent.vault_store as vault_store                     # noqa: E402
from tools.browser_vault_tool import browser_vault_list     # noqa: E402

VaultStore = vault_store.VaultStore


def build_exe() -> Path:
    exe = REPO / ".testbin" / "browser_vault_list_diff.exe"
    if exe.exists():
        return exe
    build = subprocess.run(
        ["gcc", "-O1", "-w", "-I", "include", "-I", "lib", "-I", "lib/libjson",
         "-I", "lib/libcrypto", "-I", "lib/libbase64",
         "src/tools/browser_vault_list.c", "src/agent/vault_local_store.c",
         "src/agent/vault_file.c", "src/agent/fernet.c",
         "src/agent/vault_backends/local.c", "tests/unit/browser_vault_list_diff.c",
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

    exe = build_exe()
    agree = failed = 0
    order_notes: list[str] = []

    def check(name: str, want, have) -> None:
        nonlocal agree, failed
        if want == have:
            agree += 1
        else:
            failed += 1
            print(f"FAIL {name}")
            print(f"   want: {json.dumps(want, ensure_ascii=False)[:600]!r}")
            print(f"   got:  {json.dumps(have, ensure_ascii=False)[:600]!r}")

    # ── populated ────────────────────────────────────────────────────────────
    populated = REPO / ".testbin" / "bvl_populated"
    shutil.rmtree(populated, ignore_errors=True)
    populated.mkdir(parents=True)
    store = VaultStore(populated)
    store.add_item("login", "With OTP",
                   {"identifier_type": "email", "identifier": "ada@example.com",
                    "password": "hunter2", "otp_secret": "JBSWY3DPEHPK3PXP"},
                   origin="https://mail.example.com")
    store.add_item("login", "Plain", {"identifier_type": "username", "identifier": "ada",
                                      "password": "pw2"}, origin="https://example.com")
    store.add_item("payment", "Visa", {"card_number": "4111111111111111", "cvc": "123",
                                       "exp_month": "12", "exp_year": "2030"},
                   origin="https://shop.example.com")

    backup = vault_store.get_vault_store
    vault_store.get_vault_store = lambda: VaultStore(populated)
    want_text = browser_vault_list()
    vault_store.get_vault_store = backup

    have_text = subprocess.run([str(exe), str(populated), "1"],
                               capture_output=True, text=True).stdout
    want = json.loads(want_text)
    have = json.loads(have_text)

    check("populated.items", want.get("items"), have.get("items"))
    check("populated.success", want.get("success"), have.get("success"))
    check("populated.no-hint", "hint" in want, "hint" in have)
    blob = json.dumps(have, ensure_ascii=False)
    check("populated.no-secret", [],
          [s for s in ("hunter2", "pw2", "JBSWY3DPEHPK3PXP", "4111111111111111") if s in blob])

    # specific behaviours worth naming, so a failure says which rule broke
    by_handle = {e.get("handle"): e for e in have.get("items") or []}
    want_by_handle = {e.get("handle"): e for e in want.get("items") or []}
    for handle, e in want_by_handle.items():
        check(f"entry[{e.get('label')}]", e, by_handle.get(handle))

    if want_text != have_text:
        order_notes.append("raw text differs (parsed JSON matched)")

    # ── empty ────────────────────────────────────────────────────────────────
    empty = REPO / ".testbin" / "bvl_empty"
    shutil.rmtree(empty, ignore_errors=True)
    empty.mkdir(parents=True)
    vault_store.get_vault_store = lambda: VaultStore(empty)
    want_empty_text = browser_vault_list()
    vault_store.get_vault_store = backup
    have_empty_text = subprocess.run([str(exe), str(empty), "1"],
                                     capture_output=True, text=True).stdout
    check("empty", json.loads(want_empty_text), json.loads(have_empty_text))
    if json.loads(want_empty_text) != json.loads(have_empty_text):
        print("   (the hint text is the whole point of this case)")

    print(f"browser-vault-list oracle: {agree} agree, {failed} problem(s)")
    for note in order_notes:
        print(f"  note: {note}")
    if not args.keep:
        shutil.rmtree(populated, ignore_errors=True)
        shutil.rmtree(empty, ignore_errors=True)
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
