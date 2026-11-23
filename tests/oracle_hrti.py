#!/usr/bin/env python3
"""Oracle for gateway/hosted_room_task_input.py (validate_task_input).
int64 fields cross as decimal strings; the oracle restores Python ints so the
LIVE validator sees exactly what it expects, and compares exact ValueError text.
"""
import importlib.util, json, sys
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
SRC = "C:/Users/eman5/hermes-agent/gateway/hosted_room_task_input.py"
spec = importlib.util.spec_from_file_location("hrti_live", SRC)
live = importlib.util.module_from_spec(spec); spec.loader.exec_module(live)

def to_int(v):
    try:
        return int(v)
    except (TypeError, ValueError):
        return v

def canon(o):
    return json.dumps(o, sort_keys=True, ensure_ascii=False, separators=(",", ":"))

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
    if func == "max_events":
        py = live.MAX_TASK_INPUT_EVENTS
        got = r.get("out")
    elif func == "validate":
        w = r.get("value")
        if isinstance(w, dict):
            inp = dict(w)
            inp["watermark"] = to_int(w.get("watermark"))
            seqs = w.get("event_seqs")
            if isinstance(seqs, list):
                inp["event_seqs"] = [to_int(x) for x in seqs]
        else:
            inp = w
        try:
            res = live.validate_task_input(inp)
            py = {"watermark": str(res["watermark"]),
                  "event_seqs": [str(x) for x in res["event_seqs"]]}
        except ValueError as e:
            py = {"error": str(e)}
        got = r.get("out")
    else:
        continue
    cases += 1
    if func == "max_events":
        ok = py == got
    else:
        ok = canon(py) == canon(got)
    if not ok:
        mism += 1
        print(f"MISMATCH hrti case={r.get('case')} py={py} c={got}", file=sys.stderr)
print(f"ORACLE hrti: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
