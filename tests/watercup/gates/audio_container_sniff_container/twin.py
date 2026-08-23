"""twin for audio_container_sniff_container.

LIVE tools/audio_container.py::sniff_container:

    if len(data) >= 8 and data[4:8] == b"ftyp":
        if len(data) >= 12 and data[8:12].lower() in _MP4_AUDIO_BRANDS:
            return "m4a"
        return "mp4"
    if data.startswith(b"OggS"):  return "ogg"
    if data.startswith(b"fLaC"):  return "flac"
    if len(data) >= 12 and data[:4] == b"RIFF" and data[8:12] == b"WAVE": return "wav"
    if data.startswith(b"ID3"):   return "mp3"
    if len(data) >= 2 and data[0] == 0xFF and (data[1] & 0xE0) == 0xE0:
        if (data[1] & 0xF6) == 0xF0: return "aac"
        return "mp3"
    if data.startswith(b"\x1a\x45\xdf\xa3"): return "webm"
    return None

Inputs arrive as hex strings; decoded to bytes here. The harness short-circuits
each ladder rung (`if (!result && cond)` / `else if (!result)`) so once a
container is claimed no further branch events fire — mirrored exactly here.
"""
import os
from watercup_py import Cup


def run(inputs):
    cups = []
    for raw in inputs:
        hx = "".join(raw.split())
        data = bytes.fromhex(hx) if hx else b""
        c = Cup("sniff_container")
        result = None

        # ftyp rung (always evaluated)
        if len(data) >= 12 and data[4:8] == b"ftyp":
            c.branch(True)
            brand = data[8:12].lower()
            if brand == b"m4a " or brand == b"m4b ":
                c.branch(True)
                result = "m4a"
            else:
                c.branch(False)
                result = "mp4"
        else:
            c.branch(False)

        # ogg
        if result is None:
            cond = data.startswith(b"OggS")
            c.branch(cond)
            if cond:
                result = "ogg"
        # flac
        if result is None:
            cond = data.startswith(b"fLaC")
            c.branch(cond)
            if cond:
                result = "flac"
        # wav
        if result is None:
            cond = len(data) >= 12 and data[:4] == b"RIFF" and data[8:12] == b"WAVE"
            c.branch(cond)
            if cond:
                result = "wav"
        # mp3 via ID3
        if result is None:
            cond = data.startswith(b"ID3")
            c.branch(cond)
            if cond:
                result = "mp3"
        # sync: mp3/aac
        if result is None:
            cond = len(data) >= 2 and data[0] == 0xFF and (data[1] & 0xE0) == 0xE0
            c.branch(cond)
            if cond:
                if (data[1] & 0xF6) == 0xF0:
                    c.branch(True)
                    result = "aac"
                else:
                    c.branch(False)
                    result = "mp3"
        # webm
        if result is None:
            cond = data.startswith(b"\x1a\x45\xdf\xa3")
            c.branch(cond)
            if cond:
                result = "webm"

        c.valstr(result or "")
        cups.append(c.finalize())
    return cups
