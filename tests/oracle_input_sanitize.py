#!/usr/bin/env python3
"""Oracle for hermes_cli/input_sanitize.py — replay the LIVE 3-fn pipeline.

Harness family (input_sanitize):
  {case, in:<!json|null>, strip:<!json|null>, collapse:<!json|null>, sanitize:<!json|null>}
Replays strip_leaked_bracketed_paste_wrappers / collapse_repeated_input_artifacts /
sanitize_user_prompt_text on the SAME input emitted by the C harness. Seeds re,
ast-extracts the two constants (_BOUNDARY_SUBS compiled patterns, _DESKTOP_PASTE_ARTIFACT)
+ the three fns. The null_input case: live fn receives None (harness emits null),
sanitize short-circuits via `if not isinstance(text,str) or not text: return text`
so it returns None; strip/collapse early-return on falsy -> None. C returns empty
string for NULL input on all three. Align by mapping None mines to "" as the C emits.
"""
import ast
import json
import re
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/hermes_cli/input_sanitize.py"


def main() -> int:
    src = open(SRC, encoding="utf-8").read()
    ns = {"re": re}
    for n in ast.parse(src).body:
        if isinstance(n, ast.Assign) and any(
            isinstance(t, ast.Name) and t.id in ("_BOUNDARY_SUBS", "_DESKTOP_PASTE_ARTIFACT")
            for t in n.targets):
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
        elif isinstance(n, ast.FunctionDef) and n.name in (
                "strip_leaked_bracketed_paste_wrappers",
                "collapse_repeated_input_artifacts",
                "sanitize_user_prompt_text"):
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
    strip = ns["strip_leaked_bracketed_paste_wrappers"]
    collapse = ns["collapse_repeated_input_artifacts"]
    sanitize = ns["sanitize_user_prompt_text"]

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
        print("ORACLE input_sanitize: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    total = 0
    for c in cases:
        total += 1
        q = c.get("in")
        w_strip = strip(q)
        w_coll = collapse(q)
        w_san = sanitize(q)
        # C emits empty string for NULL; live python returns None on None input
        civ = lambda x: (x if x is not None else "")
        ok = (civ(w_strip) == (c.get("strip") or "")
              and civ(w_coll) == (c.get("collapse") or "")
              and civ(w_san) == (c.get("sanitize") or ""))
        if not ok:
            mism += 1
            print(f"MISMATCH {c.get('case')} in={q!r}\n"
                  f"  strip     C={c.get('strip')!r} PY={w_strip!r}\n"
                  f"  collapse  C={c.get('collapse')!r} PY={w_coll!r}\n"
                  f"  sanitize  C={c.get('sanitize')!r} PY={w_san!r}")
    print(f"ORACLE input_sanitize: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
