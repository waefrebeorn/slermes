#!/usr/bin/env python3
"""Differential oracle for upstream Hermes shared_profile_warning.py."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

# Stub the two external dependencies before executing the live module.
state = {"own": "", "home": "", "ledger": []}

_cli = types.ModuleType("hermes_cli"); _cli.__path__ = []
_pi = types.ModuleType("hermes_cli.process_identity")
_pi.install_id = lambda project_root: state["own"]
_pi.ledger_entries = lambda **kwargs: state["ledger"]
_const = types.ModuleType("hermes_constants")
_const.hermes_home_key = lambda home: state["home"]
sys.modules["hermes_cli"] = _cli
sys.modules["hermes_cli.process_identity"] = _pi
sys.modules["hermes_constants"] = _const

SRC = "C:/Users/eman5/hermes-agent/hermes_cli/shared_profile_warning.py"
spec = importlib.util.spec_from_file_location("shrprof_live", SRC)
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
    if r.get("func") != "shared_profile_warning":
        continue
    state["own"] = r.get("own_install", "")
    state["home"] = r.get("home_key", "")
    state["ledger"] = r.get("ledger", [])
    py = live.shared_profile_warning(home=None, project_root=None)
    got = r.get("out")
    cases += 1
    if py != got:
        mism += 1
        print(f"MISMATCH in={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE shrprof: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
