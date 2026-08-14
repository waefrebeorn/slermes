#!/usr/bin/env python3
"""Oracle for the pure helpers of gateway/run.py ported in run_pure2.c.

Reads the JSON cases fixture (argv[1]), recomputes each case from LIVE
gateway/run.py, emits one JSON object per case.
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.getcwd())
sys.path.insert(0, "/home/wubu/.hermes/hermes-agent")

from gateway.run import (
    hygiene_compaction_recovered,
    compression_made_progress,
)


def main():
    with open(sys.argv[1], encoding="utf-8") as f:
        cases = json.load(f)
    for c in cases:
        op = c.get("op")
        if op == "hygiene_compaction_recovered":
            r = hygiene_compaction_recovered(
                aborted=bool(c.get("aborted", False)),
                rotated=bool(c.get("rotated", False)),
                in_place=bool(c.get("in_place", False)),
                msg_count=int(c.get("msg_count", 100)),
                new_count=int(c.get("new_count", 50)),
                approx_tokens=int(c.get("approx_tokens", 1000)),
                new_tokens=int(c.get("new_tokens", 600)),
            )
            print(json.dumps({"op": "hygiene_compaction_recovered", "out": r}))
        elif op == "compression_made_progress":
            r = compression_made_progress(
                orig_len=int(c.get("orig_len", 0)),
                new_len=int(c.get("new_len", 0)),
                orig_tokens=int(c.get("orig_tokens", 0)),
                new_tokens=int(c.get("new_tokens", 0)),
            )
            print(json.dumps({"op": "compression_made_progress", "out": r}))
        else:
            print(json.dumps({"op": "unknown"}))


if __name__ == "__main__":
    main()
