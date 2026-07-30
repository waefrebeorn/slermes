#!/usr/bin/env python3
"""
sta_oracle_file_safety_roots.py — Oracle for
agent/file_safety.py:get_safe_write_roots.

Reads the C harness JSON array line (the resolved roots) from stdin; replays
LIVE Python get_safe_write_roots() in the SAME HERMES_WRITE_SAFE_ROOT env and
compares the sorted root sets.
"""
import sys
import os
import json

sys.path.insert(0, "/home/wubu/hermes-agent-dev")
import agent.file_safety as FS  # noqa: E402


def main():
    data = sys.stdin.read()
    c_arr = None
    for ln in data.splitlines():
        ln = ln.strip()
        if not ln:
            continue
        try:
            c_arr = json.loads(ln)
        except Exception:
            continue
    if c_arr is None:
        print("MISSING C output")
        sys.exit(1)

    py_set = FS.get_safe_write_roots()  # a set of resolved path strings

    c_sorted = sorted(c_arr)
    p_sorted = sorted(py_set)
    if c_sorted == p_sorted:
        print("ok [get_safe_write_roots]")
        print(f"\nRESULT: 1/1 match, 0 mismatch")
        sys.exit(0)
    else:
        print("MISMATCH [get_safe_write_roots]")
        print(f"  C ={c_sorted}")
        print(f"  PY={p_sorted}")
        print(f"\nRESULT: 0/1 match, 1 mismatch")
        sys.exit(1)


if __name__ == "__main__":
    main()
