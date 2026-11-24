#!/usr/bin/env python3
"""Differential oracle for w16_sessionvie_e7b625 (gateway/session_viewer_close.py).

AST-extracts the LIVE _seconds, _section, _PendingClose class and the two
frozensets into a registered module namespace, then drives them directly.
"""
import __future__
import ast
import builtins
import contextlib
import json
import sys
import types
import uuid
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
if not (QUARRY / "gateway" / "session_viewer_close.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "gateway" / "session_viewer_close.py")
NAME = "w16_sessionvie_e7b625"

TREE = ast.parse(Path(SRC).read_text(encoding="utf-8"))
FLAGS = __future__.annotations.compiler_flag

def cnode(n):
    return compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec",
                   flags=FLAGS, dont_inherit=True)

NS = types.ModuleType("wlive_viewerclose")
NS.__dict__.update({"__name__": "wlive_viewerclose", "__builtins__": builtins,
                    "json": json, "contextlib": contextlib, "uuid": uuid})
sys.modules["wlive_viewerclose"] = NS

for node in TREE.body:
    if isinstance(node, ast.FunctionDef) and node.name in {"_seconds", "_section"}:
        exec(cnode(node), NS.__dict__)
    elif isinstance(node, ast.ClassDef) and node.name == "_PendingClose":
        exec(cnode(node), NS.__dict__)
    elif isinstance(node, (ast.Assign, ast.AnnAssign)):
        tg = node.targets[0] if isinstance(node, ast.Assign) else node.target
        if isinstance(tg, ast.Name) and tg.id in {"_RESET_REASONS", "ORPHAN_REASONS", "STARTUP_IDLE_S"}:
            exec(cnode(node), NS.__dict__)

_missing = [n for n in ("_seconds", "_section", "_PendingClose",
                        "_RESET_REASONS", "ORPHAN_REASONS") if n not in NS.__dict__]
if _missing:
    print(f"ORACLE {NAME}: FATAL missing live defs: {_missing}", file=sys.stderr)
    sys.exit(1)

def drive(rec):
    func = rec["func"]
    if func == "seconds":
        return NS._seconds(rec.get("raw"), rec.get("default_value", 0))
    if func == "section":
        return NS._section(rec.get("config") or {}, rec.get("key", ""))
    if func == "pending_reason":
        obj = NS._PendingClose.from_record(rec.get("record"))
        return obj.reason
    if func == "is_reset_reason":
        return rec.get("reason") in NS._RESET_REASONS
    if func == "is_orphan_reason":
        return rec.get("reason") in NS.ORPHAN_REASONS
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
