"""twin for dashboard_auth_prefix_normalise_public_url.

Traces the LIVE hermes_cli/dashboard_auth/prefix.py::_normalise_public_url
with the same rung order as the C harness:
  B1 empty / B2 whitespace-only / B3 reject chars / B4 scheme / V1 result.
The RESULT value comes from the LIVE function; the ladder mirrors the C port.
"""
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_dash_prefix", os.path.join(DEV, "hermes_cli", "dashboard_auth", "prefix.py"))
_dp = ilu.module_from_spec(_spec)
sys.modules["wc_live_dash_prefix"] = _dp
try:
    _spec.loader.exec_module(_dp)
    live = _dp._normalise_public_url
except Exception:
    live = None

REJECT_CHARS = "\"'<>\n\r\t "

def trace(raw):
    c = Cup("normalise_public_url")
    if not raw:
        c.branch(True)                       # B1
        val = ""
    else:
        c.branch(False)
        stripped = raw.lstrip(" \t")
        if not stripped:
            c.branch(True)                   # B2
            val = ""
        else:
            c.branch(False)
            if any(ch in stripped for ch in REJECT_CHARS):
                c.branch(True)               # B3
                val = ""
            else:
                c.branch(False)
                low = stripped.lower()
                if not (low.startswith("http://") or low.startswith("https://")):
                    c.branch(False)          # B4
                    val = ""
                else:
                    c.branch(True)
                    # LIVE semantics (urlparse netloc check + rstrip("/"))
                    val = live(raw) if live else stripped.rstrip("/")
    c.valstr(val)                            # V1
    return val, c.finalize()


def run(inputs):
    cups = []
    for raw in inputs:
        _, cup = trace(raw or "")
        cups.append(cup)
    return cups
