#!/usr/bin/env python3
"""
sta_oracle_fuzzy_utils.py — oracle for t_port_fuzzy_utils.c.

Reference implementations:
  count_lines(s): 0 if s is None/empty, else number of '\\n' + 1.
  trim_right(s):  strip trailing whitespace (str.rstrip()).
Emits one JSON line per case (same shape as the C harness). The runner diffs
them. These are utility functions with a clear contract (no frozen canonical
Python def), so this is a behavior-contract oracle.
"""
import sys
import json as _json


def count_lines(s):
    if not s:
        return 0
    return s.count("\n") + 1


def trim_right(s):
    if not s:
        return ""
    return s.rstrip()


def main():
    if len(sys.argv) < 2:
        sys.stderr.write("usage: sta_oracle_fuzzy_utils.py <cases.tsv>\n")
        return 2
    with open(sys.argv[1], "rb") as f:
        raw = f.read()
    text = raw.decode("utf-8")

    for line in text.split("\n"):
        if not line:
            continue
        parts = line.split("\t", 1)
        if len(parts) < 2:
            continue
        op, inp = parts[0], parts[1]
        if op == "lines":
            out = count_lines(inp)
            sys.stdout.write('{"op":"lines","out":%d}\n' % out)
        elif op == "trim":
            out = trim_right(inp)
            sys.stdout.write('{"op":"trim","out":%s}\n' % _json.dumps(out))
    return 0


if __name__ == "__main__":
    sys.exit(main())
