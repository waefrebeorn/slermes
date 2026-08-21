#!/usr/bin/env python3
"""sta_oracle_subprocess_compat.py — oracle twin for subprocess_compat_split_command_line
in src/hermes_cli/subprocess_compat_pure.c
(hermes_cli/_subprocess_compat.py:split_command_line, POSIX branch = shlex.split).
"""
import json
import sys
import shlex


def split_command_line(line):
    # POSIX branch of the Python original
    return shlex.split(line)


def main():
    fixture = sys.argv[1]
    with open(fixture) as f:
        root = json.load(f)
    for c in root.get("ops", []):
        line = c.get("line", "")
        try:
            out = split_command_line(line)
            err = 0
        except ValueError:
            out = None
            err = 1
        print(json.dumps({"line": line, "err": err, "out": out}))


if __name__ == "__main__":
    main()
