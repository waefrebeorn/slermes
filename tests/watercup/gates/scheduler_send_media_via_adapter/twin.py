"""twin for scheduler_send_media_via_adapter.

Traces the LIVE cron/scheduler.py::_send_media_via_adapter per-file ROUTING
ladder (send_voice / send_video / send_image_file / send_document dispatch)
with the same rung order as the C harness, which links the REAL port
src/cron/scheduler_runtime_impl.c::scheduler_route_media. The routing
decision itself comes from LIVE gateway/platforms/base.py
(should_send_media_as_audio + the _VIDEO_EXTS/_IMAGE_EXTS sets) — the
adapter/network delivery half is I/O and out of the traced path.

Input format "<platform>\\t<is_voice 0|1>\\t<media_path>".
"""
import os, sys
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

from watercup_py import Cup


def _load(name, rel):
    spec = ilu.spec_from_file_location(name, os.path.join(DEV, rel))
    mod = ilu.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod


# LIVE routing helpers (imported by path, exactly what scheduler.py uses).
try:
    _base = _load("wc_live_gpb", os.path.join("gateway", "platforms", "base.py"))
    should_send_media_as_audio = _base.should_send_media_as_audio
except Exception:  # pragma: no cover
    should_send_media_as_audio = None

_sched_src = open(os.path.join(DEV, "cron", "scheduler.py")).read()
import re as _re


def _grab(setname):
    m = _re.search(r"^%s\s*=\s*frozenset\(\{([^}]*)\}\)" % setname,
                   _sched_src, _re.M | _re.S)
    return set(x.strip().strip("'\"") for x in m.group(1).split(","))


VIDEO_EXTS = _grab("_VIDEO_EXTS")
IMAGE_EXTS = _grab("_IMAGE_EXTS")


def route(platform, media_path, is_voice):
    """Mirror the LIVE _send_media_via_adapter per-file ladder."""
    from pathlib import Path
    c = Cup("_send_media_via_adapter")
    ext = Path(media_path).suffix.lower()

    as_audio = bool(should_send_media_as_audio and
                    should_send_media_as_audio(platform, ext, is_voice=is_voice))
    c.branch(ext in {".ogg", ".opus", ".mp3", ".wav", ".m4a", ".flac"})   # B1
    if ext in {".ogg", ".opus", ".mp3", ".wav", ".m4a", ".flac"}:
        c.branch(platform == "telegram")                                   # B2
        if platform == "telegram":
            c.branch(ext in (".ogg", ".opus"))                             # B3
            if ext not in (".ogg", ".opus"):
                c.branch(ext in (".mp3", ".m4a"))                          # B4
        # else branch: non-telegram audio always as_audio (set above)
    else:
        pass  # B1 false: skip straight to video/image ladder

    if as_audio:
        c.branch(True)                                                     # B5
        route_name = "voice"
    else:
        c.branch(False)                                                    # B5
        if ext in VIDEO_EXTS:
            c.branch(True)                                                 # B6
            route_name = "video"
        else:
            c.branch(False)                                                # B6
            if ext in IMAGE_EXTS:
                c.branch(True)                                             # B7
                route_name = "image"
            else:
                c.branch(False)                                            # B7
                route_name = "document"

    c.valstr(route_name)
    return c.finalize()


def run(inputs):
    cups = []
    for raw in inputs:
        platform, voice, path = raw.replace("\\t", "\t").split("\t", 2)
        platform = platform if platform != "-" else None
        cups.append(route(platform, path, voice == "1"))
    return cups
