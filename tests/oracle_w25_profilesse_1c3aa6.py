#!/usr/bin/env python3
"""Differential oracle for hermes_cli/profiles_service_cleanup.py
(gate w25_profilesse_1c3aa6).

The live module is imported and all three arms are called for real, with their
seams handed fixed inputs:

  * ``owned_by_profile``: ``gateway_service_owner.pinned_home`` returns the
    fixture's ``raw`` and ``_resolve`` maps each side to the fixture's resolved
    path (None models a failed resolve), so no unit/plist is parsed.
  * ``remove_system_systemd_unit``: the unit is a real file in a temp dir (so
    ``exists()``/``unlink()`` are live), ``get_service_name`` and the unit dir are
    fixed, ``os.geteuid`` answers the fixture's ``is_root`` and ``subprocess.run``
    is a stub returning the fixture's ``stop_rc`` -- nothing is executed.
  * ``remove_windows_task``: the startup/legacy login items are real temp files,
    ``schtasks`` is a stub returning the fixture's code and stderr.

Temp paths are normalised back to the fixture's spelling before comparing: the
path string is a caller-supplied value, not something the function decides.  The
``↷`` line ``owned_by_profile`` prints is dropped from the unit arm's output --
that notice is the separately ported ``owned_by_profile``.
"""
import ast
import contextlib
import inspect
import io
import json
import os
import sys
import tempfile
import textwrap
from pathlib import Path, PurePosixPath

# Env seam: the live package may resolve Path.home() at import time and ntpath
# only knows USERPROFILE; give it one when the caller's shell dropped it.
if not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = os.environ.get("HOME") or tempfile.gettempdir()

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_profilesse_1c3aa6"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

import hermes_cli.gateway as gateway  # noqa: E402
import hermes_cli.gateway_service_owner as owner  # noqa: E402
import hermes_cli.gateway_windows as gateway_windows  # noqa: E402
from hermes_cli import profiles_service_cleanup as live  # noqa: E402

WANTED = {"owned_by_profile", "system_unit", "windows_task", "constants"}


def _unit_suffix():
    tree = ast.parse(textwrap.dedent(inspect.getsource(live.remove_system_systemd_unit)))
    for node in ast.walk(tree):
        if isinstance(node, ast.JoinedStr):
            tail = [v.value for v in node.values if isinstance(v, ast.Constant)]
            if tail and isinstance(tail[-1], str) and tail[-1].startswith("."):
                return tail[-1]
    raise AssertionError('no f"{svc_name}<suffix>" in remove_system_systemd_unit')


UNIT_SUFFIX = _unit_suffix()


def _owner_hooks(raw, resolved_raw, resolved_profile, profile_dir):
    def _resolve(p):
        text = None if p is None else str(p)
        if text == (None if raw is None else str(raw)):
            return None if resolved_raw is None else PurePosixPath(resolved_raw)
        if text == str(profile_dir):
            return None if resolved_profile is None else PurePosixPath(resolved_profile)
        return PurePosixPath(text) if text else None
    return (lambda definition: raw), _resolve


def owned_by_profile(r):
    raw = r.get("raw")
    # The module is POSIX-host code: its Paths render with "/", so the fixture's
    # paths are PurePosixPath here (a Windows Path would render "\\etc\\...")).
    definition = PurePosixPath(r.get("definition"))
    profile_dir = PurePosixPath(r.get("profile_dir"))
    pinned, resolve = _owner_hooks(raw, r.get("resolved_raw"), r.get("resolved_profile"),
                                   profile_dir)
    saved = (owner.pinned_home, owner._resolve)
    owner.pinned_home, owner._resolve = pinned, resolve
    buf = io.StringIO()
    try:
        with contextlib.redirect_stdout(buf):
            got = live.owned_by_profile(definition, profile_dir)
    finally:
        owner.pinned_home, owner._resolve = saved
    return {"owned": got, "notice": buf.getvalue().strip() or None}


class _Done:
    def __init__(self, returncode):
        self.returncode = returncode


class _UnitDir:
    """gateway._SYSTEM_UNIT_DIR whose "/" lands on the real fixture unit."""

    def __init__(self, path):
        self._path = path

    def __truediv__(self, name):
        return self._path


