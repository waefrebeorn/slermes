#!/usr/bin/env python3
"""Differential oracle for live Hermes gateway/host_topology.py (port w8_hosttopolog_1e949c)."""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
import hermes_cli.profiles  # noqa: F401  (parent-package bootstrap for _normalized's late import)

SRC = QUARRY + "/gateway/host_topology.py"
NAME = "host_topology_live_w8"
spec = importlib.util.spec_from_file_location(NAME, SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)


def compute(r):
    func = r.get("func")
    if func == "normalized":
        return live._normalized(r.get("name"))
    topo = live.HostGatewayTopology(
        pid=int(r.get("pid", 0)), profiles=tuple(r.get("profiles", [])), source="oracle")
    if func == "serves":
        return topo.serves(r.get("wanted"))
    if func == "describe":
        return topo.describe()
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s py=%r c=%r" % (r.get("func"), py, got), file=sys.stderr)
print("ORACLE w8_hosttopolog_1e949c: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
