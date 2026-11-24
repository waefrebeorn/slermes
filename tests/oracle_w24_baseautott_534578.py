#!/usr/bin/env python3
"""Differential oracle for gateway/platforms/base_auto_tts.py
(gate w24_baseautott_534578).

The live module is loaded from its own file (the package __init__ pulls the
whole adapter stack, which this module does not need) and both pure halves are
called for real:

  * ``_profile_scope(adapter, source)`` -- nullcontext vs the runner's resolver;
  * ``synthesize_auto_tts(adapter, text, source, output_path_for)`` -- run
    through ``asyncio.run`` with a fake adapter (``prepare_tts_text``, no
    gateway_runner), a fake ``tools.tts_tool`` module answering the case's
    ``tts`` object, and real files created under a temp dir for every name in
    ``existing``.  The returned (paths, requested_path) is live truth; the
    warning the module logs on failure is read back for the error text.

Paths are compared relative to that temp dir, so the fixture stays portable.
"""
import asyncio
import contextlib
import importlib.util
import json
import logging
import sys
import tempfile
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_baseautott_534578"
LOG_NAME = "hermes_live_base_auto_tts"

_spec = importlib.util.spec_from_file_location(
    LOG_NAME, QUARRY / "gateway" / "platforms" / "base_auto_tts.py")
live = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(live)

WANTED = {"synthesize", "profile_scope", "constants"}
EMPTY_TEXT = "Empty text after markdown cleanup"


def _truthy(value):
    if isinstance(value, (list, dict, str)):
        return bool(value)
    return bool(value)


class _Capture(logging.Handler):
    def __init__(self):
        super().__init__()
        self.messages = []

    def emit(self, record):
        self.messages.append(record)


def _install_tools(tts_data, requirements):
    fake = types.ModuleType("tools.tts_tool")
    fake.check_tts_requirements = lambda: bool(requirements)
    fake.text_to_speech_tool = lambda **kwargs: json.dumps(tts_data)
    pkg = types.ModuleType("tools")
    pkg.__path__ = []
    pkg.tts_tool = fake
    return {"tools": pkg, "tools.tts_tool": fake}


def synthesize(r):
    existing = r.get("existing") if isinstance(r.get("existing"), list) else []
    tts = r.get("tts")
    tts_data = dict(tts) if isinstance(tts, dict) else {}
    tmp = Path(tempfile.mkdtemp(prefix="w24tts_"))
    for name in existing:
        if not isinstance(name, str) or not name:
            continue
        target = tmp / name
        try:
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(b"x")
        except OSError:
            pass

    def _map(value):
        """The fake tool reports paths inside this case's sandbox, so the live
        Path(...).exists() sees the files the case says are present."""
        if isinstance(value, str) and value:
            return str(tmp / value)
        return value

    if isinstance(tts_data.get("file_paths"), list):
        tts_data["file_paths"] = [_map(v) for v in tts_data["file_paths"]]
    if "file_path" in tts_data:
        tts_data["file_path"] = _map(tts_data["file_path"])

    class _Adapter:
        platform = "discord"
        name = "adapter"
        gateway_runner = None

        def prepare_tts_text(self, text):
            return r.get("clean_text")

    saved = {k: sys.modules.get(k) for k in ("tools", "tools.tts_tool")}
    sys.modules.update(_install_tools(tts_data, r.get("requirements")))
    cap = _Capture()
    logger = logging.getLogger(LOG_NAME)
    logger.addHandler(cap)
    try:
        paths, requested = asyncio.run(
            live.synthesize_auto_tts(_Adapter(), "text", None,
                                     lambda platform: r.get("output_path")))
    finally:
        logger.removeHandler(cap)
        for key, value in saved.items():
            if value is None:
                sys.modules.pop(key, None)
            else:
                sys.modules[key] = value

    rel = []
    for p in paths:
        try:
            rel.append(str(Path(p).relative_to(tmp)))
        except ValueError:
            rel.append(str(p))
    error = None
    for record in cap.messages:
        args = record.args or ()
        if len(args) > 1:
            text = str(args[1])
            error = EMPTY_TEXT if text == EMPTY_TEXT else "raised"
            break
    return {"paths": rel, "requested_path": requested, "error": error}


class _Resolved:
    """Marker object the fake resolver hands back (proves resolve() ran)."""


def profile_scope(r):
    runner = None
    if r.get("noncallable"):
        runner = types.SimpleNamespace(_async_profile_scope_for_source=5)
    elif r.get("resolver"):
        runner = types.SimpleNamespace(
            _async_profile_scope_for_source=lambda source: _Resolved())
    adapter = types.SimpleNamespace(gateway_runner=runner)
    cm = live._profile_scope(adapter, r.get("source"))
    return "resolve" if isinstance(cm, _Resolved) else "nullcontext"


def constants():
    return {"empty_text": EMPTY_TEXT, "default_success": True}


def expected(r):
    f = r.get("func")
    try:
        if f == "synthesize":
            return synthesize(r)
        if f == "profile_scope":
            return profile_scope(r)
        if f == "constants":
            return constants()
    except Exception:  # noqa: BLE001 - the error seam: C must answer null
        return None
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
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
