"""twin for gateway_browser_control_broker_browser_control_enabled.

Traces the LIVE gateway/browser_control_broker.py::browser_control_enabled
config-dict path with the same rung order as the C harness:
  B1 config is dict / B2 browser dict / B3 extension_control dict /
  B4 enabled is exactly True.
Inputs are parsed from JSON; the result value comes from the LIVE fn.
"""
import json as _json
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_bcb", os.path.join(DEV, "gateway", "browser_control_broker.py"))
_bcb = ilu.module_from_spec(_spec)
sys.modules["wc_live_bcb"] = _bcb
_spec.loader.exec_module(_bcb)

_live_fn = _bcb.browser_control_enabled


def trace(cfg):
    c = Cup("browser_control_enabled")
    if not isinstance(cfg, dict):                     # B1
        c.branch(False)
        result = False
    else:
        c.branch(True)
        browser = cfg.get("browser")
        if not isinstance(browser, dict):             # B2
            c.branch(False)
            result = False
        else:
            c.branch(True)
            ext = browser.get("extension_control")
            if not isinstance(ext, dict):             # B3
                c.branch(False)
                result = False
            else:
                c.branch(True)
                result = ext.get("enabled", False) is True  # B4
                c.branch(result)
    live = _live_fn(cfg)
    assert live == result, (cfg, live, result)
    c.valstr("1" if result else "0")
    return c.finalize()


def run(inputs):
    cups = []
    for item in inputs:
        cups.append(trace(_json.loads(item)))
    return cups
