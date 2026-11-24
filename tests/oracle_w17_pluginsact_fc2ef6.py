#!/usr/bin/env python3
"""Differential oracle for w17_pluginsact_fc2ef6 / hermes_cli/plugins_activation.py."""
import importlib.util, json, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
SRC = QUARRY + "/hermes_cli/plugins_activation.py"
sys.path.insert(0, QUARRY)
if "hermes_cli" not in sys.modules:
    pkg = types.ModuleType("hermes_cli")
    pkg.__path__ = [QUARRY + "/hermes_cli"]
    sys.modules["hermes_cli"] = pkg

spec = importlib.util.spec_from_file_location("act_live_w17", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["act_live_w17"] = live
spec.loader.exec_module(live)

def compute(r):
    f = r.get("func")
    if f == "find_activation":
        return live.find_activation(r.get("summaries"), r.get("name", ""))
    if f == "gateway_reload_hint":
        return live.gateway_reload_hint(r.get("answer"), r.get("name", ""))
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
        print(f"ORACLE w17_pluginsact_fc2ef6: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH w17_pluginsact_fc2ef6 {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w17_pluginsact_fc2ef6: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
