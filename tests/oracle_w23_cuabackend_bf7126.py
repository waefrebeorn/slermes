#!/usr/bin/env python3
"""Differential oracle for tools/computer_use/cua_backend_capture.py (gate
w23_cuabackend_bf7126).

The cua-driver transport is the seam: `list_windows` / `list_apps` payloads and
the xprop `_NET_ACTIVE_WINDOW` probe are supplied by the fixture, so no daemon,
no MCP client and no subprocess is started.  Everything compared is the live
code: the self-window and shell-window predicates, `_select_capture_target`
(with sys.platform and the xprop probe swapped for the fixture's values),
`_sorted_windows` normalisation, `_png_metrics` base64 + header sniffing, the
`app=` resolution tiers (with `list_apps` answering the fixture's rows) and the
`list_apps` payload ladder run as the real mixin method over a fake session.
"""
import base64
import importlib.util
import json
import struct
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "tools" / "computer_use" / "cua_backend_capture.py"
GATE = "w23_cuabackend_bf7126"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("cua_capture_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["cua_capture_live"] = live
_spec.loader.exec_module(live)

WANTED = {"self_window", "desktop_window", "select_target", "sorted_windows",
          "png_metrics", "match_windows", "list_apps", "is_full_screen",
          "is_desktop_shell", "no_app_match_message", "no_desktop_message",
          "constants"}


class FakeApps:
    """Stands in for the monitor's self: only list_apps is consulted."""

    def __init__(self, apps):
        self._apps = apps

    def list_apps(self):
        return self._apps


class FakePayloadSession:
    _session_id = "probe"

    def __init__(self, payload):
        self.payload = payload

    @property
    def _session(self):
        payload = self.payload
        return types.SimpleNamespace(call_tool=lambda name, args: payload)


def norm(value):
    if isinstance(value, tuple):
        return [norm(v) for v in value]
    if isinstance(value, list):
        return [norm(v) for v in value]
    if isinstance(value, dict):
        return {k: norm(v) for k, v in value.items()}
    return value


def select_target(req):
    saved_sys = live.sys
    saved_probe = live._linux_x11_active_window_id
    live.sys = types.SimpleNamespace(platform=req.get("platform", "win32"))
    live._linux_x11_active_window_id = lambda: req.get("active_id")
    try:
        return live._select_capture_target(
            req.get("windows") or [],
            app_requested=bool(req.get("app_requested")),
            exact_target=bool(req.get("exact_target")),
        )
    finally:
        live.sys = saved_sys
        live._linux_x11_active_window_id = saved_probe


def expected(r):
    f = r.get("func")
    if f == "self_window":
        return live._is_cua_driver_self_window(r.get("w") or {})
    if f == "desktop_window":
        if r.get("backdrop"):
            return live._is_desktop_window(r.get("w") or {}, live._DESKTOP_BACKDROP_NAMES)
        return live._is_desktop_window(r.get("w") or {})
    if f == "select_target":
        return select_target(r)
    if f == "sorted_windows":
        return live._sorted_windows(r)
    if f == "png_metrics":
        return list(live._png_metrics(r.get("png_b64") or "", int(r.get("width", 0)), int(r.get("height", 0))))
    if f == "match_windows":
        return live._CaptureMixin._match_windows_for_app(
            FakeApps(r.get("apps") or []), r.get("windows") or [], r.get("app", ""))
    if f == "list_apps":
        return live._CaptureMixin.list_apps(FakePayloadSession(r))
    if f == "is_full_screen":
        app = r.get("app")
        return bool(app and app.strip().lower() in live._FULL_SCREEN_SENTINELS)
    if f == "is_desktop_shell":
        app = r.get("app")
        return bool(app and app.strip().lower() in live._DESKTOP_SHELL_SENTINELS)
    if f == "no_app_match_message":
        return live._NO_APP_MATCH_MSG.format(app=r.get("app"))
    if f == "no_desktop_message":
        return live._NO_DESKTOP_WINDOW_MSG.format(app=r.get("app"))
    if f == "constants":
        from tools.computer_use.cua_backend_parse import _NON_APP_WINDOW_TITLE_PREFIXES
        return {
            "full_screen_sentinels": sorted(live._FULL_SCREEN_SENTINELS),
            "desktop_shell_sentinels": sorted(live._DESKTOP_SHELL_SENTINELS),
            "desktop_window_names": list(live._DESKTOP_WINDOW_NAMES),
            "desktop_backdrop_names": list(live._DESKTOP_BACKDROP_NAMES),
            "non_app_title_prefixes": list(_NON_APP_WINDOW_TITLE_PREFIXES),
            "no_app_match_msg": live._NO_APP_MATCH_MSG,
            "no_desktop_window_msg": live._NO_DESKTOP_WINDOW_MSG,
            "no_desktop_image_msg": live._NO_DESKTOP_IMAGE_MSG,
            "full_screen_note": live._FULL_SCREEN_NOTE,
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
            py = norm(expected(r))
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
