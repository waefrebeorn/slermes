#!/usr/bin/env python3
"""
sta_oracle_video_gen_cache_dir.py — Oracle for
agent/video_gen_provider.py:_videos_cache_dir.

Reads the C harness JSON line (path/exists) from stdin; replays against LIVE
Python in the SAME HERMES_HOME (set by the test runner) and compares the path
string and the created-directory existence.
"""
import sys
import os
import json
from pathlib import Path

sys.path.insert(0, "/home/wubu/hermes-agent-dev")
import agent.video_gen_provider as VGP  # noqa: E402


def main():
    data = sys.stdin.read()
    c = None
    for ln in data.splitlines():
        ln = ln.strip()
        if not ln:
            continue
        try:
            obj = json.loads(ln)
        except Exception:
            continue
        if "path" in obj:
            c = obj
    if c is None:
        print("MISSING C output")
        sys.exit(1)

    # LIVE Python truth in the SAME HERMES_HOME (runner sets HERMES_HOME)
    py_path = VGP._videos_cache_dir()
    py_path_str = str(py_path)
    py_exists = py_path.exists() and py_path.is_dir()

    ok = (c["path"] == py_path_str) and (bool(c["exists"]) == py_exists)
    print(f"C ={c['path']} exists={c['exists']}")
    print(f"PY={py_path_str} exists={py_exists}")
    if ok:
        print("ok [_videos_cache_dir]")
        print("\nRESULT: 1/1 match, 0 mismatch")
        sys.exit(0)
    else:
        print("MISMATCH [_videos_cache_dir]")
        print("\nRESULT: 0/1 match, 1 mismatch")
        sys.exit(1)


if __name__ == "__main__":
    main()
