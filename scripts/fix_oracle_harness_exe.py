#!/usr/bin/env python3
"""Give the subprocess-style oracles a Windows-safe harness path.

Two defects, both of which the batch runner reported as the indistinguishable
"oracle produced no output":

1. `subprocess.run(["/tmp/t_port_<name>"])` is a POSIX-only path. On Windows the
   runner's `/tmp` symlink silently fails, so the exec raises FileNotFoundError
   before a single case runs.
2. The real binary is `tests/oracle/build/tt_<name>.exe`. The shell runner gets
   away without the suffix because CreateProcess appends `.exe`; Python's
   subprocess does not, so it needs the explicit candidate.

Result: resolve `tt_<name>[.exe]` next to the oracle, falling back to `/tmp`.

Idempotent; preserves the file's newline style.
"""
from __future__ import annotations

from pathlib import Path

REPO = Path("C:/Users/eman5/slermes")
MARK = "_harness_dir = os.environ.get"

HELPER = '''_harness_dir = os.environ.get("SLERMES_HARNESS_DIR") or str(
    Path(__file__).resolve().parent / "oracle" / "build")
_harness = next(
    (os.path.join(_harness_dir, c) for c in ("tt_%s.exe", "tt_%s")
     if os.path.exists(os.path.join(_harness_dir, c))),
    "/tmp/t_port_%s",  # externally provided binary
)
'''


def fix(path: Path) -> str:
    raw = path.read_bytes()
    nl = "\r\n" if b"\r\n" in raw else "\n"
    text = raw.decode("utf-8", errors="replace").replace("\r\n", "\n")
    if MARK not in text:
        return "no helper (nothing to do)"
    if ".exe" in text:
        return "already fixed"

    lines = text.split("\n")
    start = next(i for i, ln in enumerate(lines) if MARK in ln)
    end = start
    while end < len(lines) and not lines[end].lstrip().startswith("proc ="):
        end += 1

    name = None
    for ln in lines[start:end]:
        if "os.path.join(_harness_dir," in ln and '"' in ln:
            name = ln.split('"')[1]
            break
    if not name:
        return "could not read harness name"

    lines[start:end] = (HELPER % (name, name, name)).rstrip("\n").split("\n")
    text = "\n".join(lines)
    if "from pathlib import Path" not in text:
        ls = text.split("\n")
        ins = max((i for i, ln in enumerate(ls[:40]) if ln.startswith("import ")),
                  default=0)
        ls.insert(ins + 1, "from pathlib import Path")
        text = "\n".join(ls)
    path.write_bytes(text.replace("\n", nl).encode("utf-8"))
    return f"fixed {name}"


if __name__ == "__main__":
    for f in sorted((REPO / "tests").glob("sta_oracle_*.py")):
        if MARK in f.read_text(encoding="utf-8", errors="replace"):
            print(f.name, "->", fix(f))
