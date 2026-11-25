#!/usr/bin/env python3
"""Differential oracle for the ported login-control classifier.

This is the code that decides which box a password, email or card value lands in, so a wrong answer
is a credential typed into the wrong field. src/agent/vault_login_classifier.c is ~540 lines and had
never been compared with agent/vault_login_classifier.py; agent/vault_login_classifier.py imports
nothing outside the standard library, so it can be.

Compared: classify_login_control over a control grid (autocomplete tokens, input types, names and
labels that hit the phrase tables, negative cases). The C side answers
{"ok", "score", "token"}; upstream answers None or ClassifiedLoginControl(score, token).

Usage: python tests/oracle/login_classifier_oracle.py [--keep]
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

from agent.vault_login_classifier import LoginControl, classify_login_control  # noqa: E402


def controls() -> list[dict]:
    base = {"index": 0, "form_index": 0}
    out = [
        # autocomplete decides first
        {"autocomplete": "username"},
        {"autocomplete": "email"},
        {"autocomplete": "tel"},
        {"autocomplete": "current-password"},
        {"autocomplete": "new-password"},
        {"autocomplete": "one-time-code"},
        {"autocomplete": "cc-number"},
        {"autocomplete": "bogus yolo"},
        # input type
        {"autocomplete": "", "type": "password"},
        {"autocomplete": "", "type": "email"},
        {"autocomplete": "", "type": "tel"},
        {"autocomplete": "", "type": "text"},
        {"autocomplete": "", "type": "checkbox"},
        # phrase tables through name/label
        {"autocomplete": "", "name": "user_login"},
        {"autocomplete": "", "name": "email"},
        {"autocomplete": "", "name": "phone_number"},
        {"autocomplete": "", "name": "card_number"},
        {"autocomplete": "", "name": "cvc"},
        {"autocomplete": "", "name": "postal_code"},
        {"autocomplete": "", "name": "q"},
        {"autocomplete": "", "label": "Password"},
        {"autocomplete": "", "label": "Confirm password"},
        {"autocomplete": "", "label": "Create password"},
        {"autocomplete": "", "label": "E-mail address"},
        {"autocomplete": "", "label": "MileagePlus number"},
        {"autocomplete": "", "label": "Search"},
        # both
        {"autocomplete": "email", "type": "password", "name": "email"},
        {"autocomplete": "current-password", "name": "new_password"},
    ]
    return [dict(base, **c, name_=None) | {} for c in out]


def to_control(case: dict) -> LoginControl:
    payload = {k: v for k, v in case.items() if k != "name_"}
    return LoginControl.from_dict(payload)


def expected(case: dict):
    got = classify_login_control(to_control(case))
    if got is None:
        return {"none": True}
    return {"none": False, "score": got.score, "token": got.token}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--keep", action="store_true")
    args = ap.parse_args()

    grid = controls()
    cases = []
    expected_by_name = {}
    for i, control in enumerate(grid):
        name = f"control[{i}]"
        cases.append({"name": name, "control": {k: v for k, v in control.items() if k != "name_"}})
        expected_by_name[name] = expected(control)

    tin = REPO / ".testbin" / "lc_cases.json"
    tout = REPO / ".testbin" / "lc_out.json"
    tin.parent.mkdir(exist_ok=True)
    tin.write_text(json.dumps(cases), encoding="utf-8")

    exe = REPO / ".testbin" / "loginclass_diff.exe"
    if not exe.exists():
        build = subprocess.run(
            ["gcc", "-O1", "-w", "-I", "include", "-I", "lib", "-I", "lib/libjson",
             "src/agent/vault_login_classifier.c", "tests/unit/loginclass_diff.c",
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
    got = {c["name"]: c["result"] for c in json.loads(tout.read_text(encoding="utf-8"))}

    agree = failed = 0
    for case in cases:
        name = case["name"]
        want = expected_by_name[name]
        have = got.get(name) or {}
        c_ok = bool(have.get("ok")) if isinstance(have, dict) else False
        if want["none"]:
            if not c_ok:
                agree += 1
            else:
                failed += 1
                print(f"FAIL {name}: upstream says not a fill target, C classified {have}")
            continue
        if c_ok and int(have.get("score", -1)) == want["score"] and have.get("token") == want["token"]:
            agree += 1
        else:
            failed += 1
            print(f"FAIL {name}: control={case['control']}")
            print(f"   want: score={want['score']} token={want['token']!r}")
            print(f"   got:  {have}")

    print(f"login-classifier oracle: {agree} agree, {failed} problem(s)")
    if not args.keep:
        for p in (tin, tout):
            p.unlink(missing_ok=True)
        exe.unlink(missing_ok=True)
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
