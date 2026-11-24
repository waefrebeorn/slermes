#!/usr/bin/env python3
"""Differential oracle for w17_pluginscmd_645b9b / hermes_cli/plugins_cmd_toggle.py."""
import importlib.util, json, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
SRC = QUARRY + "/hermes_cli/plugins_cmd_toggle.py"
sys.path.insert(0, QUARRY)
if "hermes_cli" not in sys.modules:
    pkg = types.ModuleType("hermes_cli")
    pkg.__path__ = [QUARRY + "/hermes_cli"]
    sys.modules["hermes_cli"] = pkg

spec = importlib.util.spec_from_file_location("tog_live_w17", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["tog_live_w17"] = live
spec.loader.exec_module(live)

# The live module late-binds _entry_status from this sibling and _pc() for the
# category-active set. The fake _entry_status mirrors plugins_cmd._plugin_status
# with dir_path=None (fixtures keep that slot null, so no filesystem probe).
listing = types.ModuleType("hermes_cli.plugins_cmd_listing")
def _entry_status(entry, enabled, disabled, active):
    name, _ver, _desc, _source, dir_path, key = entry
    names = {name, key}
    if names & disabled:
        return "disabled"
    if names & enabled or names & active:
        return "enabled"
    if dir_path is not None:
        raise AssertionError("fixtures use null dir_path")
    return "not enabled"
listing._entry_status = _entry_status
sys.modules["hermes_cli.plugins_cmd_listing"] = listing

class FakePC:
    def __init__(self, active):
        self.active = set(active)
    def _category_active_names(self):
        return self.active

def compute(r):
    f = r.get("func")
    if f == "effective_selection":
        live._pc = lambda: FakePC(r.get("active") or [])
        chosen = live._effective_plugin_selection(
            r.get("entries") or [], set(r.get("enabled") or []), set(r.get("disabled") or []))
        return sorted(chosen)
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
        print(f"ORACLE w17_pluginscmd_645b9b: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH w17_pluginscmd_645b9b {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w17_pluginscmd_645b9b: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
