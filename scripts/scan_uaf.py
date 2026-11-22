#!/usr/bin/env python3
"""Heuristic scanner: pointer borrowed from a json_t used after that json_t is freed.

This pattern caused five full-suite failures on 2026-11-22. Reports candidate
locations; every report must still be verified by reading the code and running
the owning gate.
"""
import os
import re
import sys

root = sys.argv[1] if len(sys.argv) > 1 else "src"
assign = re.compile(r'\bconst\s+char\s*\*\s*(\w+)\s*=\s*([^;]+);')
use = re.compile(r'\b%s\b')
found = []
for dp, _, fs in os.walk(root):
    for fn in fs:
        if not fn.endswith(".c"):
            continue
        p = os.path.join(dp, fn)
        lines = open(p, encoding="utf-8", errors="replace").read().splitlines()
        for i, line in enumerate(lines):
            m = assign.search(line)
            if not m:
                continue
            var, expr = m.group(1), m.group(2)
            if not re.search(r'json_(obj_get|get|arr_str)|_[a-z0-9]+_arr_str|_str_or', expr):
                continue
            # Candidate roots: identifiers passed to the accessor.
            roots = set(re.findall(r'\b(\w+)\b', expr)) - {
                var, "json_obj_get", "json_get", "json_arr_str", "NULL", "str_or"}
            for root_name in roots:
                free_re = re.compile(r'json_free\s*\(\s*' + re.escape(root_name) + r'\s*\)')
                free_line = next((j for j in range(i + 1, min(i + 20, len(lines)))
                                  if free_re.search(lines[j])), None)
                if free_line is None:
                    continue
                later = "\n".join(lines[free_line + 1:free_line + 12])
                if not re.search(r'\b' + re.escape(var) + r'\b', later):
                    continue
                # A copy made before the free makes the later use safe.
                between = "\n".join(lines[i + 1:free_line + 1])
                copy_re = re.compile(r'\b(strdup|dup_str|[^\s]*str_or_empty|[^\s]*_pystr)\s*\([^)]*\b'
                                     + re.escape(var) + r'\b')
                if copy_re.search(between):
                    continue
                # Only report a use that consumes the borrowed pointer directly.
                use_re = re.compile(r'(json_string|snprintf|json_set|[^\s]*_set_s|[^\s]*_set_copy)\s*\([^;]*\b'
                                    + re.escape(var) + r'\b')
                if use_re.search(later):
                    found.append(f"{p}:{i+1} {var} borrowed from {root_name}, freed at {free_line+1}, consumed later")
                    break
for f in found:
    print(f)
print(f"candidates: {len(found)}")
sys.exit(1 if found else 0)
