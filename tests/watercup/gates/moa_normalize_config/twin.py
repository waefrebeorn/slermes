"""twin for moa_normalize_config.

Imports LIVE hermes_cli/moa_config.py::normalize_moa_config and traces the
SAME rung ladder as the C harness. The V event is a canonical summary derived
from the LIVE result dict, matching the harness's canon_config() over the
port's JSON output.
"""
import json as _json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, os.path.join(DEV, "hermes_cli"))

import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_live_moa_config",
                                   os.path.join(DEV, "hermes_cli", "moa_config.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_live_moa_config"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup


def _g(v):
    return "%.17g" % float(v)


def canon_preset(p):
    refs = "|".join("%s/%s" % (s.get("provider", ""), s.get("model", ""))
                    for s in p.get("reference_models", []))
    agg = p.get("aggregator") or {}
    mt = p.get("max_tokens")
    mtv = _g(mt) if isinstance(mt, (int, float)) else _g(-1)
    return "enabled=%s,refs=[%s],agg=%s/%s,mt=%s" % (
        p.get("enabled"), refs, agg.get("provider", ""), agg.get("model", ""), mtv)


def run(inputs):
    cups = []
    fn = mod.normalize_moa_config
    for raw_s in inputs:
        try:
            raw = _json.loads(raw_s)
        except Exception:
            raw = None

        c = Cup("normalize_moa_config")
        b_object = isinstance(raw, dict)
        c.branch(b_object)                                   # B0

        presets_raw = raw.get("presets") if b_object else None
        b_presets_obj = isinstance(presets_raw, dict)
        c.branch(b_presets_obj)                              # B1

        kept = []
        if b_presets_obj:
            for name in presets_raw.keys():
                c.loop()
                cn = str(name or "").strip()
                c.branch(bool(cn))                           # B2
                if cn:
                    kept.append(cn)
        legacy = not kept
        c.branch(legacy)                                     # B3

        dn = str((raw.get("default_preset") or "") if b_object else "").strip()
        dn_found = (dn == "default") if legacy else (dn in kept)
        c.branch(dn == "" or not dn_found)                   # B4
        c.branch(False)                                      # B5
        an = str((raw.get("active_preset") or "") if b_object else "").strip()
        an_found = (an == "default") if legacy else (an in kept)
        c.branch(not (an != "" and an_found))                # B6

        res = fn(raw)
        names = sorted(res["presets"].keys())
        parts = ["%s:%s" % (n, canon_preset(res["presets"][n])) for n in names]
        canon = "default=%s|active=%s|presets=%s" % (
            res["default_preset"], res["active_preset"], ";".join(parts))
        c.valstr(canon)
        cups.append(c.finalize())
    return cups
