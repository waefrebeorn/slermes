#!/usr/bin/env python3
"""Differential oracle for tools/discord_tool.py (gate w6_discordtool_3ef112)."""
import importlib.util
import json
import sys

QUARRY = "C:/Users/eman5/hermes-agent"
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)

SRC = QUARRY + "/tools/discord_tool.py"
spec = importlib.util.spec_from_file_location("w6_discordtool_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


def compute(r):
    func = r.get("func")
    if func == "validate":
        try:
            live._validate_snowflake(r.get("value", ""), r.get("param_name", "guild_id"))
            return None
        except ValueError as exc:
            return str(exc)
    if func == "channel_type":
        return live._channel_type_name(int(r.get("type_id", "0")))
    if func == "limit":
        return live._limit_param(r.get("limit", ""), int(r.get("default_value", "50")))
    if func == "enrich_403":
        return live._enrich_403(r.get("action", ""), r.get("body", ""))
    if func == "token_key":
        return live._token_cache_key(r.get("token", ""))
    if func == "available":
        return live._available_actions(r.get("caps", {}), r.get("allowlist"))
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        mism += 1
        print("LIVE ERROR %s %r" % (r.get("func"), exc), file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)

print("ORACLE w6_discordtool_3ef112: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
