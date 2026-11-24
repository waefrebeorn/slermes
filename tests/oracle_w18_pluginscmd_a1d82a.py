#!/usr/bin/env python3
"""Differential oracle for w18_pluginscmd_a1d82a / hermes_cli/plugins_cmd_install.py."""
import importlib.util, json, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
SRC = QUARRY + "/hermes_cli/plugins_cmd_install.py"
sys.path.insert(0, QUARRY)
if "hermes_cli" not in sys.modules:
    pkg = types.ModuleType("hermes_cli")
    pkg.__path__ = [QUARRY + "/hermes_cli"]
    sys.modules["hermes_cli"] = pkg

# Stub the two module-level imports so the live module loads without pulling
# the whole CLI output / install-phase chain (neither is used by the port).
co = types.ModuleType("hermes_cli.cli_output"); co.line_input = input
sys.modules["hermes_cli.cli_output"] = co
iph = types.ModuleType("hermes_cli.plugin_install_phase")
iph.InstallPhase = type("InstallPhase", (), {})
sys.modules["hermes_cli.plugin_install_phase"] = iph

spec = importlib.util.spec_from_file_location("inst_live_w18", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["inst_live_w18"] = live
spec.loader.exec_module(live)

def compute(r):
    f = r.get("func")
    if f == "known_issue_warnings":
        entry = types.SimpleNamespace(known_issues=r.get("known_issues", []))
        return live._known_issue_warnings(entry)
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
        print(f"ORACLE w18_pluginscmd_a1d82a: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH w18_pluginscmd_a1d82a {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w18_pluginscmd_a1d82a: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
