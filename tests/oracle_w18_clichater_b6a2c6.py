#!/usr/bin/env python3
"""Differential oracle for hermes_cli/cli_chat_error_copy.py (gate w18_clichater_b6a2c6)."""
import ast
import json
import os
import sys
import types

for _s in ("stdin", "stdout", "stderr"):
    try:
        getattr(sys, _s).reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
_pkg = "hermes_cli"
if _pkg not in sys.modules:
    try:
        import hermes_cli  # noqa: F401
    except Exception:
        _p = types.ModuleType(_pkg); _p.__path__ = [os.path.join(QUARRY, "hermes_cli")]
        sys.modules[_pkg] = _p

SRC = os.path.join(QUARRY, "hermes_cli", "cli_chat_error_copy.py")
_want = ("_short", "refusal_text")
_tree = ast.parse(open(SRC, encoding="utf-8").read())
_ns = {"t": lambda key, **k: key, "_SUMMARY_LIMIT": 120}
for _node in _tree.body:
    if isinstance(_node, ast.FunctionDef) and _node.name in _want:
        exec(compile(ast.Module(body=[_node], type_ignores=[]), SRC, "exec"), _ns)


class _Err(Exception):
    def __init__(self, msg, data):
        super().__init__(msg)
        self.data = data


def compute(r):
    func = r.get("func")
    if func == "short":
        lim = r.get("limit", 120)
        return _ns["_short"](r.get("text", ""), None if lim is None else int(lim))
    if func == "refusal_text":
        msg = str(r.get("error", ""))
        if "data" in r and isinstance(r["data"], dict):
            err = _Err(msg, r["data"])
        else:
            err = msg
        return _ns["refusal_text"](err)
    return "__UNKNOWN__"


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    py = compute(r)
    if py == "__UNKNOWN__":
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w18_clichater_b6a2c6: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
