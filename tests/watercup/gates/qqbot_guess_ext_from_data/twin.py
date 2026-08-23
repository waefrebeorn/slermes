"""twin for qqbot_guess_ext_from_data.

Imports LIVE gateway/platforms/qqbot/adapter.py::QQAdapter._guess_ext_from_data
and traces the same rung ladder as the C harness: every ||-disjunct split into
one simple predicate per rung, walked until the first hit. The V event is the
LIVE function's real extension.
"""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_qqbot", os.path.join(DEV, "gateway", "platforms", "qqbot", "adapter.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_qqbot"] = mod
spec.loader.exec_module(mod)
fn = mod.QQAdapter._guess_ext_from_data

from watercup_py import Cup

def run(inputs):
    cups = []
    for hx in inputs:
        data = bytes.fromhex(hx)
        ext = fn(data)
        c = Cup("guess_ext_from_data")
        n = len(data)
        state = {"m": False}
        def rung(cond):
            if not state["m"]:
                c.branch(bool(cond))
                if cond:
                    state["m"] = True
        rung(n >= 9 and data[:9] == b"#!SILK_V3")
        rung(n >= 6 and data[:6] == b"#!SILK")
        rung(n >= 2 and data[0] == 0x02 and data[1] == ord("!"))
        rung(n >= 4 and data[:4] == b"RIFF")
        rung(n >= 4 and data[:4] == b"fLaC")
        rung(n >= 2 and data[0] == 0xff and data[1] in (0xfb, 0xf3, 0xf2))
        rung(n >= 4 and data[:4] == b"\x30\x26\xb2\x75")
        rung(n >= 4 and data[:4] == b"OggS")
        rung(n >= 4 and data[:4] == b"\x00\x00\x00\x20")
        rung(n >= 4 and data[:4] == b"\x00\x00\x00\x1c")
        c.valstr(ext)
        cups.append(c.finalize())
    return cups
