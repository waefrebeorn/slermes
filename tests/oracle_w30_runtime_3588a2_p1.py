#!/usr/bin/env python3
"""Differential oracle for tools/bot_desktop/runtime.py (gate w30_runtime_3588a2_p1).

The live module is imported from the quarry and its own functions are called;
SEAMS ONLY are replaced, and only with answers the machine would have given:

  * ``get_hermes_home`` -> a real ``pathlib.Path`` (the join stays live);
  * ``sys.platform`` -> a platform string;
  * ``shutil.which`` -> a resolver over a set of present binaries;
  * ``os.geteuid`` -> an euid (or absent, like Windows);
  * ``state_dir``/``_X_LOCK_DIR``/``_X_UNIX_TABLE`` -> a REAL scratch directory,
    so ``_read``, ``read_text`` and ``Path.exists`` run for real on the text the
    case supplies (the file-contents seam is the file, not its parse);
  * ``_create_time``/``_pid_alive``/``_x_lock_pid``/``_x_socket_bound``/
    ``_display_in_use``/``_launcher_pid``/``published_env`` -> that answer.

Nothing else is stubbed: every compared value comes out of the live body.
``!raise: <msg>`` is how a RuntimeError is carried over the JSON-lines wire.
"""
import json
import os
import sys
import tempfile
from contextlib import contextmanager
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w30_runtime_3588a2_p1"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

# runtime.py sizes its module-level allocation-lock path at import time
# (XDG_RUNTIME_DIR or Path.home()/".cache"); this shell hands the interpreter no
# Windows home, so give it a scratch runtime dir. Nothing ported here reads it.
os.environ.setdefault("XDG_RUNTIME_DIR", tempfile.mkdtemp(prefix="w30rt-p1-"))

from tools.bot_desktop import runtime as live  # noqa: E402
from tools.bot_desktop import sandbox_host as live_sandbox  # noqa: E402
from hermes_cli import config as live_config  # noqa: E402

_MISSING = object()


@contextmanager
def patch(obj, **kw):
    saved = {}
    for k, v in kw.items():
        saved[k] = getattr(obj, k, _MISSING)
        setattr(obj, k, v)
    try:
        yield
    finally:
        for k, v in saved.items():
            if v is _MISSING:
                delattr(obj, k)
            else:
                setattr(obj, k, v)


class FakeShutil:
    """shutil.which over a fixed set of resolvable binaries."""

    def __init__(self, present):
        self.present = set(present or ())

    def which(self, name, *a, **k):
        return "/usr/bin/" + name if name in self.present else None


class FakeOS:
    """os with (or without) geteuid."""

    def __init__(self, euid):
        if euid is not None:
            self.geteuid = lambda: int(euid)


def _scratch():
    return Path(tempfile.mkdtemp(prefix="w30p1-"))


def _write(d, name, text):
    if text is None:
        return
    (d / name).write_text(text, encoding="utf-8")


def _cfg(r):
    return r.get("cfg") if isinstance(r.get("cfg"), dict) else {}


def _rc(r):
    return r.get("cfg") if isinstance(r.get("cfg"), dict) else {}


# --------------------------------------------------------------------------
# handlers
# --------------------------------------------------------------------------
def h_state_dir(r):
    with patch(live, get_hermes_home=lambda: Path(r.get("home") or "")):
        return str(live.state_dir()).replace("\\", "/")


def h_is_supported_host(r):
    with patch(live, sys=SimpleNamespace(platform=r.get("platform") or "")):
        return live.is_supported_host()


def h_missing_binaries(r):
    with patch(live, shutil=FakeShutil(r.get("present"))):
        return live.missing_binaries()


def h_package_manager(r):
    with patch(live, shutil=FakeShutil(r.get("which"))):
        return live.package_manager()


def h_is_root(r):
    with patch(live, os=FakeOS(r.get("euid"))):
        return live.is_root()


def h_installable(r):
    with patch(live, shutil=FakeShutil(r.get("which")), os=FakeOS(r.get("euid"))):
        return live.installable()


def h_install_command(r):
    with patch(live, shutil=FakeShutil(r.get("which")), os=FakeOS(r.get("euid"))):
        return live.install_command()


def h_launcher_identity(r):
    d = _scratch()
    _write(d, "launcher.pid", r.get("pidfile"))
    with patch(live, state_dir=lambda: d,
               _create_time=lambda pid: r.get("create_time"),
               _pid_alive=lambda pid: bool(r.get("alive"))):
        got = live._launcher_identity()
    return None if got is None else {"pid": got[0], "create_time": got[1]}


def h_launcher_pid(r):
    d = _scratch()
    _write(d, "launcher.pid", r.get("pidfile"))
    with patch(live, state_dir=lambda: d,
               _create_time=lambda pid: r.get("create_time"),
               _pid_alive=lambda pid: bool(r.get("alive"))):
        return live._launcher_pid()


