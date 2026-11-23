#!/usr/bin/env python3
"""Oracle for upstream Hermes pm/testenv.py (gate w5_testenv_ca5da8).

pm/testenv.py imports pm.environments at module load; a stub pm package +
stub pm.environments are registered in sys.modules (parent-package bootstrap)
so the pure helpers load without pulling the dependency machinery.
"""
import importlib.util, json, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path: sys.path.insert(0, Q)

_pkg = types.ModuleType("pm"); _pkg.__path__ = [Q + "/pm"]
sys.modules["pm"] = _pkg
_env = types.ModuleType("pm.environments")
_env.install_state_dir = lambda p: p
sys.modules["pm.environments"] = _env

spec = importlib.util.spec_from_file_location("live_testenv_w5", Q + "/pm/testenv.py")
live = importlib.util.module_from_spec(spec)
sys.modules["live_testenv_w5"] = live
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
    func = r.get("func")
    if func == "constants":
        py = {"name": live.NAME, "default_extras": list(live.DEFAULT_TEST_EXTRAS),
              "groups": list(live.GROUPS), "developer_tools": list(live.DEVELOPER_TOOLS)}
    elif func == "devtools":
        py = live.developer_tools(r.get("pinned", []))
    elif func == "parse":
        py = live.parse_extras(r.get("value", ""))
    else:
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w5_testenv_ca5da8: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
