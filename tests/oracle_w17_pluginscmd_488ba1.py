#!/usr/bin/env python3
"""Differential oracle for w17_pluginscmd_488ba1 / hermes_cli/plugins_cmd_update.py."""
import importlib.util, json, sys, types
from pathlib import Path
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
SRC = QUARRY + "/hermes_cli/plugins_cmd_update.py"
sys.path.insert(0, QUARRY)
if "hermes_cli" not in sys.modules:
    pkg = types.ModuleType("hermes_cli")
    pkg.__path__ = [QUARRY + "/hermes_cli"]
    sys.modules["hermes_cli"] = pkg

spec = importlib.util.spec_from_file_location("upd_live_w17", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["upd_live_w17"] = live
spec.loader.exec_module(live)

# _reclone_plugin_update late-binds these. The fake install core publishes a
# target named "p" and a controlled revision so only the message branch varies.
catalog = types.ModuleType("hermes_cli.plugins_cmd_catalog")
catalog._carry_user_files = lambda *a, **k: None
sys.modules["hermes_cli.plugins_cmd_catalog"] = catalog

class NT:
    name = "p"

class FakePC:
    def __init__(self, revision):
        self.revision = revision
    def _install_plugin_core(self, *a, **k):
        return (NT(), None, None)
    def _read_install_metadata(self):
        return {"p": {"revision": self.revision}}

def compute(r):
    f = r.get("func")
    if f == "reclone_message":
        live._pc = lambda: FakePC(r.get("revision", ""))
        return live._reclone_plugin_update(Path("p"), r.get("source", ""),
                                          r.get("previous_revision"))
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
        print(f"ORACLE w17_pluginscmd_488ba1: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH w17_pluginscmd_488ba1 {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w17_pluginscmd_488ba1: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
