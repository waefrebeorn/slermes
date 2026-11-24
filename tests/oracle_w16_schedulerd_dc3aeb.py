#!/usr/bin/env python3
"""Differential oracle for gate w16_schedulerd_dc3aeb (cron/scheduler_delivery_room.py)."""
import importlib.util, json, logging, os, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
logging.disable(logging.CRITICAL)
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path: sys.path.insert(0, Q)
def _mod(name):
    m = types.ModuleType(name)
    m.__path__ = [os.path.join(Q, name)]
    sys.modules[name] = m
    return m
_mod("cron")
NAME = "w16_schedulerd_dc3aeb_live"
sp = importlib.util.spec_from_file_location(NAME, os.path.join(Q, "cron", "scheduler_delivery_room.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)

def canon(v):
    if isinstance(v, (list, tuple)): return [canon(x) for x in v]
    if isinstance(v, dict): return {k: canon(x) for k, x in v.items()}
    return v

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try: r = json.loads(line)
    except Exception: continue
    func = r.get("func")
    try:
        if func == "parse_room_deliver_token":
            py = live.parse_room_deliver_token(r.get("part"))
        elif func == "resolve_room_target":
            py = live.resolve_room_target(r.get("job") or {}, r.get("address"))
        elif func == "profile_hint":
            py = live._profile_hint(r.get("room_id"), r.get("profile"))
        else: continue
        py = canon(py)
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr); continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w16_schedulerd_dc3aeb: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
