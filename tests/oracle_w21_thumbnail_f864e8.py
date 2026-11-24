#!/usr/bin/env python3
"""Differential oracle for tools/bot_desktop/thumbnail.py
(gate w21_thumbnail_f864e8).

The live `thumbnail_data_url` runs from its real source. Its own seams are
stubbed and driven from the fixture: `tools.bot_desktop.runtime` (published_env,
sandbox_screen_running, _owned_sandbox_env, _launcher_pid), the sandbox grab,
and `PIL.ImageGrab.grab` -- which returns a stand-in image that does its
resizing with the REAL Pillow (`Image.thumbnail`) and emits the fixture's JPEG
bytes. So the flow, the thumbnail size and the base64 data URL are all live;
the C side has to agree with the same Pillow arithmetic and the same framing.

`func: size` compares the C thumbnail arithmetic against live Pillow directly.
"""
import base64
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

from PIL import Image as PILImage  # noqa: E402  (the library the module calls)

GATE = "w21_thumbnail_f864e8"
SRC = "C:/Users/eman5/hermes-agent/tools/bot_desktop/thumbnail.py"

CASE = {}
SEEN = {"image": None}
THUMB_MAX = (960, 600)


def _mk(name, **attrs):
    m = types.ModuleType(name)
    m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


def _hex_bytes(key):
    text = CASE.get(key)
    return bytes.fromhex(text) if text else None


def _published_env():
    return dict(CASE.get("env") or {})


def _sandbox_running():
    return bool(CASE.get("sandbox_running"))


def _owned_sandbox():
    # None is the live "no sandbox owns this screen"; anything else is an env
    return None if CASE.get("owned_sandbox") is None else {"id": "sbx"}


def _launcher_pid():
    return CASE.get("launcher_pid")


def _grab_jpeg(env, published, max_size, quality):
    return _hex_bytes("sandbox_jpeg_hex")


class FakeImage:
    """Pillow's own resize math, the fixture's JPEG bytes on save()."""

    def __init__(self, size, payload):
        self.size = tuple(size)
        self.payload = payload

    def thumbnail(self, max_size):
        probe = PILImage.new("RGB", self.size)
        probe.thumbnail(tuple(max_size))
        self.size = probe.size
        SEEN["image"] = self

    def convert(self, mode):
        return self

    def save(self, buf, fmt, quality=None, optimize=False):
        buf.write(self.payload or b"")


def _grab(xdisplay=None):
    image = CASE.get("image") or {}
    return FakeImage((image.get("width", 0), image.get("height", 0)), _hex_bytes("jpeg_hex"))


_mk("tools")
_mk("tools.bot_desktop")
_mk("tools.bot_desktop.runtime", published_env=_published_env,
    sandbox_screen_running=_sandbox_running, _owned_sandbox_env=_owned_sandbox,
    _launcher_pid=_launcher_pid)
_mk("tools.bot_desktop.sandbox_host", grab_jpeg=_grab_jpeg)
_mk("PIL", ImageGrab=types.SimpleNamespace(grab=_grab))

_sp = importlib.util.spec_from_file_location("tools.bot_desktop.thumbnail", SRC)
live = importlib.util.module_from_spec(_sp)
sys.modules["tools.bot_desktop.thumbnail"] = live
_sp.loader.exec_module(live)


def expected(r):
    global CASE
    f = r.get("func")
    if f == "size":
        probe = PILImage.new("RGB", (int(r.get("width")), int(r.get("height"))))
        probe.thumbnail((int(r.get("max_width", 960)), int(r.get("max_height", 600))))
        return list(probe.size)
    if f == "data_url":
        raw = bytes.fromhex(r.get("hex") or "")
        if not raw:
            return None
        return "data:image/jpeg;base64," + base64.b64encode(raw).decode("ascii")
    if f == "flow":
        CASE = r
        SEEN["image"] = None
        max_size = tuple(r.get("max_size") or THUMB_MAX)
        url = live.thumbnail_data_url(max_size, int(r.get("quality", 72)))
        if url is None:
            return {"status": "none"}
        image = SEEN["image"]
        return {"status": "data", "value": url,
                "branch": "grab" if image is not None else "sandbox",
                "size": list(image.size) if image is not None else None}
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    wanted = {"size", "data_url", "flow"}
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
