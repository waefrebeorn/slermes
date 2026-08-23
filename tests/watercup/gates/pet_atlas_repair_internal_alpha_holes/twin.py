"""twin for pet_atlas_repair_internal_alpha_holes.

Traces LIVE agent/pet/generate/atlas.py::_repair_internal_alpha_holes with
the same rung ladder as the C harness: B1 image usable, B2 any enclosed hole
filled, V = resulting pixel dump "r.g.b.a|..." row-major.

Inputs use uniform opaque surroundings so the hole fill average is an exact
integer (the C port truncates the mean while LIVE rounds — see gate.json).
"""
import importlib.util as ilu
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

spec = ilu.spec_from_file_location(
    "wc_live_atlas", os.path.join(DEV, "agent", "pet", "generate", "atlas.py"))
_atlas = ilu.module_from_spec(spec)
sys.modules["wc_live_atlas"] = _atlas
spec.loader.exec_module(_atlas)

_fn = _atlas._repair_internal_alpha_holes

from PIL import Image

_ALPHA_FLOOR = 16


def trace(item):
    c = Cup("repair_internal_alpha_holes")
    parts = item.split()
    w, h = int(parts[0]), int(parts[1])
    ok = w > 0 and h > 0 and w * h <= 4096 and len(parts) - 2 >= w * h
    c.branch(ok)                                                  # B1
    if not ok:
        c.branch(False)                                           # B2
        c.valstr("")                                              # V
        return c.finalize()
    px_in = [tuple(int(x) for x in p.split(",")) for p in parts[2:2 + w * h]]
    img = Image.new("RGBA", (w, h))
    img.putdata(px_in)
    out = _fn(img)
    filled_any = any(
        px_in[i][3] <= _ALPHA_FLOOR and out.getpixel((i % w, i // w))[3] > _ALPHA_FLOOR
        for i in range(w * h))
    c.branch(filled_any)                                          # B2
    dump = "|".join("%d.%d.%d.%d" % out.getpixel((i % w, i // w))
                    for i in range(w * h)) + "|"
    c.valstr(dump)                                                # V
    return c.finalize()


def run(inputs):
    return [trace(m) for m in inputs]
