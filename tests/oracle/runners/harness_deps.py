#!/usr/bin/env python3
"""Content hash of a harness's transitive local includes.

Caching the harness object on the harness source alone is UNSOUND: a harness
`#include`s the port's .c, which includes others, so editing a port source would
silently reuse a stale object and report a parity result for code that was never
compiled. This walks the quoted-include closure and hashes the bytes, so the key
changes exactly when the compiled input changes.

Usage: harness_deps.py <harness.c> [extra-string]
Prints a hex digest.
"""
import hashlib
import re
import sys
from pathlib import Path

ROOT = Path("C:/Users/eman5/slermes")
SEARCH = [ROOT / "include", ROOT / "src", ROOT / "lib", ROOT / "include/compat"]
INCLUDE = re.compile(r'^\s*#\s*include\s*"([^"]+)"', re.M)


def resolve(name, cur):
    for c in [cur.parent / name] + [b / name for b in SEARCH]:
        if c.exists():
            return c
    return None


def walk(path, seen, h):
    path = path.resolve()
    if path in seen:
        return
    seen.add(path)
    try:
        data = path.read_bytes()
    except OSError:
        return
    h.update(str(path).encode("utf-8"))
    h.update(data)
    text = data.decode("utf-8", errors="replace")
    for inc in INCLUDE.findall(text):
        r = resolve(inc, path)
        if r:
            walk(r, seen, h)


def main():
    h = hashlib.sha256()
    walk(Path(sys.argv[1]), set(), h)
    if len(sys.argv) > 2:
        h.update(sys.argv[2].encode("utf-8"))
    print(h.hexdigest())
    return 0


if __name__ == "__main__":
    sys.exit(main())
