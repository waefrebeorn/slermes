#!/usr/bin/env python3
"""Python oracle for recognition parity (tools/voice_mode.py).

Mirrors the exact functions under test:
  - is_whisper_hallucination(text)  (direct import from tools/voice_mode.py)
  - voice_block_rms: sqrt(mean(data.astype(float64)**2))  (re-derivation of
    the formula the C voice_block_rms() ports)

Reads the same fixture JSON the C harness reads and prints the same shape.
"""
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
# Import the real Python module under test.
sys.path.insert(0, os.path.join(HERE, "..", "..", "..", ".hermes", "hermes-agent", "tools"))
sys.path.insert(0, os.path.join(HERE, "..", "..", "..", "..", ".hermes", "hermes-agent"))
sys.path.insert(0, "/home/wubu/.hermes/hermes-agent/tools")
sys.path.insert(0, "/home/wubu/.hermes/hermes-agent")

import tools.voice_mode as vm  # noqa: E402


def voice_block_rms(samples):
    if not samples:
        return 0.0
    s = [float(x) for x in samples]
    return float(math.sqrt(sum(x * x for x in s) / len(s)))


def main():
    if len(sys.argv) < 2:
        sys.stderr.write("usage: sta_oracle_recognition.py <fixture.json>\n")
        return 2
    with open(sys.argv[1], "r", encoding="utf-8") as f:
        doc = json.load(f)

    fn = doc.get("fn", "")
    if fn == "hallucination":
        r = bool(vm.is_whisper_hallucination(doc.get("text", "")))
        print(json.dumps({"fn": "hallucination", "result": r}, separators=(",", ":")))
    elif fn == "rms":
        r = voice_block_rms(doc.get("samples", []))
        print(json.dumps({"fn": "rms", "result": float(f"{r:.6f}")}, separators=(",", ":")))
    else:
        sys.stderr.write("unknown fn %s\n" % fn)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
