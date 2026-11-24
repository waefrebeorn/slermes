#!/usr/bin/env python3
"""Differential oracle for tools/environments/remote_common.py
(gate w24_remotecomm_c4bdad).

The live module is imported and its real functions are called:
``prepend_unset``, ``bash_argv``, ``client_env_with`` and
``resolve_passthrough_env`` (plus ``shlex.quote``, which ``prepend_unset``
quotes with).  Everything the live code reads from the process is substituted
with the same value the C port receives, per case:

  * ``os.environ``            <- the fixture's ``env`` object
  * ``hermes_env_loader``     <- the fixture's ``hermes_env`` object
  * ``tools.env_passthrough.get_all_passthrough`` <- ``passthrough``
  * ``tools.env_passthrough.resolve_passthrough_value`` <- identity
  * ``agent.secret_scope.is_multiplex_active`` / ``_is_global_env``
  * ``remote_common._is_hermes_internal_secret`` / ``_is_provider_env_blocklisted``

Those are registry/scope/process seams, not the logic under test; the set
algebra, the ``or``-fallthrough, quoting, sorting and the unset rule are all the
live code paths.
"""
import json
import os
import shlex
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_remotecomm_c4bdad"

os.environ.setdefault("HOME", str(REPO.parent))
os.environ.setdefault("USERPROFILE", str(REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(REPO.parent / "AppData" / "Local"))

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from tools.environments import remote_common as live  # noqa: E402
import tools.env_passthrough as env_passthrough  # noqa: E402
import agent.secret_scope as secret_scope  # noqa: E402
import tools.environments.base_session_env as base_session_env  # noqa: E402

WANTED = {
    "shlex_quote", "prepend_unset", "bash_argv", "client_env_with",
    "resolve_passthrough", "constants",
}

CASE = {}

# Registry / scope / process seams, driven by the current fixture case.
env_passthrough.get_all_passthrough = lambda: CASE.get("passthrough") or []
env_passthrough.resolve_passthrough_value = lambda key, value: value
secret_scope.is_multiplex_active = lambda: bool(CASE.get("multiplex"))
secret_scope._is_global_env = lambda name: name in (CASE.get("global_env") or [])
live._is_hermes_internal_secret = lambda key: key in (CASE.get("internal_secret") or [])
live._is_provider_env_blocklisted = (
    lambda name, _registered=None: name in (CASE.get("blocklisted") or []))


def resolved(req):
    CASE.clear()
    CASE.update(req)
    real_environ = os.environ
    os.environ = dict(req.get("env") or {})          # os.getenv view
    try:
        exec_env, unset = live.resolve_passthrough_env(
            req.get("explicit_forward") or [],
            hermes_env_loader=lambda: dict(req.get("hermes_env") or {}),
        )
    finally:
        os.environ = real_environ
        CASE.clear()
    return {"exec_env": exec_env, "unset": sorted(unset)}


def expected(r):
    f = r.get("func")
    if f == "shlex_quote":
        return shlex.quote(r.get("s") if isinstance(r.get("s"), str) else "")
    if f == "prepend_unset":
        return live.prepend_unset(r.get("cmd"), r.get("names") or [])
    if f == "bash_argv":
        return live.bash_argv(r.get("cmd"), login=bool(r.get("login")))
    if f == "client_env_with":
        real_environ = os.environ
        os.environ = dict(r.get("environ") or {})
        try:
            return live.client_env_with(dict(r.get("values") or {}))
        finally:
            os.environ = real_environ
    if f == "resolve_passthrough":
        return resolved(r)
    if f == "constants":
        plain = live.bash_argv("x", login=False)
        login = live.bash_argv("x", login=True)
        return {
            "shell_env_name_pattern": base_session_env._SHELL_ENV_NAME_RE.pattern,
            "bash": plain[0],
            "login_flag": login[1],
            "command_flag": plain[-2],
        }
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
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
