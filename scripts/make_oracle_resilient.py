#!/usr/bin/env python3
"""Wrap an oracle's per-case dispatch chain in a try/except.

An oracle's main() is one long ``if op == ...:`` chain. When upstream deletes a
helper, the first case that touches it raises and the WHOLE remaining case list
is lost -- the run reports a handful of cases instead of the real diff. This
rewrites the chain into ``_run_case(op, a1, a2)`` and calls it under a guard so
each case reports its own error and the rest still get measured.

Idempotent: refuses if ``_run_case`` is already present.
"""
from __future__ import annotations

import io
import sys
from pathlib import Path

ANCHOR_START = 'if op == "valid_section_id":'
ANCHOR_END = 'out("UNKNOWN_OP", op, None)'


def make_resilient(path: Path) -> str:
    raw = path.read_bytes().decode("utf-8")
    if "_run_case(" in raw:
        return "already resilient"
    # normalise to LF for line arithmetic, restore the file's own ending
    nl = "\r\n" if "\r\n" in raw else "\n"
    text = raw.replace("\r\n", "\n")
    lines = text.split("\n")

    start = end = None
    for i, ln in enumerate(lines):
        if start is None and ln.strip() == ANCHOR_START:
            start = i
        if start is not None and ln.strip() == ANCHOR_END:
            end = i
            break
    if start is None or end is None:
        return "anchors not found"

    indent = len(lines[start]) - len(lines[start].lstrip())
    pad = " " * indent
    block = [ln[indent:] if ln.strip() else "" for ln in lines[start:end + 1]]
    fn = ["def _run_case(op, a1, a2):"] + ["    " + b if b else "" for b in block]

    guard = [
        pad + "try:",
        pad + "    _run_case(op, a1, a2)",
        pad + "except Exception as exc:",
        pad + '    out(op, a1, {"error": "%s: %s" % (type(exc).__name__, exc)})',
    ]
    # _run_case must sit OUTSIDE main: the chain lives inside main's body, so
    # inserting it in place would leave the guard orphaned at main's indent.
    main_start = None
    for i, ln in enumerate(lines):
        if ln.startswith("def main("):
            main_start = i
            break
    if main_start is None or main_start > start:
        return "main() not found before the chain"

    new = (lines[:main_start] + fn + lines[main_start:start] + guard
           + lines[end + 1:])
    path.write_bytes(("\n".join(new)).replace("\n", nl).encode("utf-8"))
    return "rewrote %d chain lines" % (end - start + 1)


if __name__ == "__main__":
    for arg in sys.argv[1:]:
        print(arg, "->", make_resilient(Path(arg)))
