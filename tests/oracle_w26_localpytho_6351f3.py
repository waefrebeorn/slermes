#!/usr/bin/env python3
"""Differential oracle for tools/environments/local_pythonpath.py (gate w26_localpytho_6351f3).

The live module is imported and its pure provenance helpers are called directly.
The filesystem seam is fixed: ``_state()`` and ``_get_hermes_site_packages()`` are
replaced so the owned-path list is exactly the fixture's, and what is compared is
the decision those values make -- normcased-part path equality and the resulting
PYTHONPATH rewrite.  An exception is reported as ``{"error": <type name>}`` so the
C side can mirror the same seam without copying Python's message prose.
"""
import copy
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_localpytho_6351f3"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

# The live import chain resolves the Hermes home through Path.home(); give it one.
if not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = str(REPO.parent)
if not os.environ.get("HOME"):
    os.environ["HOME"] = str(REPO.parent)

from tools.environments import local_pythonpath as live  # noqa: E402

WANTED = {"same_path", "strip_pythonpath", "strip_all", "constants"}


def _patch_owned(raw):
    owned = [Path(p) for p in (raw or [])]

    class _State:
        _hermes_repo_root_aliases = tuple(owned)

    live._state = lambda: _State
    live._get_hermes_site_packages = lambda env: []


def same_path(req):
    return live._same_path(Path(req.get("left")), Path(req.get("right")))


def _with_sep(req):
    """Honour the fixture's pathsep: the live module reads os.pathsep at call time."""
    sep = req.get("sep", os.pathsep)
    previous = os.pathsep
    os.pathsep = sep
    return previous


def strip(req):
    env = copy.deepcopy(req.get("env"))
    _patch_owned(req.get("owned_paths"))
    previous = _with_sep(req)
    try:
        live._strip_hermes_owned_pythonpath(env)
    finally:
        os.pathsep = previous
    return {"env": env}


def strip_all(req):
    env = copy.deepcopy(req.get("env"))
    _patch_owned(req.get("owned_paths"))
    previous = _with_sep(req)
    try:
        live._strip_hermes_owned_pythonpath_and_runtime_markers(env)
    finally:
        os.pathsep = previous
    return {"env": env}


def constants(req):
    return {
        "markers": list(live._ACTIVE_VENV_MARKER_VARS),
        "is_windows": bool(live._IS_WINDOWS),
        "pathsep": os.pathsep,
    }


FUNCS = {"same_path": same_path, "strip_pythonpath": strip,
         "strip_all": strip_all, "constants": constants}


def expected(req):
    try:
        return FUNCS[req["func"]](req)
    except Exception as exc:  # noqa: BLE001
        return {"error": type(exc).__name__}


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            req = json.loads(line)
        except ValueError:
            continue
        if not isinstance(req, dict) or req.get("func") not in WANTED:
            continue
        cases += 1
        py = expected(req)
        got = req.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (req.get("func"), req.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
