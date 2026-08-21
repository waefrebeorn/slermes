#!/usr/bin/env python3
"""sta_oracle_plugin_packs_pure.py — oracle twin for plugin_packs_pure.c.

Calls the LIVE hermes_cli/plugin_packs.py pure helpers and emits one JSON line
per op. t_port_plugin_packs_pure.c emits the same shape; run_oracle.sh diffs.

Fixture ops:
  {"op": "is_exact_sha", "ref": "e8d5..."}
  {"op": "entry_label", "item": {"name": "x"}, "index": 0}
  {"op": "validate_config_seed", "plugin_id": "p", "seed": {"k": "v"}}
    (validate_config_seed emits {"error": "..."} on PackError, matching the
     C port's {"error":"..."} contract; otherwise the validated dict.)
"""
import json
import os
import sys
import importlib.util


def _load_module():
    here = os.path.dirname(os.path.abspath(__file__))
    repo = os.path.dirname(os.path.dirname(os.path.dirname(here)))
    cand = os.path.join(repo, "hermes_cli", "plugin_packs.py")
    spec = importlib.util.spec_from_file_location("live_plugin_packs", cand)
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
        if op == "is_exact_sha":
            out.append({"op": op, "out": bool(mod._EXACT_SHA_RE.fullmatch(case.get("ref", "")))})
        elif op == "entry_label":
            item = case.get("item", {})
            out.append({"op": op, "out": mod._entry_label(item, int(case.get("index", 0)))})
        elif op == "validate_config_seed":
            pid = case.get("plugin_id", "")
            try:
                res = mod.validate_config_seed(pid, case.get("seed", {}))
                out.append({"op": op, "out": res})
            except mod.PackError as e:
                out.append({"op": op, "out": {"error": str(e)}})
    for line in out:
        sys.stdout.write(json.dumps(line, sort_keys=True) + "\n")


if __name__ == "__main__":
    main()
