#!/usr/bin/env python3
"""Differential oracle for tools/environments/base.py (gate w30_base_0c99d4_p2).

Command construction: the live ``BaseEnvironment`` is driven on a bare instance
(no backend, no process), so the only things compared are the bash strings it
builds.  Seams arrive from the fixture line: the uuid token (heredoc / staged
stdin), the passthrough allowlist (with its profile-scope predicate faked) and
the delegated-task shell-state chain (a contextvar, set from the fixture).
"""
import json
import sys
import types
from contextlib import ExitStack
import unittest.mock as mock
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w30_base_0c99d4_p2"

if str(QUARRY) not in sys.path:  # insert(0): the quarry wins over tests/tools
    sys.path.insert(0, str(QUARRY))
if str(Path(__file__).resolve().parent) not in sys.path:
    sys.path.append(str(Path(__file__).resolve().parent))

try:  # host-blocked native extensions
    from hermes_seam_stubs import install as _install_seams

    _install_seams()
except Exception:  # pragma: no cover
    pass

from tools.environments import base  # noqa: E402

WANTED = {
    "quote_cwd_for_cd", "quote_shell_path", "stdin_redirect", "stdin_heredoc",
    "staged_stdin_path", "split_cwd_marker", "parse_cwd_marker", "wrap_command",
    "bootstrap_script",
}


class _Env(base.BaseEnvironment):
    def cleanup(self):
        pass


def bare():
    return _Env.__new__(_Env)


def _s(req, key, default=""):
    v = req.get(key)
    return v if isinstance(v, str) else default


def _b(req, key, default=False):
    return bool(req.get(key, default))


def _fake_uuid(hex_text):
    return types.SimpleNamespace(uuid4=lambda: types.SimpleNamespace(hex=hex_text))


def quote_cwd_for_cd(req):
    return base.BaseEnvironment._quote_cwd_for_cd(_s(req, "cwd"))


def quote_shell_path(req):
    return bare()._quote_shell_path(_s(req, "path"))


def stdin_redirect(req):
    return base.BaseEnvironment._redirect_stdin_from_file(_s(req, "command"), _s(req, "path"))


def stdin_heredoc(req):
    token = _s(req, "token")
    real = base.uuid
    try:
        base.uuid = _fake_uuid(token)
        return base.BaseEnvironment._embed_stdin_heredoc(_s(req, "command"), _s(req, "stdin_data"))
    finally:
        base.uuid = real


def staged_stdin_path(req):
    token = _s(req, "token")
    obj = bare()
    obj.get_temp_dir = lambda: _s(req, "temp_dir", "/tmp")
    real = base.uuid
    try:
        base.uuid = _fake_uuid(token)
        return obj._staged_stdin_path()
    finally:
        base.uuid = real


def split_cwd_marker(req):
    got = base._split_cwd_marker(_s(req, "output"), _s(req, "marker"))
    if got is None:
        return None
    path, cleaned = got
    return {"path": path, "cleaned": cleaned}


def _parsed(obj, req):
    result = {"output": _s(req, "output"), "returncode": 0}
    obj._parse_cwd_marker(result)
    out = {"output": result.get("output")}
    if "pipeline_status" in result:
        out["pipeline_status"] = result["pipeline_status"]
    if result.get("cwd_observed"):
        out["cwd_observed"] = True
        out["cwd"] = result["cwd"]
    return out


def parse_cwd_marker(req):
    obj = bare()
    obj._pipe_marker = _s(req, "pipe_marker")
    obj._cwd_marker = _s(req, "cwd_marker")
    return _parsed(obj, req)


def _enter_passthrough_scope(stack, names):
    """Fake the profile-scoped allowlist the wrapper consumes."""
    import agent.secret_scope as secret_scope
    import tools.env_passthrough as passthrough

    stack.enter_context(mock.patch.object(secret_scope, "is_multiplex_active", lambda: True))
    stack.enter_context(mock.patch.object(passthrough, "get_all_passthrough", lambda: tuple(names)))


def wrap_command(req):
    obj = bare()
    obj.cwd = _s(req, "cwd")
    obj._snapshot_path = _s(req, "snapshot_path")
    obj._cwd_marker = _s(req, "cwd_marker")
    obj._pipe_marker = _s(req, "pipe_marker")
    obj._cwd_fallback_marker = _s(req, "cwd_fallback_marker")
    obj._snapshot_ready = _b(req, "snapshot_ready")
    obj._persist_env = _b(req, "persist_env", True)
    obj._snapshot_passthrough_names = set()
    names = req.get("passthrough_names")
    names = names if isinstance(names, list) else []
    obj._profile_scoped_passthrough = bool(names)
    chain = req.get("chain")
    chain = tuple(c for c in chain if isinstance(c, str)) if isinstance(chain, list) else ()
    token = base._shell_state_chain.set(chain)
    try:
        with ExitStack() as stack:
            if names:
                _enter_passthrough_scope(stack, names)
            return obj._wrap_command(_s(req, "command"), _s(req, "cwd"),
                                     cwd_fallback=_b(req, "cwd_fallback"))
    finally:
        base._shell_state_chain.reset(token)


def bootstrap_script(req):
    obj = bare()
    obj.cwd = _s(req, "cwd")
    obj._snapshot_path = _s(req, "snapshot_path")
    obj._cwd_marker = _s(req, "cwd_marker")
    names = req.get("excluded_names")
    names = names if isinstance(names, list) else []
    return base._snapshot_bootstrap_script(
        excluded_names=names, **obj._snapshot_script_kwargs(obj.cwd))


EXPECTED = {
    "quote_cwd_for_cd": quote_cwd_for_cd,
    "quote_shell_path": quote_shell_path,
    "stdin_redirect": stdin_redirect,
    "stdin_heredoc": stdin_heredoc,
    "staged_stdin_path": staged_stdin_path,
    "split_cwd_marker": split_cwd_marker,
    "parse_cwd_marker": parse_cwd_marker,
    "wrap_command": wrap_command,
    "bootstrap_script": bootstrap_script,
}


def main():
    cases = mism = 0
    for raw in sys.stdin:
        raw = raw.strip()
        if not raw:
            continue
        try:
            req = json.loads(raw)
        except ValueError:
            continue
        if not isinstance(req, dict) or req.get("func") not in WANTED:
            continue
        cases += 1
        func = req["func"]
        if "out" not in req:
            mism += 1
            print("MISMATCH %s case=%r missing \"out\"" % (func, req.get("case")), file=sys.stderr)
            continue
        try:
            py = EXPECTED[func](req)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (func, req.get("case"), exc), file=sys.stderr)
            continue
        got = req.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py =%r\n  c  =%r" % (func, req.get("case"), py, got),
                  file=sys.stderr)
    if cases == 0:
        print("ORACLE %s: 0 cases -- fixture missing or harness dead" % GATE, file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if (mism or cases == 0) else 0)


if __name__ == "__main__":
    main()
