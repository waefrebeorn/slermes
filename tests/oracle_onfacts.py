#!/usr/bin/env python3
"""Oracle for upstream Hermes agent/onboarding_facts.py.

Host data (summary(), onboarding_names(), check_tours_enabled()) is stubbed
from the fixture; the C port reproduces the pure mapping/default logic in
read_facts() and suggested_name().
"""
import importlib.util, json, sys
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path: sys.path.insert(0, Q)
SRC = Q + "/agent/onboarding_facts.py"
spec = importlib.util.spec_from_file_location("onfacts_live", SRC)
live = importlib.util.module_from_spec(spec); spec.loader.exec_module(live)

import hermes_platform.host.summary as sm
import hermes_cli.plugin_catalog_presence as pcp
import tools.tour_tool as tt

HOST = {}
PLUGINS = []
TOURS = True
def _summary():
    return {
        "machine": {
            "os_family": HOST.get("os_family", ""),
            "native_arch": HOST.get("native_arch", ""),
        },
        "machine_kind": HOST.get("machine_kind", "computer"),
        "nvidia_platform": HOST.get("nvidia_platform"),
        "locale": HOST.get("locale"),
        "full_name": HOST.get("full_name"),
    }
sm.summary = _summary
pcp.onboarding_names = lambda: list(PLUGINS)
tt.check_tours_enabled = lambda: TOURS

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try: r = json.loads(line)
    except Exception: continue
    func = r.get("func")
    if func not in ("read_facts", "suggested_name"): continue
    HOST = {
        "os_family": r.get("os_family", ""),
        "native_arch": r.get("native_arch", ""),
        "machine_kind": r.get("machine_kind", "computer"),
        "nvidia_platform": r.get("nvidia_platform"),
        "locale": r.get("host_locale"),
        "full_name": r.get("full_name"),
    }
    PLUGINS = list(r.get("plugins", []))
    TOURS = bool(r.get("tours", False))
    if func == "read_facts":
        facts = live.read_facts(facts_source=r.get("facts_source", "local"),
                                locale=r.get("locale"))
        py = facts.stored()
    else:
        py = live.suggested_name()
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE onfacts: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
