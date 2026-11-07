#!/usr/bin/env python3
"""Oracle for tools/file_operations.py pagination + newline-regex helpers
(port src/tools/file_pagination_ops.c):
  read_pagination:   normalize_read_pagination(offset, limit) -> {offset, limit}
  search_pagination: normalize_search_pagination(offset, limit) -> {offset, limit}
  is_line_oriented_error: _is_line_oriented_newline_error(error) -> bool
  pattern_has_regex_newline: _pattern_has_regex_newline(pattern) -> bool
  maybe_warn: _maybe_warn_line_oriented_newline_pattern({total_count,error}, pattern)
               -> {total_count,error,skipped_*,warning}
The harness serializes read/search to JSON and passes (offset, limit) as
"off|lim". Live caps read limit at get_max_lines()==2000 (DEFAULT_MAX_LINES),
which the C port's MAX_LINES=2000 mirrors 1:1. We ast-extract the live fns and
seed their globals (re, Optional) so the oracle is the live code, not a copy."""
import ast
import json
import re

import sys
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")


def main() -> int:
    src = open("C:/Users/eman5/hermes-agent/tools/file_operations_common.py",
               encoding="utf-8").read()
    g = {"Optional": None,
         "DEFAULT_READ_OFFSET": 1, "DEFAULT_READ_LIMIT": 2000,
         "DEFAULT_SEARCH_OFFSET": 0, "DEFAULT_SEARCH_LIMIT": 50}
    # the live body inline-imports get_max_lines() at call time; inject a fake
    # tools.tool_output_limits so the call resolves to the C port's MAX_LINES=2000.
    import sys, types
    tl = types.ModuleType("tools.tool_output_limits")
    tl.get_max_lines = lambda: 2000
    pt = types.ModuleType("tools")
    pt.__path__ = []
    sys.modules.setdefault("tools", pt)
    sys.modules["tools.tool_output_limits"] = tl
    for n in ast.parse(src).body:
        if isinstance(n, ast.FunctionDef) and n.name in (
                "normalize_read_pagination", "normalize_search_pagination", "_coerce_int"):
            exec(compile(ast.Module(body=[n], type_ignores=[]), "foc", "exec"), g)
    norm_read = g["normalize_read_pagination"]
    norm_search = g["normalize_search_pagination"]

    src2 = open("C:/Users/eman5/hermes-agent/tools/file_operations_search.py",
                encoding="utf-8").read()
    g2 = {"re": re, "Optional": None}
    for n in ast.parse(src2).body:
        if isinstance(n, ast.FunctionDef) and n.name in (
                "_pattern_has_regex_newline", "_is_line_oriented_newline_error",
                "_maybe_warn_line_oriented_newline_pattern", "_REGEX_NEWLINE_ESCAPE_RE"):
            exec(compile(ast.Module(body=[n], type_ignores=[]), "fos", "exec"), g2)
    # seed regex constant if it's a module-level assignment, else find in module
    has_re = False
    for n in ast.parse(src2).body:
        if isinstance(n, ast.Assign):
            for t in n.targets:
                if isinstance(t, ast.Name) and t.id == "_REGEX_NEWLINE_ESCAPE_RE":
                    exec(compile(ast.Module(body=[n], type_ignores=[]), "fos", "exec"), g2); has_re = True
    if not has_re:
        m = re.search(r'_REGEX_NEWLINE_ESCAPE_RE\s*=\s*(.*)', src2)
        if m:
            g2["_REGEX_NEWLINE_ESCAPE_RE"] = re.compile(eval(m.group(1).strip(), {"re": re}))
    pat_re = g2["_pattern_has_regex_newline"]
    is_lineerr = g2["_is_line_oriented_newline_error"]
    maybe_warn = g2["_maybe_warn_line_oriented_newline_pattern"]

    # get_max_lines() == 2000 (DEFAULT_MAX_LINES); normalize_read uses it in body via
    # inline import -> handled by the injected fake tools.tool_output_limits above. This
    # __globals__ patch is a belt-and-suspenders no-op for the non-import path.
    if hasattr(norm_read, "__globals__"):
        norm_read.__globals__.setdefault("get_max_lines", lambda: 2000)

    total, mism = 0, 0
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except ValueError:
            continue
        total += 1
        fn = c.get("fn")
        if fn == "read_pagination":
            off, lim, _ = map(int, c["in"].split("|"))
            oc, lc = norm_read(off, lim)
            want = {"offset": oc, "limit": lc}
            if want != json.loads(c.get("out")):
                mism += 1
                print(f"MISMATCH read_pagination {c['in']} C={c.get('out')} PY={want}")
        elif fn == "search_pagination":
            off, lim, _ = map(int, c["in"].split("|"))
            oc, lc = norm_search(off, lim)
            want = {"offset": oc, "limit": lc}
            if want != json.loads(c.get("out")):
                mism += 1
                print(f"MISMATCH search_pagination {c['in']} C={c.get('out')} PY={want}")
        elif fn == "is_line_oriented_error":
            want = is_lineerr(c.get("in"))
            if want != (c.get("out") == "true"):
                mism += 1
                print(f"MISMATCH is_line_oriented_error {c.get('in')!r} C={c.get('out')} PY={want}")
        elif fn == "pattern_has_regex_newline":
            want = pat_re(c.get("in"))
            if want != (c.get("out") == "true"):
                mism += 1
                print(f"MISMATCH pattern_has_regex_newline {c.get('in')!r} C={c.get('out')} PY={want}")
        elif fn == "maybe_warn":
            m = re.match(r"tc=(\d+)\|(\w+)\|(.*)", c.get("in"))
            tc, errkind, pat = int(m.group(1)), m.group(2), m.group(3)
            err = ("literal \"\\n\" is not allowed in --multiline mode: use -U"
                   if errkind == "lineerr" else "some other error")
            res = _SR({"total_count": tc, "error": err, "skipped_paths": [], "skipped_omitted": 0})
            out = maybe_warn(res, pat)
            want = {"total_count": out.total_count, "error": out.error}
            if out.warning is not None:
                want["warning"] = out.warning
            if want != json.loads(c.get("out")):
                mism += 1
                print(f"MISMATCH maybe_warn {c.get('in')} C={c.get('out')} PY={want}")
    if total == 0:
        print("ORACLE file_pagination_ops: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE file_pagination_ops: {total} cases, {mism} mismatches")
    return 1 if mism else 0


class _SR:
    """thin SearchResult stand-in with the fields _maybe_warn touches."""
    def __init__(self, d: dict):
        self.total_count = d.get("total_count")
        self.error = d.get("error")
        self.skipped_paths = d.get("skipped_paths", [])
        self.skipped_omitted = d.get("skipped_omitted", 0)
        self.warning = None


if __name__ == "__main__":
    sys.exit(main())
