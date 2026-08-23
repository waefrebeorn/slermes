"""twin for gateway_health_classify_gateway_error.

Imports LIVE agent/monitoring/gateway_health.py::classify_gateway_error and
traces the SAME branch ladder as the C harness (keyword categories in the same
rung order), plus V of the LIVE return value.
"""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
DEV = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE)))))
sys.path.insert(0, DEV)

import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_gw_health", os.path.join(DEV, "agent", "monitoring", "gateway_health.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_gw_health"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup

AUTH = ("auth", "token", "unauthorized", "forbidden", "401", "403")
NET = ("network", "connection", "dns", "socket", "connect call failed",
       "failed to connect", "cannot connect", "unreachable", "name resolution")
CFG = ("config", "missing", "invalid")

def run(inputs):
    cups = []
    for raw in inputs:
        c = Cup("classify_gateway_error")
        s = str(raw or "").lower()
        if any(k in s for k in AUTH):
            c.branch(True); r = "auth_failed"
        else:
            c.branch(False)
            if "rate" in s and "limit" in s:
                c.branch(True); r = "rate_limited"
            else:
                c.branch(False)
                if "timeout" in s or "timed out" in s:
                    c.branch(True); r = "timeout"
                else:
                    c.branch(False)
                    if any(k in s for k in NET):
                        c.branch(True); r = "network_error"
                    else:
                        c.branch(False)
                        if any(k in s for k in CFG):
                            c.branch(True); r = "invalid_config"
                        else:
                            c.branch(False)
                            if "startup" in s:
                                c.branch(True); r = "startup_failed"
                            else:
                                c.branch(False)
                                if "fatal" in s:
                                    c.branch(True); r = "platform_fatal"
                                else:
                                    c.branch(False); r = "unknown"
        live = mod.classify_gateway_error(raw)
        # V carries the LIVE classification; branch ladder is traced above.
        c.valstr(live if isinstance(live, str) else str(live))
        cups.append(c.finalize())
    return cups
