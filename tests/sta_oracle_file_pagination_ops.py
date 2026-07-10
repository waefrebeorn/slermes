#!/usr/bin/env python3
"""Oracle for v554 file_pagination_ops extraction: C == LIVE Python.

Reads JSON lines from tests/t_port_file_pagination_ops.c and asserts C output
equals what LIVE tools/file_operations.py produces.

  gcc -O2 -g -I include -I src/tools -I src/agent -I lib/libjson \
      tests/t_port_file_pagination_ops.c src/tools/file_pagination_ops.o \
      lib/libjson/json.o -o /tmp/t_pg
  /tmp/t_pg | python3 tests/sta_oracle_file_pagination_ops.py
"""
import json, sys, os, re
REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, REPO)
import tools.file_operations as fo

mismatches = 0
total = 0

def parse_offlim(out):
    """Parse C JSON '{\"offset\":N,\"limit\":N}' -> (offset, limit)."""
    obj = json.loads(out)
    return obj["offset"], obj["limit"]

for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    rec = json.loads(line)
    fn = rec["fn"]
    total += 1

    if fn == "read_pagination":
        offset, limit, default = (int(x) for x in rec["in"].split("|"))
        # Python: returns (offset, limit) tuple with faithful clamping
        exp_off, exp_lim = fo.normalize_read_pagination(offset, limit)
        got_off, got_lim = parse_offlim(rec["out"])
        if (got_off, got_lim) != (exp_off, exp_lim):
            mismatches += 1
            print(f"MISMATCH {fn} {rec['in']} C=({got_off},{got_lim}) PY=({exp_off},{exp_lim})")

    elif fn == "search_pagination":
        offset, limit, default = (int(x) for x in rec["in"].split("|"))
        exp_off, exp_lim = fo.normalize_search_pagination(offset, limit)
        got_off, got_lim = parse_offlim(rec["out"])
        if (got_off, got_lim) != (exp_off, exp_lim):
            mismatches += 1
            print(f"MISMATCH {fn} {rec['in']} C=({got_off},{got_lim}) PY=({exp_off},{exp_lim})")

    elif fn == "is_line_oriented_error":
        err = rec.get("in")
        exp = fo._is_line_oriented_newline_error(err) if err else False
        got = (rec["out"] == "true")
        if got != exp:
            mismatches += 1
            print(f"MISMATCH {fn} {err!r} C={got} PY={exp}")

    elif fn == "pattern_has_regex_newline":
        pat = rec["in"]
        exp = fo._pattern_has_regex_newline(pat) if pat else False
        got = (rec["out"] == "true")
        if got != exp:
            mismatches += 1
            print(f"MISMATCH {fn} {pat!r} C={got} PY={exp}")

    elif fn == "maybe_warn":
        # in = "tc=N|errkind|PATTERN"  ; out = serialized result JSON
        parts = rec["in"].split("|", 2)
        tc = int(parts[0].split("=")[1])
        errkind = parts[1]
        pat = parts[2]
        if errkind == "lineerr":
            err = "literal \"\\n\" is not allowed in --multiline mode: use -U"
        elif errkind == "othererr":
            err = "some other error"
        else:
            err = None
        # Build a Python SearchResult-like object (total_count, error, warning)
        class R:
            def __init__(s): s.total_count = tc; s.error = err; s.warning = None
        res = fo._maybe_warn_line_oriented_newline_pattern(R(), pat)
        exp_warn = res.warning is not None
        exp_err_cleared = (res.error is None)
        got = json.loads(rec["out"])
        got_warn = "warning" in got and got["warning"] is not None
        got_err_cleared = (got.get("error") is None)
        if (got_warn, got_err_cleared) != (exp_warn, exp_err_cleared):
            mismatches += 1
            print(f"MISMATCH {fn} {rec['in']} C=(warn={got_warn},errcleared={got_err_cleared}) "
                  f"PY=(warn={exp_warn},errcleared={exp_err_cleared})")

print(f"{total} cases, {mismatches} mismatches")
sys.exit(1 if mismatches else 0)
