#!/usr/bin/env python3
"""Differential oracle for agent/checkpoints/manager_profile_rename.py
(gate w21_managerpro_68c845).

The module only binds a git store and the filesystem, so the two functions are
lifted out of the AST and executed against a fake store: `_rebase_ledger_paths`
is pure (pathlib), `migrate_profile_checkpoint_projects` sees stubbed
cm._normalize_path / _store_has_head / _list_projects / _rekey_project and
shutil.which. `_normalize_path` is ``Path(s)`` here because its real body only
resolves the live filesystem.
"""
import __future__
import ast
import json
import os
import shutil
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
SRC = Path(Q) / "agent" / "checkpoints" / "manager_profile_rename.py"
_ANN = __future__.annotations.compiler_flag


class _Logger:
    def warning(self, *a, **k):
        pass


CM = types.SimpleNamespace(
    _normalize_path=lambda s: Path(s),
    _store_path=lambda base: Path(base) / "checkpoints",
    _store_has_head=lambda store: bool(WORLD.get("has_head")),
    _list_projects=lambda store: [dict(m) for m in (WORLD.get("projects") or [])],
)
MAINT = types.SimpleNamespace(_delete_ref=lambda store, ref: True)
WORLD = {}


def _rekey_project(store, meta, old_workdir, new_workdir):
    """The real _rekey_project: git ref moves + ledger writes (store seam)."""
    if meta.get("fail"):
        raise OSError("target identity already exists: rekey refused")


G = {"__name__": "mpr_live", "Path": Path, "Dict": dict, "cm": CM,
     "maintenance": MAINT, "logger": _Logger(), "json": json, "shutil": shutil,
     "_rekey_project": _rekey_project}


def _exec(path, names, into):
    tree = ast.parse(path.read_text(encoding="utf-8"))
    for node in ast.walk(tree):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef)) and \
                node.name in names:
            node.decorator_list = []
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(path), "exec",
                         flags=_ANN, dont_inherit=True), into)


_exec(SRC, {"_rebase_ledger_paths", "migrate_profile_checkpoint_projects"}, G)


def expected(r):
    f = r.get("func")
    if f == "env_probe":
        sle = os.environ.get("SLERMES_HOME") or ""
        home = os.environ.get("HOME") or ""
        return {"sler": bool(sle), "home": bool(home), "same": bool(sle) and sle == home}
    if f == "rebase":
        ledger = r.get("ledger")
        if not isinstance(ledger, dict):
            return {}
        return G["_rebase_ledger_paths"](ledger, Path(r.get("old_workdir") or ""),
                                         Path(r.get("new_workdir") or ""))
    if f == "migrate":
        global WORLD
        w = r.get("world") if isinstance(r.get("world"), dict) else {}
        WORLD = w
        # The store lives on the filesystem; the fixture declares which renamed
        # workdirs exist, so Path.is_dir is answered from that declaration.
        dirs = set()
        for meta in w.get("projects") or []:
            if not isinstance(meta, dict) or not meta.get("exists") or not meta.get("workdir"):
                continue
            try:
                rel = Path(meta["workdir"]).relative_to(Path(w.get("old_root") or ""))
            except ValueError:
                continue
            dirs.add(str(Path(w.get("new_root") or "") / rel))
        old_git, old_isdir = shutil.which, Path.is_dir
        shutil.which = lambda name: "/usr/bin/git" if w.get("git") else None
        Path.is_dir = lambda self: str(self) in dirs
        try:
            return G["migrate_profile_checkpoint_projects"](
                Path(w.get("old_root") or ""), Path(w.get("new_root") or ""))
        finally:
            shutil.which, Path.is_dir = old_git, old_isdir
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    wanted = {"rebase", "migrate", "env_probe"}
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in wanted:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE w21_managerpro_68c845: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
