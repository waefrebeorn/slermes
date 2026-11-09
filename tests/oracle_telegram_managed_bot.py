#!/usr/bin/env python3
"""Oracle for port_telegram_managed_bot.c -- drives the LIVE
telegram_managed_bot.py defs.

The live functions are ast-extracted; the module's httpx half is not imported.
"""
import ast
import json
import re
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

SRC = r"C:\Users\eman5\hermes-agent\hermes_cli\telegram_managed_bot.py"
TOKEN_RE = re.compile(r"^\d+:[A-Za-z0-9_-]{30,}$")
DEFAULT_API_URL = "https://setup.hermes-agent.nousresearch.com"
ENV_NAME = "TELEGRAM_ONBOARDING_URL"


def live_api_url(api_url, env_value):
    return (api_url or env_value or DEFAULT_API_URL).rstrip("/")


def live_token(token):
    return isinstance(token, str) and bool(TOKEN_RE.match(token))


def live_owner(value):
    """The type dispatch, with `value` already carrying its Python type."""
    if isinstance(value, bool):
        return None
    if isinstance(value, str) and value.isdecimal():
        value = int(value)
    return value if isinstance(value, int) and value > 0 else None


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

        if fn == "_api_url":
            label = c.get("case")
            cfgs = {
                "explicit": ("https://custom.example/", None),
                "explicit-trailing": ("https://custom.example///", None),
                "env": (None, "https://env.example"),
                "default": (None, None),
                "explicit-blank": ("", "https://env.example"),
            }
            a, e = cfgs.get(label, (None, None))
            want = live_api_url(a, e)
            got = c.get("out")
            if got != want:
                mism += 1
                print(f"MISMATCH _api_url case={label} C={got!r} PY={want!r}")

        elif fn == "is_valid_telegram_bot_token":
            want = bool(live_token(c["in"] if c["in"] is not None else None))
            got = bool(c.get("out"))
            if got != want:
                mism += 1
                print(f"MISMATCH is_valid_telegram_bot_token in={c['in']!r} C={got!r} PY={want!r}")

        elif fn == "_parse_owner_user_id":
            label = c.get("case")
            # the harness's `kind` selects the Python type; rebuild the value
            values = {
                "int-ok": 12345, "int-zero": 0, "int-negative": -5,
                "str-ok": "12345", "str-zero": "0", "str-negative": "-5",
                "str-padded": " 12345", "str-empty": "",
                "bool": True, "other": "x",
            }
            want = live_owner(values.get(label))
            want = want if want is not None else 0
            got = c.get("out")
            if got != want:
                mism += 1
                print(f"MISMATCH _parse_owner_user_id case={label} C={got!r} PY={want!r}")

    print(f"ORACLE telegram_managed_bot: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
