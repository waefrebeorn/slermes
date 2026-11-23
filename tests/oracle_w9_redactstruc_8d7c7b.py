#!/usr/bin/env python3
"""Differential oracle for gate w9_redactstruc_8d7c7b (agent/redact_structured.py).

AST-extracts redact_structured and its three helpers from the quarry. The external
redaction engine (agent.redact.redact_sensitive_text) is replaced by a deterministic
stand-in (SECRET -> ***) that the C port mirrors exactly. Everything else -- the
soft walk, the 4-pass fixed point in _redact_leaf, the json.dumps/json.loads lift,
_settled and the fail-closed hard-walk/sentinel fallbacks -- is the LIVE code.
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
if not (QUARRY / "agent" / "redact_structured.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
sys.path.insert(0, str(QUARRY))
SRC = str(QUARRY / "agent/redact_structured.py")
NAME = "w9_redactstruc_8d7c7b"
PORTED = ["redact_structured", "_walk", "_redact_leaf", "_settled"]

_TREE = ast.parse(Path(SRC).read_text(encoding="utf-8"))
_FLAGS = __future__.annotations.compiler_flag
_CODE = {}
for _n in _TREE.body:
    if isinstance(_n, ast.FunctionDef) and _n.name in PORTED:
        _CODE[_n.name] = compile(ast.Module(body=[_n], type_ignores=[]), SRC, "exec",
                                 flags=_FLAGS, dont_inherit=True)
if set(_CODE) != set(PORTED):
    print(f"ORACLE {NAME}: FATAL missing live defs: {sorted(set(PORTED) - set(_CODE))}", file=sys.stderr)
    sys.exit(1)

_MOD = types.ModuleType("w9rs_live_redact_structured")
_MOD.__dict__.update({"__name__": "w9rs_live_redact_structured", "__builtins__": builtins,
                      "json": json, "Any": object,
                      "_MASK": "***", "_SENTINEL": "[redaction-unverified]",
                      "_redact": lambda text: text.replace("SECRET", "***")})
sys.modules["w9rs_live_redact_structured"] = _MOD
_NS = _MOD.__dict__
for _nm, _c in _CODE.items():
    exec(_c, _NS)


def drive(rec):
    if rec["func"] != "redact":
        raise KeyError(rec["func"])
    return _NS["redact_structured"](copy.deepcopy(rec["in"].get("payload")))


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
                print(f"  C={json.dumps(got, ensure_ascii=False)[:900]}")
                print(f"  P={json.dumps(out, ensure_ascii=False)[:900]}")
    print(f"ORACLE {NAME}: {n} cases, {mism} mismatches")
    sys.exit(1 if (mism or n == 0) else 0)


if __name__ == "__main__":
    main()
