#!/usr/bin/env python3
"""Repoint oracles at first-party names that moved, and report what vanished.

tests/verify_oracle_drift.py enumerates the drift; this repairs it. For each
dangling `from X import name`:

  - if exactly ONE module in the tree defines `name`, rewrite the import to it
    (a move is a rename of the module, the name is unchanged);
  - if several do, leave it -- a human must choose, and a guess here silently
    changes what is being measured;
  - if none do, the name is GONE upstream. The oracle's case for it is stale;
    it is reported so the port can be declared incomplete rather than left
    reporting an absent measurement as a port defect.

Idempotent; syntax-checked; never rewrites a file it cannot parse.
"""
from __future__ import annotations

import ast
import os
import re
import sys
from pathlib import Path

REPO = Path("C:/Users/eman5/slermes")
TESTS = REPO / "tests"
# The quarry (upstream hermes-agent) is enormous and its rglob alone exceeds a
# minute; the repo carries its own Python tree and the oracles resolve against
# it first, so index that. A name defined ONLY upstream still shows as "gone",
# which is the honest answer for a repo that vendors its sources.
ROOTS = [REPO]


def all_modules():
    # os.scandir with pruning, NOT Path.rglob: rglob descends into tests/
    # (22,614 Python files under tests/watercup) and only filters afterwards,
    # which cost a minute before the first print.
    out = {}
    skip = {"node_modules", "build", "__pycache__", "dist", "venv", "tests",
            "optional-skills", "evals", "scripts", "apps", "ui-tui", "docker"}

    def walk(d: Path):
        try:
            entries = list(os.scandir(d))
        except OSError:
            return
        for e in entries:
            if e.name.startswith(".") or e.name in skip:
                continue
            try:
                if e.is_dir(follow_symlinks=False):
                    walk(Path(e.path))
                elif e.name.endswith(".py"):
                    out.setdefault(Path(e.name).stem, []).append(Path(e.path))
            except OSError:
                continue

    for base in ROOTS:
        if base.is_dir():
            walk(base)
    return out


def defines(path: Path, name: str) -> bool:
    try:
        tree = ast.parse(path.read_text(encoding="utf-8", errors="replace"))
    except (OSError, SyntaxError):
        return False
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef)) \
                and node.name == name:
            return True
        if isinstance(node, ast.Assign):
            if any(isinstance(t, ast.Name) and t.id == name for t in node.targets):
                return True
    return False


def main() -> int:
    apply = "--apply" in sys.argv
    mods = all_modules()
    # name -> defining module stems
    definers = {}
    for stem, files in mods.items():
        for f in files:
            try:
                tree = ast.parse(f.read_text(encoding="utf-8", errors="replace"))
            except (OSError, SyntaxError):
                continue
            for node in tree.body:
                nm = None
                if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef,
                                     ast.ClassDef)):
                    nm = node.name
                elif isinstance(node, ast.Assign):
                    nm = next((t.id for t in node.targets
                               if isinstance(t, ast.Name)), None)
                if nm:
                    definers.setdefault(nm, set()).add(stem)

    moved, gone, ambiguous = [], [], []
    for o in sorted(TESTS.glob("sta_oracle_*.py")):
        try:
            tree = ast.parse(o.read_text(encoding="utf-8", errors="replace"))
        except SyntaxError:
            continue
        guarded = set()
        for node in ast.walk(tree):
            if isinstance(node, ast.Try):
                for sub in ast.walk(node):
                    if isinstance(sub, ast.ImportFrom):
                        for al in sub.names:
                            guarded.add(al.asname or al.name)
        for node in ast.walk(tree):
            if not isinstance(node, ast.ImportFrom) or            node.level:
                continue
            mod = node.module
            if not mod:
                continue
            for al in node.names:
                nm = al.name
                if nm == "*" or al.asname or nm in guarded:
                    continue
                cur = mod.split(".")[0]
                if cur not in ("agent", "hermes_cli", "gateway", "pm", "tools",
                               "cron", "plugins", "skills", "acp", "pet",
                               "provider", "apps", "cli"):
                    continue
                # does the CURRENT module define it, in EITHER tree? The repo
                # vendors some modules; the quarry carries the rest. Indexing
                # the whole quarry is too slow (22k files), so look the module
                # up by its relative path -- O(1) per name.
                cand = [f for f in mods.get(mod.split(".")[-1], [])]
                for base in (REPO, REPO.parent / "hermes-agent"):
                    q = base / mod.replace(".", "/")
                    for c in (q.with_suffix(".py"), q / "__init__.py"):
                        if c.exists():
                            cand.append(c)
                if any(defines(f, nm) for f in cand):
                    continue
                hits = definers.get(nm, set())
                if len(hits) == 1:
                    moved.append((o.name, mod, nm, next(iter(hits))))
                elif len(hits) > 1:
                    ambiguous.append((o.name, mod, nm, sorted(hits)[:4]))
                else:
                    gone.append((o.name, mod, nm))

    print(f"moved (unique definer): {len(moved)}")
    for f, mod, nm, new in moved[:12]:
        print(f"    {f}: {mod}.{nm} -> {new}.{nm}")
    print(f"ambiguous (needs a human): {len(ambiguous)}")
    for f, mod, nm, hits in ambiguous[:6]:
        print(f"    {f}: {mod}.{nm} -> {hits}")
    print(f"gone upstream (stale case): {len(gone)}")
    for f, mod, nm in gone[:8]:
        print(f"    {f}: {mod}.{nm}")

    if apply:
        n = 0
        byfile = {}
        for f, mod, nm, new in moved:
            byfile.setdefault(f, []).append((mod, nm, new))
        for f, items in byfile.items():
            p = TESTS / f
            t = p.read_text(encoding="utf-8")
            for mod, nm, new in items:
                t = re.sub(rf"^(\s*from\s+){re.escape(mod)}(\s+import\s+)([^#\n]*)\b{re.escape(nm)}\b",
                           lambda m: f"{m.group(1)}{new}{m.group(2)}{m.group(3)}",
                           t, flags=re.M)
            ast.parse(t)
            p.write_bytes(t.encode("utf-8"))
            n += 1
        print(f"rewrote {len(moved)} import(s) in {n} file(s)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
