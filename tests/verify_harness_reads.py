#!/usr/bin/env python3
"""Guard against unterminated heap buffers in oracle harnesses.

The plugin_prompt_sections harness read a fixture with fread() into a realloc'd
buffer and passed it straight to json_parse() without writing a trailing NUL.
realloc memory is uninitialised, so the parser read past `len` and the harness
intermittently returned 2 with no output -- reported as "C produced no output"
rather than a harness bug. Two harnesses had this exact pattern; this gate keeps
it from coming back. strtok/strtok_r users are exempt: they write NUL delimiters.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TESTS = ROOT / "tests"

BAD = []
for p in sorted(TESTS.glob("t_port_*.c")):
    t = p.read_text(encoding="utf-8", errors="replace")
    if "fread(" not in t or "realloc" not in t:
        continue
    # Only when the buffer is passed to a NUL-terminated string parser.
    if not re.search(r"(json_parse|sscanf|strcmp|strstr|strdup|atol|strtol)\s*\(", t):
        continue
    # strtok/strtok_r terminate each token in-place.
    if "strtok" in t:
        continue
    # Any NUL write at the buffer end is sufficient.
    if re.search(r"\w+\s*\[\s*(len|n|r|total|read)\w*\s*\]\s*=\s*(['\"]\\?0['\"]|0\b)", t):
        continue
    BAD.append(p.name)

if BAD:
    print("harness-read gate: heap fread buffer used as a C string without a NUL write:")
    for name in BAD:
        print("   ", name)
    sys.exit(1)
print("harness-read gate: PASS (all heap-read harnesses NUL-terminate)")
