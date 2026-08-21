#!/usr/bin/env python3
"""sta_oracle_plugin_capabilities_pure.py — oracle twin for plugin_capabilities_pure.c.

Calls the LIVE hermes_cli/plugin_capabilities.py pure helpers and emits one JSON
line per op. t_port_plugin_capabilities_pure.c emits the same shape.

Fixture ops:
  {"op": "is_valid_capability", "id": "tools.override"}
  {"op": "parse_declared_capabilities", "raw": ["tools.override","bogus","llm.model_override"], "name": "p"}
  {"op": "capability_set_hash", "caps": ["tools.override","llm.model_override"]}
  {"op": "granted_capabilities", "plugin_id": "p", "config": {...}}
  {"op": "legacy_gate_set", "entry": {"allow_tool_override": true}, "capability": "tools.override"}
  {"op": "plugin_capability_granted", "plugin_id": "p", "capability": "tools.override", "config": {...}}
"""
import json
import os
import sys
import importlib.util


def _load_module():
    here = os.path.dirname(os.path.abspath(__file__))
    repo = os.path.dirname(os.path.dirname(os.path.dirname(here)))
    cand = os.path.join(repo, "hermes_cli", "plugin_capabilities.py")
    spec = importlib.util.spec_from_file_location("live_pc", cand)
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
        if op == "is_valid_capability":
            out.append({"op": op, "out": bool(mod.VALID_CAPABILITY_IDS and case.get("id") in mod.VALID_CAPABILITY_IDS)})
        elif op == "parse_declared_capabilities":
            out.append({"op": op, "out": mod.parse_declared_capabilities(case.get("raw", []), case.get("name", "?"))})
        elif op == "capability_set_hash":
            out.append({"op": op, "out": mod.capability_set_hash(case.get("caps", []))})
        elif op == "granted_capabilities":
            out.append({"op": op, "out": list(mod.granted_capabilities(case.get("plugin_id", ""), case.get("config")))})
        elif op == "legacy_gate_set":
            out.append({"op": op, "out": bool(mod._legacy_gate_set(case.get("entry", {}), case.get("capability", "")))})
        elif op == "plugin_capability_granted":
            cfg = case.get("config")
            # Python's plugin_capability_granted passes config as a Mapping; we pass dict
            out.append({"op": op, "out": bool(mod.plugin_capability_granted(case.get("plugin_id", ""), case.get("capability", ""), cfg))})
    for line in out:
        sys.stdout.write(json.dumps(line, sort_keys=True) + "\n")


if __name__ == "__main__":
    main()
