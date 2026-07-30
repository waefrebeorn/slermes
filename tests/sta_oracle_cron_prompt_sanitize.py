#!/usr/bin/env python3
"""
sta_oracle_cron_prompt_sanitize.py — Python oracle for the PURE cron-prompt
sanitization helpers in tools/cronjob_tools.py (_check_invisible_unicode,
_strip_invisible_unicode, _scan_cron_skill_assembled), ported in
src/tools/cron_prompt_sanitize.c.

Imports the REAL tools.cronjob_tools module and exercises the genuine
functions. Output contract matches tests/t_port_cron_prompt_sanitize.c: one
JSON object per line, sorted keys, ensure_ascii=False, compact separators.
Invisible codepoints / emoji are token-encoded in the fixture and decoded here
identically to the C harness.
"""

import json
import os
import sys

_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
if _ROOT not in sys.path:
    sys.path.insert(0, _ROOT)

from tools.cronjob_tools import (  # noqa: E402
    _check_invisible_unicode,
    _strip_invisible_unicode,
    _scan_cron_skill_assembled,
)


def emit(obj):
    sys.stdout.write(json.dumps(obj, separators=(",", ":"), ensure_ascii=False) + "\n")


def dec(s):
    """Decode fixture tokens into real characters (mirror of C harness)."""
    if s is None:
        return ""
    s = s.replace("@ZWSP@", "​")   # U+200B
    s = s.replace("@ZWJ@", "​")    # U+200D
    s = s.replace("@ZWNJ@", "​")   # U+200C
    s = s.replace("@BOM@", "﻿")    # U+FEFF
    s = s.replace("@LTR@", "‎")    # U+200E
    s = s.replace("@RLM@", "‏")    # U+200F
    s = s.replace("@EMO@", "😀")  # U+1F600
    return s


def split_kv(line):
    line = line.rstrip("\n")
    if not line.strip() or line.startswith("#"):
        return None
    op, _, rest = line.partition(" ")
    return op, rest


def main():
    for raw in sys.stdin:
        parsed = split_kv(raw)
        if parsed is None:
            continue
        op, rest = parsed
        v = dec(rest) if rest else ""

        if op == "check":
            emit({"op": "check", "in": v, "error": _check_invisible_unicode(v)})

        elif op == "strip":
            cleaned, removed = _strip_invisible_unicode(v)
            emit({"op": "strip", "in": v, "cleaned": cleaned, "removed": removed})

        elif op == "scan":
            cleaned, error = _scan_cron_skill_assembled(v)
            emit({"op": "scan", "in": v, "cleaned": cleaned, "error": error})

        else:
            emit({"op": "unknown", "raw": raw.rstrip("\n")})

    return 0


if __name__ == "__main__":
    sys.exit(main())
