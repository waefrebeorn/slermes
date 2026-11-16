#!/usr/bin/env python3
"""Hoist `import os` above its first use in the oracle scripts.

scripts/fix_oracle_devroot.py repointed 87 oracles at SLERMES_DEV_ROOT, which
made them reference os.environ at module scope. Its import insertion appended
`import os` after the LAST module-level import -- but in most of these files the
first use is at line 4, before that. Python executes top to bottom, so the
result was `NameError: name 'os' is not defined` for every one of them, and the
ports reported "oracle produced no output" (an absent measurement) rather than
anything about parity.

Rule: place `import os` immediately after the module docstring, before any
statement. Idempotent; syntax-checked.
"""
import ast
import sys
from pathlib import Path

REPO = Path("C:/Users/eman5/slermes")


def docstring_end(lines):
    if not lines:
        return 0
    first = lines[0].lstrip()
    for q in ('"""', "'''"):
        if first.startswith(q):
            # single-line docstring
            if first.count(q) >= 2 and len(first) > 3:
                return 1
            i = 1
            while i < len(lines) and q not in lines[i]:
                i += 1
            return i + 1
    return 0


def fix(path):
    src = path.read_text(encoding="utf-8")
    lines = src.split("\n")
    # drop existing standalone `import os` (exact statement, any indent)
    kept = [l for l in lines if l.strip() != "import os"]
    removed = len(lines) - len(kept)
    at = docstring_end(kept)
    if at < len(kept) and kept[at].strip() == "import os":
        return 0                      # already first
    kept.insert(at, "import os")
    new = "\n".join(kept)
    ast.parse(new)                    # refuse to write a syntax error
    if new != src:
        path.write_bytes(new.encode("utf-8"))
        return 1 + removed
    return 0


def main():
    targets = sorted((REPO / "tests").glob("sta_oracle_*.py"))
    if len(sys.argv) > 1:
        targets = [REPO / "tests" / a for a in sys.argv[1:]]
    changed = 0
    for f in targets:
        changed += fix(f)
    # second pass must change nothing
    again = sum(fix(f) for f in targets)
    print(f"oracles touched: {changed} edits across {len(targets)} files; "
          f"second pass: {again} (must be 0)")
    return 0 if again == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
