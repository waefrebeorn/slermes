#!/usr/bin/env python3
"""Oracle for agent/ssl_verify.py:_coerce_insecure.

Harness family (agent_ssl_verify):
  coerce {t, in:<str>, out:int} -> _coerce_insecure(in)
The resolve_httpx_verify family is NOT oracleable here: it installs the platform
truststore (ssl.SSLContext, file existence) — genuinely runtime-coupled. Only the
pure coercion predicate is verified (its 5 harness cases).
"""
import ast
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/agent/ssl_verify.py"


def main() -> int:
    src = open(SRC, encoding="utf-8").read()
    ns = {}
    for n in ast.parse(src).body:
        if isinstance(n, ast.FunctionDef) and n.name == "_coerce_insecure":
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
    coerce = ns["_coerce_insecure"]

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
        print("ORACLE agent_ssl_verify: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    total = 0
    for c in cases:
        if c.get("t") != "coerce":
            continue  # resolve_httpx_verify runtime-coupled -> skip
        total += 1
        want = coerce(c.get("in"))
        if bool(c.get("out")) != want:
            mism += 1
            print(f"MISMATCH coerce in={c.get('in')!r} C={c.get('out')} PY={want}")
    print(f"ORACLE agent_ssl_verify: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
