#!/usr/bin/env python3
"""Differential oracle for gate w14_localpromp_2504b2 (agent/local_prompt_progress.py)."""
import importlib.util
import json
import os
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

NAME = "w14_localpromp_2504b2_live"
sp = importlib.util.spec_from_file_location(NAME, os.path.join(Q, "agent", "local_prompt_progress.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)


def compute(r):
    f = r.get("func")
    if f == "with_prompt_progress":
        return live.with_prompt_progress(r.get("kwargs") or {})
    if f == "prompt_progress":
        return live.prompt_progress(r.get("chunk") or {})
    if f == "prompt_progress_percent":
        return live.prompt_progress_percent(r.get("progress") or {})
    return None


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except Exception:
            continue
        if not isinstance(r, dict) or "out" not in r:
            continue
        cases += 1
        py, got = compute(r), r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s py=%r c=%r" % (r.get("func"), py, got), file=sys.stderr)
    print("ORACLE w14_localpromp_2504b2: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
