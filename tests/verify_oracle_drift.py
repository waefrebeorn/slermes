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
import json
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


def is_shim(path: Path) -> bool:
    """True for the stale redirect stubs this tree carries.

    Some files under the repo are not Python at all -- their whole content is a
    single absolute path (e.g. agent/anthropic_credentials.py containing
    "/home/wubu/hermes-agent-dev/agent/anthropic_credentials.py"). Resolving a
    module to one of those parses to zero top-level names, which made every
    attribute look missing and produced a wall of false drift.
    """
    try:
        raw = path.read_bytes()
    except OSError:
        return True
    if len(raw) > 512:
        return False
    txt = raw.decode("utf-8", errors="replace").strip()
    if not txt or "\n" in txt.strip():
        return False
    return txt.startswith("/") or txt.startswith("C:") or txt.startswith("~")


def module_file(mod: str) -> Path | None:
    rel = mod.replace(".", "/")
    for base in DEV_ROOTS:
        for cand in (base / f"{rel}.py", base / rel / "__init__.py"):
            if cand.exists() and not is_shim(cand):
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


def _literal_join(node: ast.AST) -> list[str] | None:
    """String literals of an `os.path.join(repo, "pkg", "mod.py")` call."""
    if not isinstance(node, ast.Call):
        return None
    fn = node.func
    if not (isinstance(fn, ast.Attribute) and fn.attr == "join"):
        return None
    if not (isinstance(fn.value, ast.Attribute) and fn.value.attr == "path"):
        return None
    if not node.args:
        return None
    base = node.args[0]
    if not (isinstance(base, ast.Name) and base.id in ("repo", "root", "base")):
        return None
    parts = []
    for a in node.args[1:]:
        if isinstance(a, ast.Constant) and isinstance(a.value, str):
            parts.append(a.value)
        else:
            return None
    return parts


def path_drift(tree: ast.AST) -> list[tuple[int, str]]:
    """Relative module paths an oracle loads that do not exist upstream.

    A `from X import Y` can resolve while the oracle's own `cand =
    os.path.join(repo, "hermes_cli", "plugin_index.py")` points at a module that
    was deleted -- the import still works, so the name-level check passes, and
    the port dies at exec_module with FileNotFoundError. Only the path check
    catches that.
    """
    out = []
    for node in ast.walk(tree):
        parts = _literal_join(node)
        if not parts:
            continue
        rel = "/".join(parts)
        if not rel.endswith(".py"):
            continue
        if not any((b / rel).exists() for b in DEV_ROOTS):
            out.append((getattr(node, "lineno", 0), rel))
    return out


