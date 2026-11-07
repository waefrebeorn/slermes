#!/usr/bin/env python3
"""Oracle for tools/file_operations_search.py pure diagnostics helpers.

Harness (t_port_file_operations_search.c) emits per family:
  search_limit:  {t:"search_limit"|"search_limit2", stdout:<str>, reason:<str|null>}
  split_diag:    {t:"split_diag", out:<str>, diag:<str>, pay:<str>}
These are pure regex string-transforms; ast-extract the live fns and hardcode
the two compiled-regex module constants (future-annotations AnnAssign lesson).
N.B. the harness emits JSON (with `null` tokens), so parse with json.loads —
ast.literal_eval rejects `null` and silently under-counts.
"""
import ast
import json
import re
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/tools/file_operations_search.py"
_SEARCH_TIMEOUT_MARKER_RE = re.compile(r"\n?\[Command timed out after \d+s\]\s*$")
_SEARCH_OUTPUT_RE = re.compile(r"^([A-Za-z]:)?[^\s:][^\n]*?[:\-]\d|^[^\s:][^\s]*$")


def main() -> int:
    src = open(SRC, encoding="utf-8").read()
    ns = {
        "re": re,
        "Any": object,
        "_SEARCH_TIMEOUT_MARKER_RE": _SEARCH_TIMEOUT_MARKER_RE,
        "_SEARCH_OUTPUT_RE": _SEARCH_OUTPUT_RE,
    }
    body = [n for n in ast.parse(src).body
            if isinstance(n, ast.FunctionDef) and n.name in
            ("_search_stdout_and_limit", "_split_tool_diagnostics")]
    exec(compile(ast.Module(body=body, type_ignores=[]), SRC, "exec"), ns)

    class ExecuteResult:
        def __init__(self, exit_code, stdout):
            self.exit_code = exit_code
            self.stdout = stdout

    total = 0
    mism = 0
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except ValueError:
            continue
        t = c.get("t")
        if t in ("search_limit", "search_limit2"):
            total += 1
            code = 124 if t == "search_limit" else 0
            in_s = c.get("in") or ""
            want_stdout, want_reason = ns["_search_stdout_and_limit"](
                ExecuteResult(code, in_s))
            got_res, got_reason = c.get("res"), c.get("reason")
            if want_stdout != got_res or want_reason != got_reason:
                mism += 1
                print(f"MISMATCH {t} in={in_s!r}\n  RES C={got_res!r} PY={want_stdout!r} "
                      f"\n  REASON C={got_reason!r} PY={want_reason!r}")
        elif t == "split_diag":
            total += 1
            out_s = c.get("out") or ""
            want_diag, want_pay = ns["_split_tool_diagnostics"](out_s)
            got_diag, got_pay = c.get("diag"), c.get("pay")
            if want_diag != got_diag or want_pay != got_pay:
                mism += 1
                print(f"MISMATCH split_diag out={out_s!r}\n  DIAG C={got_diag!r} PY={want_diag!r}\n  PAY  C={got_pay!r} PY={want_pay!r}")
    if total == 0:
        print("ORACLE file_operations_search: 0 cases (split_diag lacks input)")
        return 2
    print(f"ORACLE file_operations_search: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
