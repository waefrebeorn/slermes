#!/usr/bin/env python3
"""Faithfulness oracle for port_gateway_signal_format.c.

Recomputes gateway/platforms/signal_format.markdown_to_signal from LIVE source
for the same inputs and asserts (text, styles) match the C port exactly.
"""
import sys, json
sys.path.insert(0, "/home/wubu/hermes-agent-dev")
from gateway.platforms.signal_format import markdown_to_signal

ok = True
mismatch = 0
total = 0
for line in sys.stdin:
    line = line.strip()
    if not line or not line.startswith("{"):
        continue
    rec = json.loads(line)
    inp = rec["in"]
    c_text = rec["text"]
    c_styles = rec["styles"]
    exp_text, exp_styles = markdown_to_signal(inp)
    total += 1
    if c_text != exp_text or c_styles != exp_styles:
        ok = False
        mismatch += 1
        print("MISMATCH in=", repr(inp))
        print("   PY text=", repr(exp_text))
        print("   C  text=", repr(c_text))
        if c_text == exp_text:
            print("   PY styles=", exp_styles)
            print("   C  styles=", c_styles)

print(f"ORACLE {'OK' if ok else 'BAD'} ({total} cases, {mismatch} mismatches)")
sys.exit(0 if ok else 1)
