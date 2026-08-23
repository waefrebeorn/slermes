"""twin for gateway_run_adapter_credential_fingerprint.

Traces LIVE gateway/run.py::GatewayRunner._adapter_credential_fingerprint with
the same rung order as the C harness:
  B1 credential discovered / V fingerprint hex[:16].
Adapter is duck-typed (the LIVE fn only uses getattr).
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

import importlib.util as ilu

spec = ilu.spec_from_file_location("wc_live_gateway_run",
                                   os.path.join(DEV, "gateway", "run.py"))
_gw = ilu.module_from_spec(spec)
sys.modules["wc_live_gateway_run"] = _gw
spec.loader.exec_module(_gw)

_fp = _gw.GatewayRunner._adapter_credential_fingerprint


class _Adapter:
    def __init__(self, token):
        if token is not None:
            self.token = token


def trace(token):
    c = Cup("adapter_credential_fingerprint")
    fp = _fp(_Adapter(token))
    c.branch(fp is not None)       # B1
    c.valstr(fp or "")             # V
    return c.finalize()


def run(inputs):
    return [trace(None if p == "null" else p) for p in inputs]
