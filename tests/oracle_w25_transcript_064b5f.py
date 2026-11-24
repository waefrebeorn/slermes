#!/usr/bin/env python3
"""Differential oracle for tools/transcription_whisper_cpp.py (gate w25_transcript_064b5f).

The live module is imported as-is; what it treats as outer world is pinned:

  * ``pm.installed_package`` / ``pm.downloader`` -- a stub package manager that
    reports one fixed binary (or none) and a no-op downloader;
  * ``_model_dir`` -- the profile cache directory becomes the fixture's path.

Everything else (the shlex quoting, the command template, the ValueError text
and the _MODEL_FILES table) is the live code answering.
"""
import json
import shlex
import sys
import tempfile
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_transcript_064b5f"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from tools import transcription_whisper_cpp as live  # noqa: E402

WANTED = {"quote", "model_dir", "command", "validate_model", "model_files"}
_PM_KEYS = ("pm", "pm.downloader")


class _Source:
    def __init__(self, url, dest, sha256):
        self.url, self.dest, self.sha256 = url, dest, sha256


class _Download:
    def __init__(self, sources, partials_dir=None):
        self.sources, self.partials_dir = sources, partials_dir

    def run(self):
        return None


def _fake_pm(binary):
    pm = types.ModuleType("pm")
    dl = types.ModuleType("pm.downloader")
    dl.Source = _Source
    dl.Download = _Download
    pm.downloader = dl
    if binary is None:
        pm.installed_package = lambda name, allow_outdated=False: None
    else:
        pm.installed_package = lambda name, allow_outdated=False: types.SimpleNamespace(
            binary=Path(binary))
    return pm, dl


def _pinned(models_dir, binary):
    """(restore, pm modules installed) around a live call with fixed seams."""
    saved = {k: sys.modules.get(k) for k in _PM_KEYS}
    pm, dl = _fake_pm(binary)
    sys.modules["pm"] = pm
    sys.modules["pm.downloader"] = dl
    original = live._model_dir
    live._model_dir = lambda: Path(models_dir)

    def restore():
        live._model_dir = original
        for key, value in saved.items():
            if value is None:
                sys.modules.pop(key, None)
            else:
                sys.modules[key] = value

    return restore


def quote(req):
    return shlex.quote(Path(req.get("path")).as_posix())


def model_dir(req):
    return (Path(req.get("home") or "") / "cache" / "whisper.cpp").as_posix()


def command(req):
    restore = _pinned(req.get("models_dir") or "", req.get("binary"))
    try:
        return live.whisper_cpp_command()
    finally:
        restore()


def validate_model(req):
    restore = _pinned(tempfile.mkdtemp(prefix="w25tx_"), None)
    try:
        live.ensure_whisper_cpp_models(req.get("model_name"))
        return None
    except ValueError as exc:
        return str(exc)
    finally:
        restore()


def model_files(_req):
    return [{"name": name, "url": url, "sha256": digest}
            for name, url, digest in live._MODEL_FILES]


def expected(r):
    f = r.get("func")
    if f == "quote":
        return quote(r)
    if f == "model_dir":
        return model_dir(r)
    if f == "command":
        return command(r)
    if f == "validate_model":
        return validate_model(r)
    if f == "model_files":
        return model_files(r)
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
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
