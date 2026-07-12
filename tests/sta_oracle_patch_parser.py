#!/usr/bin/env python3
"""Oracle: tools/patch_parser.parse_v4a_patch vs LIVE Python.

Emits the SAME canonical JSON that the C harness (t_port_patch_parser) emits,
so the two can be diffed directly:

  ./t_port_patch_parser <patch_file > /tmp/c.json
  python3 sta_oracle_patch_parser.py <patch_file > /tmp/py.json
  diff /tmp/c.json /tmp/py.json   # empty => 0 mismatches

The shell wrapper sta_oracle_test.sh runs this comparison across all cases.
"""
import sys, json
sys.path.insert(0, "/home/wubu/hermes-agent-dev")
import importlib.util as u
spec = u.spec_from_file_location("pp", "/home/wubu/hermes-agent-dev/tools/patch_parser.py")
mod = u.module_from_spec(spec)
try:
    spec.loader.exec_module(mod)
except Exception as e:
    print("IMPORT_FAIL", repr(e)); sys.exit(2)

OP_NAMES = {mod.OperationType.ADD: "add", mod.OperationType.UPDATE: "update",
            mod.OperationType.DELETE: "delete", mod.OperationType.MOVE: "move"}

def canonical(ops, err):
    if err:
        return json.dumps({"error": err}, ensure_ascii=False, separators=(",", ":"))
    out = []
    for op in ops:
        hunks = []
        for h in op.hunks:
            lines = [[l.prefix, l.content] for l in h.lines]
            hunks.append({"hint": h.context_hint, "lines": lines})
        out.append({"op": OP_NAMES[op.operation], "path": op.file_path,
                    "new": op.new_path, "hunks": hunks})
    return json.dumps(out, ensure_ascii=False, separators=(",", ":"))

if __name__ == "__main__":
    if len(sys.argv) > 1:
        with open(sys.argv[1], "r", encoding="utf-8") as f:
            patch = f.read()
    else:
        patch = sys.stdin.read()
    ops, err = mod.parse_v4a_patch(patch)
    print(canonical(ops, err))
