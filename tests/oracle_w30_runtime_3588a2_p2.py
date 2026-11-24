#!/usr/bin/env python3
"""Differential oracle for tools/bot_desktop/runtime.py (gate w30_runtime_3588a2_p2).

The live bodies are called; only seams are replaced, with the answers the
machine would have handed them:

  * ``hermes_cli.config.load_config_readonly`` -> the case's bot_desktop block;
  * ``time.time`` / ``state_dir`` -> a fixed clock and a fake state dir whose
    files exist (or not) with a given mtime, so ``idle_seconds`` runs live;
  * ``idle_stop_seconds``/``is_running``/``_launcher_identity``/``idle_seconds``/
    ``lease.get`` -> the case's values, for ``_idle_due``;
  * ``placement.resolve`` -> a real ``Placement`` built from the case;
  * ``shutil.which``/``missing_binaries``/``is_supported_host``/``_launcher_pid``/
    ``published_env``/``rfb_socket_path``/``geometry``/``install_command``/
    ``resources.memory_info``/``resources.memory_blocker``/``browser.executable``/
    ``sandbox_host.*`` -> that answer;
  * ``start``/``_start_in_sandbox``/``touch_activity``/``sandbox_host.stop`` ->
    recorders, so a decision's OBSERVABLE effect (start called? stopped?) is
    what gets compared.

``!raise: <msg>`` carries a RuntimeError across the JSON-lines wire.
"""
import json
import os
import sys
import tempfile
from contextlib import contextmanager, nullcontext
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w30_runtime_3588a2_p2"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

# runtime.py sizes its module-level allocation-lock path at import time
# (XDG_RUNTIME_DIR or Path.home()/".cache"); this shell hands the interpreter no
# Windows home, so give it a scratch runtime dir. Nothing ported here reads it.
os.environ.setdefault("XDG_RUNTIME_DIR", tempfile.mkdtemp(prefix="w30rt-p2-"))

from tools.bot_desktop import runtime as live  # noqa: E402
from tools.bot_desktop import placement as live_placement  # noqa: E402
from tools.bot_desktop import lease as live_lease  # noqa: E402
from tools.bot_desktop import sandbox_host as live_sandbox  # noqa: E402
from tools.bot_desktop import browser as live_browser  # noqa: E402
from tools.bot_desktop import resources as live_resources  # noqa: E402
from hermes_cli import config as live_config  # noqa: E402
from hermes_cli import sandbox_image_switch as live_switch  # noqa: E402

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


class FakeFile:
    def __init__(self, mtime):
        self.mtime = mtime

    def stat(self):
        if self.mtime is None:
            raise OSError(2, "No such file or directory")
        return SimpleNamespace(st_mtime=self.mtime)


class FakeStateDir:
    """state_dir() stand-in: /<name> answers with the case's mtime or OSError."""

    def __init__(self, mtimes):
        self.mtimes = mtimes or {}

    def __truediv__(self, name):
        return FakeFile(self.mtimes.get(name))


class NoopPath:
    """A path whose writes are irrelevant: stop()'s cleanup must not touch a disk."""

    def __truediv__(self, name):
        return self

    def mkdir(self, *a, **k):
        return None

    def unlink(self, *a, **k):
        return None


def _plc(r):
    return live_placement.Placement(r.get("where") or "gateway", r.get("backend") or "local",
                                    r.get("reason") or "")


def _cfg(r):
    return r.get("cfg") if isinstance(r.get("cfg"), dict) else {}


# --------------------------------------------------------------------------
# handlers
# --------------------------------------------------------------------------
def h_idle_stop_seconds(r):
    cfg = _cfg(r)
    with patch(live_config, load_config_readonly=lambda: {"bot_desktop": cfg}):
        return live.idle_stop_seconds()


def h_idle_seconds(r):
    mtimes = {"activity": r.get("activity_mtime"), "env": r.get("env_mtime")}
    now = r.get("now")
    with patch(live, time=SimpleNamespace(time=lambda: float(now)),
               state_dir=lambda: FakeStateDir(mtimes)):
        return live.idle_seconds()


def h_idle_due(r):
    limit = r.get("limit")
    launcher = r.get("launcher")
    current = r.get("current")
    holder = r.get("holder")
    with patch(live,
               idle_stop_seconds=lambda: limit,
               is_running=lambda: bool(r.get("running")),
               _launcher_identity=lambda: (None if not current
                                           else (current.get("pid"), current.get("create_time"))),
               idle_seconds=lambda: r.get("idle")), \
         patch(live_lease, get=lambda: SimpleNamespace(holder=holder, viewer_id="viewer", epoch=3)):
        return live._idle_due(None if not launcher else (launcher.get("pid"), launcher.get("create_time")))


def h_should_auto_start(r):
    # cfg["auto_start"] is the caller's resolved seam value (the case's "auto_start")
    cfg = {"auto_start": bool(r.get("auto_start"))}
    env = r.get("env") if isinstance(r.get("env"), dict) else {}
    with patch(live, is_supported_host=lambda: bool(r.get("supported")),
               missing_binaries=lambda: list(r.get("missing") or [])), \
         patch(live_config, load_config_readonly=lambda: {"bot_desktop": cfg}):
        return live._should_auto_start(env)


def h_tool_placement(r):
    published = r.get("published") if isinstance(r.get("published"), dict) else {}
    with patch(live_placement, resolve=lambda: _plc(r)), \
         patch(live, sandbox_screen_running=lambda: bool(r.get("sandbox_running")),
               published_env=lambda: published, start=lambda *a, **k: None):
        try:
            return live.tool_placement()
        except RuntimeError as exc:
            return "!raise: %s" % (exc,)


