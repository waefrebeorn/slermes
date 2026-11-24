#!/usr/bin/env python3
"""Differential oracle for hermes_cli/update_owning_install.py (gate w25_updateowni_0f1bd1).

The live module is imported and its decision is driven with every seam pinned to
the fixture's value:

  * ``sys.prefix`` / ``sys.base_prefix`` / ``PYTHONPATH`` -> a namespace and a
    dict, so no interpreter state is read;
  * ``hermes_cli/main.py`` existence -> a ``Path`` subclass whose ``is_file``
    answers from the fixture, so nothing is created on disk;
  * the PM record read -> the same subclass' ``read_text``;
  * ``retarget_to_owning_install`` runs for real with ``sys.stdout`` captured and
    ``subprocess.call`` pinned, so the live f-strings and child argv are what is
    compared.

``pm.environments.install_key`` is the real sha256 over the canonical path.
"""
import contextlib
import io
import json
import pathlib
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_updateowni_0f1bd1"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from hermes_cli import update_owning_install as live  # noqa: E402
from pm.environments import install_key as live_install_key  # noqa: E402

WANTED = {"install_key", "owning_root", "pm_owner", "retarget", "constants"}
REAL_PATH = pathlib.Path
REAL_SYS = live.sys


class FakePath(type(REAL_PATH())):
    """The filesystem seam: existence and the PM record come from the fixture."""

    main_py = False
    record = None

    def is_file(self):
        return bool(FakePath.main_py)

    def read_text(self, encoding=None, errors=None):
        if FakePath.record is None:
            raise FileNotFoundError("no .project-root record")
        text = FakePath.record
        if encoding == "utf-8-sig" and text.startswith("\ufeff"):
            text = text[1:]  # what the utf-8-sig codec does before .strip()
        return text


def _obj(value):
    return value if isinstance(value, dict) else {}


def _str(value):
    return value if isinstance(value, str) else None


def install_key_case(req):
    path = req.get("path")
    return live_install_key(REAL_PATH(path if isinstance(path, str) else ""))


def owning_root(req):
    diag = _obj(req.get("diag"))
    pythonpath = diag.get("pythonpath")
    entries = pythonpath if isinstance(pythonpath, list) else []
    fake_sys = types.SimpleNamespace(prefix=diag.get("prefix") or "",
                                     base_prefix=diag.get("base_prefix") or "")
    fake_os = types.SimpleNamespace(environ={"PYTHONPATH": ";".join(
        e for e in entries if isinstance(e, str))}, pathsep=";")
    FakePath.main_py = diag.get("main_py") is True
    real_path, real_os = live.Path, live.os
    real_pm_owner = live._pm_environment_owner
    live.sys, live.os, live.Path = fake_sys, fake_os, FakePath
    live._pm_environment_owner = lambda _venv: (
        REAL_PATH(diag["pm_owner"]) if isinstance(diag.get("pm_owner"), str) else None)
    try:
        owner = live.owning_install_root(REAL_PATH(diag.get("project_root") or "."))
    finally:
        live.sys, live.os, live.Path = REAL_SYS, real_os, real_path
        live._pm_environment_owner = real_pm_owner
    return None if owner is None else str(owner)


def pm_owner(req):
    diag = _obj(req.get("diag"))
    FakePath.main_py = diag.get("main_py") is True
    FakePath.record = _str(diag.get("record"))
    real_path = live.Path
    live.Path = FakePath
    try:
        owner = live._pm_environment_owner(FakePath(diag.get("venv") or "."))
    finally:
        live.Path = real_path
    return None if owner is None else str(owner)


def retarget(req):
    spec = _obj(req.get("req"))
    owner = _str(spec.get("owner"))
    if owner is None:
        return None
    calls = []
    out = io.StringIO()
    fake_sys = types.SimpleNamespace(
        prefix=spec.get("prefix") or "",
        base_prefix=spec.get("prefix") or "",
        executable=spec.get("executable") or "",
        argv=[spec.get("executable") or ""] + [a for a in (spec.get("args") or [])
                                               if isinstance(a, str)],
        stdout=out,
    )
    real_sys = live.sys
    real_call = live.subprocess.call
    real_owner = live.owning_install_root

    def fake_call(argv, **kwargs):
        calls.append({"argv": [str(a) for a in argv],
                      "pythonpath": kwargs.get("env", {}).get("PYTHONPATH"),
                      "cwd": str(kwargs.get("cwd"))})
        return 0

    live.sys = fake_sys
    live.subprocess.call = fake_call
    live.owning_install_root = lambda _root: REAL_PATH(owner)
    try:
        with contextlib.redirect_stdout(out):
            live.retarget_to_owning_install(REAL_PATH(spec.get("project_root") or "."))
    except SystemExit:
        pass
    finally:
        live.sys = real_sys
        live.subprocess.call = real_call
        live.owning_install_root = real_owner
    if not calls:
        return None
    return {
        "lines": [line for line in out.getvalue().splitlines()],
        "pythonpath": calls[-1]["pythonpath"],
        "cwd": calls[-1]["cwd"],
        "argv": calls[-1]["argv"],
    }


def constants():
    return {
        "venv_names": ["venv", ".venv"],
        "module": "hermes_cli.main",
        "key_chars": 16,
        "record_path": "inputs/.project-root",
    }


def expected(r):
    f = r.get("func")
    if f == "install_key":
        return install_key_case(r)
    if f == "owning_root":
        return owning_root(r)
    if f == "pm_owner":
        return pm_owner(r)
    if f == "retarget":
        return retarget(r)
    if f == "constants":
        return constants()
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
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
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
