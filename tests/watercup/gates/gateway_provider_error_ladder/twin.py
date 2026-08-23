"""twin for gateway_provider_error_ladder.

Traces the LIVE gateway/run.py::_looks_like_gateway_provider_error +
_gateway_provider_error_reply + _is/_strip_auto_continue_noise with the SAME
rung order as the C harness:
  B1 empty / B2 >400 / B3 >4 newlines / B4 shape-regex /
  B5 auth / B6 policy / B7 rate / B8 connection /
  N1 is_noise / N2 strip-loop iteration.
"""
import os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

import importlib.util as ilu

spec = ilu.spec_from_file_location("wc_live_gateway_run",
                                   os.path.join(DEV, "gateway", "run.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_live_gateway_run"] = mod
spec.loader.exec_module(mod)

_looks = mod._looks_like_gateway_provider_error
_reply = mod._gateway_provider_error_reply
_is_noise = mod._is_auto_continue_noise
_strip_noise = mod._strip_auto_continue_noise


def _reply_class(text):
    """The LIVE reply ladder's rung order, evaluated rung by rung."""
    if mod._GATEWAY_AUTH_ERROR_RE.search(text):
        return "auth"
    if mod._GATEWAY_PROVIDER_POLICY_RE.search(text):
        return "policy"
    if mod._GATEWAY_RATE_LIMIT_RE.search(text):
        return "rate"
    if mod._GATEWAY_CONNECTION_ERROR_RE.search(text):
        return "connection"
    return "generic"


_REPLY_LABELS = {"auth": "Provider authentication failed.",
                 "policy": "rejected the request",
                 "rate": "rate-limiting",
                 "connection": "not responding",
                 "generic": "failed after retries"}


def _trace(mode, text):
    c = Cup("gateway_provider_error_ladder")

    if mode == "noise":
        if _is_noise(text):
            c.branch(True)
            original = text
            while _is_noise(text):
                c.branch(True)
                end = text.find("]")
                if end < 0:
                    text = ""
                    break
                text = text[end + 1:].lstrip()
            c.branch(False)
            assert text == _strip_noise(original)
            c.valstr("stripped" if text else "empty")
        else:
            c.branch(False)
            c.valstr("passthrough")
        return c.finalize()

    # looks_like ladder
    if not text:
        c.branch(True)
        shaped = False
    else:
        c.branch(False)
        body = str(text).strip()
        if len(body) > 400:
            c.branch(True)
            shaped = False
        else:
            c.branch(False)
            if body.count("\n") > 4:
                c.branch(True)
                shaped = False
            else:
                c.branch(False)
                shaped = _looks(text)
                c.branch(True if shaped else False)

    if not shaped:
        c.valstr("not-error")
    elif mode == "shape":
        c.valstr("error-shape")
    else:
        cls = _reply_class(text)          # trace each rung in order
        if cls == "auth":
            c.branch(True)
        else:
            c.branch(False)
            if cls == "policy":
                c.branch(True)
            else:
                c.branch(False)
                if cls == "rate":
                    c.branch(True)
                else:
                    c.branch(False)
                    if cls == "connection":
                        c.branch(True)
                    else:
                        c.branch(False)
        c.valstr(cls)
        assert _REPLY_LABELS[cls] in _reply(text), "twin drift"

    return c.finalize()


def run(inputs):
    cups = []
    for probe in inputs:
        mode, _, text = probe.partition("|")
        _orig = [text]
        cups.append(_trace(mode, text))
    return cups
