#!/usr/bin/env python3
"""Differential oracle for w18_pluginscmd_1fa477 / hermes_cli/plugins_cmd_listing.py."""
import importlib.util, json, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
SRC = QUARRY + "/hermes_cli/plugins_cmd_listing.py"
sys.path.insert(0, QUARRY)
if "hermes_cli" not in sys.modules:
    pkg = types.ModuleType("hermes_cli")
    pkg.__path__ = [QUARRY + "/hermes_cli"]
    sys.modules["hermes_cli"] = pkg

spec = importlib.util.spec_from_file_location("listing_live_w18", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["listing_live_w18"] = live
spec.loader.exec_module(live)

# _filter_plugin_entries / _entry_status late-bind the facade. Inject a light
# facade whose _plugin_status is exactly the name->status map supplied by the
# fixture, so only the filter pipeline under test varies.
_status_map = {}
facade = types.ModuleType("hermes_cli.plugins_cmd")
facade._category_active_names = lambda: set()
def _plugin_status(name, enabled, disabled, key, **kwargs):
    return _status_map.get(name, "-")
facade._plugin_status = _plugin_status
sys.modules["hermes_cli.plugins_cmd"] = facade
live._pc = lambda: facade

def compute(r):
    global _status_map
    f = r.get("func")
    if f == "filter_entries":
        _status_map = r.get("status_map", {})
        args = types.SimpleNamespace(
            no_bundled=r.get("no_bundled", False),
            user=r.get("user", False),
            enabled=r.get("enabled", False))
        return live._filter_plugin_entries(r.get("entries", []), args, set(), set())
    return None

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None: continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        print(f"ORACLE w18_pluginscmd_1fa477: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH w18_pluginscmd_1fa477 {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w18_pluginscmd_1fa477: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
