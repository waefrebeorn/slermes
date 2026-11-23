#!/usr/bin/env python3
"""Differential oracle for tools/tts_tool_local.py (gate w7_ttstoolloc_4467f6).

Loads the LIVE module by path under its package name with the sibling
``tools.tts_tool_delivery`` helpers stubbed. The LRU cache is driven entirely
from fixture state; int64 speaker ids arrive as decimal strings per the port kit.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


pkg = _mod("tools")
pkg.__path__ = [Q + "/tools"]
_mod("tools.tts_tool_delivery",
     _finalize_wav_output=lambda *a, **k: "", _origin=lambda: None,
     _section=lambda cfg, name: (cfg or {}).get(name) or {},
     _wav_sidecar_path=lambda p: p)

SRC = Q + "/tools/tts_tool_local.py"
spec = importlib.util.spec_from_file_location("tools.tts_tool_local", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["tools.tts_tool_local"] = live
spec.loader.exec_module(live)


def _canon(x):
    if x is None: return ("null",)
    if isinstance(x, bool): return ("bool", x)
    if isinstance(x, (int, float)): return ("num", float(x))
    if isinstance(x, str): return ("str", x)
    if isinstance(x, (list, tuple)): return ("list", [_canon(v) for v in x])
    if isinstance(x, dict): return ("obj", sorted((k, _canon(v)) for k, v in x.items()))
    return ("other", repr(x))


def compute(r):
    f = r.get("func")
    if f == "constants":
        return {"DEFAULT_KITTENTTS_MODEL": live.DEFAULT_KITTENTTS_MODEL,
                "DEFAULT_KITTENTTS_VOICE": live.DEFAULT_KITTENTTS_VOICE,
                "DEFAULT_PIPER_VOICE": live.DEFAULT_PIPER_VOICE,
                "_TTS_MODEL_CACHE_MAX": live._TTS_MODEL_CACHE_MAX,
                "_PIPER_ADVANCED_KNOBS": list(live._PIPER_ADVANCED_KNOBS)}
    if f == "voice_default":
        return r.get("voice") or live.DEFAULT_PIPER_VOICE
    if f == "cache":
        cache = {k: v for k, v in (r.get("entries") or [])}
        live._TTS_MODEL_CACHE_MAX = r.get("max", 3)
        loaded = [False]
        def load():
            loaded[0] = True
            return r.get("load_value")
        val = live._tts_cache_get_or_load(cache, r.get("key", ""), load)
        return {"value": val, "cache": [[k, v] for k, v in cache.items()], "loaded": loaded[0]}
    if f == "piper_cache_key":
        return "%s::cuda=%s" % (r.get("model_path", ""), bool(r.get("use_cuda", False)))
    if f == "speaker_id":
        raw = r.get("raw")
        if isinstance(raw, str):
            raw = int(raw)  # decimal-string seam for int64
        return raw if type(raw) is int else 0
    if f == "advanced":
        return any(k in set(r.get("keys") or []) for k in live._PIPER_ADVANCED_KNOBS)
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        want = compute(r)
    except Exception as exc:
        mism += 1
        print("LIVE ERROR %s case=%s: %r" % (r.get("func"), r.get("case"), exc), file=sys.stderr)
        continue
    got = r.get("out")
    if _canon(want) != _canon(got):
        mism += 1
        print("MISMATCH %s case=%s C=%r PY=%r" % (r.get("func"), r.get("case"), got, want),
              file=sys.stderr)
print("ORACLE w7_ttstoolloc_4467f6: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
