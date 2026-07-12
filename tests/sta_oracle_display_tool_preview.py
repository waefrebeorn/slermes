#!/usr/bin/env python3
"""Oracle: agent/display build_tool_preview / build_tool_label /
redact_tool_args_for_display vs LIVE Python.

Usage: python3 sta_oracle_display_tool_preview.py <args.json> <preview|label|redact> <tool> [max_len]
Prints the resulting string so it can be diffed against the C harness output.
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
    if len(sys.argv) < 4:
        print("usage: sta_oracle_display_tool_preview.py <args.json> <mode> <tool> [max_len]", file=sys.stderr)
        sys.exit(2)
    path, mode, tool = sys.argv[1], sys.argv[2], sys.argv[3]
    max_len = int(sys.argv[4]) if len(sys.argv) > 4 else 0
    with open(path, "r", encoding="utf-8") as f:
        args = __import__("json").loads(f.read())
    if mode == "preview":
        out = mod.build_tool_preview(tool, args, max_len=max_len)
    elif mode == "label":
        out = mod.build_tool_label(tool, args, max_len=max_len)
    elif mode == "redact":
        out = mod.redact_tool_args_for_display(tool, args)
        out = __import__("json").dumps(out, ensure_ascii=False, separators=(",", ":"))
    else:
        print("unknown mode", file=sys.stderr); sys.exit(2)
    print(out if out is not None else "")
