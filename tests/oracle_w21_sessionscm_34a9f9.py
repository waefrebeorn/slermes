#!/usr/bin/env python3
"""Differential oracle for hermes_cli/sessions_cmd_journal_mode.py
(gate w21_sessionscm_34a9f9).

`cmd_set_journal_mode` is a console command whose whole observable behaviour is
its printed lines plus the exit code, so the oracle runs the live function with
stdout captured and compares {"code", "lines"} to the C port's `out`.

Four seams are stubbed, each a thing the command cannot decide locally:

  persistence.PersistenceError        -- the store's error type
  persistence.core.admin.AdminUnsupported
  persistence_assembly.open_store_admin / profile_scope / store_location
  sqlite_support.resolve_journal_mode -- the config's configured mode

The sqlite handle itself is the returned admin object, whose
`set_journal_mode(target, force=...)` answers from the fixture (or raises
PersistenceError). The database path is a real relative file created in a temp
directory the oracle chdirs into, so `Path(db_path).exists()` is the live test.
"""
import contextlib
import importlib.util
import io
import json
import os
import shutil
import sys
import tempfile
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "hermes_cli" / "sessions_cmd_journal_mode.py"
GATE = "w21_sessionscm_34a9f9"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("sessions_cmd_journal_mode_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["sessions_cmd_journal_mode_live"] = live
_spec.loader.exec_module(live)

WANTED = {"run"}


class PersistenceError(Exception):
    pass


class AdminUnsupported:
    pass


class Admin:
    def __init__(self, case):
        self.case = case

    def set_journal_mode(self, target, force=False):
        err = self.case.get("error")
        if err:
            raise PersistenceError(err)
        result = self.case.get("result") or {}
        return result.get("current"), result.get("after")


def install(case):
    """Bind the four seams to *case*; returns the previous sys.modules entries."""
    persistence = types.ModuleType("persistence")
    persistence.PersistenceError = PersistenceError
    core = types.ModuleType("persistence.core")
    core.__path__ = []
    core_admin = types.ModuleType("persistence.core.admin")
    core_admin.AdminUnsupported = AdminUnsupported
    persistence.core = core
    core.admin = core_admin

    assembly = types.ModuleType("persistence_assembly")
    assembly.profile_scope = lambda: None
    assembly.store_location = lambda *a, **k: case.get("store_location")

    def _open(db, read_only=False):
        kind = case.get("admin", "ok")
        if kind == "none":
            return None
        if kind == "unsupported":
            return AdminUnsupported()
        return Admin(case)

    assembly.open_store_admin = _open

    sqlite_support = types.ModuleType("sqlite_support")
    sqlite_support.resolve_journal_mode = lambda: case.get("configured")

    names = {"persistence": persistence, "persistence.core": core,
             "persistence.core.admin": core_admin, "persistence_assembly": assembly,
             "sqlite_support": sqlite_support}
    saved = {k: sys.modules.get(k) for k in names}
    sys.modules.update(names)
    return saved


def restore(saved):
    for k, v in saved.items():
        if v is None:
            sys.modules.pop(k, None)
        else:
            sys.modules[k] = v


def expected(r):
    if r.get("func") != "run":
        raise AssertionError("unknown func %r" % (r.get("func"),))
    args = types.SimpleNamespace(mode=r.get("mode"), db=r.get("db"),
                                 force=bool(r.get("force", False)))
    saved = install(r)
    buf = io.StringIO()
    try:
        with contextlib.redirect_stdout(buf):
            code = live.cmd_set_journal_mode(args)
    finally:
        restore(saved)
    return {"code": code, "lines": buf.getvalue().splitlines()}


def main():
    workdir = tempfile.mkdtemp(prefix="sjm_")
    old = os.getcwd()
    os.chdir(workdir)
    cases = mism = 0
    try:
        for line in sys.stdin:
            line = line.strip()
            if not line:
                continue
            try:
                r = json.loads(line)
            except ValueError:
                continue
            if not isinstance(r, dict) or r.get("func") not in WANTED:
                continue
            cases += 1
            # The filesystem seam: make Path(db_path).exists() answer the case.
            for rel in (r.get("db"), r.get("store_location")):
                if isinstance(rel, str) and rel:
                    try:
                        if r.get("db_exists"):
                            Path(rel).parent.mkdir(parents=True, exist_ok=True)
                            Path(rel).touch()
                        elif Path(rel).exists():
                            Path(rel).unlink()
                    except OSError:
                        pass
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
    finally:
        os.chdir(old)
        shutil.rmtree(workdir, ignore_errors=True)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
