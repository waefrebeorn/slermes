#!/usr/bin/env python3
"""Differential oracle for agent/stream_max_lifetime.py (gate w11_streammax_7580a7).

Imports the LIVE module with a stub hermes_cli.config seam; the config value
and the stale timeout are injected per case from the fixture (including
Infinity/NaN via string sentinels), so resolution is fully deterministic.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)

def _pkg(name):
    m = sys.modules.get(name)
    if m is None:
        m = types.ModuleType(name); m.__path__ = []; sys.modules[name] = m
    return m

hcli = _pkg("hermes_cli")
CFG = {"agent": {}}
cfgmod = types.ModuleType("hermes_cli.config")
cfgmod.load_config_readonly = lambda: CFG
sys.modules["hermes_cli.config"] = cfgmod
hcli.config = cfgmod

SRC = QUARRY + "/agent/stream_max_lifetime.py"
spec = importlib.util.spec_from_file_location("w11_streammax_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w11_streammax_live"] = live
spec.loader.exec_module(live)


def num(v):
    if v == "__inf__": return float("inf")
    if v == "__ninf__": return float("-inf")
    if v == "__nan__": return float("nan")
    return v


def compute(r):
    func = r.get("func")
    if func == "resolve":
        ac = r.get("agent_cfg")
        if isinstance(ac, dict):
            ac = dict(ac)
            if "stream_max_lifetime" in ac:
                ac["stream_max_lifetime"] = num(ac["stream_max_lifetime"])
        CFG["agent"] = ac
        return live.resolve_stream_max_lifetime(num(r.get("stale_timeout")))
    if func == "message":
        return str(live.StreamLifetimeExceeded(r.get("elapsed"), r.get("cap")))
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        mism += 1
        print(f"MISMATCH case={r.get('case')} func={r.get('func')}: python raised {exc!r}", file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH case={r.get('case')} func={r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
if cases == 0:
    print("ORACLE w11_streammax_7580a7: 0 cases (link failure in disguise)")
    sys.exit(1)
print(f"ORACLE w11_streammax_7580a7: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