def _incomplete() -> set[str]:
    """Oracle stems whose port is declared incomplete in the registry.

    Such an oracle is not run, so a stale module path inside it is declared
    debt rather than a live measurement -- flagging it would demand repairing an
    oracle that is deliberately parked.
    """
    try:
        reg = json.loads((TESTS / "oracle" / "registry.json")
                         .read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return set()
    out = set()
    for name, entry in (reg.get("ports") or {}).items():
        if isinstance(entry, dict) and entry.get("status") == "incomplete":
            oracle = entry.get("oracle") or ""
            if oracle.startswith("tests/"):
                out.add(Path(oracle).name)
    return out


def _oracle_module(tree: ast.AST) -> Path | None:
    """The live module an oracle loads by literal path, if any."""
    for node in ast.walk(tree):
        parts = _literal_join(node)
        if not parts:
            continue
        rel = "/".join(parts)
        if not rel.endswith(".py"):
            continue
        for b in DEV_ROOTS:
            cand = b / rel
            if cand.exists():
                return cand
    return None


def _arity(fn: ast.FunctionDef | ast.AsyncFunctionDef) -> tuple[int, int]:
    """(min positional, max positional); max is None-ish via a big number."""
    a = fn.args
    pos = list(a.posonlyargs) + list(a.args)
    ndef = len(a.defaults)
    minp = len(pos) - ndef
    maxp = len(pos)
    if a.vararg:
        maxp = 10 ** 6
    return minp, maxp


def sig_drift(tree: ast.AST, modpath: Path) -> list[tuple[int, str, str]]:
    """`mod.f(a, b)` where the live `f` no longer accepts that arity.

    Name-level drift is not enough: `get_bot_mode_protocol_section(h, False)`
    still resolves, then dies with "takes from 0 to 1 positional arguments but
    2 were given". That is a crash the port reports as "oracle produced no
    output", so it belongs in the pre-run gate.
    """
    try:
        modtree = ast.parse(modpath.read_text(encoding="utf-8", errors="replace"))
    except (OSError, SyntaxError):
        return []
    aliases = _module_aliases(tree)
    defs: dict[str, ast.FunctionDef | ast.AsyncFunctionDef] = {}
    for node in ast.walk(modtree):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            defs.setdefault(node.name, node)
    out = []
    for node in ast.walk(tree):
        if not isinstance(node, ast.Call):
            continue
        fn = node.func
        if not isinstance(fn, ast.Attribute):
            continue
        base = fn.value
        if not (isinstance(base, ast.Name) and base.id in aliases):
            continue
        d = defs.get(fn.attr)
        if d is None:
            continue
        minp, maxp = _arity(d)
        np_ = len(node.args)
        if np_ < minp or np_ > maxp:
            out.append((node.lineno, fn.attr,
                        f"called with {np_} positional, def takes {minp}..{maxp}"))
    return out


def _module_aliases(tree: ast.AST) -> dict[str, str]:
    """local name -> dotted module, from this oracle's import statements."""
    aliases: dict[str, str] = {}
    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            for a in node.names:
                aliases[a.asname or a.name.split(".")[0]] = a.name
        elif isinstance(node, ast.ImportFrom):
            if node.module and not node.level:
                for a in node.names:
                    if a.name != "*":
                        aliases[a.asname or a.name] = f"{node.module}.{a.name}"
    return aliases


def _resolve_module(dotted: str) -> Path | None:
    parts = dotted.split(".")
    # Quarry first: this repo carries stale path-redirect shims (see is_shim)
    # that would otherwise shadow the real upstream module.
    for b in reversed(DEV_ROOTS):
        for cand in (b / Path(*parts).with_suffix(".py"),
                     b / Path(*parts) / "__init__.py"):
            if cand.exists() and not is_shim(cand):
                return cand
    return None


def _module_top_names(path: Path) -> set[str]:
    """Names a module DEFINES at top level (defs, classes, assignments).

    Deliberately only its own definitions: an oracle reaching for a name the
    module merely re-exports is not drift in the module.
    """
    try:
        mt = ast.parse(path.read_text(encoding="utf-8", errors="replace"))
    except (OSError, SyntaxError):
        return set()
    names: set[str] = set()
    for node in mt.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef)):
            names.add(node.name)
        elif isinstance(node, ast.Assign):
            for t in node.targets:
                if isinstance(t, ast.Name):
                    names.add(t.id)
        elif isinstance(node, ast.AnnAssign) and isinstance(node.target, ast.Name):
            names.add(node.target.id)
        elif isinstance(node, (ast.Import, ast.ImportFrom)):
            for a in node.names:
                names.add(a.asname or a.name.split(".")[0])
    return names


def _rebound_aliases(tree: ast.AST) -> set[str]:
    """Aliases an oracle restores at runtime via load_dev_module.rebind_*.

    `rebind_used(kdb, "kdb", [...])` takes the missing names from sibling
    modules, so the attribute is legitimately absent from the module it was
    imported from. This is a repair, not drift, and the gate must not report it.
    """
    out = set()
    for node in ast.walk(tree):
        if not isinstance(node, ast.Call):
            continue
        f = node.func
        name = f.id if isinstance(f, ast.Name) else (
            f.attr if isinstance(f, ast.Attribute) else "")
        if name not in ("rebind_used", "rebind_moved") or len(node.args) < 2:
            continue
        a0 = node.args[0]
        if isinstance(a0, ast.Name):
            out.add(a0.id)
        elif isinstance(node.args[1], ast.Constant) and isinstance(node.args[1].value, str):
            out.add(node.args[1].value)
    return out


