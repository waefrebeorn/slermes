#!/usr/bin/env python3
"""Oracle for gate w18_primaryrun_7e6e15 (agent/primary_runtime_snapshot.py)."""
import importlib.util
import json
import sys
import types
import contextlib
import io
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)

def _load(dotted, rel):
    parts = dotted.split(".")
    for i in range(1, len(parts)):
        pn = ".".join(parts[:i])
        if pn not in sys.modules:
            pkg = types.ModuleType(pn)
            pkg.__path__ = [QUARRY + "/" + "/".join(parts[:i])]
            sys.modules[pn] = pkg
    spec = importlib.util.spec_from_file_location(dotted, QUARRY + "/" + rel)
    m = importlib.util.module_from_spec(spec)
    sys.modules[dotted] = m
    with contextlib.redirect_stderr(io.StringIO()):
        spec.loader.exec_module(m)
    return m

live = _load("agent.primary_runtime_snapshot", "agent/primary_runtime_snapshot.py")

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
    if func not in ("take", "build"):
        continue
    # Controlled seam for the config-reading credential-owner resolver.
    live.runtime_entry_owner = lambda p, rp, bu: r.get("_owner")
    agent = SimpleNamespace(**(r.get("agent") or {}))
    cc = r.get("cc")
    agent.context_compressor = SimpleNamespace(**cc) if isinstance(cc, dict) else None
    if func == "take":
        live._snapshot_primary_runtime(agent)
        py = agent._primary_runtime
    else:
        py = live._build_primary_runtime_snapshot(agent, r.get("api_mode"))
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w18_primaryrun_7e6e15: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
