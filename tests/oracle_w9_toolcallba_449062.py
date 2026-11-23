#!/usr/bin/env python3
"""Differential oracle for gate w9_toolcallba_449062 (agent/tool_call_batches.py).

AST-extracts replace_native_tool_blocks and its two block-expansion helpers from
the quarry. coalesce_tool_call_id is stubbed to child.id. Compares the resulting
provider_data against the C port.
"""
import __future__
import ast
import builtins
import copy
import json
import sys
import types
from pathlib import Path

try:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    if hasattr(sys.stdin, "reconfigure"):
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "agent" / "tool_call_batches.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
sys.path.insert(0, str(QUARRY))
SRC = str(QUARRY / "agent/tool_call_batches.py")
NAME = "w9_toolcallba_449062"
PORTED = ["replace_native_tool_blocks", "_anthropic_children", "_bedrock_children"]

_TREE = ast.parse(Path(SRC).read_text(encoding="utf-8"))
_FLAGS = __future__.annotations.compiler_flag
_CODE = {}
for _n in _TREE.body:
    if isinstance(_n, ast.FunctionDef) and _n.name in PORTED:
        _CODE[_n.name] = compile(ast.Module(body=[_n], type_ignores=[]), SRC, "exec",
                                 flags=_FLAGS, dont_inherit=True)
if set(_CODE) != set(PORTED):
    print(f"ORACLE {NAME}: FATAL missing live defs", file=sys.stderr)
    sys.exit(1)

_MOD = types.ModuleType("w9tb_live_tool_call_batches")
_MOD.__dict__.update({"__name__": "w9tb_live_tool_call_batches", "__builtins__": builtins, "json": json})
sys.modules["w9tb_live_tool_call_batches"] = _MOD
_NS = _MOD.__dict__
for _nm, _c in _CODE.items():
    exec(_c, _NS)
_NS["coalesce_tool_call_id"] = lambda child: child.id


def _mk_child(d):
    fn = d.get("function") or {}
    return types.SimpleNamespace(id=d.get("id", ""),
                                 function=types.SimpleNamespace(name=fn.get("name", ""),
                                                               arguments=fn.get("arguments", "{}")))


def drive(rec):
    inp, func = rec["in"], rec["func"]
    if func != "replace":
        raise KeyError(func)
    provider = copy.deepcopy(inp.get("provider_data") or {})
    raw_replacements = inp.get("replacements") or {}
    replacements = {k: [_mk_child(c) for c in v] for k, v in raw_replacements.items()}
    _NS["replace_native_tool_blocks"](provider, replacements)
    return provider


def main():
    n = mism = shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        n += 1
        try:
            out = drive(rec)
        except BaseException as e:
            out = {"__raise__": type(e).__name__}
        got = rec.get("out")
        if got != out:
            mism += 1
            if shown < 10:
                shown += 1
                print(f"MISMATCH {rec.get('func')} case {n}:")
                print(f"  C={json.dumps(got, ensure_ascii=False)[:1200]}")
                print(f"  P={json.dumps(out, ensure_ascii=False)[:1200]}")
    print(f"ORACLE {NAME}: {n} cases, {mism} mismatches")
    sys.exit(1 if (mism or n == 0) else 0)


if __name__ == "__main__":
    main()
