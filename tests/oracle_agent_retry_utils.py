#!/usr/bin/env python3
"""Oracle for agent/retry_utils.py pure overload classifier.

Harness family (agent_retry_utils), emitted per line:
  {status:int, base:<str>, model:<str>, text:<str>, out:int}
Replays the live is_zai_coding_overload_error on a reconstructed error carrying
the SAME observable facts: status_code=status, and str(error)==text so the
live _error_text flattens to the same lowercased text (the C e.text is exactly
that flattened field). Ast-extract _error_text + is_zai_coding_overload_error.
"""
import ast
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/agent/retry_utils.py"

_ZAI = "https://api.z.ai/api/coding/paas/v4"
_INTERRUPTED = "[interrupt: no assistant output for this turn]"


def main() -> int:
    src = open(SRC, encoding="utf-8").read()
    ns = {}
    for n in ast.parse(src).body:
        if isinstance(n, ast.FunctionDef) and n.name in (
                "_error_text", "is_zai_coding_overload_error"):
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
    _error_text = ns["_error_text"]
    is_zai = ns["is_zai_coding_overload_error"]

    class FakeError:
        def __init__(self, status, text):
            self.status_code = status
            self.message = None
            self.body = None
            self.response = None
            self._t = text
        def __str__(self):
            return self._t

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
        print("ORACLE agent_retry_utils: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    total = 0
    for c in cases:
        total += 1
        err = FakeError(c.get("status", 0), c.get("text", ""))
        want = is_zai(base_url=c.get("base", ""), model=c.get("model", ""), error=err)
        if bool(c.get("out")) != want:
            mism += 1
            print(f"MISMATCH status={c.get('status')} base={c.get('base')!r} "
                  f"model={c.get('model')!r} text={c.get('text')!r} "
                  f"C={c.get('out')} PY={want}")
    print(f"ORACLE agent_retry_utils: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
