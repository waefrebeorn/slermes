#!/usr/bin/env python3
"""Oracle: agent/markdown_tables.realign_markdown_tables vs LIVE Python.

Usage: python3 sta_oracle_markdown_tables.py <input_file> [available_width]
Prints the realigned markdown from LIVE Python. Diff against the C harness
(t_port_markdown_tables) for fidelity.

For the narrow-terminal vertical fallback, pass available_width (e.g. 20).
"""
import sys
sys.path.insert(0, "/home/wubu/hermes-agent-dev")
import importlib.util as u
spec = u.spec_from_file_location("mt", "/home/wubu/hermes-agent-dev/agent/markdown_tables.py")
mod = u.module_from_spec(spec)
try:
    spec.loader.exec_module(mod)
except Exception as e:
    print("IMPORT_FAIL", repr(e)); sys.exit(2)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("usage: sta_oracle_markdown_tables.py <input> [available_width]", file=sys.stderr)
        sys.exit(2)
    with open(sys.argv[1], "r", encoding="utf-8") as f:
        text = f.read()
    avail = int(sys.argv[2]) if len(sys.argv) > 2 else None
    print(mod.realign_markdown_tables(text, avail), end="")
