#!/usr/bin/env python3
"""Oracle for gateway/session_bot_state.py (bot_chat_state).
The live function is imported via importlib; its SessionRef dependency is a
pre-registered stub.  The authority object graph is built from pre-resolved
fixture scalars (retiring, draining, live presence, exec_state, prompt kinds).
"""
import importlib.util, json, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = r"C:\Users\eman5\hermes-agent"

def pre():
    m = types.ModuleType("gateway.session_contract")
    class SessionRef:
        def __init__(self, profile_id, session_id):
            self.profile_id = profile_id; self.session_id = session_id
    m.SessionRef = SessionRef
    sys.modules["gateway.session_contract"] = m
    if "gateway" not in sys.modules:
        sys.modules["gateway"] = types.ModuleType("gateway")

spec = importlib.util.spec_from_file_location("bs_live", QUARRY + r"\gateway\session_bot_state.py")
live = importlib.util.module_from_spec(spec)
pre()
spec.loader.exec_module(live)

class Ctrl:
    def __init__(self, kinds): self._kinds = list(kinds)
    def snapshot(self, session_id, generation):
        return [{"kind": k} for k in self._kinds]
class LiveSession:
    def __init__(self, kinds): self.controls = Ctrl(kinds)
class Handle:
    def __init__(self, state, gen):
        self.execution_state = state; self.execution_generation = gen
class Authority:
    def __init__(self, retiring, draining, live_present, state, gen, kinds):
        self.retiring = retiring
        self.runner = types.SimpleNamespace(_draining=draining)
        self.profile_id = "bot"
        self.sessions = {"s": LiveSession(kinds)} if live_present else {}
        self._h = Handle(state, gen)
    def _handle(self, ref): return self._h

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") != "state":
        continue
    inp = r.get("value") or {}
    try:
        gen = int(inp.get("exec_gen", 0))
    except (TypeError, ValueError):
        gen = 0
    auth = Authority(bool(inp.get("retiring")), bool(inp.get("draining")),
                     bool(inp.get("live")), inp.get("exec_state", ""), gen,
                     inp.get("kinds", []))
    py = live.bot_chat_state(auth, "s")
    got = r.get("out")
    cases += 1
    if py != got:
        mism += 1
        print(f"MISMATCH botstate case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE botstate: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
