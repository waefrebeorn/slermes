#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/telegram/telegram_audio_meta.py
(gate w26_telegramau_5c2119).

The module's probes are the seams, and each is replaced by a fixed input while
the live code still runs:

  * ``shutil.which`` / ``subprocess.run`` -- answer as ffprobe would for the
    fixture's JSON blob (or refuse with a non-zero return code);
  * ``_extract_cover_jpeg`` -- a sentinel, so ``has_cover`` is observed by
    whether the live ``audio_player_fields`` asks for the cover at all;
  * the thumbnail pass -- recorded argv, so the ``-ss`` value the live
    ``_video_thumbnail_jpeg`` computes is read back;
  * the WAV branch -- a real RIFF file written with the fixture's rate and
    frame count, so ``_probe_voice_duration_seconds`` reads it with ``wave``;
    ``mutagen`` is hidden and ffprobe is absent so nothing else can answer.
"""
import json
import shutil
import struct
import sys
import tempfile
import wave
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_telegramau_5c2119"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

import importlib.util

# The module itself imports only stdlib; its package __init__ drags in the whole
# gateway tree, so load the live file directly.
_SPEC = importlib.util.spec_from_file_location(
    "telegram_audio_meta_live",
    QUARRY / "plugins" / "platforms" / "telegram" / "telegram_audio_meta.py")
live = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(live)

WANTED = {"coerce_duration", "audio_tags", "has_cover", "video_geometry",
          "video_thumb_seek", "wav_duration"}

_TMP = Path(tempfile.mkdtemp(prefix="w26tg-"))


class _Proc:
    def __init__(self, stdout="", returncode=0):
        self.stdout = stdout
        self.stderr = ""
        self.returncode = returncode


def _blob(req):
    raw = req.get("blob")
    payload = json.dumps(raw if isinstance(raw, dict) else {})
    return _Proc(payload, req.get("returncode", 0))


def coerce_duration(req):
    return live._coerce_duration_seconds(req.get("value"))


def _audio_fields(req, extract):
    proc = _blob(req)
    saved = (shutil.which, live.subprocess.run, live._extract_cover_jpeg)
    shutil.which = lambda name: "/usr/bin/ffprobe" if name == "ffprobe" else None
    live.subprocess.run = lambda *a, **k: proc
    live._extract_cover_jpeg = extract
    try:
        return live.audio_player_fields(str(_TMP / "x.mp3"))
    finally:
        shutil.which, live.subprocess.run, live._extract_cover_jpeg = saved


def audio_tags(req):
    return _audio_fields(req, lambda path: None)


def has_cover(req):
    calls = []

    def extract(path):
        calls.append(path)
        return "SENTINEL"

    _audio_fields(req, extract)
    return bool(calls)


def video_geometry(req):
    proc = _blob(req)
    saved = (shutil.which, live.subprocess.run)
    shutil.which = lambda name: "/usr/bin/ffprobe" if name == "ffprobe" else None
    live.subprocess.run = lambda *a, **k: proc
    try:
        return live._probe_video_geometry(str(_TMP / "x.mp4"))
    finally:
        shutil.which, live.subprocess.run = saved


def video_thumb_seek(req):
    seen = {}

    def fake_run(argv, **kw):
        seen["argv"] = list(argv)
        return _Proc("", 0)

    saved = (shutil.which, live.subprocess.run, live.os.path.getsize, live.os.remove)
    shutil.which = lambda name: "/usr/bin/ffmpeg"
    live.subprocess.run = fake_run
    live.os.path.getsize = lambda p: 0
    live.os.remove = lambda p: None
    try:
        live._video_thumbnail_jpeg(str(_TMP / "x.mp4"), req.get("duration"))
    finally:
        shutil.which, live.subprocess.run = saved[0], saved[1]
        live.os.path.getsize, live.os.remove = saved[2], saved[3]
    argv = seen.get("argv") or []
    if "-ss" not in argv:
        raise AssertionError("no -ss in argv: %r" % (argv,))
    return int(argv[argv.index("-ss") + 1])


def wav_duration(req):
    rate = req.get("rate")
    frames = req.get("nframes") or 0
    path = _TMP / "probe.wav"
    if not rate:
        path.write_bytes(
            b"RIFF" + struct.pack("<I", 36) + b"WAVEfmt " + struct.pack("<IHHIIHH", 16, 1, 1, 0, 0, 2, 16)
            + b"data" + struct.pack("<I", 0))
    else:
        with wave.open(str(path), "wb") as wf:
            wf.setnchannels(1)
            wf.setsampwidth(2)
            wf.setframerate(int(rate))
            wf.writeframes(b"\x00\x00" * int(frames))
    saved_which = shutil.which
    saved_mutagen = sys.modules.get("mutagen", _MISSING)
    shutil.which = lambda name: None            # no ffprobe
    sys.modules["mutagen"] = None               # `import mutagen` -> ImportError
    try:
        return live._probe_voice_duration_seconds(str(path))
    finally:
        shutil.which = saved_which
        if saved_mutagen is _MISSING:
            sys.modules.pop("mutagen", None)
        else:
            sys.modules["mutagen"] = saved_mutagen


_MISSING = object()

DISPATCH = {
    "coerce_duration": coerce_duration,
    "audio_tags": audio_tags,
    "has_cover": has_cover,
    "video_geometry": video_geometry,
    "video_thumb_seek": video_thumb_seek,
    "wav_duration": wav_duration,
}


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
            py = DISPATCH[r["func"]](r)
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
    shutil.rmtree(_TMP, ignore_errors=True)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
