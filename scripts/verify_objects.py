#!/usr/bin/env python3
"""Objects that do not define what their source defines.

The link is the last thing anyone checks, and it only fails on *missing* symbols. An object that
defines fewer symbols than its source does links fine as long as a stub elsewhere covers the gap --
that is how src/acp/server.o ended up with one global symbol while src/acp/server.c defines three,
and how `kanban_create` answered from strdup("task_id").

    python scripts/verify_objects.py              # report every disagreeing object
    python scripts/verify_objects.py --fix        # touch those sources so make rebuilds them
    python scripts/verify_objects.py --json out.json

Only non-static definitions at file scope are considered, and a symbol counts as defined when some
object in the tree exports it globally -- so a function that legitimately lives in another translation
unit is not reported.
"""
from __future__ import annotations

import json
import os
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
NM = Path(r"C:\winlibs\mingw64\bin\nm.exe")

DEF_RE = re.compile(
    r"^(?!\s)(?:[A-Za-z_][\w \t\*]*?[\s\*])(\w+)\s*\(([^;{]*)\)\s*\{", re.M)


def source_defs(path: Path) -> list[str]:
    text = path.read_text(encoding="utf-8", errors="replace")
    out = []
    # A `static` prototype earlier in the file makes the later unstyled definition internal. That is
    # C, not a defect: memory_storage.c declares 32 vtable helpers static at the top and defines them
    # without the keyword lower down.
    static_names = set(re.findall(r"^\s*static\b[^;{]*?\b(\w+)\s*\(", text, re.M))
    for m in DEF_RE.finditer(text):
        name = m.group(1)
        if name in {"if", "for", "while", "switch", "return", "sizeof", "else"}:
            continue
        decl = text[m.start():m.end()]
        if re.match(r"^static\b", decl) or re.search(r"\bstatic\b", decl.split("(")[0]):
            continue
        if name in static_names:
            continue
        out.append(name)
    return sorted(set(out))


def object_globals(objs: list[Path]) -> set[str]:
    """nm only takes so many arguments -- Windows caps the command line, so walk in batches."""
    out: set[str] = set()
    for i in range(0, len(objs), 300):
        batch = objs[i:i + 300]
        r = subprocess.run([str(NM), "-g", "--defined-only", *[str(o) for o in batch]],
                           capture_output=True, text=True, errors="replace")
        out |= set(re.findall(r"^[0-9a-fA-F]+\s+[TDBRWG]\s+(\S+)$", r.stdout, re.M))
    return out


def main() -> int:
    fix = "--fix" in sys.argv
    want_json = None
    if "--json" in sys.argv:
        want_json = Path(sys.argv[sys.argv.index("--json") + 1])

    sources = [p for p in (REPO / "src").rglob("*.c")]
    all_objs = list((REPO / "src").rglob("*.o"))
    exported = object_globals(all_objs)
    print(f"sources: {len(sources)}  objects: {len(all_objs)}  exported symbols: {len(exported)}")

    broken: dict[str, list[str]] = {}
    missing_obj: list[str] = []
    for src in sources:
        obj = src.with_suffix(".o")
        if not obj.exists():
            missing_obj.append(str(src.relative_to(REPO)))
            continue
        defs = source_defs(src)
        if not defs:
            continue
        # cheap pre-filter: does this object export anything at all?
        lack = [d for d in defs if d not in exported]
        if lack:
            broken[str(src.relative_to(REPO))] = lack

    print(f"sources without an object: {len(missing_obj)}")
    print(f"objects that do not define what their source defines: {len(broken)}")
    for path, lack in sorted(broken.items(), key=lambda kv: -len(kv[1]))[:12]:
        print(f"  {path:52} {len(lack):3} missing, e.g. {', '.join(lack[:4])}")
    if want_json:
        want_json.write_text(json.dumps(broken, indent=1), encoding="utf-8")
        print(f"wrote {want_json}")
    if fix:
        n = 0
        for path in broken:
            os.utime(REPO / path, None)
            n += 1
        print(f"touched {n} source(s); run make to rebuild them")
    return 1 if broken else 0


if __name__ == "__main__":
    sys.exit(main())
