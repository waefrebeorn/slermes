#!/usr/bin/env python3
"""Oracle for tools/url_safety.py:_allows_private_ip_resolution.

Harness family (url_safety_helpers):
  {scheme, host, out:int} -> _allows_private_ip_resolution(host, scheme)
pure HTTPS-trust check against the _TRUSTED_PRIVATE_IP_HOSTS frozenset.
"""
import ast
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/tools/url_safety.py"


def main() -> int:
    src = open(SRC, encoding="utf-8").read()
    ns = {}
    for n in ast.parse(src).body:
        if isinstance(n, ast.Assign) and any(
            isinstance(t, ast.Name) and t.id == "_TRUSTED_PRIVATE_IP_HOSTS"
            for t in n.targets):
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
        elif isinstance(n, ast.FunctionDef) and n.name == "_allows_private_ip_resolution":
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
    fn = ns["_allows_private_ip_resolution"]

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
        print("ORACLE url_safety_helpers: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    total = 0
    for c in cases:
        total += 1
        host = c.get("host")
        scheme = c.get("scheme")
        want = fn(host, scheme) if host is not None else False
        if bool(c.get("out")) != want:
            mism += 1
            print(f"MISMATCH host={host!r} scheme={scheme!r} C={c.get('out')} PY={want}")
    print(f"ORACLE url_safety_helpers: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
