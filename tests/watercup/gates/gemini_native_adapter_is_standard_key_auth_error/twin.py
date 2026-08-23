"""twin for gemini_native_adapter_is_standard_key_auth_error.

Traces the LIVE agent/gemini_native_adapter.py::is_standard_key_auth_error
with the same rung order as the C harness.
Input format: "status|reason|message".
"""
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_gemini_native_adapter",
    os.path.join(DEV, "agent", "gemini_native_adapter.py"))
_gna = ilu.module_from_spec(_spec)
sys.modules["wc_live_gemini_native_adapter"] = _gna
try:
    _spec.loader.exec_module(_gna)
except Exception:
    _gna = None


def trace(inp):
    status_s, _, rest = inp.partition("|")
    reason, _, msg = rest.partition("|")
    try:
        status = int(status_s)
    except ValueError:
        status = 0
    c = Cup("is_standard_key_auth_error")
    if status != 401:                           # B1
        c.branch(True)
        res = False
    else:
        c.branch(False)
        if reason == "ACCESS_TOKEN_TYPE_UNSUPPORTED":   # B2
            c.branch(True)
            res = True
        else:
            c.branch(False)
            res = "expected oauth 2 access token" in (msg or "").lower()  # B3
            c.branch(res)
    live = None
    if _gna is not None:
        try:
            live = bool(_gna.is_standard_key_auth_error(status, msg, reason))
        except Exception:
            live = None
    if live is not None and live != res:
        raise AssertionError(
            "ladder result %r diverges from LIVE %r for %r" % (res, live, inp))
    return res, c.finalize()


def run(inputs):
    cups = []
    for inp in inputs:
        _, cup = trace(inp)
        cups.append(cup)
    return cups
