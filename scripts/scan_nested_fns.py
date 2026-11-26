#!/usr/bin/env python3
"""Functions defined inside other functions — the signature of a body that was lost in an edit.

GCC accepts a nested function definition as an extension, so the file compiles, the inner functions
never become global, and every caller binds to a stub (or to nothing). src/agent/turn_retry_state.c
had two getters/setters nested inside `turn_retry_state_get_image_shrink_retry_attempted`, whose
`return` statement had been left behind its two neighbours; the object exported neither name.

    python scripts/scan_nested_fns.py [--json out.json]

Reports `path:line: name` for every definition that starts at brace depth >= 1. Depth is counted on
comment- and string-stripped text so braces in literals do not move the counter.
"""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]

_COMMENT_RE = re.compile(r"/\*.*?\*/|//[^\n]*", re.S)
_STRING_RE = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'')
_DEF_RE = re.compile(r"^[A-Za-z_][\w \t\*]*?[\s\*](\w+)\s*\([^;{]*\)\s*\{", re.M)
_KEYWORDS = {"if", "for", "while", "switch", "return", "sizeof", "else", "do"}


def scan(path: Path) -> list[tuple[int, str]]:
    text = path.read_text(encoding="utf-8", errors="replace")
    text = _COMMENT_RE.sub(lambda m: "\n" * m.group(0).count("\n"), text)
    text = _STRING_RE.sub('""', text)
    depth_at = {}
    depth = 0
    for i, ch in enumerate(text):
        if ch == "{":
            depth_at[i] = depth
            depth += 1
        elif ch == "}":
            depth = max(0, depth - 1)
            depth_at[i] = depth
    hits = []
    for m in _DEF_RE.finditer(text):
        name = m.group(1)
        if name in _KEYWORDS:
            continue
        brace = text.index("{", m.end() - 1)
        if depth_at.get(brace, 0) > 0:
            line = text.count("\n", 0, m.start()) + 1
            hits.append((line, name))
    return hits


def main() -> int:
    want_json = None
    if "--json" in sys.argv:
        want_json = Path(sys.argv[sys.argv.index("--json") + 1])
    files = sorted((REPO / "src").rglob("*.c"))
    found = {}
    for f in files:
        hits = scan(f)
        if hits:
            found[str(f.relative_to(REPO))] = hits
    print(f"scanned {len(files)} file(s); {len(found)} with nested definitions")
    for path, hits in list(found.items())[:15]:
        for line, name in hits[:4]:
            print(f"  {path}:{line}: {name}")
    if want_json:
        want_json.write_text(json.dumps(found, indent=1), encoding="utf-8")
        print(f"wrote {want_json}")
    return 1 if found else 0


if __name__ == "__main__":
    sys.exit(main())
