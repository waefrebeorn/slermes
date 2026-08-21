#!/usr/bin/env python3
"""sta_oracle_skills_sync_manifest.py — oracle twin for
src/tools/skills_sync_client.c pure sync-manifest helpers.

Mirrors t_port_skills_sync_manifest.c: reads the fixture, runs the REAL Python
functions from tools/skills_sync_client.py, and emits one JSON line per op.
"""
import json
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))
# Import the real module under test
import importlib.util


def load_module():
    hermes = os.path.join(os.getcwd())
    spec = importlib.util.spec_from_file_location(
        "skills_sync_client",
        os.path.join(hermes, "tools", "skills_sync_client.py"),
    )
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def main():
    mod = load_module()
    fixture = sys.argv[1]
    with open(fixture) as f:
        root = json.load(f)
    for c in root.get("ops", []):
        op = c.get("op")
        if op == "manifest_roundtrip":
            sk = c.get("skills", {})
            built = mod.build_sync_manifest_bytes(sk)
            parsed = mod.parse_sync_manifest(built)
            addr = mod.wire_address(built)
            print(json.dumps({
                "op": "manifest_roundtrip",
                "built_len": len(built),
                "parsed": parsed,
                "wire_address": addr,
            }))
        elif op == "canonical":
            obj = c.get("obj", {})
            out = mod.canonical_json_bytes(obj).decode("utf-8")
            print(json.dumps({"op": "canonical", "out": json.loads(out)}))


if __name__ == "__main__":
    main()
