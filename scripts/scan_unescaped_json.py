#!/usr/bin/env python3
"""Find hand-built JSON: snprintf/sprintf with a literal `{` and an interpolated string.

    snprintf(buf, sizeof(buf), "{\\"error\\":\\"Cannot open %s: %s\\"}", path, strerror(errno));

`path` is Windows text. Interpolated raw into JSON, `C:\\Users\\...` becomes `\\U` -- not a JSON escape --
so the message is invalid JSON or, worse, silently unescaped by a lenient reader into
`C:Users...`, naming a file that does not exist. That is exactly what file_diff's error did.

The fix is not "escape harder here": it is to build the object with the JSON API (json_object_set +
json_serialize), which escapes every field it is given.

    python scripts/scan_unescaped_json.py [path ...]      exit 1 if any candidate is found

A candidate, not a proof: a format whose %s is a constant or a number is harmless.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
# A format string that opens a JSON object and interpolates something.
PATTERN = re.compile(r'(snprintf|sprintf|\w*snprintf)\s*\([^;]*?"\s*\{\s*\\?"[^;]*?%[sd]')


def scan_file(path: Path) -> list[tuple[int, str]]:
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return []
    out = []
    for i, line in enumerate(text.splitlines(), 1):
        if "json_object_set" in line or "json_new_object" in line:
            continue
        if PATTERN.search(line):
            out.append((i, line.strip()[:120]))
    return out


def main() -> int:
    roots = [Path(a) for a in sys.argv[1:]] or [REPO / "src"]
    files: list[Path] = []
    for r in roots:
        files.extend([r] if r.is_file() else sorted(r.rglob("*.c")))
    total = 0
    per_file: dict[str, int] = {}
    for f in files:
        hits = scan_file(f)
        if hits:
            per_file[str(f.relative_to(REPO) if f.is_relative_to(REPO) else f)] = len(hits)
            total += len(hits)
    for name, n in sorted(per_file.items(), key=lambda kv: -kv[1]):
        print(f"{n:4}  {name}")
    print(f"hand-built JSON: {total} candidate(s) in {len(per_file)} of {len(files)} file(s)")
    return 1 if total else 0


if __name__ == "__main__":
    sys.exit(main())
