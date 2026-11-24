#!/usr/bin/env python3
"""Oracle for gate w18_contextcom_ac4e03 (agent/context_compressor_prune.py)."""
import importlib.util
import json
import sys
import types
import contextlib
import io

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

live = _load("agent.context_compressor_prune", "agent/context_compressor_prune.py")

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
    if func == "changed_rows":
        py = live._changed_rows(r.get("messages") or [], r.get("pruned") or [])
    elif func == "keep_running_turn":
        res = live._keep_running_turn(r.get("messages") or [], r.get("pruned") or [], r.get("turn_start"))
        py = None if res is None else {"kept": res[0], "restored": res[1]}
    elif func == "would_prune":
        mixin = live.ProactivePruneMixin()
        mixin.proactive_prune_tokens = r.get("tokens", 0)
        py = mixin.would_proactively_prune(r.get("current"))
    elif func == "constants":
        py = {
            "_COLD_CACHE_MIN_RECLAIM_TOKENS": live._COLD_CACHE_MIN_RECLAIM_TOKENS,
            "prune_needs_session_store": live.ProactivePruneMixin.prune_needs_session_store,
        }
    else:
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w18_contextcom_ac4e03: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
