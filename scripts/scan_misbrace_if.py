#!/usr/bin/env python3
"""Find `if (cond)` with no braces whose body sits at or left of the `if`.

    if (!file_path || !*file_path)
    json_free(args);                     <- this, and NOT the next line
        return strdup("{...Missing file_path...}");

That shape made transcribe_audio answer "Missing file_path" for every call it ever received, and
speak_text answer "Missing 'text' parameter" for every call. The tool looked registered, validated and
honest; the error was unconditional.

    python scripts/scan_misbrace_if.py [path ...]

Exit 1 if anything is found, so it can gate a build. A candidate, not a proof: an `if` whose body is
deliberately on the next line at a strange indent is also a hit -- read each one before editing.
"""
from __future__ import annotations

import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]

SKIP_PREFIX = ("/*", "//", "#", "}", "else", "if ", "if(")


def indent_of(line: str) -> int:
    return len(line) - len(line.lstrip())


def scan_file(path: Path) -> list[tuple[int, str, str]]:
    try:
        lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    except OSError:
        return []
    out = []
    for i, line in enumerate(lines[:-1]):
        st = line.strip()
        if not (st.startswith("if (") or st.startswith("if(")):
            continue
        if st.endswith("{") or st.endswith("\\"):        # braced, or a macro continuation
            continue
        if not st.endswith(")"):                          # condition continues on the next line
            continue
        nxt = lines[i + 1]
        nstrip = nxt.strip()
        if not nstrip or nstrip.startswith(SKIP_PREFIX):
            continue
        if indent_of(nxt) <= indent_of(line):
            out.append((i + 1, st[:72], nstrip[:60]))
    return out


def main() -> int:
    roots = [Path(a) for a in sys.argv[1:]] or [REPO / "src"]
    files: list[Path] = []
    for r in roots:
        files.extend([r] if r.is_file() else sorted(r.rglob("*.c")))
    total = 0
    for f in files:
        for lineno, cond, body in scan_file(f):
            total += 1
            try:
                rel = f.relative_to(REPO)
            except ValueError:
                rel = f
            print(f"{rel}:{lineno}: body sits at or left of its if -- `{cond}` / `{body}`")
    print(f"misbrace-if scan: {total} candidate(s) in {len(files)} file(s)")
    return 1 if total else 0


if __name__ == "__main__":
    sys.exit(main())
