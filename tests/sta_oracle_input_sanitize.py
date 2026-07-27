#!/usr/bin/env python3
"""
sta_oracle_input_sanitize.py — Python oracle for hermes_cli/input_sanitize.py,
ported in src/cli/port_input_sanitize.c.

Imports the REAL hermes_cli.input_sanitize module and calls
sanitize_user_prompt_text() on each fixture line. One JSON object per line:
{"in": <raw>, "out": <sanitized>}. ensure_ascii=False so control chars show
raw (matching the C harness which prints bytes).
"""
import json
import os
import sys

_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
if _ROOT not in sys.path:
    sys.path.insert(0, _ROOT)

import hermes_cli.input_sanitize as ins


def main():
    if len(sys.argv) < 2:
        sys.stderr.write("usage: sta_oracle_input_sanitize.py <cases.in>\n")
        return 2
    with open(sys.argv[1], "r", encoding="utf-8") as fp:
        for raw in fp:
            line = raw.rstrip("\n")
            if not line.strip() or line.startswith("#"):
                continue
            # control-char escapes in the fixture: \e -> ESC, \n -> newline
            text = line.replace("\\e", "\x1b").replace("\\n", "\n")
            out = ins.sanitize_user_prompt_text(text)
            sys.stdout.write(json.dumps({"in": text, "out": out},
                                        separators=(",", ":"),
                                        ensure_ascii=True) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
