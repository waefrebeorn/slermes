#!/usr/bin/env python3
"""Differential oracle for hermes_cli/auth_external_process.py
(gate w24_authextern_606ea5).

The live module is imported and its four functions are called with the I/O
seams answering from the case:

  * ``external_process_command`` over a profile built from the case, with the
    case's env values really set in ``os.environ`` for the call;
  * ``resolve_external_process_command`` with ``locate_command`` /
    ``find_claude_code_cli`` stubbed: the resolver records the lookup string it
    was handed and finds nothing, so the compared value is exactly the lookup
    path the live code computed (the filesystem walk itself is the seam);
  * ``external_process_launch_env_names`` and ``env_writer_launch_denylist``
    with ``providers.list_providers`` / ``left_core_migration.launch_env_names``
    answering the case's tables.

A live exception is the seam's error answer: the C port returns null there, so
the expected value is None.
"""
import json
import os
import sys
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_authextern_606ea5"

_HOME = REPO.parent
for _key, _val in (("USERPROFILE", str(_HOME)),
                   ("LOCALAPPDATA", str(_HOME / "AppData" / "Local")),
                   ("APPDATA", str(_HOME / "AppData" / "Roaming"))):
    if not os.environ.get(_key):
        os.environ[_key] = _val

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

import hermes_cli.auth_external_process as live  # noqa: E402
import hermes_platform.resolver as resolver  # noqa: E402
import agent.anthropic_adapter as claude  # noqa: E402
import providers  # noqa: E402
import hermes_cli.left_core_migration as left_core  # noqa: E402

WANTED = {"command", "lookup", "launch_env_names", "denylist"}


def _set_env(env):
    applied = []
    for name, value in (env or {}).items():
        applied.append((name, os.environ.get(name)))
        os.environ[name] = value if isinstance(value, str) else str(value)
    return applied


def _restore_env(applied):
    for name, previous in applied:
        if previous is None:
            os.environ.pop(name, None)
        else:
            os.environ[name] = previous


def command(r):
    env_vars = tuple(r.get("env_vars") or [])
    profile = SimpleNamespace(process_command_env_vars=env_vars)
    if "process_command" in r:
        profile.process_command = r["process_command"]
    applied = _set_env(r.get("env"))
    try:
        return live.external_process_command(profile)
    finally:
        _restore_env(applied)


def lookup(r):
    captured = []

    class Result:
        command = None

    def fake_locate(path, known_dirs=None):
        captured.append(path)
        return Result()

    orig_locate = resolver.locate_command
    orig_find = claude.find_claude_code_cli
    cwd = r.get("cwd")
    home = r.get("home")
    prev_cwd = os.getcwd()
    prev_home = os.environ.get("USERPROFILE")
    applied = _set_env(r.get("env"))
    resolver.locate_command = fake_locate
    claude.find_claude_code_cli = lambda path: "STUB"
    try:
        if isinstance(home, str):
            os.environ["USERPROFILE"] = home
        if isinstance(cwd, str):
            os.chdir(cwd)
        live.resolve_external_process_command(r.get("command"))
    finally:
        resolver.locate_command = orig_locate
        claude.find_claude_code_cli = orig_find
        _restore_env(applied)
        os.chdir(prev_cwd)
        if prev_home is None:
            os.environ.pop("USERPROFILE", None)
        else:
            os.environ["USERPROFILE"] = prev_home
    return captured[0] if captured else None


def _profiles(raw):
    out = []
    for entry in raw if isinstance(raw, list) else []:
        if isinstance(entry, dict):
            out.append(SimpleNamespace(**entry))
    return out


def launch_env_names(r):
    objs = _profiles(r.get("profiles"))
    orig = providers.list_providers
    providers.list_providers = lambda: objs
    try:
        return sorted(live.external_process_launch_env_names())
    finally:
        providers.list_providers = orig


def denylist(r):
    objs = _profiles(r.get("profiles"))
    raw = r.get("launch_env")
    left = [n for n in (raw if isinstance(raw, list) else [])]
    orig_providers = providers.list_providers
    orig_left = left_core.launch_env_names
    providers.list_providers = lambda: objs
    left_core.launch_env_names = lambda: frozenset(left)
    try:
        return sorted(live.env_writer_launch_denylist())
    finally:
        providers.list_providers = orig_providers
        left_core.launch_env_names = orig_left


def expected(r):
    f = r.get("func")
    if f == "command":
        return command(r)
    if f == "lookup":
        return lookup(r)
    if f == "launch_env_names":
        return launch_env_names(r)
    if f == "denylist":
        return denylist(r)
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
        except Exception:  # noqa: BLE001 - the live TypeError/AttributeError seam
            py = None
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
