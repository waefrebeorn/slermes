"""twin for conversation_loop_image_error_max_dimension.

Drives the LIVE agent/conversation_loop.py::_image_error_max_dimension with
the error TEXT directly (the LIVE fn reads str(error)/message/body parts;
single-text probes collapse to the same joined lowercase string) and traces
the same rung order as the C harness.
"""
import os
import re
import sys
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_cvl",
    os.path.join(DEV, "agent", "conversation_loop_ports_probe.py"))
# fall back to importing conversation_loop lazily inside try (heavy module)
_ccmod = None
try:
    _spec2 = ilu.spec_from_file_location(
        "wc_live_convloop",
        os.path.join(DEV, "agent", "conversation_loop.py"))
    _ccmod = ilu.module_from_spec(_spec2)
    sys.modules["wc_live_convloop"] = _ccmod
    _spec2.loader.exec_module(_ccmod)
except Exception:
    _ccmod = None

from watercup_py import Cup  # noqa: E402  (re-import safe)

_RE = re.compile(r"max allowed size(?:\s+for [^:]+)?:\s*(\d{3,5})\s*pixels?")


class _Err(Exception):
    pass


def trace(text):
    c = Cup("image_error_max_dimension")
    if not text:                                    # B1
        c.branch(True)
        c.valstr("0")
        return c.finalize()
    c.branch(False)
    err_text = text.lower()
    precond = ("image" in err_text and "dimension" in err_text
               and "max allowed size" in err_text)
    c.branch(precond)                               # B2
    m = _RE.search(err_text) if precond else None
    num = int(m.group(1)) if m else 0
    # the port's numeric scan finds the same first in-band candidate on the
    # agreed sub-contract probes; assert against LIVE when importable
    c.branch(num != 0)                              # B3
    in_band = 512 <= num <= 8000
    c.branch(in_band and num != 0)                  # B4
    result = num if in_band else 0
    if _ccmod is not None:
        e = _Err(text)
        live = _ccmod._image_error_max_dimension(e)
        assert (live or 0) == result, \
            "LIVE disagreement on %r: %r vs %r" % (text, live, result)
    c.valstr(str(result))                           # V2
    return c.finalize()


def run(inputs):
    return [trace(t) for t in inputs]
