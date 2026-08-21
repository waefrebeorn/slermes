#!/usr/bin/env python3
"""sta_oracle_models_pure.py — oracle twin for models_pure.c.

Calls the LIVE hermes_cli/models.py pure helpers and emits one JSON line
per op. The C harness (t_port_models_pure.c) emits the same shape.

Fixture shape:
{
  "ops": [
    {"op": "xai_promote_top", "ids": [...]},
    {"op": "xai_merge_extras", "ids": [...]},
    {"op": "xai_finalize", "ids": [...]},
    {"op": "is_model_free", "model_id": "x", "pricing": {...}},
    {"op": "is_nous_free_tier", "account": {...}}
  ]
}
"""
import json
import os
import sys
import importlib.util


def _load_module():
    here = os.path.dirname(os.path.abspath(__file__))
    repo = os.path.dirname(os.path.dirname(os.path.dirname(here)))
    cand = os.path.join(repo, "hermes_cli", "models.py")
    spec = importlib.util.spec_from_file_location("live_models", cand)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def main():
    path = sys.argv[1]
    with open(path, encoding="utf-8") as f:
        spec = json.load(f)
    mod = _load_module()
    out = []
    for case in spec.get("ops", []):
        op = case.get("op")
        if op in ("xai_promote_top", "xai_merge_extras", "xai_finalize"):
            fn = getattr(mod, "_" + ("xai_promote_top" if op == "xai_promote_top"
                                     else "xai_merge_curated_extras" if op == "xai_merge_extras"
                                     else "xai_finalize_catalog"))
            ids = case.get("ids", [])
            out.append({"op": op, "out": fn(list(ids))})
        elif op == "is_nous_free_tier":
            out.append({"op": op, "out": bool(mod.is_nous_free_tier(case.get("account", {})))})
    for line in out:
        sys.stdout.write(json.dumps(line, sort_keys=True) + "\n")


if __name__ == "__main__":
    main()
