#!/usr/bin/env python3
"""Differential oracle for tools/image_generation_catalog.py (gate imgcatalog)."""
import importlib.util, json, os, sys
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, Q)

def _bootstrap(parts):
    for i in range(1, len(parts)):
        pname = ".".join(parts[:i])
        if pname in sys.modules: continue
        pdir = os.path.join(Q, *parts[:i]); init = os.path.join(pdir, "__init__.py")
        sp = importlib.util.spec_from_file_location(pname, init, submodule_search_locations=[pdir])
        pm = importlib.util.module_from_spec(sp); sys.modules[pname] = pm
        try: sp.loader.exec_module(pm)
        except Exception: pass

def _load_live():
    parts = "tools/image_generation_catalog".split("/")
    _bootstrap(parts)
    name = ".".join(parts)
    sp = importlib.util.spec_from_file_location(name, os.path.join(Q, "tools/image_generation_catalog.py"))
    live = importlib.util.module_from_spec(sp); sys.modules[name] = live
    sp.loader.exec_module(live)
    return live

live = _load_live()

def compat(v):
    if isinstance(v, (set, frozenset)): return sorted(v)
    if isinstance(v, (list, tuple)): return [compat(x) for x in v]
    if isinstance(v, dict): return {str(k): compat(x) for k, x in v.items()}
    if isinstance(v, int) and abs(v) > 9007199254740992: return str(v)
    return v

def compute(r):
    f = r.get("func")
    if f == "default_model": return live.DEFAULT_MODEL
    if f == "default_aspect_ratio": return live.DEFAULT_ASPECT_RATIO
    if f == "valid_aspect_ratios": return list(live.VALID_ASPECT_RATIOS)
    if f == "upscaler":
        return {
            "UPSCALER_MODEL": live.UPSCALER_MODEL,
            "UPSCALER_FACTOR": live.UPSCALER_FACTOR,
            "UPSCALER_SAFETY_CHECKER": live.UPSCALER_SAFETY_CHECKER,
            "UPSCALER_DEFAULT_PROMPT": live.UPSCALER_DEFAULT_PROMPT,
            "UPSCALER_NEGATIVE_PROMPT": live.UPSCALER_NEGATIVE_PROMPT,
            "UPSCALER_CREATIVITY": str(live.UPSCALER_CREATIVITY),
            "UPSCALER_RESEMBLANCE": str(live.UPSCALER_RESEMBLANCE),
            "UPSCALER_GUIDANCE_SCALE": live.UPSCALER_GUIDANCE_SCALE,
            "UPSCALER_NUM_INFERENCE_STEPS": live.UPSCALER_NUM_INFERENCE_STEPS,
        }
    if f == "model_field":
        entry = live.FAL_MODELS.get(r.get("key", "")) or {}
        return compat(entry.get(r.get("field", "")))
    if f == "model_keys": return list(live.FAL_MODELS.keys())
    if f == "build_entry":
        p = r.get("params", {})
        style = p.get("style", "image_size_preset")
        def opt(k): return p.get(k) if p.get(k) is not None else None
        es = p.get("edit_supports")
        mri = p.get("max_reference_images")
        res = live._model(
            p.get("display", ""), p.get("speed", ""), p.get("strengths", ""), p.get("price", ""),
            style=style, sizes=opt("sizes"), defaults=p.get("defaults", {}),
            supports=set(p.get("supports", [])),
            edit_endpoint=opt("edit_endpoint"),
            edit_supports=set(es) if es is not None else None,
            max_reference_images=int(mri) if mri is not None else None,
            edit_image_param=opt("edit_image_param"),
        )
        return compat(res)
    return None

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try: r = json.loads(line)
    except Exception: continue
    if r.get("func") is None: continue
    cases += 1
    py, got = compute(r), r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH imgcatalog %s py=%r c=%r" % (r.get("func"), py, got), file=sys.stderr)
print("ORACLE imgcatalog: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