def h_recorded_launcher_pid(r):
    d = _scratch()
    _write(d, "launcher.pid", r.get("pidfile"))
    with patch(live, state_dir=lambda: d):
        return live._recorded_launcher_pid()


def h_x_lock_pid(r):
    d = _scratch()
    _write(d, ".X21-lock", r.get("raw"))
    with patch(live, _X_LOCK_DIR=d):
        return live._x_lock_pid(int(r.get("num") or 21))


def h_x_socket_bound(r):
    d = _scratch()
    table = d / "unix"
    if r.get("unix_table") is not None:
        table.write_text(r["unix_table"], encoding="utf-8")
    with patch(live, _X_UNIX_TABLE=table):
        return live._x_socket_bound(int(r.get("num") or 21))


def h_display_in_use(r):
    with patch(live,
               _x_lock_pid=lambda num: r.get("lock_pid"),
               _pid_alive=lambda pid: bool(r.get("alive")),
               _x_socket_bound=lambda num: bool(r.get("socket_bound"))):
        return live._display_in_use(int(r.get("num") or 21))


def h_pick_display(r):
    d = _scratch()
    _write(d, "display", r.get("recorded"))
    in_use = set(r.get("in_use") or ())
    with patch(live, state_dir=lambda: d, _display_in_use=lambda num: num in in_use):
        try:
            return live._pick_display()
        except RuntimeError as exc:
            return "!raise: %s" % (exc,)


def h_published_env(r):
    d = _scratch()
    _write(d, "env", r.get("raw"))
    with patch(live_sandbox, _read_marker=lambda: r.get("marker") or None), \
         patch(live, state_dir=lambda: d,
               _sandbox_published_env=lambda: dict(r.get("sandbox_published") or {}),
               _launcher_pid=lambda: r.get("launcher_pid")):
        return live.published_env()


def h_rfb_socket_path(r):
    d = _scratch()
    if r.get("exists"):
        (d / "rfb.sock").write_text("", encoding="utf-8")
    with patch(live_sandbox, _read_marker=lambda: r.get("marker") or None), \
         patch(live, state_dir=lambda: d, _launcher_pid=lambda: r.get("launcher_pid")):
        got = live.rfb_socket_path()
    if got is None:
        return None
    # the scratch dir stands in for the case's state dir; the join itself is live
    return str(got).replace(str(d), r.get("state_dir") or "").replace("\\", "/")


def h_is_running(r):
    env = r.get("env") if isinstance(r.get("env"), dict) else {}
    with patch(live, published_env=lambda: env):
        return live.is_running()


def h_host_published_env(r):
    published = r.get("published") if isinstance(r.get("published"), dict) else {}
    with patch(live_sandbox, _read_marker=lambda: r.get("marker") or None), \
         patch(live, published_env=lambda: published):
        return live.host_published_env()


def h_desktop_env(r):
    base = r.get("base") if isinstance(r.get("base"), dict) else {}
    published = r.get("published") if isinstance(r.get("published"), dict) else {}
    with patch(live_sandbox, _read_marker=lambda: r.get("marker") or None), \
         patch(live, published_env=lambda: published, touch_activity=lambda: None,
               is_running=lambda: bool(r.get("running"))):
        return live.desktop_env(base)


def h_geometry(r):
    cfg = r.get("cfg") if isinstance(r.get("cfg"), dict) else {}
    with patch(live_config, load_config_readonly=lambda: {"bot_desktop": cfg}):
        return live.geometry()


HANDLERS = {
    "state_dir": h_state_dir,
    "is_supported_host": h_is_supported_host,
    "missing_binaries": h_missing_binaries,
    "package_manager": h_package_manager,
    "is_root": h_is_root,
    "installable": h_installable,
    "install_command": h_install_command,
    "launcher_identity": h_launcher_identity,
    "launcher_pid": h_launcher_pid,
    "recorded_launcher_pid": h_recorded_launcher_pid,
    "x_lock_pid": h_x_lock_pid,
    "x_socket_bound": h_x_socket_bound,
    "display_in_use": h_display_in_use,
    "pick_display": h_pick_display,
    "published_env": h_published_env,
    "rfb_socket_path": h_rfb_socket_path,
    "is_running": h_is_running,
    "host_published_env": h_host_published_env,
    "desktop_env": h_desktop_env,
    "geometry": h_geometry,
}


def expected(r):
    fn = HANDLERS.get(r.get("func"))
    if fn is None:
        raise AssertionError("unknown func %r" % (r.get("func"),))
    return fn(r)


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
        if not isinstance(r, dict) or r.get("func") not in HANDLERS:
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
            print("MISMATCH %s case=%r\n  py=%r\n  c =%r" % (r.get("func"), r.get("case"), py, got),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
