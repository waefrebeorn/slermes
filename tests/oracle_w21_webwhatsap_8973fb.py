#!/usr/bin/env python3
"""Differential oracle for hermes_cli/web_whatsapp_session.py
(gate w21_webwhatsap_8973fb).

The live module is imported as-is and both guards run unmodified. Only the host
seams are stubbed:
  * gateway.status._pid_exists / get_process_start_time (the process table),
  * pathlib.Path (a recording virtual filesystem: exists / is_symlink /
    read_text / rename / mkdir),
  * datetime.now(UTC) and tempfile.mkdtemp (the clock and the temp name).
The comparison is the HTTPException projection (status + detail) plus the
ordered filesystem operations, so the C port must reproduce the real 409/500
decisions, not a paraphrase of them.
"""
import datetime as _dt
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "hermes_cli" / "web_whatsapp_session.py"
GATE = "w21_webwhatsap_8973fb"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from fastapi import HTTPException  # noqa: E402


class _FS:
    def __init__(self, state, session_path):
        self.state = state
        self.session_path = session_path
        self.exists_map = {}
        self.ops = []
        self.text = state.get("pid_text") or ""
        self.rename_error = state.get("rename_error")
        self.mkdir_error = state.get("mkdir_error")
        self.read_raises = bool(state.get("read_raises"))
        self.exists_map[session_path] = bool(state.get("exists"))
        self.exists_map[session_path + "/bridge.pid"] = bool(state.get("pid_file_exists"))
        backup = (state.get("backup_dir") or "") + "/session"
        self.exists_map[backup] = False
        self.exists_map[state.get("backup_dir") or ""] = True

    def exists(self, name):
        return self.exists_map.get(name, False)

    def is_symlink(self, name):
        return name == self.session_path and bool(self.state.get("is_symlink"))


class _FakePath:
    def __init__(self, name, fs):
        self._name = name
        self._fs = fs

    def __truediv__(self, other):
        return _FakePath(self._name.rstrip("/") + "/" + str(other), self._fs)

    @property
    def parent(self):
        name = self._name.rstrip("/")
        cut = name.rfind("/")
        return _FakePath(name[:cut] if cut > 0 else "/", self._fs)

    def exists(self):
        return self._fs.exists(self._name)

    def is_symlink(self):
        return self._fs.is_symlink(self._name)

    def read_text(self, encoding=None):
        if self._fs.read_raises:
            raise OSError("permission denied")
        text = self._fs.text
        # utf-8-sig: the seam strips the byte-order mark, as pathlib does.
        if encoding == "utf-8-sig" and text.startswith("\ufeff"):
            text = text[1:]
        return text

    def rename(self, target):
        if self._fs.rename_error:
            raise OSError(self._fs.rename_error)
        self._fs.ops.append(("rename", target._name))
        self._fs.exists_map[self._name] = False
        self._fs.exists_map[target._name] = True

    def mkdir(self, mode=0o777, **kw):
        if self._fs.mkdir_error:
            raise OSError(self._fs.mkdir_error)
        self._fs.ops.append(("mkdir", mode))

    def __str__(self):
        return self._name


def load():
    spec = importlib.util.spec_from_file_location("web_whatsapp_session_live", SRC)
    mod = importlib.util.module_from_spec(spec)
    sys.modules["web_whatsapp_session_live"] = mod
    spec.loader.exec_module(mod)
    return mod


LIVE = load()


def _stub_process_table(state):
    status = types.ModuleType("gateway.status")
    status._pid_exists = lambda pid: bool(state.get("pid_exists"))
    status.get_process_start_time = lambda pid: state.get("current_start")
    gateway = types.ModuleType("gateway")
    gateway.__path__ = []
    gateway.status = status
    sys.modules["gateway"] = gateway
    sys.modules["gateway.status"] = status


class _Stamp:
    def __init__(self, text):
        self.text = text

    def strftime(self, fmt):
        assert fmt == "%Y%m%dT%H%M%SZ", fmt
        return self.text


class _FakeDatetime:
    stamp = "20261123T120000Z"

    @classmethod
    def now(cls, tz=None):
        return _Stamp(cls.stamp)


def _raise_for(state, exc):
    if isinstance(exc, HTTPException):
        return {"raised": True, "status": exc.status_code, "detail": exc.detail}
    raise exc


def expected(r):
    state = r if isinstance(r, dict) else {}
    session_path = state.get("session_path") or "/sessions/w21"
    fs = _FS(state, session_path)
    _stub_process_table(state)
    LIVE.Path = lambda name: _FakePath(str(name), fs)
    LIVE.datetime = _FakeDatetime
    LIVE.tempfile = types.SimpleNamespace(
        mkdtemp=lambda prefix=None, dir=None: state.get("backup_dir") or "")
    path = _FakePath(session_path, fs)

    if r.get("func") == "bridge_check":
        try:
            LIVE._assert_whatsapp_bridge_stopped(path)
        except Exception as exc:  # noqa: BLE001
            return _raise_for(state, exc)
        return {"raised": False, "status": None, "detail": None}

    assert r.get("func") == "backup", r.get("func")
    out = {"raised": False, "status": None, "detail": None,
           "actions": [], "mkdir_mode": None}
    try:
        LIVE._backup_whatsapp_session(path)
    except Exception as exc:  # noqa: BLE001
        out.update(_raise_for(state, exc))
        out["actions"] = [op for op, _ in fs.ops]
        return out
    out["actions"] = [op for op, _ in fs.ops]
    for op, arg in fs.ops:
        if op == "mkdir":
            out["mkdir_mode"] = "%04o" % arg
    return out


def main():
    cases = mism = 0
    wanted = {"bridge_check", "backup"}
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
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
