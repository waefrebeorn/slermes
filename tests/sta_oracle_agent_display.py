#!/usr/bin/env python3
"""Oracle: agent/display.summarize_shell_command vs LIVE Python.

Usage: python3 sta_oracle_agent_display.py <command_file>
Prints the summarized shell command from LIVE Python. Diff against the C
harness (t_port_agent_display) for fidelity.
"""
import sys
sys.path.insert(0, "/home/wubu/hermes-agent-dev")
import importlib.util as u

spec = u.spec_from_file_location("disp", "/home/wubu/hermes-agent-dev/agent/display.py")
mod = u.module_from_spec(spec)
try:
    spec.loader.exec_module(mod)
except Exception as e:
    print("IMPORT_FAIL", repr(e)); sys.exit(2)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("usage: sta_oracle_agent_display.py <command_file>", file=sys.stderr)
        sys.exit(2)
    with open(sys.argv[1], "r", encoding="utf-8") as f:
        cmd = f.read()
    print(mod.summarize_shell_command(cmd))
