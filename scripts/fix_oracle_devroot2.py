#!/usr/bin/env python3
"""Repoint oracles that resolve the dev repo one level too high.

Two shapes compute the wrong root:
  * dirname^3(here)          where here = dirname(__file__)  -> C:/Users
  * dirname^3(__file__)                                   -> C:/Users/eman5
The Hermes checkout is C:/Users/eman5/hermes-agent, so anything joining
"tools/..." or "agent/..." onto those roots misses.

Rather than touch every oracle (some legitimately want the slermes parent),
this only rewrites oracles whose batch error shows the wrong root, and it
switches them to SLERMES_DEV_ROOT, which _oracle_boot.py exports.

Re-run after the suite is idle.
"""
import re
import sys
from pathlib import Path

REPO = Path("C:/Users/eman5/slermes")
BUILD = REPO / "tests/oracle/build"
GOOD = 'os.environ.get("SLERMES_DEV_ROOT") or '

PATTERNS = [
    "os.path.dirname(os.path.dirname(os.path.dirname(here)))",
    "os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))",
    "os.path.dirname(os.path.dirname(os.path.dirname(os.path.realpath(os.path.abspath(__file__)))))",
]


def main() -> int:
    # which ports actually hit the wrong root?
    broken = set()
    for f in BUILD.glob("batch_*.err"):
        t = f.read_text(encoding="utf-8", errors="replace")
        if re.search(r"C:\\\\Users\\\\eman5\\\\(?!hermes-agent)", t) or \
           "C:\\Users\\eman5\\tools" in t or "C:\\Users\\eman5\\agent" in t:
            broken.add(f.stem[len("batch_"):])
    print("ports with a wrong-root error:", len(broken))

    # map port -> oracle file
    reg = __import__("json").loads(
        (REPO / "tests/oracle/registry.json").read_text(encoding="utf-8"))
    fixed = 0
    for port in sorted(broken):
        entry = reg["ports"].get(port)
        if not isinstance(entry, dict):
            continue
        rel = entry.get("oracle", "")
        if not rel or not rel.startswith("tests/"):
            continue
        f = REPO / rel
        if not f.exists():
            print("  missing oracle file:", rel)
            continue
        t = f.read_text(encoding="utf-8")
        new = t
        for pat in PATTERNS:
            if pat in new:
                new = new.replace(pat, GOOD + pat)
        if new != t:
            f.write_text(new, encoding="utf-8")
            fixed += 1
            print("  fixed", f.name)
    print("oracles rewritten:", fixed)
    return 0


if __name__ == "__main__":
    sys.exit(main())
