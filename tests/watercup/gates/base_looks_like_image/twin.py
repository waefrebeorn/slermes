"""twin for base_looks_like_image.

Imports LIVE gateway/platforms/base.py::_looks_like_image and traces the SAME
magic-byte branch ladder as the C harness. Inputs are hex-encoded bytes.
"""
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

spec = ilu.spec_from_file_location("wc_pb_base", os.path.join(DEV, "gateway", "platforms", "base.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_pb_base"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup

FN = "_looks_like_image"


def run(inputs):
    cups = []
    for raw in inputs:
        c = Cup(FN)
        data = bytes.fromhex(raw) if raw else b""
        c.valstr(raw or "")
        live_res = mod._looks_like_image(data)
        result = False
        if len(data) < 4:
            c.branch(True)
        else:
            c.branch(False)
            if data[:8] == b"\x89PNG\r\n\x1a\n":
                c.branch(True); result = True
            else:
                c.branch(False)
                if data[:3] == b"\xff\xd8\xff":
                    c.branch(True); result = True
                else:
                    c.branch(False)
                    if data[:6] in {b"GIF87a", b"GIF89a"}:
                        c.branch(True); result = True
                    else:
                        c.branch(False)
                        if data[:2] == b"BM":
                            c.branch(True); result = True
                        else:
                            c.branch(False)
                            if data[:4] == b"RIFF" and len(data) >= 12 and data[8:12] == b"WEBP":
                                c.branch(True); result = True
                            else:
                                c.branch(False)
        assert result == live_res, "twin ladder diverged from LIVE"
        c.valstr("True" if live_res else "False")
        cups.append(c.finalize())
    return cups
