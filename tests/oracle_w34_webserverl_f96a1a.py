#!/usr/bin/env python3
"""Differential oracle for hermes_cli/web_server_lifecycle.py (gate
w34_webserverl_f96a1a).

The live module is imported BY FILE PATH and every process/socket/thread seam
is replaced with the answer the fixture supplies, so what is compared is
exactly the decision those answers feed:

  * ``_valid_parent_start_marker`` / ``_parent_start_markers_match`` /
    ``_parent_start_marker_mismatch_is_conclusive`` called directly;
  * ``_is_serve_orphaned`` with the start-marker probe and ``pid_exists`` as
    injected callables (the fixture says "marker", "ProcessLookupError" or
    "raised");
  * ``_start_parent_death_watchdog`` under a captured ``threading.Thread``, a
    fake ``time.sleep`` and an orphan stub, reading a patched os.environ --
    the captured loop is run for exactly one poll so ``poll`` is observable;
  * ``_is_addr_in_use_error`` on a synthetic OSError carrying the fixture's
    errno/winerror;
  * ``_port_bind_conflict`` with ``sys.modules['socket']`` swapped for a fake
    whose ``bind`` raises the fixture's error (family, port-0 skip and the
    non-conflict paths all stay observable);
  * ``_PORT_IN_USE_SENTINEL.format`` and ``_report_port_in_use`` (sentinel
    writer stubbed, stdout captured);
  * ``_dashboard_forwarded_allow_ips`` on the fixture's plain dict;
  * ``_maybe_open_browser`` under a captured opener thread with
    ``webbrowser.open``, ``sys.platform`` and DISPLAY/WAYLAND_DISPLAY patched;
  * the module literals.
"""
import contextlib
import io
import json
import os
import sys
import types
import webbrowser
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w34_webserverl_f96a1a"
MIN_CASES = 10

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))
if os.name == "nt" and not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = str(QUARRY.parent)

# stub helper only; appended so it can never shadow a quarry package
sys.path.append(str(Path(__file__).resolve().parent))
from hermes_seam_stubs import install as _install_seams  # noqa: E402

_install_seams()

from hermes_cli import web_server_lifecycle as live  # noqa: E402

WANTED = {
    "constants", "valid_marker", "markers_match", "mismatch_conclusive",
    "orphaned", "watchdog_plan", "addr_in_use", "port_preflight",
    "sentinel", "hint", "allow_ips", "browser",
}


class _Captured:
    """Stand-in for threading.Thread that records the target and never runs it."""

    target = None

    def __init__(self, target=None, daemon=None, name=None, **kw):
        _Captured.target = target

    def start(self):
        pass


def constants(_r):
    return {
        "port_in_use_exit_code": live.PORT_IN_USE_EXIT_CODE,
        "orphan_exit_ceiling_s": float(live._ORPHAN_EXIT_CEILING_S),
        "sentinel": live._PORT_IN_USE_SENTINEL,
        "default_forwarded_allow_ips": list(live._DEFAULT_DASHBOARD_FORWARDED_ALLOW_IPS),
    }


def valid_marker(r):
    return live._valid_parent_start_marker(r.get("marker"))


def markers_match(r):
    return live._parent_start_markers_match(r.get("actual"), r.get("expected"))


def mismatch_conclusive(r):
    return live._parent_start_marker_mismatch_is_conclusive(r.get("actual"), r.get("expected"))


class _Probe:
    def __init__(self, rep):
        self.rep = rep if isinstance(rep, dict) else {}

    def __call__(self, _pid):
        kind = self.rep.get("kind")
        if kind == "marker":
            return self.rep.get("value")
        if kind == "lookup":
            raise ProcessLookupError(self.rep.get("value"))
        raise OSError(13, "probe failed")


class _PidExists:
    def __init__(self, rep):
        self.rep = rep if isinstance(rep, dict) else {}

    def __call__(self, _pid):
        if self.rep.get("kind") == "bool":
            return bool(self.rep.get("value"))
        raise OSError(5, "pid probe failed")


def orphaned(r):
    exp = r.get("expected")
    expected = exp.get("value") if isinstance(exp, dict) and exp.get("kind") == "marker" else None
    return live._is_serve_orphaned(
        int(r.get("desktop_pid") or 0),
        expected,
        pid_exists=_PidExists(r.get("pid")),
        process_start_marker=_Probe(r.get("probe")),
    )


def watchdog_plan(r):
    """Run the real arming code with the loop captured instead of started."""
    saved_env = dict(os.environ)
    saved = (live.threading, live.time, live._is_serve_orphaned, live._request_orphan_shutdown)
    seen = {"pid": None, "sleeps": [], "calls": 0}
    _Captured.target = None
    try:
        for key in ("HERMES_PARENT_PID", "HERMES_PARENT_START_MARKER",
                    "HERMES_PARENT_NONCE", "HERMES_SERVE_WATCHDOG_POLL_S"):
            os.environ.pop(key, None)
        for key, field in (("HERMES_PARENT_PID", "parent_pid"),
                           ("HERMES_PARENT_START_MARKER", "start_marker"),
                           ("HERMES_PARENT_NONCE", "nonce"),
                           ("HERMES_SERVE_WATCHDOG_POLL_S", "poll_env")):
            if r.get(field) is not None:
                os.environ[key] = r[field]
        live.threading = SimpleNamespace(Thread=_Captured)
        live._request_orphan_shutdown = lambda: None
        live.time = SimpleNamespace(sleep=lambda s: seen["sleeps"].append(s))

        def _orphan_stub(pid, marker=None, **kw):
            seen["pid"] = pid
            seen["calls"] += 1
            return seen["calls"] > 1  # first poll: alive -> sleep; then: gone

        live._is_serve_orphaned = _orphan_stub
        live._start_parent_death_watchdog()
        target = _Captured.target
        if target is None:
            return {"armed": False, "desktop_pid": None, "poll": None}
        target()
        poll = seen["sleeps"][0] if seen["sleeps"] else None
        return {"armed": True, "desktop_pid": seen["pid"], "poll": poll}
    finally:
        live.threading, live.time, live._is_serve_orphaned, live._request_orphan_shutdown = saved
        os.environ.clear()
        os.environ.update(saved_env)


