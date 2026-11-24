#!/usr/bin/env python3
"""Differential oracle for w16_sessionvau_0697e7 (gateway/session_vault_prompt.py).

AST-extracts the LIVE nested callbacks from vault_prompt_scope into a registered
module namespace.  `ask` is a recording scripted seam, so the param dicts are
read from the live functions themselves and save_login is driven by the live
function with a scripted raw response.
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
if not (QUARRY / "gateway" / "session_vault_prompt.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "gateway" / "session_vault_prompt.py")
NAME = "w16_sessionvau_0697e7"

TREE = ast.parse(Path(SRC).read_text(encoding="utf-8"))
FLAGS = __future__.annotations.compiler_flag

def cnode(n):
    return compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec",
                   flags=FLAGS, dont_inherit=True)

NS = types.ModuleType("wlive_vaultprompt")
NS.__dict__.update({"__name__": "wlive_vaultprompt", "__builtins__": builtins, "json": json})
sys.modules["wlive_vaultprompt"] = NS

for node in TREE.body:
    if isinstance(node, ast.Assign):
        tg = node.targets[0]
        if isinstance(tg, ast.Name) and tg.id in {
                "UNLOCK_TIMEOUT_S", "SAVE_LOGIN_TIMEOUT_S", "CODE_TIMEOUT_S"}:
            exec(cnode(node), NS.__dict__)
    if isinstance(node, ast.FunctionDef) and node.name == "vault_prompt_scope":
        for sub in node.body:
            if isinstance(sub, ast.FunctionDef) and sub.name in {
                    "ask", "unlock_prompt", "save_login", "code_prompt"}:
                exec(cnode(sub), NS.__dict__)

_missing = [n for n in ("unlock_prompt", "save_login", "code_prompt",
                        "UNLOCK_TIMEOUT_S", "SAVE_LOGIN_TIMEOUT_S", "CODE_TIMEOUT_S")
            if n not in NS.__dict__]
if _missing:
    print(f"ORACLE {NAME}: FATAL missing live defs: {_missing}", file=sys.stderr)
    sys.exit(1)

_REC = {"params": None, "raw": None}

def ask(method, params, timeout, **kw):
    _REC["params"] = params
    return _REC["raw"]

NS.ask = ask

def drive(rec):
    func = rec["func"]
    if func == "unlock_params":
        _REC["params"] = None
        NS.unlock_prompt(rec.get("backend"), rec.get("display_name"))
        return _REC["params"]
    if func == "save_login_params":
        _REC["params"] = None
        _REC["raw"] = "not-json"   # force the safe path; params are still recorded
        NS.save_login(rec.get("origin"), rec.get("site"))
        return _REC["params"]
    if func == "code_params":
        _REC["params"] = None
        NS.code_prompt(rec.get("site"), rec.get("hint"))
        return _REC["params"]
    if func == "save_login":
        _REC["raw"] = rec.get("raw")
        return NS.save_login(rec.get("origin", ""), rec.get("site", ""))
    if func == "timeouts":
        return {"unlock": NS.UNLOCK_TIMEOUT_S,
                 "save_login": NS.SAVE_LOGIN_TIMEOUT_S,
                 "code": NS.CODE_TIMEOUT_S}
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
