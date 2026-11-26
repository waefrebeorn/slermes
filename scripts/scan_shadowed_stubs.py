#!/usr/bin/env python3
"""Find stubs that shadow a real implementation (or that disagree with its signature).

src/infrastructure_stubs.c exists so the link succeeds. Every function it defines that also exists
elsewhere in the tree is a *lie with a plausible answer*: the linker binds the call to one of them, and
whichever wins decides what the product does. kanban_create_task is the one that surfaced: the stub is

    char *kanban_create_task(const char *b, const char *t, const char *d) { return strdup("task_id"); }

while every caller passes a `kanban_task_spec_t *`. The tool reported
`{"ok": true, "task_id": "task_id"}` for a task that was never created.

    python scripts/scan_shadowed_stubs.py            # report
    python scripts/scan_shadowed_stubs.py --define   # emit the #define lines the file needs

The file already uses `#define name stub_shadowed_name` for a few dozen symbols -- the mechanism works;
the list is just incomplete.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
STUBS = REPO / "src" / "infrastructure_stubs.c"

DEF_RE = re.compile(r"^[A-Za-z_][\w \t\*]*?\b(\w+)\s*\(([^;{)]*)\)\s*\{", re.M)


def defs(path: Path) -> dict[str, str]:
    text = path.read_text(encoding="utf-8", errors="replace")
    out: dict[str, str] = {}
    for m in DEF_RE.finditer(text):
        name, args = m.group(1), " ".join(m.group(2).split())
        if name in {"if", "for", "while", "switch", "return", "sizeof"}:
            continue
        out.setdefault(name, args)
    return out


def main() -> int:
    want_define = "--define" in sys.argv
    stub_defs = defs(STUBS)
    real: dict[str, list[tuple[str, str]]] = {}
    for f in (REPO / "src").rglob("*.c"):
        if f.name == "infrastructure_stubs.c":
            continue
        for name, args in defs(f).items():
            real.setdefault(name, []).append((str(f.relative_to(REPO)), args))

    shadowed, mismatched = [], []
    for name, sargs in stub_defs.items():
        if name.startswith("stub_shadowed_"):
            continue
        if name in real:
            entry = (name, sargs, real[name][0])
            if sargs != real[name][0][1]:
                mismatched.append(entry)
            else:
                shadowed.append(entry)

    print(f"stub functions: {len(stub_defs)}")
    print(f"shadow a real implementation: {len(shadowed)}")
    for name, sargs, (path, rargs) in shadowed[:15]:
        print(f"  {name:44} real in {path}")
    print(f"shadow with a DIFFERENT signature: {len(mismatched)}")
    for name, sargs, (path, rargs) in mismatched:
        print(f"  {name}")
        print(f"      stub: ({sargs})")
        print(f"      real: ({rargs})   in {path}")

    if want_define:
        print("\n#define lines to add:")
        existing = set(re.findall(r"#define (\w+) stub_shadowed_", STUBS.read_text(encoding="utf-8")))
        for name, _, _ in shadowed + mismatched:
            if name not in existing:
                print(f"#define {name} stub_shadowed_{name}")

    return 1 if (shadowed or mismatched) else 0


if __name__ == "__main__":
    sys.exit(main())
