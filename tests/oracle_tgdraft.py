#!/usr/bin/env python3
"""Differential oracle for telegram_draft_budget.py (gate tgdraft)."""
import importlib.util, json, os, sys, types
from collections import deque
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"; sys.path.insert(0, Q)

def stub(name, **attrs):
    parts = name.split(".")
    for i in range(1, len(parts)):
        pn = ".".join(parts[:i])
        if pn not in sys.modules:
            pm = types.ModuleType(pn); pm.__path__ = [os.path.join(Q, *parts[:i])]; sys.modules[pn] = pm
    m = types.ModuleType(name)
    for k, v in attrs.items(): setattr(m, k, v)
    sys.modules[name] = m
    return m

stub("plugins.platforms.telegram.telegram_ids", normalize_telegram_chat_id=lambda x: x)
parts = "plugins/platforms/telegram/telegram_draft_budget".split("/")
for i in range(1, len(parts)):
    pn = ".".join(parts[:i])
    if pn not in sys.modules:
        pm = types.ModuleType(pn); pm.__path__ = [os.path.join(Q, *parts[:i])]; sys.modules[pn] = pm
name = ".".join(parts)
sp = importlib.util.spec_from_file_location(name, os.path.join(Q, "plugins/platforms/telegram/telegram_draft_budget.py"))
live = importlib.util.module_from_spec(sp); sys.modules[name] = live; sp.loader.exec_module(live)
live.normalize_telegram_chat_id = lambda x: "K"

class Loop:
    def __init__(self, now): self.now = now
    def time(self): return self.now

def compute(r):
    now = r.get("now", 0.0)
    live.asyncio = types.SimpleNamespace(get_running_loop=lambda: Loop(now))
    adapter = types.SimpleNamespace()
    adapter.__dict__["_telegram_ai_action_calls"] = {"K": deque(r.get("calls", []))}
    delay = live.claim_ai_action(adapter, 0)
    calls = list(adapter.__dict__["_telegram_ai_action_calls"]["K"])
    return {"delay": delay, "calls": calls}

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try: rr = json.loads(line)
    except Exception: continue
    if rr.get("func") is None: continue
    cases += 1
    r = rr.get("out")
    py, got = compute(rr), r
    if py != got:
        mism += 1
        print("MISMATCH tgdraft py=%r c=%r" % (py, got), file=sys.stderr)
print("ORACLE tgdraft: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
