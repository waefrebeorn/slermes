#!/usr/bin/env python3
"""Differential oracle for the ported vault_store helpers (src/agent/vault_store.c).

That file carries the vault's pure pieces -- the kind and identifier-type constants, origin
normalisation, OTP-secret normalisation and secret scrubbing -- and nothing had compared them with
upstream. The store itself (Fernet file, list_items/get_meta/resolve_secret) is not ported, so this
verifies exactly what exists and claims nothing about the rest.

Usage: python tests/oracle/vault_store_oracle.py [--keep]
"""
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path

QUARRY = Path(os.environ.get("HERMES_QUARRY", r"C:/Users/eman5/hermes-agent"))
REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(QUARRY))

from agent import vault_store as vs  # noqa: E402

ORIGINS = [
    "https://Example.COM/login?next=/x",
    "http://example.com:8080/a/b",
    "https://sub.example.co.uk/",
    "example.com",
    "www.example.com/path",
    "https://user:pw@example.com/x",
    "  https://example.com/  ",
    "",
    "not a url",
    "https://example.com",
    "HTTP://EXAMPLE.COM/A",
]

OTPS = [
    "JBSWY3DPEHPK3PXP",
    "jbswy3dpehpk3pxp",
    "JBSW Y3DP EHPK 3PXP",
    "jbsw-y3dp-ehpk-3pxp",
    "otpauth://totp/Example:alice@example.com?secret=JBSWY3DPEHPK3PXP&issuer=Example",
    "",
    "   ",
]

SCRUB_TEXT = [
    ("the password is hunter2 and that is all", {"password": "hunter2"}),
    ("card 4111111111111111 exp 12/30", {"card_number": "4111111111111111"}),
    ("nothing secret here", {"password": "hunter2"}),
    ("two: hunter2 and 4111111111111111", {"password": "hunter2", "card_number": "4111111111111111"}),
    ("hunter2", {"password": ""}),
]


def cases() -> list[dict]:
    out = [
        {"name": "kinds", "op": "kinds"},
        {"name": "idtypes", "op": "idtypes"},
    ]
    for i, v in enumerate(ORIGINS):
        out.append({"name": f"origin[{i}]", "op": "origin", "value": v})
    for kind in ("login", "payment", "address"):
        out.append({"name": f"fields[{kind}]", "op": "fields", "value": kind})
        out.append({"name": f"required[{kind}]", "op": "required", "value": kind})
    for i, v in enumerate(OTPS):
        out.append({"name": f"otp[{i}]", "op": "otp", "value": v})
    for i, (text, secret) in enumerate(SCRUB_TEXT):
        out.append({"name": f"scrub[{i}]", "op": "scrub", "value": text, "secret": secret})
    return out


def expected(case: dict):
    """Upstream's answer as ("ok", value) or ("err", exception name).

    The helpers refuse some inputs with VaultError (an origin with no scheme, for one), and the C port
    signals the same refusal with NULL, so both sides are compared as ok-or-refusal rather than as
    strings.
    """
    op = case["op"]
    try:
        if op == "kinds":
            return ("ok", list(vs.VAULT_KINDS))
        if op == "idtypes":
            return ("ok", list(vs.LOGIN_IDENTIFIER_TYPES))
        if op == "origin":
            return ("ok", vs.normalize_origin(case["value"]))
        if op == "otp":
            return ("ok", vs.normalize_otp_secret(case["value"]))
        if op == "fields":
            kind = case["value"]
            return ("ok", dict(vs.PAYMENT_FIELDS if kind == "payment" else
                               vs.ADDRESS_FIELDS if kind == "address" else {}))
        if op == "required":
            kind = case["value"]
            return ("ok", list(vs.REQUIRED_FIELDS.get(kind, ())))
        if op == "scrub":
            return ("ok", vs.scrub_secret_from_text(case["value"], case["secret"]))
    except Exception as exc:                                   # noqa: BLE001 - the refusal IS the answer
        return ("err", type(exc).__name__)
    raise SystemExit(f"unknown op {op}")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--keep", action="store_true")
    args = ap.parse_args()

    case_list = cases()
    tin = REPO / ".testbin" / "vstore_cases.json"
    tout = REPO / ".testbin" / "vstore_out.json"
    tin.parent.mkdir(exist_ok=True)
    tin.write_text(json.dumps(case_list), encoding="utf-8")

    exe = REPO / ".testbin" / "vstore_diff.exe"
    if not exe.exists():
        build = subprocess.run(
            ["gcc", "-O1", "-w", "-I", "include", "-I", "lib", "-I", "lib/libjson",
             "src/agent/vault_store.c", "tests/unit/vstore_diff.c", "lib/libjson/json.o",
             "-o", str(exe)],
            cwd=str(REPO), capture_output=True, text=True)
        if build.returncode != 0:
            print("cannot build the harness:")
            print(build.stderr[-2500:])
            return 2

    proc = subprocess.run([str(exe), str(tin), str(tout)], capture_output=True, text=True)
    if proc.returncode != 0:
        print("harness failed:", proc.returncode, proc.stderr[-800:])
        return 2
    got = {c["name"]: c["result"] for c in json.loads(tout.read_text(encoding="utf-8"))}

    agree = failed = 0
    refused = 0
    for case in case_list:
        kind, want = expected(case)
        have = got.get(case["name"])
        if kind == "err":
            # A refusal: the C side must also decline (NULL), never invent a value.
            if have is None:
                agree += 1
                refused += 1
            else:
                failed += 1
                print(f"FAIL {case['name']} ({case['op']}): upstream refused ({want}), C returned {have!r}")
            continue
        if have == want:
            agree += 1
        else:
            failed += 1
            print(f"FAIL {case['name']} ({case['op']})")
            print(f"   want: {want!r}")
            print(f"   got:  {have!r}")

    print(f"vault-store oracle: {agree} agree ({refused} of them refusals), {failed} problem(s)")
    if not args.keep:
        for p in (tin, tout):
            p.unlink(missing_ok=True)
        exe.unlink(missing_ok=True)
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
