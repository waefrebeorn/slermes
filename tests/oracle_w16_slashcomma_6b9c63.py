#!/usr/bin/env python3
"""Differential oracle for w16_slashcomma_6b9c63 (gateway/slash_commands_login.py).

AST-extracts the LIVE _LOGIN_BLOCKED_PLATFORMS constant. Drives the exact
paired_dm / attempt-key prefix of _handle_login_command against that live set.
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
if not (QUARRY / "gateway" / "slash_commands_login.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "gateway" / "slash_commands_login.py")
NAME = "w16_slashcomma_6b9c63"

TREE = ast.parse(Path(SRC).read_text(encoding="utf-8"))
FLAGS = __future__.annotations.compiler_flag
NS = {"__builtins__": builtins}
for node in TREE.body:
    if isinstance(node, (ast.Assign, ast.AnnAssign)):
        tg = node.targets[0] if isinstance(node, ast.Assign) else node.target
        if isinstance(tg, ast.Name) and tg.id == "_LOGIN_BLOCKED_PLATFORMS":
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec",
                         flags=FLAGS, dont_inherit=True), NS)
MOD = types.ModuleType("wlive_slashlogin")
MOD.__dict__.update(NS)
sys.modules["wlive_slashlogin"] = MOD
BLOCKED = MOD._LOGIN_BLOCKED_PLATFORMS

def pval(platform):
    if isinstance(platform, dict) and "value" in platform:
        return platform["value"]
    return platform

def drive(rec):
    func = rec["func"]
    platform, cid, uid = rec.get("platform"), rec.get("chat_id"), rec.get("user_id")
    pv = str(pval(platform))
    if func == "paired_dm":
        pl = pv.lower()
        return (rec.get("chat_type") in {"dm", "private"} and bool(cid) and pl not in BLOCKED)
    if func == "attempt_key":
        return [pv, str(cid), str(uid or "")]
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
