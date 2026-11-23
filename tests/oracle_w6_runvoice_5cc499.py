#!/usr/bin/env python3
"""Oracle for gateway/run_voice.py (w6_runvoice_5cc499)."""
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

live = load("gateway.run_voice", ["gateway", "run_voice.py"])
class _V: pass
vobj = _V()

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try: r = json.loads(line)
    except Exception: continue
    func = r.get("func")
    try:
        if func == "voice_key":
            py = live.GatewayVoiceMixin._voice_key(vobj, live.Platform(r["platform"]), r["chat_id"], r.get("profile"))
        elif func == "is_legacy":
            py = bool(live._is_legacy_voice_key(r["key"]))
        else:
            continue
    except Exception as e:
        mism += 1; cases += 1
        print(f"ORACLE w6_runvoice_5cc499 ERROR case={r.get('case')} {e!r}", file=sys.stderr)
        continue
    got = r.get("out"); cases += 1
    if py != got:
        mism += 1
        print(f"MISMATCH w6_runvoice_5cc499 case={r.get('case')} func={func} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w6_runvoice_5cc499: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
