#!/usr/bin/env python3
"""Differential oracle for tools/tts_tool_lifecycle.py (gate w11_ttstoolli_234137)."""
import importlib.util
import json
import logging
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")
logging.disable(logging.CRITICAL)

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)

class _Any:
    def __init__(self, n="Any"): self._n = n
    def __call__(self, *a, **k): return _Any(self._n + "()")
    def __getattr__(self, n): return _Any(self._n + "." + n)
    def __iter__(self): return iter(())
    def __len__(self): return 0
    def __bool__(self): return True

for nm in ("hermes_yaml", "ruamel"):
    mod = types.ModuleType(nm)
    mod.__path__ = []
    mod.__getattr__ = lambda n: _Any(nm + "." + n)
    sys.modules[nm] = mod

spec = importlib.util.spec_from_file_location("w11_ttstoolli_live", QUARRY + "/tools/tts_tool_lifecycle.py")
live = importlib.util.module_from_spec(spec)
sys.modules["w11_ttstoolli_live"] = live
spec.loader.exec_module(live)

# Dependency seams: no plugins, no real timers, deterministic origin config.
live._lookup_plugin_provider = lambda name: None

class FakeTimer:
    def __init__(self, *a, **k): self.cancelled = False
    def start(self): pass
    def cancel(self): self.cancelled = True

live._make_keep_warm_timer = FakeTimer
STATE = {"keep": 60.0}

class Origin:
    _SDK_ON_DEMAND = {}
    def _load_tts_config(self):
        return {"keep_warm_seconds": STATE["keep"]}
    def _get_provider(self, cfg):
        return None

live._origin = lambda: Origin()


def seed_caches(sizes):
    live._LOCAL_TTS_MODEL_CACHES.clear()
    for name, count in (sizes or {}).items():
        live._LOCAL_TTS_MODEL_CACHES[name] = {"m%d" % i: i for i in range(int(count))}


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
    got = r.get("out")
    try:
        if func == "keep_warm":
            STATE["keep"] = r.get("raw")
            want = live._keep_warm_seconds()
        elif func == "release":
            seed_caches(r.get("cache_sizes", {}))
            want = live.release_tts_provider(r.get("provider"))
        elif func == "warm":
            want = live.warm_tts_provider({"providers": {}}, r.get("provider"))
        elif func == "lease":
            op = r.get("op", "holders")
            seed_caches(r.get("cache_sizes", {}))
            STATE["keep"] = r.get("keep_warm", 60.0)
            live._reset_tts_leases_for_tests()
            for x in (r.get("leases") or []):
                live._tts_leases.add(x)
            if op == "acquire":
                res = live.acquire_tts_lease(r.get("lease", ""))
            elif op == "release":
                res = live.release_tts_lease(r.get("lease", ""))
            else:
                res = live.tts_lease_holders()
            want = {"result": res, "holders": live.tts_lease_holders()}
        elif func == "warmers":
            want = sorted(live._local_tts_warmers().keys())
        else:
            continue
    except BaseException as ex:
        print("MISMATCH %s oracle raised %s: %s" % (func, type(ex).__name__, ex), file=sys.stderr)
        cases += 1
        mism += 1
        continue
    cases += 1
    if want != got:
        mism += 1
        print("MISMATCH in=%s py=%r c=%r" % (json.dumps(r, ensure_ascii=False), want, got), file=sys.stderr)
print("ORACLE w11_ttstoolli_234137: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or cases == 0 else 0)