def h_ensure_started_for_tool(r):
    cfg = {"auto_start": bool(r.get("auto_start"))}
    env = r.get("env") if isinstance(r.get("env"), dict) else {}
    published = r.get("published") if isinstance(r.get("published"), dict) else env
    calls = {"start": 0, "touch": 0}

    def _start(*a, **k):
        calls["start"] += 1

    def _touch():
        calls["touch"] += 1

    with patch(live_placement, resolve=lambda: _plc(r)), \
         patch(live_config, load_config_readonly=lambda: {"bot_desktop": cfg}), \
         patch(live, published_env=lambda: published, start=_start, touch_activity=_touch,
               is_supported_host=lambda: bool(r.get("supported")),
               missing_binaries=lambda: list(r.get("missing") or [])):
        live.ensure_started_for_tool()
    return {"start_calls": calls["start"], "touch_calls": calls["touch"]}


def h_start(r):
    sd = Path(tempfile.mkdtemp(prefix="w30p2-sd-"))
    with patch(live_placement, resolve=lambda: _plc(r)), \
         patch(live, is_supported_host=lambda: bool(r.get("supported")),
               missing_binaries=lambda: list(r.get("missing") or []),
               package_manager=lambda: r.get("pm"),
               installable=lambda: bool(r.get("installable")),
               install_command=lambda: r.get("install_command"),
               _start_in_sandbox=lambda wait: "SANDBOX",
               _flocked=lambda p: nullcontext(),
               state_dir=lambda: sd,
               _launcher_pid=lambda: 4242,
               published_env=lambda: {"DISPLAY": ":21"},
               status=lambda *a, **k: "PROCEED"):
        try:
            got = live.start()
        except RuntimeError as exc:
            return "!raise: %s" % (exc,)
    if got == "SANDBOX":
        return "sandbox"
    if got == "PROCEED":
        return "proceed"
    return "unexpected: %r" % (got,)


def h_status(r):
    env = r.get("env") if isinstance(r.get("env"), dict) else {}
    mem = live_resources.MemoryInfo(r.get("memory_available_mb"), r.get("memory_limit_mb"))
    with patch(live_placement, resolve=lambda: _plc(r)), \
         patch(live_sandbox, _read_marker=lambda: None), \
         patch(live_browser, executable=lambda: r.get("browser")), \
         patch(live_resources, memory_info=lambda: mem,
               memory_blocker=lambda m, need=None: r.get("blocker")), \
         patch(live, is_supported_host=lambda: bool(r.get("supported")),
               missing_binaries=lambda: list(r.get("missing") or []),
               _launcher_pid=lambda: r.get("pid"),
               published_env=lambda: env,
               rfb_socket_path=lambda: r.get("socket"),
               geometry=lambda: r.get("geometry"),
               install_command=lambda: r.get("install_command"),
               _profile_name=lambda: "default"):
        return live.status(r.get("profile")).as_dict()


def h_sandbox_status(r):
    published = r.get("published") if isinstance(r.get("published"), dict) else {}
    missing = list(r.get("missing") or [])
    sw = r.get("switch")
    switch_obj = None
    if isinstance(sw, dict):
        switch_obj = SimpleNamespace(current_image=sw.get("current_image"),
                                     target_image=sw.get("target_image"),
                                     containers=list(sw.get("containers") or []))
    env = object() if r.get("env", True) else None
    with patch(live_sandbox,
               missing_binaries=lambda e: missing if e is not None else [],
               published_env=lambda e, p: dict(published) if e is not None else {},
               plugin_image_advice=lambda backend: r.get("advice"),
               SANDBOX_IMAGE_HINT=r.get("image_hint") or ""), \
         patch(live_switch, pending=lambda: switch_obj), \
         patch(live, _owned_sandbox_env=lambda: env, _sandbox_env=lambda create: env,
               geometry=lambda: r.get("geometry"), _profile_name=lambda: "default"):
        return live._sandbox_status(r.get("profile"), _plc(r)).as_dict()


def h_stop(r):
    marker = {"profile": "default"} if r.get("marker") else None
    env = object() if r.get("marker") or r.get("where") == "terminal" else None
    still_due = r.get("still_due")

    def _still_due():
        return bool(still_due)

    with patch(live_placement, resolve=lambda: _plc(r)), \
         patch(live_sandbox, _read_marker=lambda: marker,
               _marker=lambda: NoopPath(),
               stop=lambda e, p: bool(r.get("sandbox_stopped"))), \
         patch(live, is_supported_host=lambda: bool(r.get("supported")),
               _owned_sandbox_env=lambda: env,
               _launcher_pid=lambda: r.get("launcher_pid"),
               _reap_orphaned_server=lambda sd: bool(r.get("reaped")),
               _kill_group_then_wait=lambda pgid, pid, grace=2.0: None,
               _flocked=lambda p: nullcontext(),
               state_dir=lambda: NoopPath()):
        return live.stop(None if still_due is None else _still_due)


def h_in_sandbox(r):
    with patch(live_placement, resolve=lambda: _plc(r)):
        return live.in_sandbox()


HANDLERS = {
    "idle_stop_seconds": h_idle_stop_seconds,
    "idle_seconds": h_idle_seconds,
    "idle_due": h_idle_due,
    "should_auto_start": h_should_auto_start,
    "tool_placement": h_tool_placement,
    "ensure_started_for_tool": h_ensure_started_for_tool,
    "start": h_start,
    "status": h_status,
    "sandbox_status": h_sandbox_status,
    "stop": h_stop,
    "in_sandbox": h_in_sandbox,
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
