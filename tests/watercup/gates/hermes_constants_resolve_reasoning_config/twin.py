"""twin for hermes_constants_resolve_reasoning_config.

Imports LIVE hermes_constants.py and traces the SAME branch ladder rung-order
as the C harness (traced copy of reasoning_resolve_config), using LIVE
resolve_per_model_reasoning_effort / parse_reasoning_effort.
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from watercup_py import Cup

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_hc", os.path.join(DEV, "hermes_constants.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_hc"] = mod
spec.loader.exec_module(mod)


def enc_result(r):
    if r is None:
        return "<none>"
    if r.get("enabled") is False:
        return "disabled"
    if "effort" in r:
        return "enabled:%s" % r["effort"]
    return "<none>"


def derive_model(cfg):
    model_cfg = cfg.get("model")
    if isinstance(model_cfg, str):
        return model_cfg.strip()
    if isinstance(model_cfg, dict):
        return str(model_cfg.get("default") or model_cfg.get("model") or "").strip()
    return ""


def run(inputs):
    cups = []
    for raw in inputs:
        model_arg, _, cfg_json = raw.partition("|")
        import json as _json
        try:
            cfg = _json.loads(cfg_json) if cfg_json else None
        except Exception:
            cfg = None
        c = Cup("reasoning_resolve_config")
        use_cfg = cfg if isinstance(cfg, dict) else {}
        c.branch(isinstance(cfg, dict))                          # B0
        raw_agent = cfg.get("agent") if isinstance(cfg, dict) else None
        c.branch(isinstance(raw_agent, dict))                    # B1
        agent_cfg = raw_agent if isinstance(raw_agent, dict) else {}

        eff_model = model_arg
        derived = False
        if (not model_arg) and isinstance(cfg, dict):
            eff_model = derive_model(cfg)
            derived = True
        c.branch(derived)                                        # B2
        c.valstr(eff_model or "")

        overrides = agent_cfg.get("reasoning_overrides") or {}
        c.branch(isinstance(overrides, dict) and bool(overrides))  # B3
        result = None
        if isinstance(overrides, dict) and bool(overrides):
            # LIVE resolve_per_model_reasoning_effort guards on truthy overrides;
            # the C port guards on JSON_OBJECT — same verdict on these inputs.
            per = mod.resolve_per_model_reasoning_effort(eff_model, overrides)
            c.branch(per is not None)                            # B4
            if per is not None:
                result = per
        if result is None:
            effort = agent_cfg.get("reasoning_effort", "")
            if effort == "" or effort is None:
                result = mod.parse_reasoning_effort(None if effort is None else "")
            elif isinstance(effort, bool):
                result = mod.parse_reasoning_effort(False) if effort is False else None
            elif isinstance(effort, str):
                result = mod.parse_reasoning_effort(effort)
            else:
                result = None
        c.valstr(enc_result(result))
        cups.append(c.finalize())
    return cups
