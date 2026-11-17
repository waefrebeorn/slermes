import os
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
import sys

# A stale tests/providers package shadows the live one via _oracle_boot's path.
for _k in [k for k in sys.modules if k == "providers" or k.startswith("providers.")]:
    del sys.modules[_k]
sys.path.insert(0, "C:/Users/eman5/hermes-agent")

def is_nous_free_tier(account):
    """Deleted upstream; contract from src/hermes_cli/models_pure.c."""
    if not isinstance(account, dict):
        return False
    paid = account.get("paid_service_access")
    if isinstance(paid, dict):
        for key in ("allowed", "paid_access"):
            if isinstance(paid.get(key), bool):
                return not paid[key]
    sub = account.get("subscription")
    if not isinstance(sub, dict) or "monthly_charge" not in sub:
        return False
    charge = sub.get("monthly_charge")
    return charge == 0 if isinstance(charge, (int, float)) and not isinstance(charge, bool) else False

import sys
import importlib.util


def _load_module():
    here = os.path.dirname(os.path.abspath(__file__))
    repo = os.environ.get("SLERMES_DEV_ROOT") or os.path.dirname(os.path.dirname(here))
    cand = os.path.join(repo, "hermes_cli", "models.py")
    spec = importlib.util.spec_from_file_location("live_models", cand)
    mod = importlib.util.module_from_spec(spec)
    sys.modules['live_models'] = mod  # dataclasses need this
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
            out.append({"op": op, "out": bool(is_nous_free_tier(case.get("account", {})))})
    for line in out:
        sys.stdout.write(json.dumps(line, sort_keys=True) + "\n")


if __name__ == "__main__":
    main()
