#!/usr/bin/env python3
"""Oracle for tools/xai_http.py:_coerce_expires_after (port
src/cli/port_tools_xai_http.c). Ast-extract the live fn and seed the SAFE/MAX
seconds constants it references (its body reads them as globals).

Harness emits {fn:"coerce_expires", in:<str|null>, out:<str|null>} where out is the
C's "null" or a decimal — the oracle must map live's None -> "null" and live ints
to the same decimal string.
"""
import ast
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/tools/xai_http.py"
MAX_XAI_STORAGE_EXPIRES_AFTER_SECONDS = 30 * 24 * 60 * 60   # 2592000
SAFE_XAI_STORAGE_EXPIRES_AFTER_SECONDS = 2 * 24 * 60 * 60   # 172800


def main() -> int:
    src = open(SRC, encoding="utf-8").read()
    ns = {"MAX_XAI_STORAGE_EXPIRES_AFTER_SECONDS": MAX_XAI_STORAGE_EXPIRES_AFTER_SECONDS,
          "SAFE_XAI_STORAGE_EXPIRES_AFTER_SECONDS": SAFE_XAI_STORAGE_EXPIRES_AFTER_SECONDS,
          "Any": object, "Optional": object}
    body = [n for n in ast.parse(src).body
            if isinstance(n, ast.FunctionDef) and n.name == "_coerce_expires_after"]
    exec(compile(ast.Module(body=body, type_ignores=[]), SRC, "exec"), ns)
    fn = ns["_coerce_expires_after"]

    total = 0
    mism = 0
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            import json
            c = json.loads(line)
        except ValueError:
            continue
        total += 1
        want_raw = fn(c.get("in"))
        want = "null" if want_raw is None else str(int(want_raw))
        got = c.get("out")
        if want != got:
            mism += 1
            print(f"MISMATCH in={c.get('in')!r} C={got!r} PY={want!r}")
    if total == 0:
        print("ORACLE xai_expires: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE xai_expires: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
