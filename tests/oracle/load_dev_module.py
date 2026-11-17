#!/usr/bin/env python3
"""Load a dev-tree module for an oracle, correctly.

`importlib.util.spec_from_file_location` + `module_from_spec` + `exec_module`
does NOT put the module in `sys.modules`. That is fine until the module uses a
library that resolves the caller's globals through `sys.modules` -- `@dataclass`
does exactly that (`sys.modules.get(cls.__module__).__dict__`), so a dataclass
defined in a loaded module dies with

    AttributeError: 'NoneType' object has no attribute '__dict__'

which looks like a parity failure but is the loader. Registering before exec
fixes it, and the fix belongs in one shared place rather than in every oracle.

Usage, replacing the four-line idiom:

    import load_dev_module
    mod = load_dev_module.load("live_console_engine", ["hermes_cli/console_engine.py"])
"""
from __future__ import annotations

import importlib.util
import sys
from pathlib import Path

REPO = Path("C:/Users/eman5/slermes")


def roots() -> list[Path]:
    """Where a Python module may legitimately live, in search order."""
    out = []
    for env in ("SLERMES_DEV_ROOT", "SLERMES_ROOT"):
        import os
        v = os.environ.get(env)
        if v:
            out.append(Path(v))
    out += [REPO, REPO.parent / "hermes-agent"]
    return out


def find(rel: str):
    p = Path(rel)
    if p.is_absolute() and p.exists():
        return p
    for r in roots():
        c = r / rel
        if c.exists():
            return c
    return None


def rebind_moved(mod, attrs, modules):
    """Restore attributes upstream moved out of `mod`.

    `from hermes_cli import kanban_db as kdb` then `kdb._scratch_tip_shown(...)`
    dies with AttributeError once the helper moves to kanban_db_workspace. The
    oracle's contract is with the module surface it was written against, so take
    the name from wherever it now lives instead of allowlisting the drift as an
    absent measurement.

    `attrs` are the names the oracle reaches on `mod`; `modules` are candidate
    donors (module objects or dotted names) in priority order. A name `mod`
    still defines itself is never overridden.
    """
    import importlib
    donors = [importlib.import_module(m) if isinstance(m, str) else m
              for m in modules]
    for a in attrs:
        if hasattr(mod, a):
            continue
        for d in donors:
            if hasattr(d, a):
                setattr(mod, a, getattr(d, a))
                break
    return mod


def rebind_used(mod, alias: str, modules, source: Path):
    """Rebind every `<alias>.NAME` this oracle source actually reaches for.

    Writing the moved names by hand means one AttributeError per missing name;
    deriving them from the AST fixes the whole surface in one edit and cannot
    drift from what the oracle really calls.
    """
    import ast
    tree = ast.parse(Path(source).read_text(encoding="utf-8", errors="replace"))
    names = sorted({n.attr for n in ast.walk(tree)
                    if isinstance(n, ast.Attribute)
                    and isinstance(n.value, ast.Name) and n.value.id == alias})
    return rebind_moved(mod, names, modules)


def load(name: str, candidates: list[str], package: str | None = None):
    """Load the first existing candidate under `name` and register it."""
    for rel in candidates:
        path = find(rel)
        if path is None:
            continue
        spec = importlib.util.spec_from_file_location(name, path)
        if spec is None or spec.loader is None:
            continue
        mod = importlib.util.module_from_spec(spec)
        # THE FIX: dataclasses (and anything else that looks up the defining
        # module) require sys.modules[name] to exist before the body runs.
        sys.modules[name] = mod
        try:
            spec.loader.exec_module(mod)
        except BaseException:
            sys.modules.pop(name, None)
            raise
        return mod
    raise ImportError(f"none of {candidates} found; roots={[str(r) for r in roots()]}")
