#!/usr/bin/env python3
"""Differential oracle for gate w10_gatewayque_6a8e6b (hermes_cli/gateway_queue.py)."""
import importlib.util, json, os, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path: sys.path.insert(0, Q)
def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items(): setattr(m, k, v)
    sys.modules[name] = m
    return m
h = _mod("hermes_cli"); h.__path__ = [os.path.join(Q, "hermes_cli")]
NAME = "w10_gatewayque_6a8e6b_live"
sp = importlib.util.spec_from_file_location(NAME, os.path.join(Q, "hermes_cli", "gateway_queue.py"))
live = importlib.util.module_from_spec(sp); sys.modules[NAME] = live; sp.loader.exec_module(live)
NS = types.SimpleNamespace

def canon(v):
    if isinstance(v, (list, tuple)): return [canon(x) for x in v]
    if isinstance(v, dict): return {k: canon(x) for k, x in v.items()}
    return v

def make_state(d):
    d = d or {}
    return NS(pending=d.get("pending") or [], queued_inputs=d.get("queued_inputs") or {},
              queued_at=d.get("queued_at") or {}, pending_seq=d.get("pending_seq", 0),
              adopted=set(d.get("adopted") or []), missed=set(), info={})

class FakeStore:
    def __init__(self, state, receipts):
        self._state, self.receipts = state, receipts or {}
        self.connection = self
    def state(self, _sid): return self._state
    def call(self, verb, **kw):
        rec = self.receipts.get(kw.get("admission_id"))
        if rec is None or rec.get("error"):
            raise live.GatewayClientError("receipt lost")
        return rec.get("receipt")

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try: r = json.loads(line)
    except Exception: continue
    func = r.get("func")
    try:
        if func == "published_queue":
            st = make_state(r.get("state"))
            py = live.published_queue(NS(session_id="s"), NS(connection=FakeStore(st, {})))
        elif func == "own_head":
            py = live._own_head(make_state(r.get("state")))
        elif func == "queued_rows":
            st = make_state(r.get("state"))
            py = live.queued_rows(NS(session_id="s"), FakeStore(st, r.get("receipts")))
        else: continue
        py = canon(py)
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr); continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w10_gatewayque_6a8e6b: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
