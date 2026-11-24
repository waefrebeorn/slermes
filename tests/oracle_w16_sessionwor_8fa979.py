#!/usr/bin/env python3
"""Differential oracle for w16_sessionwor_8fa979 (gateway/session_worker_child.py).

AST-extracts LIVE _limit + _DEFAULTS and the CHILD_SEPARATOR constant from
persistence/workers/vocabulary.py; the split-based helpers are driven against
that exact constant.
"""
import __future__
import ast
import builtins
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
if not (QUARRY / "gateway" / "session_worker_child.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "gateway" / "session_worker_child.py")
VOCAB = str(QUARRY / "persistence" / "workers" / "vocabulary.py")
NAME = "w16_sessionwor_8fa979"

def extract(path, names):
    tree = ast.parse(Path(path).read_text(encoding="utf-8"))
    flags = __future__.annotations.compiler_flag
    ns = {}
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in names:
            exec(compile(ast.Module(body=[node], type_ignores=[]), path, "exec",
                         flags=flags, dont_inherit=True), ns)
        elif isinstance(node, (ast.Assign, ast.AnnAssign)):
            tg = node.targets[0] if isinstance(node, ast.Assign) else node.target
            if isinstance(tg, ast.Name) and tg.id in names:
                exec(compile(ast.Module(body=[node], type_ignores=[]), path, "exec",
                             flags=flags, dont_inherit=True), ns)
    return ns

NS = {"__builtins__": builtins}
NS.update(extract(SRC, {"_limit", "_DEFAULTS"}))
MOD = types.ModuleType("wlive_workerchild")
MOD.__dict__.update(NS)
sys.modules["wlive_workerchild"] = MOD
NS = MOD.__dict__
SEP = extract(VOCAB, {"CHILD_SEPARATOR"})["CHILD_SEPARATOR"]

if "_limit" not in NS or not isinstance(SEP, str):
    print(f"ORACLE {NAME}: FATAL missing live defs", file=sys.stderr)
    sys.exit(1)

def drive(rec):
    func = rec["func"]
    if func == "limit":
        return NS["_limit"](rec.get("section") or {}, rec.get("key", ""))
    if func == "root_execution_id":
        eid = rec.get("execution_id", "")
        return eid.split(SEP, 1)[0]
    if func == "is_child":
        eid = rec.get("execution_id", "")
        return eid.split(SEP, 1)[0] != eid
    raise KeyError(func)

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
                print(f"  C={json.dumps(got, ensure_ascii=False)[:600]}")
                print(f"  P={json.dumps(out, ensure_ascii=False)[:600]}")
    print(f"ORACLE {NAME}: {n} cases, {mism} mismatches")
    sys.exit(1 if (mism or n == 0) else 0)

if __name__ == "__main__":
    main()
