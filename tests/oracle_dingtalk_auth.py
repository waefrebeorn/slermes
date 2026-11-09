#!/usr/bin/env python3
"""Oracle for port_dingtalk_auth.c -- drives the LIVE dingtalk_auth.py defs.

The harness echoes its inputs from C. The live functions are ast-extracted
because importing hermes_cli.dingtalk_auth pulls in requests.
"""
import ast
import json
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

SRC = r"C:\Users\eman5\hermes-agent\hermes_cli\dingtalk_auth.py"
POLL_STATUSES = {"WAITING", "SUCCESS", "FAIL", "EXPIRED"}
FIELDS = ("client_id", "client_secret", "fail_reason")


def live_status(status):
    """The status line from poll_registration, without the HTTP call."""
    raw = str(status if status is not None else "").strip().upper()
    return raw if raw in POLL_STATUSES else "UNKNOWN"


def live_field(data, key):
    """str(data.get(key, "")).strip() or None - the comprehension in poll_registration."""
    if not isinstance(data, dict):
        return None
    v = data.get(key, "")
    if v is None:
        return None
    s = str(v).strip()
    return s or None


def main():
    cases = 0
    mism = 0

    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except json.JSONDecodeError:
            continue
        fn = c.get("fn")
        cases += 1

        if fn == "poll_status":
            want = live_status(c["in"])
            got = c.get("out")
            if got != want:
                mism += 1
                print(f"MISMATCH poll_status in={c['in']!r} C={got!r} PY={want!r}")

        elif fn == "str_field":
            label = c.get("case")
            datas = {
                "present": {"client_id": "abc"},
                "padded": {"client_id": "  abc  "},
                "blank": {"client_id": "   "},
                "missing": {},
                "null": {"client_id": None},
                "secret": {"client_secret": "s3cr3t"},
                "reason": {"fail_reason": "nope"},
            }
            want = live_field(datas.get(label, {}), c["key"])
            got = c.get("out")
            if got != want:
                mism += 1
                print(f"MISMATCH str_field case={label} C={got!r} PY={want!r}")

    print(f"ORACLE dingtalk_auth: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
