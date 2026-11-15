#!/usr/bin/env python3
"""Resolve remaining orphan PoP targets by FUNCTION NAME in the twin tree.

A module path can be wrong while the function is right: the twin directory is
named after the module it was extracted from, so an annotation that says
`tools/tirith_security.py:check_tool` may have its twin filed under a different
module prefix entirely.

For each (module, function) pair still unresolved, look for any twin whose
directory ends in `___<function>`. If exactly one module prefix claims it, the
annotation is re-pointable to that twin's module.
"""
import re
import sys
from pathlib import Path

REPO = Path("C:/Users/eman5/slermes")
QUARRY = Path("C:/Users/eman5/hermes-agent")
GATES = REPO / "tests" / "watercup" / "gates"
POP = re.compile(r"PoP:\s*(\S+)\s*@\s*([A-Za-z0-9_./-]+\.py)\s*:\s*(\S+)")

sys.path.insert(0, str(REPO / "tests"))
import slermes_parity_battleground as B  # noqa: E402


def mod_to_prefix(mod: str) -> str:
    stem = mod[:-3] if mod.endswith(".py") else mod
    return "real__" + stem.replace("\\", "/").lstrip("./").replace("/", "_") + "_py___"


def main() -> int:
    # twin directory name -> module it belongs to
    twins = {}
    for p in GATES.iterdir():
        if not p.is_dir() or not p.name.startswith("real__"):
            continue
        body = p.name[len("real__"):]
        if "___" not in body:
            continue
        mod_part, fn = body.rsplit("___", 1)
        twins.setdefault(fn, set()).add(mod_part)

    idx = B.CIndexer()
    idx.build()
    pairs, seen = [], set()
    for a in idx.pop_annotations:
        if a.python_module_exists is not False or not a.python_file:
            continue
        for fn in a.python_functions:
            if (a.python_file, fn) in seen:
                continue
            seen.add((a.python_file, fn))
            pairs.append((a.python_file, fn))

    resolved, ambiguous, gone = [], [], []
    for mod, fn in pairs:
        hits = twins.get(fn)
        if not hits:
            gone.append((mod, fn))
        elif len(hits) == 1:
            resolved.append((mod, fn, next(iter(hits))))
        else:
            ambiguous.append((mod, fn, sorted(hits)))

    print(f"orphan (module, function) pairs: {len(pairs)}")
    print(f"  resolvable by twin function name (unique): {len(resolved)}")
    print(f"  ambiguous (several modules claim the name): {len(ambiguous)}")
    print(f"  no twin anywhere with that function:        {len(gone)}")
    print()
    print("resolvable samples:")
    for mod, fn, owner in resolved[:15]:
        print(f"  {mod}:{fn}  -> twin module {owner}")
    print()
    print("still gone (samples):")
    for mod, fn in gone[:15]:
        print(f"  {mod}:{fn}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
