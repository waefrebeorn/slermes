"""twin for gateway_display_resolve_display_setting.

Traces the LIVE gateway/display_config.py::resolve_display_setting with the
SAME rung ladder as the C harness, then takes the resolved value from a direct
call to the LIVE function.

Input encoding: "<platform>|<setting>|<fallback|->|<config_json>".
"""
import os
import sys
import json as _json
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_display_config",
    os.path.join(DEV, "gateway", "display_config.py"))
_dc = ilu.module_from_spec(_spec)
sys.modules["wc_live_display_config"] = _dc
if _spec.loader is not None:
    _spec.loader.exec_module(_dc)


def trace(user_config, platform_key, setting):
    c = Cup("resolve_display_setting")
    display = user_config.get("display") if isinstance(user_config, dict) else None
    ok1 = isinstance(display, dict)
    c.branch(ok1)                                                  # B1
    if not ok1:
        return c  # caller fills V from LIVE result

    pk = bool(platform_key)
    c.branch(pk)                                                   # B2
    if pk:
        platforms = display.get("platforms")
        ok3 = isinstance(platforms, dict)
        c.branch(ok3)                                              # B3
        if ok3:
            plat = platforms.get(platform_key)
            ok4 = isinstance(plat, dict)
            c.branch(ok4)                                          # B4
            if ok4:
                hit5 = plat.get(setting) is not None
                c.branch(hit5)                                     # B5
                if hit5:
                    return c  # caller fills V from LIVE result
    g = display.get(setting)
    c.branch(g is not None)                                        # B6
    return c


def run(inputs):
    cups = []
    for raw in inputs:
        plat_s, setting, fb_s, cfg_s = raw.split("|", 3)
        platform = "" if plat_s == "-" else plat_s
        fallback = None if fb_s == "-" else fb_s
        cfg = _json.loads(cfg_s)
        c = trace(cfg, platform, setting)
        result = _dc.resolve_display_setting(cfg, platform, setting, fallback)
        # encode exactly like the C string layer
        if result is None:
            enc = "null"
        elif isinstance(result, bool):
            enc = "true" if result else "false"
        elif isinstance(result, float):
            enc = "%.0f" % result
        else:
            enc = str(result)
        c.valstr(enc)
        cups.append(c.finalize())
    return cups
