#!/usr/bin/env python3
"""Differential oracle for the ported fill-script builder (vl_build_fill_js vs build_fill_js).

The script this compares is the one that actually types a secret into a page, and it carries the
origin assertion that stops a fill when the page navigated between inspection and write. So the two
must agree byte for byte, not merely "both produce valid JavaScript".

Usage: python tests/oracle/fill_js_oracle.py [--keep]
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

from agent.vault_login_classifier import build_fill_js  # noqa: E402


def cases() -> list[dict]:
    return [
        {"name": "password", "origin": "https://example.com", "nonce": "n1",
         "fills": [{"index": 0, "token": "current-password", "type": "password", "value": "hunter2"}]},
        {"name": "username", "origin": "https://example.com", "nonce": "n1",
         "fills": [{"index": 1, "token": "username", "type": "text", "value": "ada"}]},
        {"name": "two-fills", "origin": "https://app.example.com:8443", "nonce": "abcd",
         "fills": [{"index": 0, "token": "email", "type": "email", "value": "ada@example.com"},
                   {"index": 2, "token": "current-password", "type": "password", "value": "hunter2"}]},
        {"name": "select", "origin": "https://shop.example.com", "nonce": "xy",
         "fills": [{"index": 3, "token": "cc-exp-month", "type": "select", "value": "12"}]},
        {"name": "empty", "origin": "https://example.com", "nonce": "", "fills": []},
    ]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--keep", action="store_true")
    args = ap.parse_args()

    case_list = cases()
    tin = REPO / ".testbin" / "filljs_cases.json"
    tout = REPO / ".testbin" / "filljs_out.json"
    tin.parent.mkdir(exist_ok=True)
    tin.write_text(json.dumps(case_list), encoding="utf-8")

    exe = REPO / ".testbin" / "filljs_diff.exe"
    if not exe.exists():
        build = subprocess.run(
            ["gcc", "-O1", "-w", "-I", "include", "-I", "lib", "-I", "lib/libjson",
             "src/agent/port_vault_login_js.c", "tests/unit/filljs_diff.c",
             "lib/libjson/json.o", "-o", str(exe)],
            cwd=str(REPO), capture_output=True, text=True)
        if build.returncode != 0:
            print("cannot build the harness:")
            print(build.stderr[-2500:])
            return 2

    proc = subprocess.run([str(exe), str(tin), str(tout)], capture_output=True, text=True)
    if proc.returncode != 0:
        print("harness failed:", proc.returncode, proc.stderr[-800:])
        return 2
    got = {c["name"]: c["js"] for c in json.loads(tout.read_text(encoding="utf-8"))}

    agree = failed = 0
    for case in case_list:
        want = build_fill_js(case["fills"], case["origin"], case["nonce"])
        have = got.get(case["name"])
        if have == want:
            agree += 1
            continue
        failed += 1
        print(f"FAIL {case['name']}")
        if have is None:
            print("   C returned nothing")
            continue
        # First divergence, with context: a byte-for-byte diff of JS is unreadable otherwise.
        at = next((i for i, (a, b) in enumerate(zip(want, have)) if a != b), min(len(want), len(have)))
        print(f"   first difference at byte {at} (want {len(want)} B, got {len(have)} B)")
        print(f"   want: {want[max(0, at-60):at+80]!r}")
        print(f"   got:  {have[max(0, at-60):at+80]!r}")

    print(f"fill-js oracle: {agree} agree, {failed} problem(s)")
    if not args.keep:
        for p in (tin, tout):
            p.unlink(missing_ok=True)
        exe.unlink(missing_ok=True)
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
