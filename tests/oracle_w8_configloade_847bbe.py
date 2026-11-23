#!/usr/bin/env python3
"""Differential oracle for live Hermes gateway/config_loader.py (port w8_configloade_847bbe)."""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
# Parent-package bootstrap: make the quarry gateway package importable before
# loading the file under a private module name.
import gateway  # noqa: F401

SRC = QUARRY + "/gateway/config_loader.py"
NAME = "config_loader_live_w8_configloade"
spec = importlib.util.spec_from_file_location(NAME, SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)


def compute(r):
    func = r.get("func")
    if func == "bridge_lookup":
        return list(live._bridge_lookup(
            r.get("yaml_cfg", {}), r.get("gateway_section"), r.get("gw_data", {}),
            r.get("key", ""), r.get("mode", "presence")))
    if func == "quick_commands_ok":
        return live._quick_commands_ok(r.get("value"))
    if func == "str_keyed":
        return live._str_keyed(r.get("value"))
    if func == "platform_section":
        return list(live.platform_section(
            r.get("yaml_cfg", {}), r.get("name", ""), r.get("gateway_platforms")))
    if func == "snapshot_authored_extra":
        return live.snapshot_authored_extra(r.get("platforms_data", {}))
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
print("ORACLE w8_configloade_847bbe: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
