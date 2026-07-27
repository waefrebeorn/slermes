#!/usr/bin/env python3
"""
sta_oracle_battery.py — Python oracle for agent/battery.py,
ported in src/agent/port_battery.c.

Imports the REAL agent.battery module. Drives the pure surface:
battery_category(status), battery_glyph(status), format_battery(status).
The reading source (read_battery/_read_battery_uncached/clear_cache) is not
ported and not exercised.

Fixture line:  status <available> <percent|null> <plugged|null>
  e.g.  status true 82 false
        status false null null
        status true null true
Output: one JSON object per line (compact, ensure_ascii=False so glyphs show).
"""
import json
import os
import sys

_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
if _ROOT not in sys.path:
    sys.path.insert(0, _ROOT)

import agent.battery as bat


def parse_null(s):
    return None if s == "null" else s


def main():
    if len(sys.argv) < 2:
        sys.stderr.write("usage: sta_oracle_battery.py <cases.in>\n")
        return 2
    with open(sys.argv[1], "r", encoding="utf-8") as fp:
        for raw in fp:
            line = raw.rstrip("\n")
            if not line.strip() or line.startswith("#"):
                continue
            op, _, rest = line.partition(" ")
            if op != "status":
                continue
            parts = rest.split(" ")
            available = parts[0] == "true"
            pct = parse_null(parts[1]) if len(parts) > 1 else None
            plugged = parse_null(parts[2]) if len(parts) > 2 else None
            pct_v = None if pct is None else int(pct)
            pl_v = None if plugged is None else (plugged == "true")

            from dataclasses import replace
            status = bat.BatteryStatus(
                available=available,
                percent=pct_v,
                plugged=pl_v,
            )
            out = {
                "available": status.available,
                "percent": status.percent,
                "plugged": status.plugged,
                "category": bat.battery_category(status),
                "glyph": bat.battery_glyph(status),
                "format": bat.format_battery(status),
            }
            sys.stdout.write(json.dumps(out, separators=(",", ":"),
                                        ensure_ascii=False) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
