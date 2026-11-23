#!/usr/bin/env python3
"""Oracle for upstream Hermes gateway/platforms/base_media_flags.py (gate mediapl)."""
import importlib.util, json, sys, types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

# The live module imports MessageEvent only for typing; stub the import so
# loading is side-effect free.
for _n in ("gateway", "gateway.platforms"):
    if _n not in sys.modules:
        _p = types.ModuleType(_n); _p.__path__ = []; sys.modules[_n] = _p
_ev = types.ModuleType("gateway.platforms.event")
_ev.MessageEvent = type("MessageEvent", (), {})
sys.modules["gateway.platforms.event"] = _ev

SRC = "C:/Users/eman5/hermes-agent/gateway/platforms/base_media_flags.py"
spec = importlib.util.spec_from_file_location("mediapl_live", SRC)
live = importlib.util.module_from_spec(spec); spec.loader.exec_module(live)

class Event:
    def __init__(self, d):
        self.media_text_inlined = d.get("media_text_inlined", [])
        self.media_urls = d.get("media_urls", [])

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try: r = json.loads(line)
    except Exception: continue
    if r.get("func") != "pad_inline_flags": continue
    py = live.pad_inline_flags(Event(r.get("event") or {}))
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE mediapl: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
