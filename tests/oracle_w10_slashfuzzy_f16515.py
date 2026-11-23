#!/usr/bin/env python3
"""Differential oracle for gate w10_slashfuzzy_f16515 (tui_gateway/slash_fuzzy.py)."""
import importlib.util, json, math, os, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)
_tg = types.ModuleType("tui_gateway")
_tg.__path__ = [os.path.join(Q, "tui_gateway")]
sys.modules["tui_gateway"] = _tg
NAME = "w10_slashfuzzy_f16515_live"
sp = importlib.util.spec_from_file_location(NAME, os.path.join(Q, "tui_gateway", "slash_fuzzy.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)

def compute(r):
    f = r.get("func")
    if f == "tokenize":
        return live.tokenize_search_text(r.get("value", ""))
    if f == "normalize":
        return live.normalize_slash_search_query(r.get("query", ""))
    if f == "score":
        v = live.score_slash_completion_item(r.get("item", {}), r.get("query", ""))
        return "inf" if (isinstance(v, float) and math.isinf(v)) else v
    if f == "rank":
        return live.fuzzy_rank_slash_items(r.get("items", []), r.get("catalog", []), r.get("query", ""))[0]
    return None

n = m = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if not isinstance(r, dict) or "out" not in r:
        continue
    n += 1
    py, got = compute(r), r.get("out")
    if py != got:
        m += 1
        print("MISMATCH %s py=%r c=%r" % (r.get("func"), py, got), file=sys.stderr)
print("ORACLE w10_slashfuzzy_f16515: %d cases, %d mismatches" % (n, m))
sys.exit(1 if m else 0)