class _OsErr(OSError):
    pass


def _exc_from(r):
    exc = _OsErr("bind failed")
    exc.errno = r.get("errno")
    if r.get("winerror") is not None:
        exc.winerror = r["winerror"]
    return exc


def addr_in_use(r):
    return live._is_addr_in_use_error(_exc_from(r))


class _FakeSock:
    def __init__(self, family, style, seen):
        self.seen = seen
        seen["family"] = family
        seen["style"] = style

    def setsockopt(self, *_a):
        return None

    def bind(self, addr):
        self.seen["addr"] = addr
        mode = self.seen["mode"]
        if mode == "ok":
            return None
        if mode == "error":
            raise RuntimeError("probe exploded")
        raise self.seen["exc"]

    def close(self):
        self.seen["closed"] = True


def port_preflight(r):
    seen = {"mode": r.get("bind") or "raise", "exc": _exc_from(r)}
    fake = types.ModuleType("socket")
    fake.AF_INET = "AF_INET"
    fake.AF_INET6 = "AF_INET6"
    fake.SOCK_STREAM = "SOCK_STREAM"
    fake.SOL_SOCKET = "SOL_SOCKET"
    fake.SO_REUSEADDR = 2
    fake.socket = lambda family, style: _FakeSock(family, style, seen)
    saved = sys.modules.get("socket")
    sys.modules["socket"] = fake
    try:
        conflict = bool(live._port_bind_conflict(r.get("host"), int(r.get("port") or 0)))
    finally:
        if saved is not None:
            sys.modules["socket"] = saved
        else:
            sys.modules.pop("socket", None)
    probed = int(r.get("port") or 0) != 0
    return {"conflict": conflict, "family": seen.get("family") if probed else None}


def sentinel(r):
    return live._PORT_IN_USE_SENTINEL.format(port=int(r.get("port") or 0))


def hint(r):
    buf = io.StringIO()
    saved = live._write_machine_sentinel_line
    live._write_machine_sentinel_line = lambda line: None
    try:
        with contextlib.redirect_stdout(buf):
            live._report_port_in_use(r.get("host"), int(r.get("port") or 0))
    finally:
        live._write_machine_sentinel_line = saved
    text = buf.getvalue()
    return text[:-1] if text.endswith("\n") else text


def allow_ips(r):
    cfg = r.get("config")
    return live._dashboard_forwarded_allow_ips(cfg if isinstance(cfg, dict) else {})


def browser(r):
    saved_env = dict(os.environ)
    saved = (live.threading, live.sys, live.time)
    opened = []
    original_open = webbrowser.open
    _Captured.target = None
    try:
        for key in ("DISPLAY", "WAYLAND_DISPLAY"):
            os.environ.pop(key, None)
        if r.get("display") is not None:
            os.environ["DISPLAY"] = r["display"]
        if r.get("wayland") is not None:
            os.environ["WAYLAND_DISPLAY"] = r["wayland"]
        live.sys = SimpleNamespace(platform=r.get("platform"))
        live.threading = SimpleNamespace(Thread=_Captured)
        live.time = SimpleNamespace(sleep=lambda _s: None)
        webbrowser.open = lambda url, *a, **k: opened.append(url)
        live._maybe_open_browser(r.get("host"), int(r.get("port") or 0),
                                 bool(r.get("open_browser")), r.get("profile") or "")
        target = _Captured.target
        if target is None:
            return None
        target()
        return opened[0] if opened else None
    finally:
        webbrowser.open = original_open
        live.threading, live.sys, live.time = saved
        os.environ.clear()
        os.environ.update(saved_env)


def expected(r):
    f = r.get("func")
    if f == "constants":
        return constants(r)
    if f == "valid_marker":
        return valid_marker(r)
    if f == "markers_match":
        return markers_match(r)
    if f == "mismatch_conclusive":
        return mismatch_conclusive(r)
    if f == "orphaned":
        return orphaned(r)
    if f == "watchdog_plan":
        return watchdog_plan(r)
    if f == "addr_in_use":
        return addr_in_use(r)
    if f == "port_preflight":
        return port_preflight(r)
    if f == "sentinel":
        return sentinel(r)
    if f == "hint":
        return hint(r)
    if f == "allow_ips":
        return allow_ips(r)
    if f == "browser":
        return browser(r)
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
    if cases < MIN_CASES:
        print("ORACLE %s: FAILED gate -- %d cases < %d" % (GATE, cases, MIN_CASES),
              file=sys.stderr)
        sys.exit(1)
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
