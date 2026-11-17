#!/usr/bin/env python3
"""Rebind oracle attribute reads whose target upstream moved to a sibling module.

The drift gate reports `alias.NAME` reads that the module no longer defines. When
the name simply moved (hermes_cli.kanban_db -> kanban_db_workspace, tools.cronjob_tools
-> cronjob_prompt_scan, ...), the oracle's contract is with the surface it was
written against, so restore it from wherever the name now lives instead of
allowlisting the drift as an absent measurement.

Names that are gone entirely (deleted upstream, renamed) are NOT touched here --
those are real debt and stay in the gate.

Idempotent: skips an oracle that already carries a rebind_used call for the
alias, and syntax-checks the result.
"""
from __future__ import annotations

import ast
import os
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tests" / "oracle"))
import load_dev_module as ldm  # noqa: E402

REPO = Path(__file__).resolve().parents[1]
QUARRY = Path("C:/Users/eman5/hermes-agent")


def is_shim(p: Path) -> bool:
    """A stale redirect stub: whole file is one absolute path string."""
    try:
        if p.stat().st_size > 512:
            return False
        t = p.read_text(encoding="utf-8", errors="replace").strip()
    except OSError:
        return False
    lines = [l for l in t.splitlines() if l.strip()]
    return (len(lines) == 1
            and (t.startswith("/") or t.startswith("C:") or t.startswith("~")))


def resolve_module(dotted: str):
    """Quarry-first, shim-skipping: slermes carries redirect stubs whose single
    string parses to zero top-level names, so every attribute reads as missing."""
    rel = dotted.replace(".", "/") + ".py"
    for r in reversed(ldm.roots()):
        c = r / rel
        if c.exists() and not is_shim(c):
            return c
    return None


def candidates_for(dotted: str) -> list[Path]:
    """Sibling modules of `dotted` inside the quarry (bounded: one directory)."""
    parts = dotted.split(".")
    pkg = QUARRY.joinpath(*parts[:-1])
    if not pkg.is_dir():
        return []
    return sorted(p for p in pkg.glob("*.py") if not is_shim(p))


def used_attrs(tree: ast.AST, alias: str) -> list[str]:
    return sorted({n.attr for n in ast.walk(tree)
                   if isinstance(n, ast.Attribute)
                   and isinstance(n.value, ast.Name) and n.value.id == alias})


def module_names(path: Path) -> set[str]:
    try:
        t = ast.parse(path.read_text(encoding="utf-8", errors="replace"))
    except SyntaxError:
        return set()
    out = set()
    for n in t.body:
        if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef)):
            out.add(n.name)
        elif isinstance(n, ast.Assign):
            for tg in n.targets:
                if isinstance(tg, ast.Name):
                    out.add(tg.id)
        elif isinstance(n, ast.AnnAssign) and isinstance(n.target, ast.Name):
            out.add(n.target.id)
    return out


def main() -> int:
    changed = 0
    for f in sorted((REPO / "tests").glob("sta_oracle_*.py")):
        src = f.read_text(encoding="utf-8", errors="replace")
        if "rebind_used" in src:
            continue
        tree = ast.parse(src)
        aliases: dict[str, str] = {}
        for n in ast.walk(tree):
            if isinstance(n, ast.Import):
                for a in n.names:
                    aliases[a.asname or a.name.split(".")[0]] = a.name
            elif isinstance(n, ast.ImportFrom) and n.module and n.level == 0:
                for a in n.names:
                    if a.name != "*":
                        # `from hermes_cli import _install_repair as ir` binds
                        # ir to hermes_cli._install_repair, not to the package.
                        aliases[a.asname or a.name] = n.module + "." + a.name
        lines = src.splitlines()
        inserts: list[str] = []
        for alias, dotted in sorted(aliases.items()):
            if "." not in dotted and dotted in ("os", "sys", "json", "re",
                                                "pathlib", "typing", "types",
                                                "dataclasses", "subprocess"):
                continue
            path = resolve_module(dotted)
            if path is None:
                continue
            have = module_names(path)
            need = used_attrs(tree, alias)
            missing = [x for x in need if x not in have]
            if not missing:
                continue
            donors: list[str] = []
            found = set()
            sib = candidates_for(dotted)
            for cand in sib:
                if cand == path:
                    continue
                found |= (module_names(cand) & set(missing))
            if found:
                pkg = dotted.rsplit(".", 1)[0]
                # Only donors that actually define one of the missing names:
                # importing every sibling forces optional deps (mcp, yaml) that
                # the oracle has no business requiring.
                donors = ['"%s.%s"' % (pkg, c.stem) for c in sib
                          if c != path and (module_names(c) & set(missing))]
            if not donors:
                continue
            inserts.append((alias, donors))
        if not inserts:
            continue
        # Each rebind must sit AFTER the import that binds its alias. Inserting
        # after "the last module-level import" is wrong when the oracle's
        # imports live inside a try: block -- the call then runs before the
        # alias exists (NameError).
        anchors: dict[str, int] = {}
        for n in ast.walk(tree):
            if isinstance(n, ast.Import):
                for a in n.names:
                    anchors.setdefault(a.asname or a.name.split(".")[0],
                                       n.end_lineno or n.lineno)
            elif isinstance(n, ast.ImportFrom) and n.module and n.level == 0:
                for a in n.names:
                    if a.name != "*":
                        anchors.setdefault(a.asname or a.name,
                                           n.end_lineno or n.lineno)
        edits: list[tuple[int, str]] = []
        for alias, donors in inserts:
            at = anchors.get(alias)
            if at is None:
                continue
            ins = ("_ldm.rebind_used(%s, %r, [%s], Path(__file__))"
                   % (alias, alias, ", ".join(donors)))
            edits.append((at, ins))
        if not edits:
            continue
        if "import load_dev_module" not in src:
            lines.insert(0, "import load_dev_module as _ldm")
            for k in anchors:
                anchors[k] += 1
        if not re.search(r"^from pathlib import .*Path", src, re.M):
            lines.insert(0, "from pathlib import Path")
            for k in anchors:
                anchors[k] += 1
        for at, ins in sorted(edits, reverse=True):
            lines[at:at] = [ins]
        out = "\n".join(lines)
        ast.parse(out)
        f.write_bytes(out.encode("utf-8"))
        changed += 1
        print("%-46s +%d rebind" % (f.name, len(edits)))
    print("changed:", changed)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