def system_unit(r):
    svc = r.get("svc_name")
    is_root = bool(r.get("is_root"))
    stop_rc = r.get("stop_rc") if isinstance(r.get("stop_rc"), int) else 0
    tmp = Path(tempfile.mkdtemp(prefix="w25_unit_"))
    unit = tmp / (svc + UNIT_SUFFIX)
    if r.get("exists"):
        unit.write_text("# unit\n", encoding="utf-8")
    profile_dir = PurePosixPath("/w25/profile")

    owned = bool(r.get("owned"))
    pinned, resolve = _owner_hooks(None if owned else "/elsewhere/.hermes",
                                   None, str(profile_dir), profile_dir)

    saved = (owner.pinned_home, owner._resolve, gateway._SYSTEM_UNIT_DIR,
             gateway.get_service_name, live.subprocess.run,
             getattr(os, "geteuid", None))
    calls = []

    def _run(args, **kw):
        calls.append(list(args))
        return _Done(stop_rc)

    owner.pinned_home, owner._resolve = pinned, resolve
    gateway._SYSTEM_UNIT_DIR = _UnitDir(unit)
    gateway.get_service_name = lambda: svc
    live.subprocess.run = _run
    os.geteuid = lambda: 0 if is_root else 1000

    buf = io.StringIO()
    try:
        with contextlib.redirect_stdout(buf):
            got = live.remove_system_systemd_unit(profile_dir)
    finally:
        (owner.pinned_home, owner._resolve, gateway._SYSTEM_UNIT_DIR,
         gateway.get_service_name, live.subprocess.run, saved_euid) = saved
        if saved_euid is None:
            if hasattr(os, "geteuid"):
                del os.geteuid
        else:
            os.geteuid = saved_euid

    unit_str = r.get("unit")
    lines = [ln.replace(str(unit), unit_str) for ln in buf.getvalue().splitlines()]
    lines = [ln for ln in lines if not ln.startswith("↷ ")]
    return {"removed": got, "messages": lines}


def windows_task(r):
    task = r.get("task_name")
    registered = bool(r.get("registered"))
    code = r.get("delete_code") if isinstance(r.get("delete_code"), int) else 0
    err = r.get("err") or ""
    entries = r.get("entries") if isinstance(r.get("entries"), list) else []
    tmp = Path(tempfile.mkdtemp(prefix="w25_task_"))

    mapping = {}
    real_paths = []
    for i in range(2):
        real = tmp / ("entry%d" % i)
        exists = False
        if len(entries) > i and isinstance(entries[i], list) and len(entries[i]) == 2:
            exists = bool(entries[i][1])
            mapping[str(real)] = entries[i][0]
        if exists:
            real.write_text("login item\n", encoding="utf-8")
        real_paths.append(real)

    saved = (gateway_windows.get_task_name, gateway_windows.is_task_registered,
             gateway_windows._exec_schtasks, gateway_windows.get_startup_entry_path,
             gateway_windows._legacy_startup_entry_path)
    gateway_windows.get_task_name = lambda: task
    gateway_windows.is_task_registered = lambda: registered
    gateway_windows._exec_schtasks = lambda args, timeout=None: (code, "", err)
    gateway_windows.get_startup_entry_path = lambda: real_paths[0]
    gateway_windows._legacy_startup_entry_path = lambda: real_paths[1]

    buf = io.StringIO()
    try:
        with contextlib.redirect_stdout(buf):
            got = live.remove_windows_task()
    finally:
        (gateway_windows.get_task_name, gateway_windows.is_task_registered,
         gateway_windows._exec_schtasks, gateway_windows.get_startup_entry_path,
         gateway_windows._legacy_startup_entry_path) = saved

    lines = []
    for line in buf.getvalue().splitlines():
        for real, shown in mapping.items():
            line = line.replace(real, shown)
        lines.append(line)
    return {"removed": got, "messages": lines}


def constants():
    return {
        "system_unit_dir": Path(gateway._SYSTEM_UNIT_DIR).as_posix(),
        "unit_suffix": UNIT_SUFFIX,
        "run_timeout_s": 30.0,
        "schtasks_timeout_s": float(gateway_windows._SCHTASKS_TIMEOUT_S),
    }


def expected(r):
    f = r.get("func")
    if f == "owned_by_profile":
        return owned_by_profile(r)
    if f == "system_unit":
        return system_unit(r)
    if f == "windows_task":
        return windows_task(r)
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
