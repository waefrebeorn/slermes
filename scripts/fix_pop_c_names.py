#!/usr/bin/env python3
"""Repair PoP annotations whose C slot does not name a function in the file.

The standard (docs/dev/c-standards.md 2.1) is
`PoP: <c_function> @ <python_file>:<python_function>`. 358 annotations in the
tree instead carry the PYTHON name in the C slot, e.g.

    /* PoP: cron_request_id @ hermes_state_keys.py:cron_request_id */
    char *hsk_cron_request_id(const char *job_id, ...)

The scanner attributes the C function by that slot, so an annotation naming a
function that is not in the file attributes the work to nothing. That is the
"unbacked annotation is worse than none" case.

Rewrite rule, deliberately narrow: if the annotated name is not defined in the
file, look for a DEFINED function whose name ends with `_<annotated>` (or equals
it). Exactly one match -> rewrite. Zero or several -> leave it and report, so a
human decides. Nothing is rewritten on a guess.
"""
import re
import sys
from pathlib import Path

REPO = Path("C:/Users/eman5/slermes")
POP = re.compile(r"PoP:\s+([A-Za-z_]\w*)\s+@\s+([\w./]+):([A-Za-z_]\w*)")
DEF = re.compile(r"^[A-Za-z_][\w \t\*]*?\b([A-Za-z_]\w*)\s*\([^;]*?\)\s*\{", re.M)


def main():
    apply = "--apply" in sys.argv
    changed = unmatched = 0
    for c in sorted((REPO / "src").rglob("port_*.c")):
        src = c.read_text(encoding="utf-8")
        defined = set(DEF.findall(src))
        out = src
        for m in POP.finditer(src):
            cn, pf, py = m.group(1), m.group(2), m.group(3)
            if cn in defined:
                continue
            cands = sorted(d for d in defined
                           if d == cn or d.endswith("_" + cn) or d.endswith(cn))
            if len(cands) != 1:
                unmatched += 1
                if not apply:
                    print(f"  ? {c.relative_to(REPO).as_posix()}: {cn} -> {cands[:3]}")
                continue
            real = cands[0]
            old = f"PoP: {cn} @ {pf}:{py}"
            new = f"PoP: {real} @ {pf}:{py}"
            if old in out:
                out = out.replace(old, new, 1)
                changed += 1
                if not apply:
                    print(f"  ! {c.relative_to(REPO).as_posix()}: {cn} -> {real}")
        if apply and out != src:
            c.write_bytes(out.encode("utf-8"))

    print(f"{'rewrote' if apply else 'would rewrite'} {changed} annotation(s); "
          f"{unmatched} unresolved (left for a human)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
