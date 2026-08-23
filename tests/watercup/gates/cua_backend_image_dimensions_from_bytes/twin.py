"""twin for cua_backend_image_dimensions_from_bytes.

Imports the LIVE Python tools/computer_use/cua_backend.py::
_image_dimensions_from_bytes and traces the SAME branch ladder as the C
harness (cua_backend_helpers.c port). Inputs are hex-encoded byte strings;
both sides decode before sniffing.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_cua_backend", os.path.join(DEV, "tools", "computer_use", "cua_backend.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_cua_backend"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup

_PNG_MAGIC = b"\x89PNG\r\n\x1a\n"
_SOF = {0xC0, 0xC1, 0xC2, 0xC3, 0xC5, 0xC6, 0xC7,
        0xC9, 0xCA, 0xCB, 0xCD, 0xCE, 0xCF}


def run(inputs):
    cups = []
    for raw in inputs:
        data = bytes.fromhex(raw)
        c = Cup("image_dimensions_from_bytes")
        n = len(data)
        w = h = 0

        if data and n >= 2:                                          # B1
            c.branch(True)
            if n >= 24 and data.startswith(_PNG_MAGIC):              # B2
                c.branch(True)
                w = int.from_bytes(data[16:20], "big")
                h = int.from_bytes(data[20:24], "big")
                if w > 0 and h > 0:                                  # B3
                    c.branch(True)
                else:
                    c.branch(False)
                c.valstr(f"{w}x{h}")
                cups.append(c.finalize())
                continue
            c.branch(False)

            if data.startswith(b"\xff\xd8"):                         # B4
                c.branch(True)
                i = 2
                while i + 9 < n:
                    c.loop()
                    if data[i] != 0xFF:                              # B5
                        c.branch(True)
                        i += 1
                        continue
                    c.branch(False)
                    marker = data[i + 1]
                    i += 2
                    if marker in {0xD8, 0xD9} or 0xD0 <= marker <= 0xD7:   # B6
                        c.branch(True)
                        continue
                    c.branch(False)
                    if i + 2 > n:                                    # B7
                        c.branch(True)
                        break
                    c.branch(False)
                    segment_len = int.from_bytes(data[i:i + 2], "big")
                    if segment_len < 2 or i + segment_len > n:       # B8
                        c.branch(True)
                        break
                    c.branch(False)
                    if marker in _SOF:                               # B9
                        c.branch(True)
                        if segment_len >= 7:                         # B10
                            c.branch(True)
                            hh = int.from_bytes(data[i + 3:i + 5], "big")
                            ww = int.from_bytes(data[i + 5:i + 7], "big")
                            if ww > 0 and hh > 0:                    # B11
                                w, h = ww, hh
                                c.branch(True)
                            else:
                                c.branch(False)
                        else:
                            c.branch(False)
                        break                                        # SOF reached
                    else:
                        c.branch(False)
                    i += segment_len
            else:
                c.branch(False)
        else:
            c.branch(False)

        c.valstr(f"{w}x{h}")
        cups.append(c.finalize())
    return cups
