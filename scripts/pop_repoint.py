#!/usr/bin/env python3
"""Re-point PoP annotations at the Python files that actually exist.

Upstream rehomed the messaging platforms from `gateway/platforms/<name>.py`
(single files) to `plugins/platforms/<name>/adapter.py` (packages), and a few
modules moved between trees entirely. The annotations still name the old path,
so the scanner calls ~1489 annotations orphans even though the source is right
there -- parity credit that is verifiable but recorded as not.

Resolution order for an annotation module M:
  1. exact path in the repo, then in the quarry
  2. basename match anywhere in either tree
  3. plugins/platforms/<stem>/adapter.py          (the rehoming)
  4. plugins/platforms/<stem>/__init__.py
  5. <stem>.py at a tree root

Rewrites only on a UNIQUE resolution. Ambiguous or unresolved targets are
reported, never guessed.
"""
import re
import sys
from pathlib import Path

REPO = Path("C:/Users/eman5/slermes")
QUARRY = Path("C:/Users/eman5/hermes-agent")
ROOTS = [REPO, QUARRY]
POP = re.compile(r"(PoP:\s*\S+\s*@\s*)([A-Za-z0-9_./-]+\.py)(\s*:\s*\S+\s*\*/)")
# The loose form carries the module too: "Port of Python <mod>.py:<fn>()". It is
# a different shape, so it needs its own substitution pattern.
POP_LOOSE = re.compile(r"(Port of Python\s+)([A-Za-z0-9_./-]+\.py)(\s*:)")

# Every .py in both trees, indexed by basename and by package-qualified path.
all_py = []
by_base = {}
for root in ROOTS:
    for py in root.rglob("*.py"):
        s = py.as_posix()
        if "/.git/" in s or "/node_modules/" in s:
            continue
        rel = py.relative_to(root)
        all_py.append((root, rel))
        by_base.setdefault(py.name, []).append((root, rel))


def resolve(mod: str):
    """Return the canonical annotation path for `mod`, or None."""
    norm = mod.replace("\\", "/").lstrip("./")
    stem = Path(norm).stem
    parts = norm.split("/")

    # 1. exact
    for root in ROOTS:
        if (root / norm).exists():
            return norm

    # 2. basename anywhere. On multiple hits prefer one sharing the annotation's
    # own top-level directory: `agent/anthropic_adapter.py` should resolve to
    # agent/anthropic_adapter.py, not an unrelated same-named file elsewhere.
    hits = by_base.get(Path(norm).name, [])
    if len(hits) == 1:
        return hits[0][1].as_posix()
    if len(hits) > 1:
        top = parts[0] if len(parts) > 1 else None
        same = [h for h in hits if top and h[1].parts[0] == top]
        if len(same) == 1:
            return same[0][1].as_posix()

    # 3/4. rehomed platform package. Two shapes:
    #     gateway/platforms/<pkg>/<file>.py -> plugins/platforms/<pkg>/<file>.py
    #     gateway/platforms/<name>.py        -> plugins/platforms/<name>/adapter.py
    if len(parts) >= 2:
        pkg = parts[-2]
        fname = parts[-1]
        cand = f"plugins/platforms/{pkg}/{fname}"
        for root in ROOTS:
            if (root / cand).exists():
                return cand
    for cand in (f"plugins/platforms/{stem}/adapter.py",
                 f"plugins/platforms/{stem}/__init__.py"):
        for root in ROOTS:
            if (root / cand).exists():
                return cand

    # 5. tree root
    for root in ROOTS:
        if (root / f"{stem}.py").exists():
            return f"{stem}.py"
    return None


def main() -> int:
    dry = "--apply" not in sys.argv

    # Collect every annotation site.
    sites = []
    # Every .c under src/, not just port_*: most annotation sites live in the
    # ordinary linked files (gateway/platforms/telegram.py alone is claimed 248
    # times from files named nothing like port_*.c).
    units = list(REPO.glob("src/**/*.c"))
    for c in units:
        txt = c.read_text(encoding="utf-8", errors="replace")
        mods = {m.group(2) for m in POP.finditer(txt)} | {m.group(2) for m in POP_LOOSE.finditer(txt)}
        for mod in mods:
            sites.append((c, mod))

    cache = {}
    changed = unresolved = already = 0
    for c, mod in sites:
        if mod not in cache:
            cache[mod] = resolve(mod)
        new = cache[mod]
        if new is None:
            unresolved += 1
            print(f"  UNRESOLVED {mod}")
        elif new == mod:
            already += 1
        else:
            changed += 1
            print(f"  REPOINT   {mod:46} -> {new}")

    print()
    print(f"distinct modules: {len(cache)}   already-ok: {already}   "
          f"repointed: {changed}   unresolved: {unresolved}")

    if dry:
        print("\n(dry run -- pass --apply to rewrite)")
        return 0

    for c, mod in sites:
        new = cache[mod]
        if new is None or new == mod:
            continue
        txt = c.read_text(encoding="utf-8", errors="replace")
        txt, n = POP.subn(lambda m: m.group(1) + new + m.group(3), txt)
        txt, n2 = POP_LOOSE.subn(lambda m: m.group(1) + new + m.group(3), txt)
        if n or n2:
            c.write_text(txt, encoding="utf-8", newline="")
    print("applied.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
