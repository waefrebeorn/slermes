"""twin for base_extract_media.

Imports LIVE gateway/platforms/base.py::extract_media and traces the SAME
rung order as the C harness over its media-list result:
  B1 empty content / LOOP matches with B2 path-nonempty, V path,
  B3 per-entry is_voice.
LIVE semantics (masking, dedupe, audio-gated is_voice) come from the LIVE
function itself; the ladder structure is shared with the C port.
"""
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

spec = ilu.spec_from_file_location(
    "wc_pb_base_extract_media", os.path.join(DEV, "gateway", "platforms", "base.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_pb_base_extract_media"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup

FN = "extract_media"


def trace(content):
    c = Cup(FN)
    if not content:
        c.branch(True)                       # B1
        return c.finalize()
    c.branch(False)
    media, _cleaned = mod.BasePlatformAdapter.extract_media(content)
    for path, is_voice in media:             # LOOP over matches
        c.loop()
        c.branch(bool(path))                 # B2
        c.valstr(path)
        c.branch(bool(is_voice))             # B3
    return c.finalize()


def run(inputs):
    return [trace(k) for k in inputs]
