#!/usr/bin/env python3
"""Point the three /tmp-hardcoded oracles at the real harness directory.

`subprocess.run(["/tmp/t_port_<name>"])` is a POSIX-only path; on Windows it
raises FileNotFoundError before the case list runs, which the batch runner
reports as "oracle produced no output" -- indistinguishable from a dead
oracle. Resolve the harness relative to the repo instead, keeping /tmp as a
fallback so an externally-provided binary still works.

Idempotent; LF-safe (writes bytes with the file's own newline style).
"""
from __future__ import annotations

import re
from pathlib import Path

REPO = Path("C:/Users/eman5/slermes")
CALL = re.compile(r'\[\s*"/tmp/(t_port_[A-Za-z0-9_]+)"\s*\]')
HELPER = (
    '_harness_dir = os.environ.get("SLERMES_HARNESS_DIR") or str(\n'
    '    Path(__file__).resolve().parent / "oracle" / "build")\n'
    '_harness = os.path.join(_harness_dir, "tt_%s")\n'
    'if not os.path.exists(_harness):\n'
    '    _harness = "/tmp/%s"  # externally provided binary\n'
)


def fix(path: Path) -> str:
    raw = path.read_bytes()
    nl = "\r\n" if b"\r\n" in raw else "\n"
    text = raw.decode("utf-8", errors="replace").replace("\r\n", "\n")

    m = CALL.search(text)
    if not m:
        return "no /tmp call"
    name = m.group(1)
    if "_harness_dir" in text:
        return "already fixed"

    helper = HELPER % (name, name)
    text = CALL.sub('[%s]' % "_harness", text, count=1)

    lines = text.split("\n")
    # Insert the resolver just before the subprocess call that uses it.
    idx = next(i for i, ln in enumerate(lines) if "_harness]" in ln)
    lines[idx:idx] = helper.rstrip("\n").split("\n")
    text = "\n".join(lines)

    if "import os" not in text:
        lines = text.split("\n")
        last = max((i for i, ln in enumerate(lines[:40]) if ln.startswith("import ")),
                   default=0)
        lines.insert(last + 1, "import os")
        text = "\n".join(lines)

    path.write_bytes(text.replace("\n", nl).encode("utf-8"))
    return f"fixed {name}"


if __name__ == "__main__":
    for f in sorted((REPO / "tests").glob("sta_oracle_*.py")):
        if "/tmp/t_port_" in f.read_text(encoding="utf-8", errors="replace"):
            print(f.name, "->", fix(f))
