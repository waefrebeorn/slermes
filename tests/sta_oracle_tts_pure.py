#!/usr/bin/env python3
"""Oracle for the pure TTS helpers of tools/tts_tool.py ported to C.

Reads the JSON cases fixture (argv[1]), recomputes each case from LIVE
tools/tts_tool.py, emits one JSON object per case.
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.getcwd())
sys.path.insert(0, "/home/wubu/.hermes/hermes-agent")

from tools import tts_tool as m  # noqa: E402


def main():
    with open(sys.argv[1], encoding="utf-8") as f:
        cases = json.load(f)
    for c in cases:
        op = c.get("op")
        arg = c.get("arg")
        if op == "resolve_profile":
            cfg = json.loads(c.get("cfg", "{}")) if isinstance(c.get("cfg", "{}"), str) and c.get("cfg") else {}
            p = m._resolve_audio_delivery_profile(arg, cfg)
            # Python returns an AudioDeliveryProfile dataclass-like namedtuple
            print(json.dumps({"op": "resolve_profile",
                              "platform": p.platform,
                              "max_file_bytes": p.max_file_bytes,
                              "safety_ratio": float(p.safety_ratio)}))
        elif op == "split_sentence":
            chunks = m._split_oversized_sentence(arg if isinstance(arg, str) else "",
                                                 int(c.get("max_chars", 10)))
            print(json.dumps({"op": "split_sentence", "out": chunks}))
        else:
            print(json.dumps({"op": "unknown"}))


if __name__ == "__main__":
    main()
