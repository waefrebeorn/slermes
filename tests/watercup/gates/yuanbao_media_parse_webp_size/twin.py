"""twin for yuanbao_media_parse_webp_size.

Imports LIVE gateway/platforms/yuanbao_media.py::_parse_webp_size and traces
the SAME ladder as the C harness over hex-encoded byte buffers restricted to
the agreed sub-contract (>=30 bytes, RIFF/WEBP, VP8/VP8L chunks).
"""
import importlib.util as ilu
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

spec = ilu.spec_from_file_location(
    "wc_ybm", os.path.join(DEV, "gateway", "platforms", "yuanbao_media.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_ybm"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup


def unhex(s):
    return bytes.fromhex(s) if s else b""


def run(inputs):
    cups = []
    for raw in inputs:
        buf = unhex(raw)
        c = Cup("parse_webp_size")
        res = mod._parse_webp_size(buf)
        too_short = len(buf) < 30
        c.branch(too_short)
        bad_header = (not too_short) and (
            buf[:4] != b"RIFF" or buf[8:12] != b"WEBP")
        c.branch(bad_header)
        chunk = buf[12:16] if len(buf) >= 16 else b""
        is_vp8 = (not too_short) and (not bad_header) and chunk == b"VP8 "
        c.branch(is_vp8)
        is_vp8l = (not is_vp8) and chunk == b"VP8L"
        c.branch(is_vp8l)
        if res is None:
            c.valstr("none")
        else:
            c.valstr("width=%d,height=%d" % (res["width"], res["height"]))
        cups.append(c.finalize())
    return cups
