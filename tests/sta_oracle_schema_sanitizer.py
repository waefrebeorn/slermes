#!/usr/bin/env python3
"""Oracle: tools/schema_sanitizer.strip_pattern_and_format / strip_slash_enum
vs LIVE Python.

Usage: python3 sta_oracle_schema_sanitizer.py <pf|se> <tools.json>
Prints "COUNT=<n>\\n<serialized tools json>" from LIVE Python, in the same
shape the C harness (t_port_schema_sanitizer) emits, so the two can be
diffed directly. Key order is preserved (both Python and libjson keep
insertion order), so the byte diff is meaningful.
"""
import sys
sys.path.insert(0, "/home/wubu/hermes-agent-dev")
import importlib.util as u

spec = u.spec_from_file_location("ss", "/home/wubu/hermes-agent-dev/tools/schema_sanitizer.py")
mod = u.module_from_spec(spec)
try:
    spec.loader.exec_module(mod)
except Exception as e:
    print("IMPORT_FAIL", repr(e)); sys.exit(2)

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("usage: sta_oracle_schema_sanitizer.py <tools.json> <pf|se>", file=sys.stderr)
        sys.exit(2)
    path, mode = sys.argv[1], sys.argv[2]
    with open(path, "r", encoding="utf-8") as f:
        tools = __import__("json").loads(f.read())
    if mode == "pf":
        out, stripped = mod.strip_pattern_and_format(tools)
    elif mode == "se":
        out, stripped = mod.strip_slash_enum(tools)
    else:
        print("unknown mode", file=sys.stderr); sys.exit(2)
    print("COUNT=%d" % stripped)
    print(__import__("json").dumps(out, ensure_ascii=False, separators=(",", ":")))
