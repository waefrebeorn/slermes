#!/usr/bin/env python3
"""Oracle for tools/cronjob_prompt_scan.py::_first_pattern_error.

Ast-extract the LIVE `_first_pattern_error` (it only needs `re`). The harness
emits `text` + `pats` (list of [pattern, pid]); we call the live function with
the SAME pattern set. Pattern space is the simple ERE/subset the cron scanner
uses, so POSIX REG_ICASE == Python re.IGNORECASE for these literals.
"""
import ast
import json
import re
import sys

SRC = "C:/Users/eman5/hermes-agent/tools/cronjob_prompt_scan.py"


def main() -> int:
    src = open(SRC, encoding="utf-8").read()
    ns = {"re": re}
    for n in ast.parse(src).body:
        if isinstance(n, ast.FunctionDef) and n.name == "_first_pattern_error":
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
    FN = ns["_first_pattern_error"]

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
        print("ORACLE cronjob_prompt_scan: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    total = 0
    for c in cases:
        if c.get("tag") != "fpe":
            continue
        total += 1
        want = FN(c["text"], c["pats"])
        if want != c["out"]:
            mism += 1
            print(f"MISMATCH {c.get('case')} text={c['text']!r}: "
                  f"C={c['out']!r} PY={want!r}")
    print(f"ORACLE cronjob_prompt_scan: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
