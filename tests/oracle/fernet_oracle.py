#!/usr/bin/env python3
"""Differential oracle for the ported Fernet implementation (src/agent/fernet.c).

Three independent checks, because "my encryption round-trips through my decryption" proves nothing
about whether a vault written by Python can be read:

  1. spec vectors  -- fernet/spec generate.json gives key + IV + timestamp + plaintext + the exact
                      token; the C output must equal that token byte for byte.
  2. spec refusals -- fernet/spec invalid.json gives 8 tokens that must be rejected (bad MAC, short,
                      bad base64, bad block size, bad padding, clock skew, expired TTL, bad IV).
  3. interop       -- cryptography.Fernet must decrypt what C encrypted, and C must decrypt what
                      cryptography.Fernet encrypted. That is the direction that decides whether the
                      real vault file is readable.

Needs cryptography, which is not importable from the quarry venv on this box; set FERNET_PY to an
interpreter that has it (or PYTHONPATH to a --target install). Without it the oracle exits 3 and says
SKIPPED rather than pretending to pass.

Usage: python tests/oracle/fernet_oracle.py [--keep]
"""
from __future__ import annotations

import argparse
import base64
import datetime as dt
import json
import os
import secrets
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
FIXTURES = REPO / "tests" / "fixtures" / "fernet"

try:
    from cryptography.fernet import Fernet, InvalidToken
except Exception as exc:                                                     # noqa: BLE001
    print(f"SKIPPED: cryptography is not importable ({type(exc).__name__}: {exc})")
    print("         set FERNET_PY to an interpreter that has it, or PYTHONPATH to a --target install")
    raise SystemExit(3)


def ts_of(value: str) -> int:
    return int(dt.datetime.fromisoformat(value).timestamp())


def run_c(cases: list[dict]) -> dict:
    tin = REPO / ".testbin" / "fernet_cases.json"
    tout = REPO / ".testbin" / "fernet_out.json"
    tin.parent.mkdir(exist_ok=True)
    tin.write_text(json.dumps(cases), encoding="utf-8")

    exe = REPO / ".testbin" / "fernet_diff.exe"
    if not exe.exists():
        build = subprocess.run(
            ["gcc", "-O1", "-w", "-I", "include", "-I", "lib", "-I", "lib/libjson",
             "-I", "lib/libcrypto", "-I", "lib/libbase64",
             "src/agent/fernet.c", "tests/unit/fernet_diff.c",
             "lib/libjson/json.o", "lib/libcrypto/crypto.o", "lib/libbase64/base64.o",
             "-lcrypto", "-o", str(exe)],
            cwd=str(REPO), capture_output=True, text=True)
        if build.returncode != 0:
            print("cannot build the harness:")
            print(build.stderr[-2500:])
            raise SystemExit(2)

    proc = subprocess.run([str(exe), str(tin), str(tout)], capture_output=True, text=True)
    if proc.returncode != 0:
        print("harness failed:", proc.returncode, proc.stderr[-800:])
        raise SystemExit(2)
    return {c["name"]: c for c in json.loads(tout.read_text(encoding="utf-8"))}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--keep", action="store_true")
    args = ap.parse_args()

    generate = json.loads((FIXTURES / "generate.json").read_text(encoding="utf-8"))
    invalid = json.loads((FIXTURES / "invalid.json").read_text(encoding="utf-8"))

    cases: list[dict] = []
    # 1. spec vectors
    for i, vec in enumerate(generate):
        cases.append({"name": f"spec-generate[{i}]", "op": "enc", "secret": vec["secret"],
                      "plain_b64": base64.b64encode(vec["src"].encode()).decode(),
                      "iv": vec["iv"], "ts": ts_of(vec["now"])})
    # 2. spec refusals
    for i, vec in enumerate(invalid):
        cases.append({"name": f"spec-invalid[{i}]", "op": "dec", "secret": vec["secret"],
                      "token": vec["token"], "now": ts_of(vec["now"]),
                      "ttl": vec.get("ttl_sec", -1)})
    # 3. interop, both directions
    key = Fernet.generate_key().decode()
    ctos = [b"", b"x", b"hello", b"a longer payload " * 20, bytes(range(256))]
    for i, plain in enumerate(ctos):
        iv = list(secrets.token_bytes(16))
        cases.append({"name": f"c-enc[{i}]", "op": "enc", "secret": key,
                      "plain_b64": base64.b64encode(plain).decode(), "iv": iv,
                      "ts": int(dt.datetime.now(dt.timezone.utc).timestamp())})
    pto_tokens = {}
    for i, plain in enumerate(ctos):
        token = Fernet(key.encode()).encrypt(plain).decode()
        pto_tokens[f"py-enc[{i}]"] = (token, plain)
        cases.append({"name": f"py-enc[{i}]", "op": "dec", "secret": key,
                      "token": token, "now": int(dt.datetime.now(dt.timezone.utc).timestamp()),
                      "ttl": 60})

    got = run_c(cases)
    agree = failed = 0

    # 1
    for i, vec in enumerate(generate):
        name = f"spec-generate[{i}]"
        have = got.get(name, {})
        if have.get("token") == vec["token"]:
            agree += 1
        else:
            failed += 1
            print(f"FAIL {name}")
            print(f"   want: {vec['token']}")
            print(f"   got:  {have.get('token') or have.get('error')}")

    # 2
    for i, vec in enumerate(invalid):
        name = f"spec-invalid[{i}]"
        have = got.get(name, {})
        if "error" in have:
            agree += 1
        else:
            failed += 1
            print(f"FAIL {name} ({vec['desc']}): C accepted a token the spec says is invalid"
                  f" -> {have.get('plain_b64')!r}")

    # 3a: cryptography decrypts C tokens
    for i, plain in enumerate(ctos):
        name = f"c-enc[{i}]"
        have = got.get(name, {})
        token = have.get("token")
        if not token:
            failed += 1
            print(f"FAIL {name}: C refused to encrypt ({have.get('error')})")
            continue
        try:
            back = Fernet(key.encode()).decrypt(token.encode())
            if back == plain:
                agree += 1
            else:
                failed += 1
                print(f"FAIL {name}: cryptography decrypted to {back!r}, expected {want!r}")
        except InvalidToken:
            failed += 1
            print(f"FAIL {name}: cryptography rejected the C token as invalid")

    # 3b: C decrypts cryptography tokens
    for name, (token, plain) in pto_tokens.items():
        have = got.get(name, {})
        want = base64.b64encode(plain).decode()
        if have.get("plain_b64") == want:
            agree += 1
        else:
            failed += 1
            print(f"FAIL {name}: C could not read a cryptography token")
            print(f"   want b64: {want}")
            print(f"   got  b64: {have.get('plain_b64')!r} error={have.get('error')!r}")

    print(f"fernet oracle: {agree} agree, {failed} problem(s)")
    if not args.keep:
        for p in (REPO / ".testbin" / "fernet_cases.json", REPO / ".testbin" / "fernet_out.json"):
            p.unlink(missing_ok=True)
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
