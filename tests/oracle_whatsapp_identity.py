#!/usr/bin/env python3
"""Oracle for whatsapp_identity (backfilled).
  gateway/whatsapp_identity.py:normalize_whatsapp_identifier
  gateway/whatsapp_identity.py:to_whatsapp_jid
  gateway/whatsapp_identity.py:normalize_whatsapp_mention_jid
"""
import ast
import json
import sys


SRC_NORMALIZEWHATSAPPIDENTIFIER = "C:/Users/eman5/hermes-agent/gateway/whatsapp_identity.py"
SRC_TOWHATSAPPJID = "C:/Users/eman5/hermes-agent/gateway/whatsapp_identity.py"
SRC_NORMALIZEWHATSAPPMENTIONJID = "C:/Users/eman5/hermes-agent/gateway/whatsapp_identity.py"

def _load_normalizewhatsappidentifier():
    src = open(SRC_NORMALIZEWHATSAPPIDENTIFIER, encoding="utf-8").read()
    ns = {}  # TODO(seed): ns["Path"]=Path; ns["math"]=...; logger no-op; runtime fake
    for n in ast.parse(src).body:
        if isinstance(n, ast.FunctionDef) and n.name == "normalize_whatsapp_identifier":
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC_NORMALIZEWHATSAPPIDENTIFIER, "exec"), ns)
    return ns["normalize_whatsapp_identifier"]

def _load_towhatsappjid():
    src = open(SRC_TOWHATSAPPJID, encoding="utf-8").read()
    ns = {"re": __import__("re")}  # seed _BARE_PHONE_RE
    for n in ast.parse(src).body:
        if isinstance(n, ast.Assign) and any(isinstance(t, ast.Name) and t.id == "_BARE_PHONE_RE" for t in n.targets):
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC_TOWHATSAPPJID, "exec"), ns)
        if isinstance(n, ast.FunctionDef) and n.name == "to_whatsapp_jid":
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC_TOWHATSAPPJID, "exec"), ns)
    return ns["to_whatsapp_jid"]

def _load_normalizewhatsappmentionjid():
    src = open(SRC_NORMALIZEWHATSAPPMENTIONJID, encoding="utf-8").read()
    ns = {}  # TODO(seed): ns["Path"]=Path; ns["math"]=...; logger no-op; runtime fake
    for n in ast.parse(src).body:
        if isinstance(n, ast.FunctionDef) and n.name == "normalize_whatsapp_mention_jid":
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC_NORMALIZEWHATSAPPMENTIONJID, "exec"), ns)
    return ns["normalize_whatsapp_mention_jid"]

def main() -> int:
    FN = {
        "normalize_whatsapp_identifier": _load_normalizewhatsappidentifier(),
        "to_whatsapp_jid": _load_towhatsappjid(),
        "normalize_whatsapp_mention_jid": _load_normalizewhatsappmentionjid(),
    }

    cases = []
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            cases.append(json.loads(line))
        except Exception:
            pass
    if not cases:
        print("ORACLE whatsapp_identity: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    total = 0
    for c in cases:
        tag = c.get("tag") or c.get("case", "?")
        total += 1
        # TODO(bind): map harness fields to the live python args below.
        #   cur = FN["to_whatsapp_jid"](c["in"])  and compare to c["out"]
        want = FN["to_whatsapp_jid"](c["in"])
        want2 = want if isinstance(want, (str, bool, int, float)) or want is None else json.dumps(want)
        if c.get("out") != want2:
            mism += 1
            print(f"MISMATCH {c.get(chr(39)+'case'+chr(39))} C={c.get(chr(39)+'out'+chr(39))!r} PY={want2!r}")
    print(f"ORACLE whatsapp_identity: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
