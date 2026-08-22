#!/usr/bin/env python3
"""sta_oracle_model_data_policy_guard.py — Python twin for
src/hermes_cli/model_data_policy_guard.c. Imports the LIVE module (spec-loaded
directly) and mirrors the C ops:
  {"kind":"warn","model":m,"provider":p|null}
"""
import importlib.util
import json
import os
import sys
from pathlib import Path

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


def main():
    fixture = sys.argv[1]
    with open(fixture) as f:
        root = json.load(f)
    for c in root.get("ops", []):
        kind = c.get("kind")
        if kind == "warn":
            w = mdp.data_training_warning(
                c.get("model", ""), provider=c.get("provider"))
            out = (
                {"model": w.model, "provider": w.provider, "message": w.message}
                if w is not None
                else None
            )
        else:
            out = None
        print(json.dumps({"out": out}, ensure_ascii=False))


if __name__ == "__main__":
    main()
