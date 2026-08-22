#!/usr/bin/env python3
"""sta_oracle_model_selection_guards.py — Python twin for
src/hermes_cli/model_selection_guards.c. Imports the LIVE modules
(spec-loaded directly; the package __init__ drags the whole platform stack)
and mirrors the C ops. The network pricing fallbacks (models.dev lookup,
usage_pricing route) are patched to raise, matching the C harness's NULL
seams — both sides then run the pure model_info-trusted path only.
"""
import importlib.util
import json
import os
import sys
from pathlib import Path
from types import SimpleNamespace

PY_ROOT = str(Path.home() / ".hermes" / "hermes-agent")
if not os.path.isdir(PY_ROOT):
    PY_ROOT = "/home/wubu/hermes-agent-dev"
sys.path.insert(0, PY_ROOT)


def _load(name, relpath):
    if name in sys.modules:
        return sys.modules[name]
    spec = importlib.util.spec_from_file_location(
        name, os.path.join(PY_ROOT, relpath))
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod


mdp = _load("hermes_cli.model_data_policy_guard",
            "hermes_cli/model_data_policy_guard.py")
mcg = _load("hermes_cli.model_cost_guard", "hermes_cli/model_cost_guard.py")
msg = _load("hermes_cli.model_selection_guards",
            "hermes_cli/model_selection_guards.py")

# Kill the network-coupled fallbacks exactly like the C NULL seams do.
import agent.models_dev as _models_dev  # noqa: E402
import agent.usage_pricing as _usage_pricing  # noqa: E402


def _raise(*a, **k):
    raise RuntimeError("network fallback disabled in oracle")


_models_dev.get_model_info = _raise
_usage_pricing.resolve_billing_route = _raise
_usage_pricing.get_pricing_entry = _raise


def _model_info_from(d):
    if d is None:
        return None
    return SimpleNamespace(
        cost_input=d.get("cost_input"),
        cost_output=d.get("cost_output"),
        provider_id=d.get("provider_id"),
        has_cost_data=lambda: (
            d.get("cost_input") is not None or d.get("cost_output") is not None
        ),
    )


def _warn_dict(w):
    return {"kind": w.kind, "title": w.title, "model": w.model,
            "provider": w.provider, "message": w.message}


def main():
    fixture = sys.argv[1]
    with open(fixture) as f:
        root = json.load(f)
    for c in root.get("ops", []):
        kind = c.get("kind")
        if kind in ("warnings", "combined"):
            kwargs = dict(
                provider=c.get("provider"),
                base_url=c.get("base_url"),
                api_key=c.get("api_key"),
                model_info=_model_info_from(c.get("model_info")),
            )
            if kind == "warnings":
                include = c.get("include_kinds")
                if include is not None:
                    kwargs["include_kinds"] = include
                out = [_warn_dict(w) for w in
                       msg.selection_warnings(c.get("model"), **kwargs)]
            else:
                w = msg.combined_selection_warning(c.get("model"), **kwargs)
                out = _warn_dict(w) if w is not None else None
        elif kind == "combined_message":
            warnings = [
                SimpleNamespace(**w) for w in c.get("warnings", [])
            ]
            out = msg.combined_message(warnings)
        else:
            out = None
        print(json.dumps({"out": out}, ensure_ascii=False))


if __name__ == "__main__":
    main()
