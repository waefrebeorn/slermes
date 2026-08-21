#!/usr/bin/env python3
"""sta_oracle_yuanbao_map_entry.py — oracle twin for the map-entry codec
helpers in src/hermes_cli/yuanbao_proto_helpers.c
(gateway/platforms/yuanbao_proto.py: _encode_map_entry, _decode_map_entry).

Mirrors t_port_yuanbao_map_entry.c using the REAL Python functions.
"""
import json
import os
import sys
import importlib.util


def load_module():
    spec = importlib.util.spec_from_file_location(
        "yuanbao_proto",
        os.path.join(os.path.dirname(__file__), "..", "..",
                     "gateway", "platforms", "yuanbao_proto.py"),
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
        if op == "encode":
            k = c.get("key", "")
            v = c.get("value", "")
            b = mod._encode_map_entry(k, v)
            print(json.dumps({"op": "encode", "key": k, "value": v,
                              "hex": b.hex()}))
        elif op == "decode":
            h = c.get("hex", "")
            data = bytes.fromhex(h)
            key, val = mod._decode_map_entry(data)
            print(json.dumps({"op": "decode", "hex": h,
                              "out": [key, val]}))


if __name__ == "__main__":
    main()
