#!/usr/bin/env python3
"""Differential oracle for tools/environments/base.py (gate w30_base_0c99d4_p1).

The live module is imported live; every seam is fed from the fixture line:
the host environ is written into ``os.environ`` (never read from the host), the
backend bash process is a stub object (sudo / non-login probes), the exec
channel result is a dict (fetch_realpath, cwd parsing) and the uuid token is
injected before ``BaseEnvironment.__init__`` runs.

Compared here are the value decisions of the base environment: sandbox-root
derivation, the host-filesystem predicate, the activity throttle, env-name
validation and the profile-scoped exclusion set, the probe coercions, the
session path/marker derivations, the per-task snapshot digest, cwd-marker
parsing/publication, the readlink parse, the cwd identity hook, the temp dir
and the quoting kwargs.
"""
import json
import os
import sys
import tempfile
import types
import unittest.mock as mock
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w30_base_0c99d4_p1"

# append, never insert(0): tests/ would shadow the quarry package
# insert(0): the quarry must win over this repo's own tests/tools and
# tests/hermes_cli, which otherwise shadow the package being loaded.
if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))
if str(Path(__file__).resolve().parent) not in sys.path:
    sys.path.append(str(Path(__file__).resolve().parent))

try:  # host-blocked native extensions (pydantic _pydantic_core, psutil)
    from hermes_seam_stubs import install as _install_seams

    _install_seams()
except Exception:  # pragma: no cover - stubs are only needed on a hostile host
    pass

from tools.environments import base  # noqa: E402

WANTED = {
    "sandbox_dir", "uses_host_filesystem", "activity_due", "env_name_valid",
    "snapshot_exclusions", "sudo_nopasswd", "probe_nonlogin", "init_paths",
    "session_markers", "task_snapshot_path", "update_cwd", "fetch_realpath",
    "usable_cwd", "temp_dir", "script_kwargs",
}

ENV_KEYS = ("TERMINAL_SANDBOX_DIR", "HERMES_HOME", "LOCALAPPDATA", "HERMES_DATA_DIR_SUFFIX")


class _Env(base.BaseEnvironment):
    def cleanup(self):  # noqa: D401 - abstract surface only
        pass


def bare():
    """An instance with no connection and no backend: class values only."""
    return _Env.__new__(_Env)


def _s(req, key, default=""):
    v = req.get(key)
    return v if isinstance(v, str) else default


def _b(req, key, default=False):
    v = req.get(key, default)
    return bool(v)


def sandbox_dir(req):
    env = req.get("env")
    saved = dict(os.environ)
    try:
        for k in ENV_KEYS:
            os.environ.pop(k, None)
        if isinstance(env, dict):
            for k, v in env.items():
                if isinstance(v, str):
                    os.environ[k] = v
        return base.get_sandbox_dir().as_posix()
    finally:
        os.environ.clear()
        os.environ.update(saved)


def uses_host_filesystem(req):
    if _b(req, "present", True):
        obj = types.SimpleNamespace(uses_host_filesystem=req.get("value"))
    else:
        obj = object()
    return base.env_uses_host_filesystem(obj)


def activity_due(req):
    state = {"last_touch": req.get("last_touch"), "start": req.get("start")}
    if "interval" in req:
        state["interval"] = req["interval"]
    before = state["last_touch"]
    now = req.get("now")
    seen = []

    def cb(msg):
        if _b(req, "raise"):
            raise RuntimeError("callback exploded")
        seen.append(msg)

    real_time = base.time
    old_cb = base.get_activity_callback()
    try:
        base.time = types.SimpleNamespace(monotonic=lambda: now)
        base.set_activity_callback(cb if _b(req, "has_cb") else None)
        base.touch_activity_if_due(state, _s(req, "label"))
    finally:
        base.time = real_time
        base.set_activity_callback(old_cb)
    fired = state["last_touch"] != before
    return {
        "fired": fired,
        "last_touch": state["last_touch"],
        "message": seen[0] if seen else None,
    }


def env_name_valid(req):
    name = req.get("name")
    if not isinstance(name, str):
        return False
    return base._SHELL_ENV_NAME_RE.fullmatch(name) is not None


def snapshot_exclusions(req):
    import agent.secret_scope as secret_scope
    import tools.env_passthrough as passthrough

    names = req.get("names")
    names = names if isinstance(names, list) else []
    extra = req.get("additional")
    extra = extra if isinstance(extra, list) else []
    obj = bare()
    obj._profile_scoped_passthrough = _b(req, "profile_scoped")
    obj._snapshot_passthrough_names = set()
    obj._additional_profile_scoped_passthrough_names = lambda: tuple(extra)
    with mock.patch.object(secret_scope, "is_multiplex_active", lambda: _b(req, "multiplex")), \
            mock.patch.object(passthrough, "get_all_passthrough", lambda: tuple(names)):
        return list(obj._snapshot_excluded_passthrough_names())


