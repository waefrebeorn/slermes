#!/usr/bin/env python3
"""Differential oracle for tools/stt_lease.py (noc_w5_sttlease_a1f613.h)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
SRC = QUARRY + "/tools/stt_lease.py"
sys.path.insert(0, QUARRY)

if "tools" not in sys.modules:
    _pkg = types.ModuleType("tools")
    _pkg.__path__ = [QUARRY + "/tools"]
    sys.modules["tools"] = _pkg

spec = importlib.util.spec_from_file_location("sttlease_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["sttlease_live"] = live
spec.loader.exec_module(live)


def compute(r):
    provider_arg = r.get("provider_arg")
    config_provider = r.get("config_provider")
    has_fw = bool(r.get("has_fw", False))
    lazy_ok = bool(r.get("lazy_install_ok", False))
    model_name = r.get("model_name", "base")
    hot_before = bool(r.get("hot_before", False))
    load_ok = bool(r.get("load_ok", False))
    load_error = r.get("load_error")
    elapsed = int(r.get("elapsed_ms", 0))

    class TT:
        pass
    tt = TT()
    tt._load_stt_config = lambda: {}
    tt._get_provider = lambda cfg: config_provider
    tt._HAS_FASTER_WHISPER = has_fw
    tt._try_lazy_install_stt = lambda: lazy_ok
    tt._local_model = object() if hot_before else None
    tt._local_model_name = model_name if hot_before else "___other___"

    def get_or_load(name, cfg):
        if load_error is not None:
            raise RuntimeError(load_error)
        return object() if load_ok else None
    tt._get_or_load_local_model = get_or_load
    tt._touch_transcription_time = lambda: None
    tt._get_idle_unload_seconds = lambda cfg: 1
    tt._start_idle_unload_watcher = lambda n: None
    sys.modules["tools.transcription_tools"] = tt

    tl = types.ModuleType("tools.transcription_local")
    tl._normalize_local_model = lambda x: model_name
    sys.modules["tools.transcription_local"] = tl

    seq = iter([0.0, elapsed / 1000.0])

    class FakeTime:
        @staticmethod
        def monotonic():
            return next(seq, elapsed / 1000.0)
    live.time = FakeTime
    return live.warm_stt_provider(stt_config={}, provider=provider_arg)


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        print(f"ORACLE sttlease: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH sttlease {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE sttlease: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
