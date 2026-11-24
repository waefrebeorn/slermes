#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/whatsapp/adapter.py slice 2
(gate w27_adapter_42b735_p2): bridge lifecycle + endpoint addressing.

``WhatsAppAdapter`` instances are made with ``__new__`` and given only the
attributes each method reads -- no config, no aiohttp session, no process.  The
seam is the process/filesystem boundary, which the oracle feeds real temp
files and a stub ``poll()``:

  * ``_bridge_url``                       the loopback endpoint string;
  * ``_read_bridge_pidfile``              pid / start-time / port line parsing;
  * ``_bridge_died``                      the (code, message, retryable) it
                                          hands to ``_set_fatal_error``;
  * ``_is_connected``                     the enablement predicate.
"""
import contextlib
import importlib.util
import io
import json
import os
import sys
import tempfile
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w27_adapter_42b735_p2"

# The msys bash that runs this oracle has no USERPROFILE, and pathlib.Path.home()
# (touched at import time by hermes_constants) raises without it.  Give it the
# real Windows home the repo lives under.
if not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = str(Path(REPO).parent)
if not os.environ.get("HOME"):
    os.environ["HOME"] = os.environ["USERPROFILE"]

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location(
    "wa_adapter_live_" + GATE, QUARRY / "plugins" / "platforms" / "whatsapp" / "adapter.py")
live = importlib.util.module_from_spec(_spec)
sys.modules[_spec.name] = live
with contextlib.redirect_stdout(io.StringIO()):
    _spec.loader.exec_module(live)

ADAPTER = live.WhatsAppAdapter
WANTED = {"bridge_url", "pidfile_parse", "bridge_died", "is_connected"}


def bare():
    """An adapter with no config, no session and no process."""
    inst = ADAPTER.__new__(ADAPTER)
    inst.platform = live.Platform.WHATSAPP
    return inst


def bridge_url(req):
    inst = bare()
    inst._bridge_port = int(req.get("port") or 0)
    return inst._bridge_url(str(req.get("path") or ""))


def pidfile_parse(req):
    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / "bridge.pid"
        # write_bytes: write_text translates newlines on Windows and would hand the
        # upstream parser a different file than the one the fixture describes.
        path.write_bytes(str(req.get("text") or "").encode("utf-8"))
        pid, start, port = live._read_bridge_pidfile(Path(tmp))
    return [pid, start, port]


def bridge_died(req):
    inst = bare()
    captured = {}
    inst._bridge_log = req.get("log")
    rc = req.get("rc")
    inst._bridge_process = None if not req.get("has_process") else SimpleNamespace(poll=lambda rc=rc: rc)

    def set_fatal_error(code, message, retryable=True):
        captured.update(code=code, detail=message, retryable=retryable)

    inst._set_fatal_error = set_fatal_error
    inst._close_bridge_log = lambda: None
    inst._bridge_died(str(req.get("detail") or ""), str(req.get("code") or "whatsapp_bridge_exited"))
    return captured


def is_connected(req):
    import hermes_cli.gateway as gateway_mod

    original = gateway_mod.get_env_value
    gateway_mod.get_env_value = lambda name, default=None: req.get("env") or ""
    try:
        cfg = req.get("config")
        config = None
        if isinstance(cfg, dict):
            config = SimpleNamespace(enabled=bool(cfg.get("enabled")), extra=cfg.get("extra") or {})
        return bool(live._is_connected(config))
    finally:
        gateway_mod.get_env_value = original


DISPATCH = {
    "bridge_url": bridge_url,
    "pidfile_parse": pidfile_parse,
    "bridge_died": bridge_died,
    "is_connected": is_connected,
}


def expected(req):
    return DISPATCH[req["func"]](req)


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
            with contextlib.redirect_stdout(io.StringIO()):
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
