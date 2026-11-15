#!/usr/bin/env python3
"""Regroup build/objects.mk into named per-subsystem object lists.

"Cupboards and jars": one variable per subsystem (the cupboard), each holding
that subsystem's object files (the jars). The file used to be two giant token
lists -- CLI_OBJ (264 objects) and PORT_OBJ (871) -- so there was nowhere to
look when a subsystem's linkage was wrong, and no way to tell a deliberate
omission from an oversight.

Both lists are cupboarded. The transformation is semantics-preserving by
construction and verified: expand every variable, and the multiset of object
tokens must be identical before and after.

    python scripts/organize_objects.py            # write build/objects.mk
    python scripts/organize_objects.py --check    # verify only, change nothing
"""
from __future__ import annotations

import re
import sys
from collections import Counter
from pathlib import Path

REPO = Path("C:/Users/eman5/slermes")
MK = REPO / "build" / "objects.mk"

# The lists that get cupboarded, in emission order.
TARGETS = ["CLI_OBJ", "PORT_OBJ"]

# Cupboard name -> path segment after src/. Order is emission order.
CUPBOARDS = [
    ("ROOT", ""),
    ("HERMES_CLI", "hermes_cli"),
    ("CLI", "cli"),
    ("AGENT", "agent"),
    ("GATEWAY", "gateway"),
    ("TOOLS", "tools"),
    ("CRON", "cron"),
    ("TUI", "tui"),
    ("PLUGINS", "plugins"),
    ("DESKTOP", "desktop"),
]


def parse_defs(text: str) -> dict[str, list[str]]:
    """Every `NAME = ...` definition, with backslash continuations folded in."""
    defs: dict[str, list[str]] = {}
    lines = text.splitlines()
    i = 0
    while i < len(lines):
        m = re.match(r'^([A-Za-z_0-9]+)\s*[:+]?=\s*(.*)$', lines[i])
        if not m:
            i += 1
            continue
        buf = m.group(2)
        while buf.rstrip().endswith("\\") and i + 1 < len(lines):
            buf = buf.rstrip().rstrip("\\")
            i += 1
            buf += " " + lines[i]
        defs[m.group(1)] = [t for t in buf.split() if t != "\\"]
        i += 1
    return defs


def expand(tokens_list, defs, seen=None):
    """Recursively inline $(NAME) references."""
    seen = seen or set()
    out = []
    for t in tokens_list:
        m = re.fullmatch(r'\$\(([A-Za-z_0-9]+)\)', t)
        if m and m.group(1) in defs and m.group(1) not in seen:
            seen = seen | {m.group(1)}
            out.extend(expand(defs[m.group(1)], defs, seen))
        else:
            out.append(t)
    return out


def cupboard_for(obj: str) -> str:
    rel = obj[len("src/"):] if obj.startswith("src/") else obj
    seg = rel.split("/", 1)[0]
    for name, key in CUPBOARDS:
        if seg == key:
            return name
    return "MISC"


def render(name: str, tokens: list[str], width: int = 78) -> list[str]:
    """Emit `NAME = a.o \\\n    b.o ...` wrapped at `width`."""
    if not tokens:
        return [f"{name} ="]
    out = []
    line = f"{name} = {tokens[0]}"
    for tok in tokens[1:]:
        if len(line) + 1 + len(tok) + 1 > width:
            out.append(line + " \\")
            line = "    " + tok
        else:
            line += " " + tok
    out.append(line)
    return out


def main() -> int:
    check_only = "--check" in sys.argv
    text = MK.read_text(encoding="utf-8", errors="replace")
    defs = parse_defs(text)

    # Everything that is not one of TARGETS is preserved verbatim.
    keep, skipping = [], False
    for ln in text.splitlines():
        m = re.match(r'^([A-Za-z_0-9]+)\s*[:+]?=', ln)
        if m and m.group(1) in TARGETS:
            skipping = True
            continue
        if skipping:
            # Every line of a cupboarded block goes, including the last one --
            # that last line is part of the definition, not a new statement.
            if not ln.rstrip().endswith("\\"):
                skipping = False
            continue
        keep.append(ln)
    tail = "\n".join(keep).strip("\n")

    before = Counter()
    for t in TARGETS:
        before.update(expand(defs[t], defs))

    header = [
        "# ── Object File Lists ──────────────────────────────────────────────",
        "# build/objects.mk -- cupboards and jars.",
        "#",
        "# Each CUPBOARD_* variable holds one subsystem's object files (the jars).",
        "# Add a new object to its subsystem's cupboard, never to a global list.",
        "# CLI_OBJ and PORT_OBJ are concatenations of their cupboards.",
        "#",
        "# Regenerate:  python scripts/organize_objects.py",
        "# Verify only: python scripts/organize_objects.py --check",
        "# Included by the top-level Makefile.",
        "",
        "",
    ]

    body = list(header)
    for target in TARGETS:
        tokens = defs[target]
        refs = [t for t in tokens if t.startswith("$(")]
        objs = [t for t in tokens if not t.startswith("$(")]

        groups: dict[str, list[str]] = {}
        for o in objs:
            groups.setdefault(cupboard_for(o), []).append(o)

        known = [n for n, _ in CUPBOARDS]
        order = known + [n for n in sorted(groups) if n not in known]
        prefix = "CUPBOARD_" if target == "CLI_OBJ" else "CUPBOARD_PORT_"

        body.append(f"# ── {target} ──")
        for name in order:
            if name not in groups:
                continue
            body += render(prefix + name, groups[name]) + [""]

        composed = [f"$({prefix}{n})" for n in order if n in groups] + refs
        body += render(target, composed) + [""]

    new_text = "\n".join(body) + "\n" + tail + "\n"
    new_defs = parse_defs(new_text)
    after = Counter()
    for t in TARGETS:
        after.update(expand(new_defs[t], new_defs))

    if before != after:
        print("REFUSING: the expanded object multiset changed")
        print(f"  missing: {list((before - after).elements())[:10]}")
        print(f"  extra:   {list((after - before).elements())[:10]}")
        return 1

    print(f"linked objects (union): {len(before)} -- unchanged")
    for target in TARGETS:
        objs = [t for t in expand(defs[target], defs) if t.endswith(".o")]
        print(f"  {target}: {len(objs)} jars in "
              f"{len({cupboard_for(o) for o in objs})} cupboards")

    if check_only:
        print("check: OK (file not written)")
        return 0
    MK.write_text(new_text, encoding="utf-8", newline="\n")
    print(f"wrote {MK.relative_to(REPO)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
