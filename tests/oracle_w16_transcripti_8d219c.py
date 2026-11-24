#!/usr/bin/env python3
"""Differential oracle for gate w16_transcripti_8d219c (tools/transcription_chunking.py)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


def _pkg(name):
    m = _mod(name)
    m.__path__ = []
    return m


tools = _pkg("tools")
_mod("tools.tts_command_provider", _get_provider_section=lambda *a, **k: {})
_mod("tools.transcription_audio", _find_ffmpeg_binary=lambda: None,
     _probe_audio_duration=lambda p: None, _run_quiet=lambda *a, **k: None,
     _transcode_audio_for_stt=lambda *a: (None, ""))
_mod("tools.voice_mode_transcript", is_whisper_hallucination=lambda t: False)

cspec = importlib.util.spec_from_file_location(
    "tools.transcription_common", Q + "/tools/transcription_common.py")
common = importlib.util.module_from_spec(cspec)
sys.modules["tools.transcription_common"] = common
cspec.loader.exec_module(common)

NAME = "w16tck_live"
spec = importlib.util.spec_from_file_location(NAME, Q + "/tools/transcription_chunking.py")
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)

G = "w16_transcripti_8d219c"


def compute(r):
    inp = r.get("in") or {}
    func = r.get("func")
    size = float(inp.get("size", 0))
    live.os = types.SimpleNamespace(path=types.SimpleNamespace(getsize=lambda p: size))
    if func == "upload_limit":
        limit = live.upload_limit(inp.get("provider", ""), inp.get("model"))
        return {"max_bytes": limit[0], "max_seconds": limit[1]}
    if func == "fits":
        return live._fits("/x", (inp.get("max_bytes"), inp.get("max_seconds")),
                          inp.get("duration"))
    if func == "exceeds":
        live._probe_audio_duration = lambda p: inp.get("probe_duration")
        return live.exceeds_upload_limit(
            "/x", (inp.get("max_bytes"), inp.get("max_seconds")))
    if func == "cut_points":
        return live._cut_points(float(inp.get("duration", 0)), float(inp.get("target", 0)),
                                inp.get("silences") or [])
    if func == "too_large":
        return live._too_large("/x", inp.get("max_bytes"), inp.get("reason", ""))
    return None


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
        mism += 1
        print(f"ORACLE {G}: live raised {exc!r} case={r.get('case')}", file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {G} {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
