"""twin for runtime_footer_resolve_footer_config.

Imports LIVE gateway/runtime_footer.py::resolve_footer_config and traces the
SAME ladder as the C harness. Input encoding: "<config_json>|<platform_key>".
"""
import importlib.util as ilu
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

spec = ilu.spec_from_file_location(
    "wc_rf", os.path.join(DEV, "gateway", "runtime_footer.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_rf"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup


def run(inputs):
    cups = []
    for raw in inputs:
        cfg_s, _, key_s = raw.partition("|")
        user_config = json.loads(cfg_s) if cfg_s else None
        platform_key = key_s or None
        c = Cup("resolve_footer_config")
        cfg = (user_config or {}).get("display") or {}
        has_display = bool(user_config) and isinstance(
            (user_config or {}).get("display"), dict)
        c.branch(has_display)
        if isinstance(cfg, dict) and has_display:
            global_cfg = cfg.get("runtime_footer")
            is_obj = isinstance(global_cfg, dict)
            c.branch(is_obj)
            if is_obj:
                c.branch("enabled" in global_cfg)
                fields = global_cfg.get("fields")
                c.branch(isinstance(fields, list) and bool(fields))
            if platform_key:
                platforms = cfg.get("platforms") or {}
                plat_cfg = platforms.get(platform_key)
                c.branch(isinstance(plat_cfg, dict))
                plat_footer = plat_cfg.get("runtime_footer") \
                    if isinstance(plat_cfg, dict) else None
                c.branch(isinstance(plat_footer, dict))
                if isinstance(plat_footer, dict):
                    c.branch("enabled" in plat_footer)
                    pfields = plat_footer.get("fields")
                    c.branch(isinstance(pfields, list) and bool(pfields))
        resolved = mod.resolve_footer_config(user_config, platform_key)
        c.valstr("enabled=%s|fields=%s" % (
            "True" if resolved["enabled"] else "False",
            ",".join(resolved["fields"])))
        cups.append(c.finalize())
    return cups
