#!/usr/bin/env python3
"""Differential oracle for theme_bridge.py (gate themeb)."""
import importlib.util, json, os, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"; sys.path.insert(0, Q)
def stub_pkg(name):
    if name not in sys.modules:
        m = types.ModuleType(name); m.__path__ = [os.path.join(Q, *name.split("."))]; sys.modules[name] = m
stub_pkg("tui_gateway")
srv = types.ModuleType("tui_gateway.server")
srq = types.ModuleType("tui_gateway.server_requests")
sys.modules["tui_gateway.server"] = srv
sys.modules["tui_gateway.server_requests"] = srq
sys.modules["tui_gateway"].server_requests = srq
name = "theme_bridge_live"
sp = importlib.util.spec_from_file_location(name, os.path.join(Q, "tui_gateway/theme_bridge.py"))
live = importlib.util.module_from_spec(sp); sys.modules[name] = live; sp.loader.exec_module(live)

class FakeTime:
    def __init__(self, elapsed): self.elapsed = elapsed; self.n = 0
    def monotonic(self):
        self.n += 1
        return 0.0 if self.n == 1 else self.elapsed

def compute(r):
    answer = r.get("answer")
    result = r.get("result")
    live.time = FakeTime(r.get("elapsed", 0.0))
    srv._ask = lambda *a, **kw: answer
    srq.send = lambda *a, **kw: result
    return live.theme_request(r.get("sid", "s1"), dict(r.get("payload", {})))

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try: r = json.loads(line)
    except Exception: continue
    if r.get("func") is None: continue
    cases += 1
    py, got = compute(r), r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH themeb py=%r c=%r" % (py, got), file=sys.stderr)
print("ORACLE themeb: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
