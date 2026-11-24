#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/slack/adapter.py gate
w27_adapter_74f618_p1 (module-level text helpers).

Drives the LIVE adapter module (imported as a package) and compares against
the C values emitted by t_port_w27_adapter_74f618_p1.c.
"""
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w27_adapter_74f618_p1"

# The live module drags gateway/agent imports that resolve a home directory at
# import time; pin it to the repo parent (no user state is read).
if not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = str(REPO.parent)
if not os.environ.get("HOME"):
    os.environ["HOME"] = str(REPO.parent)
if not os.environ.get("HERMES_HOME"):
    os.environ["HERMES_HOME"] = str(QUARRY)


if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

import plugins.platforms.slack.adapter as live  # noqa: E402

WANTED = {
    "unfurl_kwargs", "is_table_row", "disp_width", "pad", "split_table_row",
    "align_table", "wrap_markdown_tables", "permalink_path", "str_or_empty",
    "int_or_zero", "first_truthy",
}


def expected(r):
    f = r.get("func")
    if f == "unfurl_kwargs":
        return live._slack_unfurl_kwargs(r.get("extra"))
    if f == "is_table_row":
        return live._is_table_row(r.get("line", ""))
    if f == "disp_width":
        return live._disp_width(r.get("s", ""))
    if f == "pad":
        return live._pad(r.get("cell", ""), r.get("width", 0))
    if f == "split_table_row":
        return live._split_table_row(r.get("line", ""))
    if f == "align_table":
        return live._align_table(r.get("rows") or [])
    if f == "wrap_markdown_tables":
        return live._wrap_markdown_tables(r.get("text", ""))
    if f == "permalink_path":
        return live._slack_permalink_path(r.get("channel_id"), r.get("message_ts"))
    if f == "str_or_empty":
        return live._str_or_empty(r.get("value"))
    if f == "int_or_zero":
        return live._int_or_zero(r.get("value", ""))
    if f == "first_truthy":
        return live._first_truthy(r.get("mapping") or {}, tuple(r.get("keys") or ()))
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
