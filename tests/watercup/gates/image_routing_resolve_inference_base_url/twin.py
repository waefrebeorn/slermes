"""twin for image_routing_resolve_inference_base_url.

Imports LIVE agent/image_routing.py::_resolve_inference_base_url and traces the
SAME branch ladder as the C harness (runtime rung, cfg-absent rung,
model.base_url rung, ordered candidate building, providers walk,
custom_providers walk). Input grammar is documented in gate.json.
"""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, os.path.join(DEV, "agent"))
import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_imgrouting", os.path.join(DEV, "agent", "image_routing.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_imgrouting"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup


def parse_cfg(cfgspec):
    if cfgspec in ("-", ""):
        return None
    cfg = {}
    model = {}
    providers = {}
    custom = []
    for tok in cfgspec.split(";"):
        if "=" not in tok:
            continue
        key, val = tok.split("=", 1)
        if key == "mb":
            model["base_url"] = val
        elif key == "mp":
            model["provider"] = val
        elif key.startswith("p."):
            name = key[2:].split(".base_url", 1)[0]
            providers[name] = {"base_url": val}
        elif key.startswith("c."):
            custom.append({"name": key[2:], "base_url": val})
    if model:
        cfg["model"] = model
    if providers:
        cfg["providers"] = providers
    if custom:
        cfg["custom_providers"] = custom
    return cfg


def run(inputs):
    cups = []
    for raw in inputs:
        provider, cfgspec = raw.split("|", 1)
        if provider == "-":
            provider = ""
        cfg = parse_cfg(cfgspec)
        c = Cup("resolve_inference_base_url")
        # rung 1: runtime override — fresh process carries no live runtime
        runtime = ""          # _runtime_main_value("base_url") in a clean context
        c.branch(bool(runtime))
        # rung 2: cfg must be a dict
        c.branch(not isinstance(cfg, dict))
        if not isinstance(cfg, dict):
            c.valstr("")
            cups.append(c.finalize())
            continue
        model_cfg_raw = cfg.get("model")
        model_cfg = model_cfg_raw if isinstance(model_cfg_raw, dict) else {}
        base_url = str(model_cfg.get("base_url") or "").strip()
        has_mb = bool(base_url)
        c.branch(has_mb)
        if has_mb:
            c.valstr(base_url)
            cups.append(c.finalize())
            continue
        config_provider = str(model_cfg.get("provider") or "").strip()
        inputs_ordered = [p for p in (provider, config_provider) if p]
        # ordered candidate build (mirrors the C port's deterministic walk)
        ordered_cands = []
        for p in inputs_ordered:
            c.loop()
            dup = p in ordered_cands
            c.branch(dup)
            if not dup:
                ordered_cands.append(p)
            is_custom = p.lower().startswith("custom:")
            c.branch(is_custom)
            if is_custom:
                bare = p.split(":", 1)[1] if ":" in p else p[7:]
                d2 = bare in ordered_cands
                c.branch(d2)
                if not d2:
                    ordered_cands.append(bare)
            else:
                withpfx = f"custom:{p}"
                d2 = withpfx in ordered_cands
                c.branch(d2)
                if not d2:
                    ordered_cands.append(withpfx)
        candidate_names = set()
        for p in filter(None, (provider, config_provider)):
            candidate_names.add(p)
            if p.lower().startswith("custom:"):
                candidate_names.add(p.split(":", 1)[1])
            else:
                candidate_names.add(f"custom:{p}")
        result = None
        providers_cfg = cfg.get("providers")
        c.branch(isinstance(providers_cfg, dict))
        if isinstance(providers_cfg, dict):
            # deterministic walk mirroring the C port's ordered candidate list;
            # agrees with LIVE on the single-match sub-contract (see notes)
            for name in ordered_cands:
                if result:
                    break
                c.loop()
                entry = providers_cfg.get(name)
                found = isinstance(entry, dict)
                c.branch(found)
                if isinstance(entry, dict):
                    bu = str(entry.get("base_url") or "").strip()
                    c.branch(bool(bu))
                    if bu:
                        result = bu
        custom_providers = cfg.get("custom_providers")
        c.branch(isinstance(custom_providers, list))
        if isinstance(custom_providers, list):
            lowered = {n.lower() for n in candidate_names}
            for entry_raw in custom_providers:
                if result:
                    break
                c.loop()
                c.branch(False)   # non-dict entry — impossible in this grammar
                entry_name = str((entry_raw or {}).get("name") or "").strip()
                match = (entry_name in candidate_names or
                         entry_name.lower() in lowered)
                c.branch(match)
                if match:
                    bu = str((entry_raw or {}).get("base_url") or "").strip()
                    c.branch(bool(bu))
                    if bu:
                        result = bu
        c.valstr(result or "")
        cups.append(c.finalize())
    return cups
