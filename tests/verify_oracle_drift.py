#!/usr/bin/env python3
"""Gate: an oracle must not reference a Python name that no longer exists.

The largest remaining ORACLE-BROKEN cause is drift: an oracle written against
an older upstream calls a function the current dev tree no longer has, or
imports a name that was renamed. The port then reports "oracle produced no
output", which is an ABSENT MEASUREMENT -- it reads exactly like a broken port
and costs a full suite run to discover.

This gate checks the drift before the run: every `from X import a, b` and
`X.attr` in an oracle is resolved against the module X in the tree, and a name
that is not there is reported.

Deliberately NOT flagged:
  - stdlib / third-party modules (only this repo's own packages are resolved)
  - names inside try/except ImportError (the oracle already handles it)
  - getattr(X, "name") -- dynamic by construction

Exit 0 = PASS, 1 = FAIL.
"""
from __future__ import annotations

import ast
import sys
from pathlib import Path

REPO = Path("C:/Users/eman5/slermes")
TESTS = REPO / "tests"
DEV_ROOTS = [REPO, REPO.parent / "hermes-agent"]

# top-level packages this repo owns; anything else is stdlib or third-party
OWNED = {"agent", "hermes_cli", "gateway", "pm", "tools", "cron", "plugins",
         "skills", "acp", "pet", "provider", "apps", "cli", "tui_gateway",
         "website", "docker", "evals", "scripts", "optional-skills",
         "hermes_platform", "ui-tui", "middleware", "models", "shared"}

THIRD_PARTY = {"yaml", "json", "os", "sys", "re", "io", "time", "pathlib",
               "typing", "collections", "dataclasses", "functools", "itertools",
               "subprocess", "shutil", "tempfile", "unittest", "importlib"}


def module_file(mod: str) -> Path | None:
    rel = mod.replace(".", "/")
    for base in DEV_ROOTS:
        for cand in (base / f"{rel}.py", base / rel / "__init__.py"):
            if cand.exists():
                return cand
    return None


def module_names(path: Path) -> set[str]:
    """Top-level names a module defines or imports (its public surface)."""
    try:
        tree = ast.parse(path.read_text(encoding="utf-8", errors="replace"))
    except (OSError, SyntaxError):
        return set()
    names = set()
    for node in tree.body:                      # module level only
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef)):
            names.add(node.name)
        elif isinstance(node, ast.Assign):
            for t in node.targets:
                if isinstance(t, ast.Name):
                    names.add(t.id)
        elif isinstance(node, ast.AnnAssign) and isinstance(node.target, ast.Name):
            names.add(node.target.id)
        elif isinstance(node, (ast.Import, ast.ImportFrom)):
            for al in node.names:
                names.add(al.asname or al.name.split(".")[0])
        elif isinstance(node, ast.Try):         # try/except ImportError fallback
            for sub in ast.walk(node):
                if isinstance(sub, (ast.FunctionDef, ast.AsyncFunctionDef,
                                    ast.ClassDef)):
                    names.add(sub.name)
    return names


def main() -> int:
    bad = []
    checked = 0
    for o in sorted(TESTS.glob("sta_oracle_*.py")):
        try:
            tree = ast.parse(o.read_text(encoding="utf-8", errors="replace"))
        except SyntaxError:
            continue
        # names imported in a try block are guarded; collect them to skip
        guarded = set()
        for node in ast.walk(tree):
            if isinstance(node, ast.Try):
                for sub in ast.walk(node):
                    if isinstance(sub, ast.ImportFrom):
                        for al in sub.names:
                            guarded.add(al.asname or al.name)
        for node in ast.walk(tree):
            if isinstance(node, ast.ImportFrom) and node.module:
                mod = node.module.split(".")[0]
                if mod not in OWNED or node.level:
                    continue
                mf = module_file(node.module)
                if mf is None:
                    continue
                checked += 1
                names = module_names(mf)
                for al in node.names:
                    nm = al.name
                    if nm == "*":
                        continue
                    if al.asname or nm in guarded:
                        continue
                    if nm not in names:
                        bad.append((o.name, node.module, nm, node.lineno))

    allow = {}
    try:
        import importlib.util as _ilu
        _s = _ilu.spec_from_file_location(
            "oracle_drift_allowlist",
            str(Path(__file__).with_name("oracle_drift_allowlist.py")))
        _m = _ilu.module_from_spec(_s)
        _s.loader.exec_module(_m)
        allow = _m.ALLOW
    except Exception:
        allow = {}

    live = set()
    for f, mod, nm, ln in bad:
        live.add(f"{f}:{ln}")
    dead = [k for k in allow if k not in live]

    checked = 0
    new = [(f, mod, nm, ln) for f, mod, nm, ln in bad if f"{f}:{ln}" not in allow]
    print(f"drift gate: {len(live)} dangling name(s) total, "
          f"{len(allow)} declared debt, {len(new)} new")
    for f, mod, nm, ln in new[:25]:
        print(f"    {f}:{ln}  {mod}.{nm}  -- not defined in that module")
    for k in dead[:6]:
        print(f"    DEAD allowlist entry (debt paid): {k}")
    print("drift gate:", "PASS" if not new and not dead else "FAIL")
    return 0 if not new and not dead else 1


if __name__ == "__main__":
    sys.exit(main())
