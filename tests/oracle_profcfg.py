#!/usr/bin/env python3
"""Oracle for hermes_cli/profiles_config.py:_migrate_profile_config_if_outdated (gate profcfg).

The live function is driven with stubbed config_backend / hermes_constants /
hermes_cli.config dependencies so the real guard ordering executes and the
observable decision (whether migrate_config was attempted) can be compared to
the pure C decision core.  No real filesystem mutations occur.
"""
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
SRC = QUARRY + "/hermes_cli/profiles_config.py"
spec = importlib.util.spec_from_file_location("profcfg_live", SRC)
live = importlib.util.module_from_spec(spec)
spec.loader.exec_module(live)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") != "migrate":
        continue

    live.supports_file_tooling = lambda: bool(r.get("supports", False))
    live.config_exists = lambda path: bool(r.get("exists", False))

    attempts = []
    hc = types.ModuleType("hermes_constants")
    hc.set_hermes_home_override = lambda path: "token"
    hc.reset_hermes_home_override = lambda token: None
    sys.modules["hermes_constants"] = hc

    cc = types.ModuleType("hermes_cli.config")
    def check_config_version(cur=r.get("cur", 0), latest=r.get("latest", 0)):
        return (cur, latest)
    def migrate_config(**kwargs):
        attempts.append(("migrate", kwargs))
    cc.check_config_version = check_config_version
    cc.migrate_config = migrate_config
    sys.modules["hermes_cli.config"] = cc

    live._migrate_profile_config_if_outdated(Path("C:/fake/profile"))
    py = 1 if attempts else 0
    got = r.get("out")
    cases += 1
    if py != int(bool(got)):
        mism += 1
        print(f"MISMATCH {r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE profcfg: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