def attr_drift(tree: ast.AST) -> list[tuple[int, str, str]]:
    """`alias._helper` where the live module no longer defines `_helper`.

    Name-level drift (`from X import Y`) cannot see this: `from hermes_cli
    import banner as m` resolves fine, then `m._canonical_github_remote(...)`
    dies with AttributeError and the port reports "oracle produced no output".
    That is an absent measurement, so it belongs in this gate.

    Restricted to underscore-prefixed attributes: those are the module-private
    helpers an oracle targets, which keeps re-exported and method-style accesses
    out of the result.
    """
    aliases = _module_aliases(tree)
    if not aliases:
        return []
    rebound = _rebound_aliases(tree)
    cache: dict[str, set[str] | None] = {}
    out: list[tuple[int, str, str]] = []
    seen: set[tuple[int, str]] = set()
    for node in ast.walk(tree):
        if not isinstance(node, ast.Attribute):
            continue
        if not isinstance(node.value, ast.Name):
            continue
        if not node.attr.startswith("_"):
            continue
        dotted = aliases.get(node.value.id)
        if not dotted or node.value.id in rebound:
            continue
        if dotted not in cache:
            p = _resolve_module(dotted)
            cache[dotted] = _module_top_names(p) if p else None
        names = cache[dotted]
        if names is None:
            continue            # the module itself is gone: path_drift owns it
        if node.attr in names:
            continue
        key = (node.lineno, node.attr)
        if key in seen:
            continue
        seen.add(key)
        out.append((node.lineno, node.attr, f"not defined in {dotted}"))
    return out


def main() -> int:
    import sys as _sys
    dump = "--dump" in _sys.argv
    incomplete = _incomplete()
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

    pathbad = []
    sigbad = []
    attrbad = []
    for o in sorted(TESTS.glob("sta_oracle_*.py")):
        if o.name in incomplete:
            continue
        try:
            tree = ast.parse(o.read_text(encoding="utf-8", errors="replace"))
        except SyntaxError:
            continue
        for ln, rel in path_drift(tree):
            pathbad.append((o.name, ln, rel))
        modpath = _oracle_module(tree)
        if modpath is not None:
            for ln, name, why in sig_drift(tree, modpath):
                sigbad.append((o.name, ln, f"{name}: {why}"))
        for ln, name, why in attr_drift(tree):
            attrbad.append((o.name, ln, f"{name}: {why}"))

    live = set()
    for f, mod, nm, ln in bad:
        live.add(f"{f}:{ln}")
    for f, ln, _rel in pathbad:
        live.add(f"{f}:{ln}")
    for f, ln, _w in sigbad:
        live.add(f"{f}:{ln}")
    for f, ln, _w in attrbad:
        live.add(f"{f}:{ln}")
    dead = [k for k in allow if k not in live]

    checked = 0
    new = [(f, mod, nm, ln) for f, mod, nm, ln in bad if f"{f}:{ln}" not in allow]
    newpaths = [(f, ln, rel) for f, ln, rel in pathbad if f"{f}:{ln}" not in allow]
    print(f"drift gate: {len(live)} dangling name(s) total, "
          f"{len(allow)} declared debt, {len(new)} new")
    print(f"drift gate: {len(pathbad)} missing module path(s), "
          f"{len(newpaths)} new")
    for f, mod, nm, ln in new[:25]:
        print(f"    {f}:{ln}  {mod}.{nm}  -- not defined in that module")
    for f, ln, rel in newpaths[:25]:
        print(f"    {f}:{ln}  {rel}  -- no such module in either tree")
    newsigs = [(f, ln, w) for f, ln, w in sigbad if f"{f}:{ln}" not in allow]
    print(f"drift gate: {len(sigbad)} arity mismatch(es), {len(newsigs)} new")
    for f, ln, w in newsigs[:25]:
        print(f"    {f}:{ln}  {w}")
    newattrs = ([] if dump else
                [(f, ln, w) for f, ln, w in attrbad if f"{f}:{ln}" not in allow])
    if dump:
        # Every finding, uncapped and unfiltered: reconciliation needs the true
        # live set, and the normal report prints only NEW findings (max 25 per
        # class), which can never show a dead entry.
        for f, mod, nm, ln in bad:
            print(f"    {f}:{ln}  {mod}.{nm}  -- not defined in that module")
        for f, ln, rel in pathbad:
            print(f"    {f}:{ln}  {rel}  -- no such module in either tree")
        for f, ln, w in sigbad:
            print(f"    {f}:{ln}  {w}")
        for f, ln, w in attrbad:
            print(f"    {f}:{ln}  {w}")
        return 0
    print(f"drift gate: {len(attrbad)} attribute mismatch(es), {len(newattrs)} new")
    for f, ln, w in newattrs[:25]:
        print(f"    {f}:{ln}  {w}")
    for k in dead[:6]:
        print(f"    DEAD allowlist entry (debt paid): {k}")
    ok = not new and not dead and not newpaths and not newsigs and not newattrs
    print("drift gate:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
