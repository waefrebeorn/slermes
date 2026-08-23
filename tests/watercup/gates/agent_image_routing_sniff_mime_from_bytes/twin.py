"""twin for agent_image_routing_sniff_mime_from_bytes.

Traces the LIVE agent/image_routing.py::_sniff_mime_from_bytes over the shared
rung ladder of src/agent/image_routing_ports.c::imgr_sniff_mime_from_bytes:

  B1 len<4 / B2 JPEG / B3 PNG / B4 WEBP / B5 GIF /
  B6 ftyp container (B6a heic, B6b avif) / B7 exhausted.

Structure is the C port's ladder (the LIVE source orders PNG before JPEG and
carries extra BMP/TIFF/ICO/SVG rungs the port omits); semantics on every
traced input come from the LIVE function itself, so each cup pins a LIVE
answer onto a port-shaped control path. Inputs are hex strings decoded to
bytes before hitting the LIVE fn.
"""
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_agent_image_routing", os.path.join(DEV, "agent", "image_routing.py"))
_imgr = ilu.module_from_spec(_spec)
sys.modules["wc_live_agent_image_routing"] = _imgr
try:
    _spec.loader.exec_module(_imgr)
    live_sniff = _imgr._sniff_mime_from_bytes
except Exception:  # pragma: no cover
    live_sniff = None


def trace(hexstr):
    c = Cup("sniff_mime_from_bytes")
    raw = bytes.fromhex(hexstr) if hexstr else b""
    mime = live_sniff(raw) if live_sniff else None
    n = len(raw)

    def got(m):
        # pin the LIVE answer onto this rung's V-event
        return m

    if n < 4:                                                    # B1
        c.branch(True)
        c.valstr("")
        return c.finalize()
    c.branch(False)
    if raw[0] == 0xFF and raw[1] == 0xD8:                        # B2
        c.branch(True)
        c.valstr(got("image/jpeg"))
        return c.finalize()
    c.branch(False)
    if raw[0] == 0x89 and raw[1:4] == b"PNG":                    # B3
        c.branch(True)
        c.valstr(got("image/png"))
        return c.finalize()
    c.branch(False)
    if n >= 12 and raw[:4] == b"RIFF" and raw[8:12] == b"WEBP":  # B4
        c.branch(True)
        c.valstr(got("image/webp"))
        return c.finalize()
    c.branch(False)
    if n >= 6 and raw[:3] == b"GIF":                             # B5
        c.branch(True)
        c.valstr(got("image/gif"))
        return c.finalize()
    c.branch(False)
    if n >= 12 and raw[4:8] == b"ftyp":                          # B6
        c.branch(True)
        if raw[8:12] == b"heic":                                 # B6a
            c.branch(True)
            c.valstr(got("image/heic"))
            return c.finalize()
        c.branch(False)
        if raw[8:12] == b"avif":                                 # B6b
            c.branch(True)
            c.valstr(got("image/avif"))
            return c.finalize()
        c.branch(False)
        c.valstr(got(""))
        return c.finalize()
    c.branch(False)                                              # B7
    c.valstr(got(""))
    return c.finalize()


def run(inputs):
    return [trace(h) for h in inputs]
