#!/usr/bin/env python3
"""
sta_oracle_billing_links.py — Python oracle for agent/billing_links.py,
ported in src/agent/port_billing_links.c.

Imports the REAL agent.billing_links module and calls the genuine functions.
Reads the fixture from argv[1] (one op per line). For build_billing_block we
emit the to_dict() JSON (compact, sorted keys off to match asdict order).
Output contract: one JSON object per line, compact separators, ensure_ascii=False.
"""
import json
import os
import sys

_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
if _ROOT not in sys.path:
    sys.path.insert(0, _ROOT)

import agent.billing_links as bl


def emit(obj):
    sys.stdout.write(json.dumps(obj, separators=(",", ":"), ensure_ascii=False) + "\n")


def main():
    if len(sys.argv) < 2:
        sys.stderr.write("usage: sta_oracle_billing_links.py <cases.in>\n")
        return 2
    fixture = sys.argv[1]
    with open(fixture, "r", encoding="utf-8") as fp:
        for raw in fp:
            line = raw.rstrip("\n")
            if not line.strip() or line.startswith("#"):
                continue
            op, _, rest = line.partition(" ")
            rest = rest.strip()
            if op == "is_nous":
                # is_nous <provider> <base_url>
                parts = rest.split(" ", 1)
                prov = parts[0] if parts else ""
                bu = parts[1] if len(parts) > 1 else ""
                emit({"op": "is_nous", "provider": prov, "base_url": bu,
                      "out": bl.is_nous_inference_route(prov, bu)})
            elif op == "build":
                # build <provider> <base_url> <model> [message]
                parts = rest.split(" ", 3)
                prov = parts[0] if len(parts) > 0 else ""
                bu = parts[1] if len(parts) > 1 else ""
                mdl = parts[2] if len(parts) > 2 else ""
                msg = parts[3] if len(parts) > 3 else ""
                b = bl.build_billing_block(provider=prov, base_url=bu, model=mdl, message=msg)
                emit({"op": "build", "out": b.to_dict()})
            else:
                emit({"op": "unknown", "raw": line})


if __name__ == "__main__":
    sys.exit(main())
