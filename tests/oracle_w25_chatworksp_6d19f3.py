#!/usr/bin/env python3
"""Differential oracle for hermes_cli/web_routers/chat_workspaces.py
(gate w25_chatworksp_6d19f3).

The live router module is imported and ``resolve_chat_cwd`` is called for real.
Its seams are given as fixed inputs: ``os.path`` is swapped for a shim for the
duration of the call, so ``expanduser`` resolves against the fixture's ``home``,
``abspath`` against the fixture's ``cwd`` (POSIX path semantics -- the dashboard
runs on Linux) and ``isdir`` answers the fixture's flag instead of touching the
disk.  ``_collect_workspaces`` itself is I/O; what is compared is its returned
dict literal, whose key order the oracle reads out of the live source so the C
payload cannot drift from it.
"""
import ast
import inspect
import json
import os
import posixpath
import sys
import tempfile
import textwrap
from pathlib import Path

# Env seam: the live package resolves Path.home() at import time and ntpath only
# knows USERPROFILE; give it one when the caller's shell dropped it.
if not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = os.environ.get("HOME") or tempfile.gettempdir()

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_chatworksp_6d19f3"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from fastapi import HTTPException  # noqa: E402

from hermes_cli.web_routers import chat_workspaces as live  # noqa: E402

WANTED = {"resolve_chat_cwd", "payload", "constants"}


class _PathShim:
    """os.path with the host seams replaced by the fixture's answers."""

    def __init__(self, cwd, home, isdir_flag):
        self._cwd = cwd if isinstance(cwd, str) and cwd else "/"
        self._home = home if isinstance(home, str) else ""
        self._isdir = isdir_flag

    def expanduser(self, p):
        if not p.startswith("~"):
            return p
        if p == "~":
            return self._home
        if p.startswith("~/"):
            return self._home + p[1:]
        # "~user" needs the account database: an unported seam, no fixture uses it.
        raise KeyError("no such user")

    def abspath(self, p):
        if not posixpath.isabs(p):
            p = posixpath.join(self._cwd, p)
        return posixpath.normpath(p)

    def isdir(self, p):
        return self._isdir


def _payload_keys():
    tree = ast.parse(textwrap.dedent(inspect.getsource(live._collect_workspaces)))
    for node in ast.walk(tree):
        if isinstance(node, ast.Return) and isinstance(node.value, ast.Dict):
            return [k.value for k in node.value.keys if isinstance(k, ast.Constant)]
    raise AssertionError("no returned dict literal in _collect_workspaces")


PAYLOAD_KEYS = _payload_keys()


def _with_shim(cwd, home, isdir_flag, call):
    saved = os.path
    os.path = _PathShim(cwd, home, isdir_flag)
    try:
        return call()
    finally:
        os.path = saved


def resolve_chat_cwd(r):
    try:
        return _with_shim(r.get("cwd"), r.get("home"), r.get("isdir"),
                          lambda: live.resolve_chat_cwd(r.get("raw")))
    except HTTPException as exc:
        return {"status": exc.status_code, "detail": exc.detail}


def payload(r):
    if PAYLOAD_KEYS != ["projects", "repos", "default_cwd", "home", "scan_enabled"]:
        raise AssertionError("live payload keys changed: %r" % (PAYLOAD_KEYS,))
    projects = r.get("projects")
    repos = r.get("repos")
    home = r.get("home")
    out = {}
    for key in PAYLOAD_KEYS:
        if key == "projects":
            out[key] = projects if isinstance(projects, list) else []
        elif key == "repos":
            out[key] = repos if isinstance(repos, list) else []
        elif key == "default_cwd":
            out[key] = r.get("default_cwd")
        elif key == "home":
            out[key] = home if isinstance(home, str) else ""
        elif key == "scan_enabled":
            out[key] = bool(r.get("enabled"))
    return out


def constants():
    probe = "__w25_probe__"
    try:
        _with_shim("/", "/home/u", False, lambda: live.resolve_chat_cwd(probe))
        raise AssertionError("live accepted a path that is not a directory")
    except HTTPException as exc:
        return {
            "payload_keys": PAYLOAD_KEYS,
            "bad_cwd_status": exc.status_code,
            "error_template": exc.detail.replace(probe, "{text}"),
        }


def expected(r):
    f = r.get("func")
    if f == "resolve_chat_cwd":
        return resolve_chat_cwd(r)
    if f == "payload":
        return payload(r)
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
