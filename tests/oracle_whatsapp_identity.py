#!/usr/bin/env python3
"""Oracle for gateway/whatsapp_identity.py:to_whatsapp_jid.

Consumes t_port_whatsapp_identity.c lines {"in": str, "out": str}. Ast-extracts
the LIVE to_whatsapp_jid + _BARE_PHONE_RE and replays on the emitted input.
"""
import ast
import json
import re
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/gateway/whatsapp_identity.py"
ns = {"str": str, "re": re}
src = open(SRC, encoding="utf-8").read()
for n in ast.parse(src).body:
    if isinstance(n, ast.Assign) and any(getattr(x, "id", "") == "_BARE_PHONE_RE" for x in n.targets):
        exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
    if isinstance(n, ast.FunctionDef) and n.name == "to_whatsapp_jid":
        exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
live = ns["to_whatsapp_jid"]

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    py = live(r["in"])
    cases += 1
    if py != r["out"]:
        mism += 1
        print(f"MISMATCH in={r['in']!r} py={py!r} c={r['out']!r}", file=sys.stderr)
print(f"ORACLE whatsapp_identity: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
