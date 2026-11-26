#!/usr/bin/env python3
"""Move a mid-function `json_free(obj)` down past the error message that still uses a pointer into obj.

Shape it fixes (found by scripts/scan_json_freed_use.py):

        json_free(args);
        char buf[512];
        snprintf(buf, sizeof(buf), "Cannot stat %s: %s", path, strerror(errno));
        return strdup(buf);

`path` points into args, so the message is built from freed memory. Nothing here needs the free to
happen first: build the message, free, return.

Deliberately conservative -- it only rewrites when ALL of these hold:
  * the free is the whole statement on its own line,
  * within the next 6 lines there is a `return` at the same brace depth,
  * the flagged pointer is used before that return,
  * no other `json_free(same object)` appears in between (no double free).

Dry run by default: prints the proposed edits. `--apply` writes them.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import scan_json_freed_use as scanner  # noqa: E402


def transform(path: Path, apply: bool) -> int:
    raw = path.read_text(encoding="utf-8")
    lines = raw.splitlines(keepends=True)
    findings = scanner.scan_file(path)
    if not findings:
        return 0

    edits = []
    for line_no, _fname, ptr, obj, free_line, _code in findings:
        fi = free_line - 1                      # 0-based index of the free
        if fi < 0 or fi >= len(lines):
            continue
        if not re.fullmatch(rf"\s*json_free\s*\(\s*{re.escape(obj)}\s*\)\s*;\s*", lines[fi]):
            continue                            # not a standalone statement: leave it
        depth = 0
        return_idx = None
        for j in range(fi + 1, min(fi + 7, len(lines))):
            code = scanner.clean(lines[j])
            if re.search(rf"\bjson_free\s*\(\s*{re.escape(obj)}\s*\)", code):
                return_idx = None               # another free of the same object: skip
                break
            if depth == 0 and re.search(r"\breturn\b", code) and j > fi:
                return_idx = j
                break
            depth += code.count("{") - code.count("}")
        if return_idx is None:
            continue
        span = "".join(scanner.clean(l) for l in lines[fi + 1:return_idx + 1])
        if not re.search(rf"(?<![.>\w]){re.escape(ptr)}\b", span):
            continue                            # the pointer is not used before the return
        # Moving the free down must not leave an *earlier* exit with nothing freed: if any line
        # between the old and new position returns, the rewrite would leak on that path.
        between = [scanner.clean(l) for l in lines[fi:return_idx]]
        if any(re.search(r"\b(return|goto)\b", l) for l in between):
            print(f"{path}:{fi+1}: SKIP -- another exit sits between the free and the message")
            continue
        edits.append((fi, return_idx, obj, ptr))

    edits = sorted(set(edits))            # the scanner can report one site twice
    for fi, return_idx, obj, ptr in sorted(edits, reverse=True):
        freed = lines.pop(fi)
        # popping shifts everything above fi down by one, so the return line now sits at
        # return_idx - 1; insert before it, never after (after is dead code and leaks).
        insert_at = return_idx - 1 if return_idx > fi else return_idx
        lines.insert(insert_at, freed)
        print(f"{path}:{fi+1}: move json_free({obj}) (used via `{ptr}`) below the message at line {return_idx+1}")

    if apply and edits:
        path.write_text("".join(lines), encoding="utf-8", newline="")
    return len(edits)


def main() -> int:
    apply = "--apply" in sys.argv
    roots = [a for a in sys.argv[1:] if not a.startswith("--")] or ["src/tools"]
    total = 0
    for root in roots:
        p = Path(root)
        for f in ([p] if p.is_file() else sorted(p.rglob("*.c"))):
            total += transform(f, apply)
    print(f"{total} edit(s) {'applied' if apply else 'proposed'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
