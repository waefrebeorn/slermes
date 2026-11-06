#!/usr/bin/env python3
"""Oracle: gateway/platforms/helpers.py PURE table helpers vs LIVE Python.

Ast-extracts the four pure helpers from the live module so no heavy imports
(runtime, utils, platform adapters) are needed:
  is_table_row                          {t, in, out:int}
  split_markdown_table_row              {t, in, cells:[...]}
  convert_table_to_bullets              {t, in, out}
  (convert_notable is the same fn, skips early when no table)
Only the functions + the inline regex/globals they reference are exec'd.
"""
import ast
import logging
import re
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/gateway/platforms/helpers.py"
MT = "C:/Users/eman5/hermes-agent/agent/markdown_tables.py"
FUNS = ("is_table_row", "split_markdown_table_row", "_render_table_block",
        "convert_table_to_bullets")


class _QuietLogger(logging.Logger):
    def _log(self, *a, **k):
        pass


_quiet = _QuietLogger("gh_oracle")
_quiet.addHandler(logging.NullHandler())


def main() -> int:
    src = open(SRC, encoding="utf-8").read()
    ns = {"re": re, "logging": logging, "logger": _quiet}
    for n in ast.parse(src).body:
        if isinstance(n, (ast.Assign, ast.AnnAssign)):
            targets = n.targets if isinstance(n, ast.Assign) else (n.target,)
            rhs = n.value if isinstance(n, ast.Assign) else n.value
            def _allowed(rhs):
                # static scalar/container, or a bare re.compile/match call (pure)
                if isinstance(rhs, (ast.Constant, ast.List, ast.Tuple,
                                    ast.Dict, ast.Set, ast.BinOp)):
                    return True
                if isinstance(rhs, ast.Call) and isinstance(rhs.func, ast.Attribute) \
                        and isinstance(rhs.func.value, ast.Name) \
                        and rhs.func.value.id == "re" \
                        and rhs.func.attr in ("compile", "match"):
                    return True
                return False
            if _allowed(rhs) and any(isinstance(t, ast.Name) for t in targets):
                exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
        elif isinstance(n, ast.FunctionDef) and n.name in FUNS:
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
    # split_markdown_table_row inline-imports agent.markdown_tables.split_table_row;
    # ast-extract it and register a fake module so the import resolves.
    mt = {}
    for n in ast.parse(open(MT, encoding="utf-8").read()).body:
        if isinstance(n, ast.FunctionDef) and n.name == "split_table_row":
            exec(compile(ast.Module(body=[n], type_ignores=[]), MT, "exec"), mt)
    _pkg = type(sys)("agent")
    _pkg.__path__ = []
    _sub = type(sys)("agent.markdown_tables")
    _sub.split_table_row = mt["split_table_row"]
    sys.modules["agent"] = _pkg
    sys.modules["agent.markdown_tables"] = _sub
    is_table_row = ns["is_table_row"]
    split_row = ns["split_markdown_table_row"]
    convert = ns["convert_table_to_bullets"]

    mism = 0
    total = 0
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        b = json_loads(line)
        if b is None:
            continue
        t = b.get("t")
        total += 1
        if t == "is_table_row":
            want = bool(is_table_row(b.get("in")))
            if want != bool(b.get("out")):
                mism += 1
                print(f"MISMATCH is_table_row {b.get('in')!r} C={b.get('out')} PY={want}")
        elif t == "split_row":
            want = list(split_row(b.get("in")))
            if want != b.get("cells"):
                mism += 1
                print(f"MISMATCH split_row {b.get('in')!r} C={b.get('cells')} PY={want}")
        elif t in ("convert", "convert_notable"):
            want = (convert(b.get("in")) or "").replace("\r\n", "\n").rstrip("\n")
            got = (b.get("out") or "").replace("\\n", "\n").rstrip("\n")
            if want != got:
                mism += 1
                print(f"MISMATCH {t} C={got!r} PY={want!r}")
    print(f"ORACLE gateway_platforms_helpers: {total} cases, {mism} mismatches")
    return 1 if mism else 0


def json_loads(s):
    import json
    try:
        return json.loads(s)
    except Exception:
        return None


if __name__ == "__main__":
    sys.exit(main())
