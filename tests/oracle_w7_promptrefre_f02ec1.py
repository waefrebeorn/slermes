#!/usr/bin/env python3
"""Differential oracle for agent/prompt_refresh.py (w7_promptrefre_f02ec1 gate)."""
import importlib.util, json, sys, types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
PYROOT = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, PYROOT)
SRC = PYROOT + "/agent/prompt_refresh.py"
spec = importlib.util.spec_from_file_location("live_prompt_refresh_w7", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)

class DB:
    def __init__(self, epoch):
        self.epoch = epoch
    def prompt_refresh_epoch(self, sid):
        return self.epoch

class Agent:
    def __init__(self, spec):
        present = spec.get("reader_present", False)
        self._session_db = DB(spec.get("epoch")) if present else None
        self.session_id = spec.get("session_id")

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    func = r.get("func")
    try:
        if func == "key":
            py = live.SESSION_EPOCH_KEY
        elif func == "applied":
            py = live._applied_epoch(r.get("session_row"))
        elif func == "requested":
            py = live.requested_epoch(Agent(r))
        elif func == "stored":
            agent = Agent(r)
            ret = live.stored_prompt_refresh_epoch(agent, r.get("session_row"))
            stamped = getattr(agent, "_prompt_refresh_epoch", None)
            py = {"ret": ret, "stamped": stamped}
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w7_promptrefre_f02ec1: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
