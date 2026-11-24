#!/usr/bin/env python3
"""Differential oracle for w17_pluginscmd_207eca / hermes_cli/plugins_cmd_remove.py."""
import importlib.util, json, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
SRC = QUARRY + "/hermes_cli/plugins_cmd_remove.py"
sys.path.insert(0, QUARRY)
if "hermes_cli" not in sys.modules:
    pkg = types.ModuleType("hermes_cli")
    pkg.__path__ = [QUARRY + "/hermes_cli"]
    sys.modules["hermes_cli"] = pkg

spec = importlib.util.spec_from_file_location("rm_live_w17", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["rm_live_w17"] = live
spec.loader.exec_module(live)

class FakePC:
    """Read at call time via live._pc(); only the two refusal branches are reached."""
    def __init__(self, directory, entries):
        self.directory = directory
        self.entries = entries
    def _plugins_dir(self):
        return self.directory
    def _discover_all_plugins(self):
        return self.entries
    def _user_installed_plugin_dir(self, name):
        return None

def compute(r):
    f = r.get("func")
    if f == "dashboard_remove_error":
        live._pc = lambda: FakePC(r.get("plugins_dir", ""), r.get("entries") or [])
        return live.dashboard_remove_user_plugin(r.get("name", ""))
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
        print(f"ORACLE w17_pluginscmd_207eca: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH w17_pluginscmd_207eca {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w17_pluginscmd_207eca: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