def sudo_nopasswd(req):
    obj = bare()
    obj._passwordless_sudo_probe_supported = _b(req, "supported")
    raises = _b(req, "probe_raises")
    rc = req.get("probe_rc")

    def run(*a, **k):
        if raises:
            raise RuntimeError("probe could not spawn")
        return object()

    def wait(proc, timeout=None):
        if raises:
            raise RuntimeError("probe could not spawn")
        return {"returncode": rc}

    obj._run_bash = run
    obj._wait_for_process = wait
    return obj._sudo_nopasswd_works()


def probe_nonlogin(req):
    obj = bare()
    timeout = req.get("snapshot_timeout")
    obj._snapshot_timeout = timeout if isinstance(timeout, (int, float)) and not isinstance(timeout, bool) else 30
    raises = _b(req, "probe_raises")
    rc = req.get("probe_rc")
    stdout = req.get("probe_stdout")
    exc_text = _s(req, "probe_exc", "boom")

    def run(*a, **k):
        if raises:
            raise ValueError(exc_text)
        return object()

    def wait(proc, timeout=None):
        if raises:
            raise ValueError(exc_text)
        return {"returncode": rc, "stdout": stdout}

    obj._run_bash = run
    obj._wait_for_process = wait
    prefer, detail = obj._probe_nonlogin_fallback(_s(req, "detail"))
    return [prefer, detail]


def init_paths(req):
    temp_dir = req.get("temp_dir")
    temp_dir = temp_dir if isinstance(temp_dir, str) else "/tmp"
    sid = _s(req, "session_id")
    fake_hex = sid + "000000000000"
    obj = bare()
    obj.get_temp_dir = lambda: temp_dir
    real_uuid = base.uuid
    try:
        base.uuid = types.SimpleNamespace(uuid4=lambda: types.SimpleNamespace(hex=fake_hex))
        base.BaseEnvironment.__init__(obj, cwd="/x", timeout=10)
    finally:
        base.uuid = real_uuid
    return {
        "snapshot_path": obj._snapshot_path,
        "cwd_file": obj._cwd_file,
        "cwd_marker": obj._cwd_marker,
        "pipe_marker": obj._pipe_marker,
        "cwd_fallback_marker": obj._cwd_fallback_marker,
    }


def session_markers(req):
    sid = _s(req, "session_id")
    return {
        "cwd": base._cwd_marker(sid),
        "cwd_fallback": base._cwd_fallback_marker(sid),
        "pipe": base._pipe_marker(sid),
    }


def task_snapshot_path(req):
    obj = bare()
    obj._snapshot_path = _s(req, "snapshot_path")
    return obj._task_snapshot_path(_s(req, "task_id"))


def update_cwd(req):
    obj = bare()
    obj.cwd = _s(req, "env_cwd")
    obj._pipe_marker = _s(req, "pipe_marker")
    obj._cwd_marker = _s(req, "cwd_marker")
    result = {"output": _s(req, "output"), "returncode": 0}
    obj._update_cwd(result, move_session=_b(req, "move_session", True))
    out = {"output": result.get("output")}
    if "pipeline_status" in result:
        out["pipeline_status"] = result["pipeline_status"]
    if result.get("cwd_observed"):
        out["cwd_observed"] = True
        out["cwd"] = result["cwd"]
    out["session_cwd"] = obj.cwd
    return out


def fetch_realpath(req):
    obj = bare()
    result = req.get("result")
    result = result if isinstance(result, dict) else {}
    obj.execute = lambda *a, **k: result
    return obj.fetch_realpath(_s(req, "remote_path"))


def usable_cwd(req):
    cwd, fallback = bare()._usable_cwd(_s(req, "cwd"))
    return {"cwd": cwd, "fallback": fallback}


def temp_dir(req):
    return bare().get_temp_dir()


def script_kwargs(req):
    obj = bare()
    obj._snapshot_path = _s(req, "snapshot_path")
    obj._cwd_marker = _s(req, "cwd_marker")
    snap = req.get("snap")
    return obj._snapshot_script_kwargs(_s(req, "cwd"), snap if isinstance(snap, str) else None)


EXPECTED = {
    "sandbox_dir": sandbox_dir,
    "uses_host_filesystem": uses_host_filesystem,
    "activity_due": activity_due,
    "env_name_valid": env_name_valid,
    "snapshot_exclusions": snapshot_exclusions,
    "sudo_nopasswd": sudo_nopasswd,
    "probe_nonlogin": probe_nonlogin,
    "init_paths": init_paths,
    "session_markers": session_markers,
    "task_snapshot_path": task_snapshot_path,
    "update_cwd": update_cwd,
    "fetch_realpath": fetch_realpath,
    "usable_cwd": usable_cwd,
    "temp_dir": temp_dir,
    "script_kwargs": script_kwargs,
}


def main():
    workdir = tempfile.mkdtemp(prefix="w30_base_p1_")
    start_cwd = os.getcwd()
    os.chdir(workdir)
    cases = mism = 0
    try:
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
    finally:
        os.chdir(start_cwd)
    if cases == 0:
        print("ORACLE %s: 0 cases -- the fixture line is missing or the harness is dead" % GATE,
              file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if (mism or cases == 0) else 0)


if __name__ == "__main__":
    main()
