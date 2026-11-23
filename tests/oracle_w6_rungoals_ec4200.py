#!/usr/bin/env python3
"""Oracle for gateway/run_goals.py (w6_rungoals_ec4200)."""
import importlib.util, json, sys, os, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = r"C:\Users\eman5\hermes-agent"
sys.path.insert(0, QUARRY)

def load(name, rel):
    parent = name.rsplit(".", 1)[0]
    if parent not in sys.modules:
        __import__(parent)
    spec = importlib.util.spec_from_file_location(name, os.path.join(QUARRY, *rel))
    m = importlib.util.module_from_spec(spec)
    sys.modules[name] = m
    spec.loader.exec_module(m)
    return m

live = load("gateway.run_goals", ["gateway", "run_goals.py"])
import hermes_cli.config as hcfg
hcfg.load_config = lambda: {}   # deterministic fallback when the goals block is falsy

class _Self: pass

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try: r = json.loads(line)
    except Exception: continue
    func = r.get("func")
    try:
        if func == "final_text":
            kind, v = r.get("agent_kind"), r.get("value")
            if kind == "dict": a = v   # fixture value IS the agent_result dict
            elif kind == "str": a = v
            else: a = 0
            ev = None
            if r.get("has_event"):
                ev = types.SimpleNamespace(_streamed_final_response=r.get("streamed"))
            py = live.GatewayGoalsMixin._final_text_for_post_turn_hooks(a, ev)
        elif func == "goal_max_turns":
            s = _Self(); s.config = {"goals": r.get("goals")}
            py = int(live.GatewayGoalsMixin._goal_max_turns_from_config(s))
        else:
            continue
    except Exception as e:
        mism += 1; cases += 1
        print(f"ORACLE w6_rungoals_ec4200 ERROR case={r.get('case')} {e!r}", file=sys.stderr)
        continue
    got = r.get("out"); cases += 1
    if py != got:
        mism += 1
        print(f"MISMATCH w6_rungoals_ec4200 case={r.get('case')} func={func} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w6_rungoals_ec4200: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
